#include "B3DApplication.h"
#include "B3DEntry.h"
#include "Material/B3DMaterial.h"
#include "CoreObject/B3DRenderThread.h"
#include "GpuBackend/B3DRenderWindow.h"
#include "GpuBackend/B3DGpuCommandBuffer.h"
#include "GpuBackend/B3DGpuProgram.h"
#include "GpuBackend/B3DGpuPipelineState.h"
#include "GpuBackend/B3DGpuBuffer.h"
#include "GpuBackend/B3DVertexDescription.h"
#include "Mesh/B3DMeshData.h"
#include "Math/B3DQuaternion.h"
#include "Utility/B3DTime.h"
#include "Renderer/B3DRendererUtility.h"
#include "B3DEngineConfig.h"
#include "Image/B3DTexture.h"
#include "GpuBackend/B3DRenderTexture.h"
#include "Renderer/B3DRenderer.h"
#include "Renderer/B3DRendererManager.h"
#include "GpuBackend/B3DGpuParameterSetPool.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This example uses the low-level rendering API to render a textured cube mesh. This is opposed to using scene objects
// and components, in which case objects are rendered automatically based on their transform and other properties.
//
// Using low-level rendering API gives you full control over rendering, similar to using Vulkan, DirectX or OpenGL APIs.
//
// In order to use the low-level rendering system we need to override the Application class so we get notified of updates
// and start-up/shut-down events. This is normally not necessary for a high level scene object based model.
//
// The rendering is performed on the render thread, as opposed to the main thread, where majority of bsf's code executes.
//
// The example first sets up necessary resources, like GPU programs, pipeline state, vertex & index buffers. Then every
// frame it binds the necessary rendering resources and executes the draw call.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace b3d
{
	u32 kWindowWidth = 1280;
	u32 kWindowHeight = 720;

	// Declare the methods we'll use to do work on the render thread. Normally methods and objects usable on the render thread
	// will be in the "render" namespace. This is not always true, especially for thread-safe objects, but it is almost always true if
	// an object has variants of its type usable from main and render threads.
	namespace render
	{
		void Setup(const TShared<RenderWindow>& renderWindow);
		void Render();
		void Shutdown();
	} // namespace render

	// Override the default Application so we can get notified when engine starts-up, shuts-down and when it executes
	// every frame
	class MyApplication : public Application
	{
	public:
		// Pass along the start-up structure to the parent, we don't need to handle it
		MyApplication(const ApplicationCreateInformation& createInformation)
			: Application(createInformation)
		{}

	private:
		// Called when the engine is first started up
		void OnStartUp() override
		{
			// Ensure all parent systems are initialized first
			Application::OnStartUp();

			// Get the primary window that was created during start-up. This will be the final destination for all our
			// rendering.
			TShared<RenderWindow> renderWindow = GetPrimaryWindow();

			// Get the version of the render window usable on the render thread, and send it along to setup()
			TShared<render::RenderWindow> renderWindowRenderProxy = B3DGetRenderProxy(renderWindow);

			// Initialize all the resources we need for rendering. Since we do rendering on a separate thread (the render
			// thread), we don't call the method directly, but rather queue it for execution using the RenderThread class.
			GetRenderThread().PostCommand([renderWindowRenderProxy] { render::Setup(renderWindowRenderProxy); });
		}

		// Called when the engine is about to be shut down
		void OnShutDown() override
		{
			// Queue the method for execution on the render thread
			GetRenderThread().PostCommand(&render::Shutdown);

			// Shut-down engine components
			Application::OnShutDown();
		}

		// Called every frame, before any other engine system (optionally use postUpdate())
		void PreUpdate() override
		{
			// Queue the method for execution on the render thread
			GetRenderThread().PostCommand(&render::Render);

			// Call the default version of this method to handle normal functionality
			Application::PreUpdate();
		}
	};
} // namespace b3d

/** Main entry point into the application. */
int B3DMain()
{
	using namespace b3d;

	// Define a video mode for the resolution of the primary rendering window.
	VideoMode videoMode(kWindowWidth, kWindowHeight);

	ApplicationCreateInformation createInformation = Application::BuildCreateInformation(videoMode, "Low Level Rendering", false);

	// Start-up the engine using our custom MyApplication class. This will also create the primary rendering window.
	// We provide the initial resolution of the window, its title and fullscreen state.
	Application::StartUp<MyApplication>(createInformation);

	// Runs the main loop that does most of the work. This method will exit when user closes the main
	// window or exits in some other way.
	Application::Instance().RunMainLoop();

	// Clean up when done
	Application::ShutDown();

	return 0;
}

namespace b3d
{
	namespace render
	{
		// Declarations for some helper methods we'll use during setup
		void WriteBoxVertices(const AABox& box, u8* positions, u8* uvs, u32 stride);
		void WriteBoxIndices(u32* indices);
		const char* GetVertexProgSource();
		const char* GetFragmentProgSource();
		Matrix4 CreateWorldViewProjectionMatrix();

		// Fields where we'll store the resources required during calls to render(). These are initialized in setup()
		// and cleaned up in shutDown()
		TShared<GpuGraphicsPipelineState> gPipelineState;
		TShared<Texture> gSurfaceTex;
		TShared<SamplerState> gSurfaceSampler;
		TShared<GpuParameterSet> gGpuParams;
		TShared<VertexDescription> gVertexDescription;
		TShared<GpuBuffer> gVertexBuffer;
		TShared<GpuBuffer> gIndexBuffer;
		TShared<RenderTexture> gRenderTarget;
		TShared<RenderWindow> gRenderWindow;
		TShared<GpuCommandBufferPool> gCommandBufferPool;
		bool gUseHLSL = true;

		const u32 kVertexCount = 24;
		const u32 kIndexCount = 36;

		// Structure that will hold uniform block variables for the GPU programs
		struct UniformBlock
		{
			Matrix4 GMatWvp; // World view projection matrix
			Color GTint; // Tint to apply on top of the texture
		};

		// Initializes any resources required for rendering
		void Setup(const TShared<RenderWindow>& renderWindow)
		{
			// Determine which shading language to use (depending on the GPU backend chosen during build)
			gUseHLSL = strcmp(B3D_GPU_BACKEND, "bsfD3D12GpuBackend") == 0;

			const TShared<GpuDevice>& gpuDevice = GetApplication().GetPrimaryGpuDevice();
			GpuWorkContext& gpuContext = GetRenderer()->GetGpuContext();

			// Create a pool to allocate GPU command buffer from
			GpuCommandBufferPoolCreateInformation poolCreateInformation;
			poolCreateInformation.Thread = B3D_CURRENT_THREAD_ID;
			poolCreateInformation.Type = GQT_GRAPHICS;

			gCommandBufferPool = gpuDevice->CreateGpuCommandBufferPool(poolCreateInformation);

			// This will be the primary output for our rendering (created by the main thread on start-up)
			gRenderWindow = renderWindow;

			// Create a vertex GPU program
			const char* vertProgSrc = GetVertexProgSource();

			GpuProgramCreateInformation vertexProgramCreateInformation;
			vertexProgramCreateInformation.Type = GPT_VERTEX_PROGRAM;
			vertexProgramCreateInformation.EntryPoint = "main";
			vertexProgramCreateInformation.Language = gUseHLSL ? "hlsl" : "vksl";
			vertexProgramCreateInformation.Source = vertProgSrc;

			TShared<GpuProgram> vertProg = gpuDevice->CreateGpuProgram(vertexProgramCreateInformation);

			// Create a fragment GPU program
			const char* fragProgSrc = GetFragmentProgSource();

			GpuProgramCreateInformation fragmentProgramCreateInformation;
			fragmentProgramCreateInformation.Type = GPT_FRAGMENT_PROGRAM;
			fragmentProgramCreateInformation.EntryPoint = "main";
			fragmentProgramCreateInformation.Language = gUseHLSL ? "hlsl" : "vksl";
			fragmentProgramCreateInformation.Source = fragProgSrc;

			TShared<GpuProgram> fragProg = gpuDevice->CreateGpuProgram(fragmentProgramCreateInformation);

			// Create a graphics pipeline state
			BlendStateInformation blendDesc;
			blendDesc.RenderTargets[0].BlendEnable = true;
			blendDesc.RenderTargets[0].RenderTargetWriteMask = 0b0111; // RGB, don't write to alpha
			blendDesc.RenderTargets[0].ColorBlendOperation = BO_ADD;
			blendDesc.RenderTargets[0].ColorSourceFactor = BF_SOURCE_ALPHA;
			blendDesc.RenderTargets[0].ColorDestinationFactor = BF_INV_SOURCE_ALPHA;

			DepthStencilStateInformation depthStencilDesc;
			depthStencilDesc.DepthWriteEnable = false;
			depthStencilDesc.DepthReadEnable = false;

			GpuGraphicsPipelineStateInformation pipelineDesc;
			pipelineDesc.BlendState = blendDesc;
			pipelineDesc.DepthStencilState = depthStencilDesc;
			pipelineDesc.VertexProgram = vertProg;
			pipelineDesc.FragmentProgram = fragProg;

			gPipelineState = gpuDevice->CreateGpuGraphicsPipelineState(pipelineDesc);

			// Create an object containing GPU program parameters
			GpuParameterSetPool& pool = render::GetRenderer()->GetGpuContext().GetParameterSetPool();
			gGpuParams = pool.Create(gPipelineState->GetParameterLayout()->GetSet(0), 0);

			// Create a vertex declaration for shader inputs
			TInlineArray<VertexElement, 8> vertexElements;
			vertexElements.Add(VertexElement(VET_FLOAT3, VES_POSITION));
			vertexElements.Add(VertexElement(VET_FLOAT2, VES_TEXCOORD));

			gVertexDescription = B3DMakeShared<VertexDescription>(vertexElements);

			// Create & fill the vertex buffer for a box mesh
			u32 vertexStride = gVertexDescription->GetVertexStride();

			GpuBufferCreateInformation vertexBufferCreateInformation;
			vertexBufferCreateInformation.Type = GpuBufferType::Vertex;
			vertexBufferCreateInformation.Vertex.Count = kVertexCount;
			vertexBufferCreateInformation.Vertex.ElementSize = vertexStride;

			gVertexBuffer = gpuDevice->CreateGpuBuffer(vertexBufferCreateInformation);

			u8* vbData = (u8*)B3DStackAllocate(vertexStride * kVertexCount);
			u8* positions = vbData + gVertexDescription->GetElementOffsetFromStream(VES_POSITION);
			u8* uvs = vbData + gVertexDescription->GetElementOffsetFromStream(VES_TEXCOORD);

			AABox box(Vector3::kOne * -10.0f, Vector3::kOne * 10.0f);
			WriteBoxVertices(box, positions, uvs, vertexStride);

			GpuBufferUtility::Write(gpuContext, gVertexBuffer, 0, vertexStride * kVertexCount, vbData, GpuBufferWriteFlag::Discard);
			B3DStackFree(vbData);

			// Create & fill the index buffer for a box mesh
			GpuBufferCreateInformation ibDesc;
			ibDesc.Type = GpuBufferType::Index;
			ibDesc.Index.Count = kIndexCount;
			ibDesc.Index.Type = IT_32BIT;

			gIndexBuffer = gpuDevice->CreateGpuBuffer(ibDesc);
			u32* ibData = (u32*)B3DStackAllocate(kIndexCount * sizeof(u32));
			WriteBoxIndices(ibData);

			GpuBufferUtility::Write(gpuContext, gIndexBuffer, 0, kIndexCount * sizeof(u32), ibData, GpuBufferWriteFlag::Discard);
			B3DStackFree(ibData);

			// Create a simple 2x2 checkerboard texture to map to the object we're about to render
			TShared<PixelData> pixelData = PixelData::Create(2, 2, 1, PF_RGBA8);
			pixelData->SetColorAt(Color::kWhite, 0, 0);
			pixelData->SetColorAt(Color::kBlack, 1, 0);
			pixelData->SetColorAt(Color::kWhite, 1, 1);
			pixelData->SetColorAt(Color::kBlack, 0, 1);

			gSurfaceTex = gpuDevice->CreateTexture(pixelData);

			// Create a sampler state for the texture above
			SamplerStateCreateInformation samplerStateCreateInformation;
			samplerStateCreateInformation.MinFilter = FO_POINT;
			samplerStateCreateInformation.MagFilter = FO_POINT;

			gSurfaceSampler = gpuDevice->FindOrCreateSamplerState(samplerStateCreateInformation);

			// Create a color attachment texture for the render surface
			TextureCreateInformation colorAttDesc;
			colorAttDesc.Width = kWindowWidth;
			colorAttDesc.Height = kWindowHeight;
			colorAttDesc.Format = PF_RGBA8;
			colorAttDesc.Usage = TextureUsageFlag::RenderTarget;

			TShared<Texture> colorAtt = gpuDevice->CreateTexture(colorAttDesc);

			// Create a depth attachment texture for the render surface
			TextureCreateInformation depthAttDesc;
			depthAttDesc.Width = kWindowWidth;
			depthAttDesc.Height = kWindowHeight;
			depthAttDesc.Format = PF_D32;
			depthAttDesc.Usage = TextureUsageFlag::DepthStencil;

			TShared<Texture> depthAtt = gpuDevice->CreateTexture(depthAttDesc);

			// Create the render surface
			RenderTextureCreateInformation desc;
			desc.ColorSurfaces[0].Texture = colorAtt;
			desc.DepthStencilSurface.Texture = depthAtt;

			gRenderTarget = RenderTexture::Create(desc);
		}

		// Render the box, called every frame
		void Render()
		{
			const TShared<GpuDevice>& gpuDevice = GetApplication().GetPrimaryGpuDevice();
			GpuWorkContext& gpuContext = GetRenderer()->GetGpuContext();

			// Fill out the uniform block variables
			UniformBlock uniformBlock;
			uniformBlock.GMatWvp = CreateWorldViewProjectionMatrix();
			uniformBlock.GTint = Color(1.0f, 1.0f, 1.0f, 0.5f);

			// Create a uniform block buffer for holding the uniform variables
			TShared<GpuBuffer> uniformBuffer = gpuDevice->CreateGpuBuffer(GpuBufferCreateInformation::CreateUniform(sizeof(UniformBlock)));

			{
				GpuBufferMappedScope mappedBufferScope = uniformBuffer->Map(GpuMapOption::Write);
				memcpy(mappedBufferScope.GetMappedMemory(), &uniformBlock, sizeof(uniformBlock));
			}

			// Assign the uniform buffer & texture
			gGpuParams->SetUniformBuffer("Params", uniformBuffer);
			gGpuParams->SetUniformBuffer("Params", uniformBuffer);

			gGpuParams->SetSampledTexture("gMainTexture", gSurfaceTex);
			gGpuParams->SetSamplerState("gMainTexSamp", gSurfaceSampler);

			// Create a command buffer
			TShared<GpuCommandBuffer> commandBuffer = gCommandBufferPool->Create(GpuCommandBufferCreateInformation::Create("LowLevelRendering"));

			// Bind render surface & clear it
			RenderPassCreateInformation renderPassCreateInformation(gRenderTarget, gGpuParams, RT_NONE, RT_NONE);
			renderPassCreateInformation.ClearMask = RT_COLOR_ALL | RT_DEPTH;
			renderPassCreateInformation.ClearColor = Color::kBlue;

			commandBuffer->BeginRenderPass(renderPassCreateInformation);

			// Bind the pipeline state
			commandBuffer->SetGpuGraphicsPipelineState(gPipelineState);

			// Set the vertex & index buffers, as well as vertex declaration and draw type
			commandBuffer->SetVertexBuffers(0, &gVertexBuffer, 1);
			commandBuffer->SetIndexBuffer(gIndexBuffer);
			commandBuffer->SetVertexDescription(gVertexDescription);
			commandBuffer->SetDrawOperation(DOT_TRIANGLE_LIST);

			// Bind the GPU program parameters (i.e. resource descriptors)
			commandBuffer->SetGpuParameterSet(gGpuParams);

			// Draw
			commandBuffer->DrawIndexed(0, kIndexCount, 0, kVertexCount, 1, 0);

			// Blit the image from the render texture, to the render window
			commandBuffer->EndRenderPass();

			// Get the color attachment
			TShared<Texture> colorTexture = gRenderTarget->GetColorTexture(0);

			// Use the helper RendererUtility to draw a full-screen quad of the provided texture and output it to the currently
			// bound render target. Internally this uses the same calls we used above, just with a different pipeline and mesh.
			GetRendererUtility().Blit(*commandBuffer, BlitInformation::BlitColor(colorTexture, gRenderWindow));

			// Submit the command buffer through the render thread's GPU work context
			gpuContext.SubmitCommandBuffer(commandBuffer);

			// Present the rendered image to the user
			gpuDevice->PresentRenderWindow(gRenderWindow);
		}

		// Clean up any resources
		void Shutdown()
		{
			gCommandBufferPool = nullptr;
			gPipelineState = nullptr;
			gSurfaceTex = nullptr;
			gGpuParams = nullptr;
			gVertexDescription = nullptr;
			gVertexBuffer = nullptr;
			gIndexBuffer = nullptr;
			gRenderTarget = nullptr;
			gRenderWindow = nullptr;
			gSurfaceSampler = nullptr;
		}

		/////////////////////////////////////////////////////////////////////////////////////
		//////////////////////////////////HELPER METHODS/////////////////////////////////////
		/////////////////////////////////////////////////////////////////////////////////////
		void WriteBoxVertices(const AABox& box, u8* positions, u8* uvs, u32 stride)
		{
			AABox::Corner vertOrder[] = {
				AABox::NearLeftBottom, AABox::NearRightBottom, AABox::NearRightTop, AABox::NearLeftTop,
				AABox::FarRightBottom, AABox::FarLeftBottom, AABox::FarLeftTop, AABox::FarRightTop,
				AABox::FarLeftBottom, AABox::NearLeftBottom, AABox::NearLeftTop, AABox::FarLeftTop,
				AABox::NearRightBottom, AABox::FarRightBottom, AABox::FarRightTop, AABox::NearRightTop,
				AABox::FarLeftTop, AABox::NearLeftTop, AABox::NearRightTop, AABox::FarRightTop,
				AABox::FarLeftBottom, AABox::FarRightBottom, AABox::NearRightBottom, AABox::NearLeftBottom
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
			else
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
			else
			{
				static const char* src = R"(
layout (binding = 0, std140) uniform Params
{
	mat4 gMatWVP;
	vec4 gTint;
};

layout (binding = 1) uniform sampler gMainTexSamp;
layout (binding = 2) uniform texture2D gMainTexture;

layout (location = 0) in vec2 texcoord0;
layout (location = 0) out vec4 fragColor;

void main()
{
	vec4 color = texture(sampler2D(gMainTexture, gMainTexSamp), texcoord0.st);
	fragColor = color * gTint;
}
)";

				return src;
			}
		}

		Matrix4 CreateWorldViewProjectionMatrix()
		{
			Matrix4 proj = Matrix4::ProjectionPerspective(Degree(75.0f), 16.0f / 9.0f, 0.05f, 1000.0f);
			if(const TShared<GpuDevice> gpuDevice = GetApplication().GetPrimaryGpuDevice())
				gpuDevice->ConvertProjectionMatrix(proj, proj);

			Vector3 cameraPos = Vector3(0.0f, -20.0f, 50.0f);
			Vector3 lookDir = -Vector3::Normalize(cameraPos);

			Quaternion cameraRot(kIdentityTag);
			cameraRot.LookRotation(lookDir);

			Matrix4 view = Matrix4::View(cameraPos, cameraRot);

			Quaternion rotation(Vector3::kUnitY, Degree(GetTime().GetRealTimeInSeconds() * 90.0f));
			Matrix4 world = Matrix4::TRS(Vector3::kZero, rotation, Vector3::kOne);

			Matrix4 viewProj = proj * view * world;

			// GLSL uses column major matrices, so transpose
			if(!gUseHLSL)
				viewProj = viewProj.Transpose();

			return viewProj;
		}
	} // namespace render
} // namespace b3d
