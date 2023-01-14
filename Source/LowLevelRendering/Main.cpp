#include "BsApplication.h"
#include "Material/BsMaterial.h"
#include "CoreThread/BsCoreThread.h"
#include "RenderAPI/BsRenderAPI.h"
#include "RenderAPI/BsRenderWindow.h"
#include "RenderAPI/BsCommandBuffer.h"
#include "RenderAPI/BsGpuProgram.h"
#include "RenderAPI/BsGpuPipelineState.h"
#include "RenderAPI/BsBlendState.h"
#include "RenderAPI/BsDepthStencilState.h"
#include "RenderAPI/BsGpuParamBlockBuffer.h"
#include "RenderAPI/BsIndexBuffer.h"
#include "RenderAPI/BsVertexDataDesc.h"
#include "Mesh/BsMeshData.h"
#include "Math/BsQuaternion.h"
#include "Utility/BsTime.h"
#include "Renderer/BsRendererUtility.h"
#include "BsEngineConfig.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This example uses the low-level rendering API to render a textured cube mesh. This is opposed to using scene objects
// and components, in which case objects are rendered automatically based on their transform and other properties.
//
// Using low-level rendering API gives you full control over rendering, similar to using Vulkan, DirectX or OpenGL APIs.
//
// In order to use the low-level rendering system we need to override the Application class so we get notified of updates
// and start-up/shut-down events. This is normally not necessary for a high level scene object based model.
//
// The rendering is performed on the core (i.e. rendering) thread, as opposed to the main thread, where majority of
// bsf's code executes.
//
// The example first sets up necessary resources, like GPU programs, pipeline state, vertex & index buffers. Then every
// frame it binds the necessary rendering resources and executes the draw call.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace bs
{
	u32 windowResWidth = 1280;
	u32 windowResHeight = 720;

	// Declare the methods we'll use to do work on the core thread. Note the "ct" namespace, which we use because we render
	// on the core thread (ct = core thread). Every object usable on the core thread lives in this namespace.
	namespace ct
	{
		void Setup(const SPtr<RenderWindow>& renderWindow);
		void Render();
		void Shutdown();
	} // namespace ct

	// Override the default Application so we can get notified when engine starts-up, shuts-down and when it executes
	// every frame
	class MyApplication : public Application
	{
	public:
		// Pass along the start-up structure to the parent, we don't need to handle it
		MyApplication(const START_UP_DESC& desc)
			: Application(desc)
		{}

	private:
		// Called when the engine is first started up
		void OnStartUp() override
		{
			// Ensure all parent systems are initialized first
			Application::OnStartUp();

			// Get the primary window that was created during start-up. This will be the final destination for all our
			// rendering.
			SPtr<RenderWindow> renderWindow = GetPrimaryWindow();

			// Get the version of the render window usable on the core thread, and send it along to setup()
			SPtr<ct::RenderWindow> renderWindowCore = renderWindow->GetCore();

			// Initialize all the resources we need for rendering. Since we do rendering on a separate thread (the "core
			// thread"), we don't call the method directly, but rather queue it for execution using the CoreThread class.
			GetCoreThread().QueueCommand(std::bind(&ct::Setup, renderWindowCore));
		}

		// Called when the engine is about to be shut down
		void OnShutDown() override
		{
			// Queue the method for execution on the core thread
			GetCoreThread().QueueCommand(&ct::Shutdown);

			// Shut-down engine components
			Application::OnShutDown();
		}

		// Called every frame, before any other engine system (optionally use postUpdate())
		void PreUpdate() override
		{
			// Queue the method for execution on the core thread
			GetCoreThread().QueueCommand(&ct::Render);

			// Call the default version of this method to handle normal functionality
			Application::PreUpdate();
		}
	};
} // namespace bs

// Main entry point into the application
#if B3D_PLATFORM == B3D_PLATFORM_ID_WIN32
#	include <windows.h>

int CALLBACK WinMain(
	_In_ HINSTANCE hInstance,
	_In_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nCmdShow)
#else
int main()
#endif
{
	using namespace bs;

	// Define a video mode for the resolution of the primary rendering window.
	VideoMode videoMode(windowResWidth, windowResHeight);

	// Start-up the engine using our custom MyApplication class. This will also create the primary rendering window.
	// We provide the initial resolution of the window, its title and fullscreen state.
	Application::StartUp<MyApplication>(videoMode, "bsf Example App", false);

	// Runs the main loop that does most of the work. This method will exit when user closes the main
	// window or exits in some other way.
	Application::Instance().RunMainLoop();

	// Clean up when done
	Application::ShutDown();

	return 0;
}

namespace bs
{
	namespace ct
	{
		// Declarations for some helper methods we'll use during setup
		void WriteBoxVertices(const AABox& box, u8* positions, u8* uvs, u32 stride);
		void WriteBoxIndices(u32* indices);
		const char* GetVertexProgSource();
		const char* GetFragmentProgSource();
		Matrix4 CreateWorldViewProjectionMatrix();

		// Fields where we'll store the resources required during calls to render(). These are initialized in setup()
		// and cleaned up in shutDown()
		SPtr<GraphicsPipelineState> gPipelineState;
		SPtr<Texture> gSurfaceTex;
		SPtr<SamplerState> gSurfaceSampler;
		SPtr<GpuParams> gGpuParams;
		SPtr<VertexDeclaration> gVertexDecl;
		SPtr<VertexBuffer> gVertexBuffer;
		SPtr<IndexBuffer> gIndexBuffer;
		SPtr<RenderTexture> gRenderTarget;
		SPtr<RenderWindow> gRenderWindow;
		bool gUseHLSL = true;
		bool gUseVKSL = false;

		const u32 NUM_VERTICES = 24;
		const u32 NUM_INDICES = 36;

		// Structure that will hold uniform block variables for the GPU programs
		struct UniformBlock
		{
			Matrix4 GMatWvp; // World view projection matrix
			Color GTint; // Tint to apply on top of the texture
		};

		// Initializes any resources required for rendering
		void Setup(const SPtr<RenderWindow>& renderWindow)
		{
			// Determine which shading language to use (depending on the RenderAPI chosen during build)
			gUseHLSL = strcmp(B3D_RENDER_BACKEND, "bsfD3D11RenderAPI") == 0;
			gUseVKSL = strcmp(B3D_RENDER_BACKEND, "bsfVulkanRenderAPI") == 0;

			// This will be the primary output for our rendering (created by the main thread on start-up)
			gRenderWindow = renderWindow;

			// Create a vertex GPU program
			const char* vertProgSrc = GetVertexProgSource();

			GpuProgramCreateInformation vertProgDesc;
			vertProgDesc.Type = GPT_VERTEX_PROGRAM;
			vertProgDesc.EntryPoint = "main";
			vertProgDesc.Language = gUseHLSL ? "hlsl" : gUseVKSL ? "vksl"
																 : "glsl4_1";
			vertProgDesc.Source = vertProgSrc;

			SPtr<GpuProgram> vertProg = GpuProgram::Create(vertProgDesc);

			// Create a fragment GPU program
			const char* fragProgSrc = GetFragmentProgSource();

			GpuProgramCreateInformation fragProgDesc;
			fragProgDesc.Type = GPT_FRAGMENT_PROGRAM;
			fragProgDesc.EntryPoint = "main";
			fragProgDesc.Language = gUseHLSL ? "hlsl" : gUseVKSL ? "vksl"
																 : "glsl4_1";
			fragProgDesc.Source = fragProgSrc;

			SPtr<GpuProgram> fragProg = GpuProgram::Create(fragProgDesc);

			// Create a graphics pipeline state
			BLEND_STATE_DESC blendDesc;
			blendDesc.RenderTargetDesc[0].BlendEnable = true;
			blendDesc.RenderTargetDesc[0].RenderTargetWriteMask = 0b0111; // RGB, don't write to alpha
			blendDesc.RenderTargetDesc[0].BlendOp = BO_ADD;
			blendDesc.RenderTargetDesc[0].SrcBlend = BF_SOURCE_ALPHA;
			blendDesc.RenderTargetDesc[0].DstBlend = BF_INV_SOURCE_ALPHA;

			DEPTH_STENCIL_STATE_DESC depthStencilDesc;
			depthStencilDesc.DepthWriteEnable = false;
			depthStencilDesc.DepthReadEnable = false;

			PIPELINE_STATE_DESC pipelineDesc;
			pipelineDesc.BlendState = BlendState::Create(blendDesc);
			pipelineDesc.DepthStencilState = DepthStencilState::Create(depthStencilDesc);
			pipelineDesc.VertexProgram = vertProg;
			pipelineDesc.FragmentProgram = fragProg;

			gPipelineState = GraphicsPipelineState::Create(pipelineDesc);

			// Create an object containing GPU program parameters
			gGpuParams = GpuParams::Create(gPipelineState);

			// Create a vertex declaration for shader inputs
			SPtr<VertexDataDesc> vertexDesc = VertexDataDesc::Create();
			vertexDesc->AddVertElem(VET_FLOAT3, VES_POSITION);
			vertexDesc->AddVertElem(VET_FLOAT2, VES_TEXCOORD);

			gVertexDecl = VertexDeclaration::Create(vertexDesc);

			// Create & fill the vertex buffer for a box mesh
			u32 vertexStride = vertexDesc->GetVertexStride();

			VertexBufferCreateInformation vbDesc;
			vbDesc.VertexCount = NUM_VERTICES;
			vbDesc.VertexSize = vertexStride;

			gVertexBuffer = VertexBuffer::Create(vbDesc);

			u8* vbData = (u8*)gVertexBuffer->Lock(0, vertexStride * NUM_VERTICES, GBL_WRITE_ONLY_DISCARD);
			u8* positions = vbData + vertexDesc->GetElementOffsetFromStream(VES_POSITION);
			u8* uvs = vbData + vertexDesc->GetElementOffsetFromStream(VES_TEXCOORD);

			AABox box(Vector3::kOne * -10.0f, Vector3::kOne * 10.0f);
			WriteBoxVertices(box, positions, uvs, vertexStride);

			gVertexBuffer->Unlock();

			// Create & fill the index buffer for a box mesh
			IndexBufferCreateInformation ibDesc;
			ibDesc.IndexCount = NUM_INDICES;
			ibDesc.IndexType = IT_32BIT;

			gIndexBuffer = IndexBuffer::Create(ibDesc);
			u32* ibData = (u32*)gIndexBuffer->Lock(0, NUM_INDICES * sizeof(u32), GBL_WRITE_ONLY_DISCARD);
			WriteBoxIndices(ibData);

			gIndexBuffer->Unlock();

			// Create a simple 2x2 checkerboard texture to map to the object we're about to render
			SPtr<PixelData> pixelData = PixelData::Create(2, 2, 1, PF_RGBA8);
			pixelData->SetColorAt(Color::kWhite, 0, 0);
			pixelData->SetColorAt(Color::kBlack, 1, 0);
			pixelData->SetColorAt(Color::kWhite, 1, 1);
			pixelData->SetColorAt(Color::kBlack, 0, 1);

			gSurfaceTex = Texture::Create(pixelData);

			// Create a sampler state for the texture above
			SAMPLER_STATE_DESC samplerDesc;
			samplerDesc.MinFilter = FO_POINT;
			samplerDesc.MagFilter = FO_POINT;

			gSurfaceSampler = SamplerState::Create(samplerDesc);

			// Create a color attachment texture for the render surface
			TextureCreateInformation colorAttDesc;
			colorAttDesc.Width = windowResWidth;
			colorAttDesc.Height = windowResHeight;
			colorAttDesc.Format = PF_RGBA8;
			colorAttDesc.Usage = TU_RENDERTARGET;

			SPtr<Texture> colorAtt = Texture::Create(colorAttDesc);

			// Create a depth attachment texture for the render surface
			TextureCreateInformation depthAttDesc;
			depthAttDesc.Width = windowResWidth;
			depthAttDesc.Height = windowResHeight;
			depthAttDesc.Format = PF_D32;
			depthAttDesc.Usage = TU_DEPTHSTENCIL;

			SPtr<Texture> depthAtt = Texture::Create(depthAttDesc);

			// Create the render surface
			RENDER_TEXTURE_DESC desc;
			desc.ColorSurfaces[0].Texture = colorAtt;
			desc.DepthStencilSurface.Texture = depthAtt;

			gRenderTarget = RenderTexture::Create(desc);
		}

		// Render the box, called every frame
		void Render()
		{
			// Fill out the uniform block variables
			UniformBlock uniformBlock;
			uniformBlock.GMatWvp = CreateWorldViewProjectionMatrix();
			uniformBlock.GTint = Color(1.0f, 1.0f, 1.0f, 0.5f);

			// Create a uniform block buffer for holding the uniform variables
			SPtr<GpuParamBlockBuffer> uniformBuffer = GpuParamBlockBuffer::Create(sizeof(UniformBlock));
			uniformBuffer->Write(0, &uniformBlock, sizeof(uniformBlock));

			// Assign the uniform buffer & texture
			gGpuParams->SetParameterBlockBuffer(GPT_FRAGMENT_PROGRAM, "Params", uniformBuffer);
			gGpuParams->SetParameterBlockBuffer(GPT_VERTEX_PROGRAM, "Params", uniformBuffer);

			gGpuParams->SetTexture(GPT_FRAGMENT_PROGRAM, "gMainTexture", gSurfaceTex);

			// HLSL uses separate sampler states, so we need to use a different name for the sampler
			if(gUseHLSL)
				gGpuParams->SetSamplerState(GPT_FRAGMENT_PROGRAM, "gMainTexSamp", gSurfaceSampler);
			else
				gGpuParams->SetSamplerState(GPT_FRAGMENT_PROGRAM, "gMainTexture", gSurfaceSampler);

			// Create a command buffer
			SPtr<CommandBuffer> cmds = CommandBuffer::Create(GQT_GRAPHICS);

			// Get the primary render API access point
			RenderAPI& rapi = RenderAPI::Instance();

			// Bind render surface & clear it
			rapi.SetRenderTarget(gRenderTarget, 0, RT_NONE, cmds);
			rapi.ClearRenderTarget(FBT_COLOR | FBT_DEPTH, Color::kBlue, 1, 0, 0xFF, cmds);

			// Bind the pipeline state
			rapi.SetGraphicsPipeline(gPipelineState, cmds);

			// Set the vertex & index buffers, as well as vertex declaration and draw type
			rapi.SetVertexBuffers(0, &gVertexBuffer, 1, cmds);
			rapi.SetIndexBuffer(gIndexBuffer, cmds);
			rapi.SetVertexDeclaration(gVertexDecl, cmds);
			rapi.SetDrawOperation(DOT_TRIANGLE_LIST, cmds);

			// Bind the GPU program parameters (i.e. resource descriptors)
			rapi.SetGpuParams(gGpuParams, cmds);

			// Draw
			rapi.DrawIndexed(0, NUM_INDICES, 0, NUM_VERTICES, 1, cmds);

			// Submit the command buffer
			rapi.SubmitCommandBuffer(cmds);

			// Blit the image from the render texture, to the render window
			rapi.SetRenderTarget(gRenderWindow);

			// Get the color attachment
			SPtr<Texture> colorTexture = gRenderTarget->GetColorTexture(0);

			// Use the helper RendererUtility to draw a full-screen quad of the provided texture and output it to the currently
			// bound render target. Internally this uses the same calls we used above, just with a different pipeline and mesh.
			GetRendererUtility().Blit(colorTexture);

			// Present the rendered image to the user
			rapi.SwapBuffers(gRenderWindow);
		}

		// Clean up any resources
		void Shutdown()
		{
			gPipelineState = nullptr;
			gSurfaceTex = nullptr;
			gGpuParams = nullptr;
			gVertexDecl = nullptr;
			gVertexBuffer = nullptr;
			gIndexBuffer = nullptr;
			gRenderTarget = nullptr;
			gRenderWindow = nullptr;
			gSurfaceSampler = nullptr;
		}

		/////////////////////////////////////////////////////////////////////////////////////
		//////////////////////////////////HELPER METHODS/////////////////////////////////////
		/////////////////////////////////////////////////////////////////////////////////////
		void WriteBoxVertices(const AABox& box, UINT8* positions, UINT8* uvs, u32 stride)
		{
			AABox::Corner vertOrder[] = {
				AABox::NEAR_LEFT_BOTTOM, AABox::NEAR_RIGHT_BOTTOM, AABox::NEAR_RIGHT_TOP, AABox::NEAR_LEFT_TOP,
				AABox::FAR_RIGHT_BOTTOM, AABox::FAR_LEFT_BOTTOM, AABox::FAR_LEFT_TOP, AABox::FAR_RIGHT_TOP,
				AABox::FAR_LEFT_BOTTOM, AABox::NEAR_LEFT_BOTTOM, AABox::NEAR_LEFT_TOP, AABox::FAR_LEFT_TOP,
				AABox::NEAR_RIGHT_BOTTOM, AABox::FAR_RIGHT_BOTTOM, AABox::FAR_RIGHT_TOP, AABox::NEAR_RIGHT_TOP,
				AABox::FAR_LEFT_TOP, AABox::NEAR_LEFT_TOP, AABox::NEAR_RIGHT_TOP, AABox::FAR_RIGHT_TOP,
				AABox::FAR_LEFT_BOTTOM, AABox::FAR_RIGHT_BOTTOM, AABox::NEAR_RIGHT_BOTTOM, AABox::NEAR_LEFT_BOTTOM
			};

			for(auto& entry : vertOrder)
			{
				Vector3 pos = box.GetCorner(entry);
				memcpy(positions, &pos, sizeof(pos));

				positions += stride;
			}

			for(u32 i = 0; i < 6; i++)
			{
				Vector2 uv;

				uv = Vector2(0.0f, 1.0f);
				memcpy(uvs, &uv, sizeof(uv));
				uvs += stride;

				uv = Vector2(1.0f, 1.0f);
				memcpy(uvs, &uv, sizeof(uv));
				uvs += stride;

				uv = Vector2(1.0f, 0.0f);
				memcpy(uvs, &uv, sizeof(uv));
				uvs += stride;

				uv = Vector2(0.0f, 0.0f);
				memcpy(uvs, &uv, sizeof(uv));
				uvs += stride;
			}
		}

		void WriteBoxIndices(u32* indices)
		{
			for(u32 face = 0; face < 6; face++)
			{
				u32 faceVertOffset = face * 4;

				indices[face * 6 + 0] = faceVertOffset + 2;
				indices[face * 6 + 1] = faceVertOffset + 1;
				indices[face * 6 + 2] = faceVertOffset + 0;
				indices[face * 6 + 3] = faceVertOffset + 0;
				indices[face * 6 + 4] = faceVertOffset + 3;
				indices[face * 6 + 5] = faceVertOffset + 2;
			}
		}

		const char* GetVertexProgSource()
		{
			if(gUseHLSL)
			{
				static const char* src = R"(
cbuffer Params
{
	float4x4 gMatWVP;
	float4 gTint;
}	

void main(
	in float3 inPos : POSITION,
	in float2 uv : TEXCOORD0,
	out float4 oPosition : SV_Position,
	out float2 oUv : TEXCOORD0)
{
	oPosition = mul(gMatWVP, float4(inPos.xyz, 1));
	oUv = uv;
}
)";

				return src;
			}
			else if(gUseVKSL)
			{
				static const char* src = R"(
layout (binding = 0, std140) uniform Params
{
	mat4 gMatWVP;
	vec4 gTint;
};

layout (location = 0) in vec3 bs_position;
layout (location = 1) in vec2 bs_texcoord0;

layout (location = 0) out vec2 texcoord0;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
	gl_Position = gMatWVP * vec4(bs_position.xyz, 1);
	texcoord0 = bs_texcoord0;
}
)";

				return src;
			}
			else
			{
				static const char* src = R"(
layout (std140) uniform Params
{
	mat4 gMatWVP;
	vec4 gTint;
};

in vec3 bs_position;
in vec2 bs_texcoord0;

out vec2 texcoord0;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
	gl_Position = gMatWVP * vec4(bs_position.xyz, 1);
	texcoord0 = bs_texcoord0;
}
)";
				return src;
			}
		}

		const char* GetFragmentProgSource()
		{
			if(gUseHLSL)
			{
				static const char* src = R"(
cbuffer Params
{
	float4x4 gMatWVP;
	float4 gTint;
}

SamplerState gMainTexSamp : register(s0);
Texture2D gMainTexture : register(t0);

float4 main(in float4 inPos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
	float4 color = gMainTexture.Sample(gMainTexSamp, uv);
	return color * gTint;
}
)";

				return src;
			}
			else if(gUseVKSL)
			{
				static const char* src = R"(
layout (binding = 0, std140) uniform Params
{
	mat4 gMatWVP;
	vec4 gTint;
};

layout (binding = 1) uniform sampler2D gMainTexture;

layout (location = 0) in vec2 texcoord0;
layout (location = 0) out vec4 fragColor;

void main()
{
	vec4 color = texture(gMainTexture, texcoord0.st);
	fragColor = color * gTint;
}
)";

				return src;
			}
			else
			{

				static const char* src = R"(
layout (std140) uniform Params
{
	mat4 gMatWVP;
	vec4 gTint;
};

uniform sampler2D gMainTexture;

in vec2 texcoord0;
out vec4 fragColor;

void main()
{
	vec4 color = texture(gMainTexture, texcoord0.st);
	fragColor = color * gTint;
}
)";
				return src;
			}
		}

		Matrix4 CreateWorldViewProjectionMatrix()
		{
			Matrix4 proj = Matrix4::ProjectionPerspective(Degree(75.0f), 16.0f / 9.0f, 0.05f, 1000.0f);
			bs::RenderAPI::ConvertProjectionMatrix(proj, proj);

			Vector3 cameraPos = Vector3(0.0f, -20.0f, 50.0f);
			Vector3 lookDir = -Vector3::Normalize(cameraPos);

			Quaternion cameraRot(BsIdentity);
			cameraRot.LookRotation(lookDir);

			Matrix4 view = Matrix4::View(cameraPos, cameraRot);

			Quaternion rotation(Vector3::kUnitY, Degree(GetTime().GetTime() * 90.0f));
			Matrix4 world = Matrix4::TRS(Vector3::kZero, rotation, Vector3::kOne);

			Matrix4 viewProj = proj * view * world;

			// GLSL uses column major matrices, so transpose
			if(!gUseHLSL)
				viewProj = viewProj.Transpose();

			return viewProj;
		}
	} // namespace ct
} // namespace bs
