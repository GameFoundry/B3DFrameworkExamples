#include "B3DApplication.h"
#include "B3DEntry.h"
#include "B3DExampleFramework.h"
#include "Material/B3DShader.h"
#include "Material/B3DVariation.h"
#include "Material/B3DPass.h"
#include "CoreObject/B3DRenderThread.h"
#include "GpuBackend/B3DRenderWindow.h"
#include "GpuBackend/B3DGpuCommandBuffer.h"
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
#include "FrameGraph/B3DFrameGraph.h"

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
// The GPU programs (and the blend/depth state they render with) come from a BSL shader that ships with the example data
// ("ExampleLowLevelRendering.bsl"). Everything below the shader - GPU programs, pipeline state, vertex & index buffers,
// parameter binding and draw calls - is created manually through the low-level API, from the information the shader's
// pass provides.
//
// The example first sets up those resources. Every frame it then describes its GPU work as a frame graph: a pass that renders the
// box into a color and a depth texture, and a pass that copies the color texture into the window. The graph places the
// textures in memory it reuses between frames, and records every barrier between the passes. The engine runs in explicit mode,
// so the graph's barriers are the only ones the GPU work gets.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace b3d
{
	u32 kWindowWidth = 1280;
	u32 kWindowHeight = 720;
	float FrameTimeSeconds = 0.0f;

	// Declare the methods we'll use to do work on the render thread. Normally methods and objects usable on the render thread
	// will be in the "render" namespace. This is not always true, especially for thread-safe objects, but it is almost always true if
	// an object has variants of its type usable from main and render threads.
	namespace render
	{
		void Setup(const TShared<RenderWindow>& renderWindow, const TShared<Shader>& boxShader);
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

			// Load the box shader from the example data. The shader provides the GPU program code and pipeline states,
			// which we use to manually construct the objects needed for rendering in Setup().
			mBoxShader = ExampleFramework::LoadShader("ExampleLowLevelRendering.bsl");

			TShared<render::Shader> boxShaderRenderProxy = B3DGetRenderProxy(mBoxShader);

			// Initialize all the resources we need for rendering. Since we do rendering on a separate thread (the render
			// thread), we don't call the method directly, but rather queue it for execution using the RenderThread class.
			GetRenderThread().PostCommand([renderWindowRenderProxy, boxShaderRenderProxy] { render::Setup(renderWindowRenderProxy, boxShaderRenderProxy); });
		}

		// Called when the engine is about to be shut down
		void OnShutDown() override
		{
			// Queue the method for execution on the render thread
			GetRenderThread().PostCommand(&render::Shutdown);

			// Release the shader before engine shut-down proceeds, so the resources it keeps alive are destroyed while the resource system is still running
			mBoxShader = HShader();

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

		// Shader whose pass provides the GPU programs and pipeline states the box renders with
		HShader mBoxShader;
	};
} // namespace b3d

/** Main entry point into the application. */
int B3DMain()
{
	using namespace b3d;

	// Define a video mode for the resolution of the primary rendering window.
	VideoMode videoMode(kWindowWidth, kWindowHeight);

	ApplicationCreateInformation createInformation = Application::BuildCreateInformation(videoMode, "Low Level Rendering", false);

	// Leave every resource at rest after its creation and upload, which the frame graph expects of resources it did not create
	gGpuExplicitBarriers.Set(true);

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
		Matrix4 CreateWorldViewProjectionMatrix();

		// Fields where we'll store the resources required during calls to render(). These are initialized in setup()
		// and cleaned up in shutDown()
		TShared<GpuGraphicsPipelineState> gPipelineState;
		TShared<Texture> gSurfaceTex;
		TShared<SamplerState> gSurfaceSampler;
		TShared<VertexDescription> gVertexDescription;
		TShared<GpuBuffer> gVertexBuffer;
		TShared<GpuBuffer> gIndexBuffer;
		TShared<RenderWindow> gRenderWindow;
		TextureCreateInformation gColorInformation;
		TextureCreateInformation gDepthInformation;

		const u32 kVertexCount = 24;
		const u32 kIndexCount = 36;

		// Structure that will hold uniform block variables for the GPU programs
		struct UniformBlock
		{
			Matrix4 GMatWvp; // World view projection matrix
			Color GTint; // Tint to apply on top of the texture
		};

		// Initializes any resources required for rendering
		void Setup(const TShared<RenderWindow>& renderWindow, const TShared<Shader>& boxShader)
		{
			const TShared<GpuDevice>& gpuDevice = GetApplication().GetPrimaryGpuDevice();
			GpuWorkContext& gpuContext = GetRenderer()->GetGpuContext();

			// This will be the primary output for our rendering (created by the main thread on start-up)
			gRenderWindow = renderWindow;

			// Grab the shader's first variation compatible with the active GPU backend, and ensure it is compiled
			const TShared<Variation> shaderVariation = boxShader->GetCompatibleVariations()[0];
			if(!shaderVariation->IsCompiled())
				shaderVariation->Compile().BlockUntilComplete();

			// The variation's pass provides everything needed to construct the rendering pipeline: the GPU program
			// descriptors (compiled bytecode, or source to compile from), and the blend/rasterizer/depth-stencil
			// states described in the shader's BSL source. Create the GPU programs and the graphics pipeline from it
			// manually through the low-level API.
			const PassInformation& passInformation = shaderVariation->GetPass(0)->GetInformation();

			GpuGraphicsPipelineStateCreateInformation pipelineCreateInformation;
			pipelineCreateInformation.VertexProgram = gpuDevice->CreateGpuProgram(passInformation.VertexProgramCreateInformation);
			pipelineCreateInformation.FragmentProgram = gpuDevice->CreateGpuProgram(passInformation.FragmentProgramCreateInformation);
			pipelineCreateInformation.BlendState = passInformation.BlendStateInformation;
			pipelineCreateInformation.RasterizerState = passInformation.RasterizerStateInformation;
			pipelineCreateInformation.DepthStencilState = passInformation.DepthStencilStateInformation;

			gPipelineState = gpuDevice->CreateGpuGraphicsPipelineState(pipelineCreateInformation);

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

			// Describe the color and depth textures the box renders into. The frame graph creates them every frame, in memory
			// it reuses between frames.
			gColorInformation.Width = kWindowWidth;
			gColorInformation.Height = kWindowHeight;
			gColorInformation.Format = PF_RGBA8;
			gColorInformation.Usage = TextureUsageFlag::RenderTarget;
			gColorInformation.ClearColor = Color::kBlue;

			gDepthInformation.Width = kWindowWidth;
			gDepthInformation.Height = kWindowHeight;
			gDepthInformation.Format = PF_D32;
			gDepthInformation.Usage = TextureUsageFlag::DepthStencil;
		}

		// Render the box, called every frame
		void Render()
		{
			const TShared<GpuDevice>& gpuDevice = GetApplication().GetPrimaryGpuDevice();

			// Get the GPU context, which is a thread-local object that provides access to the GPU command buffer and other GPU resources. 
			// Renderer provides a GPU context for the render thread, which is where this method executes.
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

			// Describe the frame's GPU work as a graph of passes. The window is a resource the graph did not create, so it is imported.
			FrameGraph frameGraph(gpuContext, "Low level rendering");
			frameGraph.ImportRenderTarget(gRenderWindow);

			// Create the color and depth textures the box renders into. They only live within this graph.
			const TShared<Texture> colorTexture = frameGraph.CreateTexture("Box color", gColorInformation);
			const TShared<Texture> depthTexture = frameGraph.CreateTexture("Box depth", gDepthInformation);

			// The first pass clears both textures to their clear values and renders the box into them
			FrameGraphRasterPass boxPass;
			boxPass.SetColorAttachment(0, colorTexture, FrameGraphAttachmentLoad::Clear);
			boxPass.SetDepthStencilAttachment(depthTexture, FrameGraphDepthStencilMode::Write, FrameGraphAttachmentLoad::Clear);

			// Parameter sets created through the pass tell the graph which resources the pass reads and writes. Assign the
			// uniform buffer & texture.
			const TShared<GpuParameterSet> boxParameters = gpuContext.GetParameterSetPool().Create(gPipelineState->GetParameterLayout()->GetSet(0), 0);
			boxPass.AddParameterSet(boxParameters);
			boxParameters->SetUniformBuffer("Params", uniformBuffer);
			boxParameters->SetSampledTexture("gMainTexture", gSurfaceTex);
			boxParameters->SetSamplerState("gMainTexSamp", gSurfaceSampler);

			// The graph calls the function when it records the pass, after the pass's barriers, with the attachments bound
			frameGraph.AddRasterPass("Box", std::move(boxPass), [boxParameters](FrameGraphPassContext& passContext)
			{
				GpuCommandBuffer& commandBuffer = passContext.GetCommandBuffer();

				// Bind the pipeline state
				commandBuffer.SetGpuGraphicsPipelineState(gPipelineState);

				// Set the vertex & index buffers, as well as vertex declaration and draw type
				commandBuffer.SetVertexBuffers(0, &gVertexBuffer, 1);
				commandBuffer.SetIndexBuffer(gIndexBuffer);
				commandBuffer.SetVertexDescription(gVertexDescription);
				commandBuffer.SetDrawOperation(DOT_TRIANGLE_LIST);

				// Bind the GPU program parameters (i.e. resource descriptors)
				commandBuffer.SetGpuParameterSet(boxParameters);

				// Draw
				commandBuffer.DrawIndexed(0, kIndexCount, 0, kVertexCount, 1, 0);
			});

			// The second pass draws a full-screen quad that samples the color texture into the window, using the blit material
			// of the renderer utility. It overwrites the whole window, so the previous contents are not loaded.
			BlitMat* const blitMaterial = BlitMat::GetVariation(1, true, false);

			FrameGraphRasterPass blitPass;
			blitPass.SetRenderTarget(gRenderWindow, FrameGraphAttachmentLoad::DontCare);

			const TShared<GpuParameterSet> blitParameters = blitMaterial->Prepare(blitPass, colorTexture);

			frameGraph.AddRasterPass("Blit to window", std::move(blitPass), [blitMaterial, blitParameters](FrameGraphPassContext& passContext)
			{
				blitMaterial->Execute(passContext, blitParameters, Area2(0.0f, 0.0f, (float)kWindowWidth, (float)kWindowHeight), false);
			});

			// Place the textures in memory and plan the barriers between the passes, then record and submit the passes. The
			// graph leaves the window ready to be presented.
			frameGraph.Compile();
			frameGraph.Execute();

			// Present the rendered image to the user
			gpuDevice->PresentRenderWindow(gRenderWindow);
		}

		// Clean up any resources
		void Shutdown()
		{
			gPipelineState = nullptr;
			gSurfaceTex = nullptr;
			gVertexDescription = nullptr;
			gVertexBuffer = nullptr;
			gIndexBuffer = nullptr;
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

			FrameTimeSeconds += GetTime().GetFrameDelta();

			Quaternion rotation(Vector3::kUnitY, Degree(FrameTimeSeconds * 90.0f));
			Matrix4 world = Matrix4::TRS(Vector3::kZero, rotation, Vector3::kOne);

			Matrix4 viewProj = proj * view * world;

			// The shader is compiled with column-major matrix packing, so transpose our row-major matrix
			viewProj = viewProj.Transpose();

			return viewProj;
		}
	} // namespace render
} // namespace b3d
