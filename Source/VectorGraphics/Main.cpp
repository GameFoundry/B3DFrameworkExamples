#include "BsApplication.h"
#include "Material/BsMaterial.h"
#include "CoreThread/BsRenderThread.h"
#include "RenderAPI/BsRenderWindow.h"
#include "RenderAPI/BsGpuCommandBuffer.h"
#include "RenderAPI/BsGpuProgram.h"
#include "RenderAPI/BsGpuPipelineState.h"
#include "RenderAPI/BsGpuBuffer.h"
#include "RenderAPI/BsVertexDescription.h"
#include "Mesh/BsMeshData.h"
#include "Math/BsQuaternion.h"
#include "Utility/BsTime.h"
#include "Renderer/BsRendererUtility.h"
#include "BsEngineConfig.h"
#include "Image/BsTexture.h"
#include "RenderAPI/BsRenderTexture.h"
#include "Renderer/BsRenderer.h"
#include "Renderer/BsRendererManager.h"
#include "VectorGraphics/BsVectorGraphics.h"

namespace bs
{
	u32 kWindowWidth = 1280;
	u32 kWindowHeight = 1280;

	u32 kCheckmarkWidth = 512;
	u32 kCheckmarkHeight = 1024;

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
			GetCoreThread().PostCommand(std::bind(&ct::Setup, renderWindowCore));
		}

		// Called when the engine is about to be shut down
		void OnShutDown() override
		{
			// Queue the method for execution on the core thread
			GetCoreThread().PostCommand(&ct::Shutdown);

			// Shut-down engine components
			Application::OnShutDown();
		}

		// Called every frame, before any other engine system (optionally use postUpdate())
		void PreUpdate() override
		{
			// Queue the method for execution on the core thread
			GetCoreThread().PostCommand(&ct::Render);

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
	VideoMode videoMode(kWindowWidth, kWindowHeight);

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
		// Fields where we'll store the resources required during calls to Render(). These are initialized in Setup()
		// and cleaned up in ShutDown()
		SPtr<VectorPath> gTestPath;
		SPtr<VectorPath> gCheckmarkPath;
		SPtr<VectorPathRenderable> gTestPathRenderable;
		SPtr<VectorPathRenderable> gCheckmarkPathRenderable;

		SPtr<RenderTexture> gRenderTarget;
		SPtr<RenderWindow> gRenderWindow;

		// Initializes any resources required for rendering
		void Setup(const SPtr<RenderWindow>& renderWindow)
		{
			// This will be the primary output for our rendering (created by the main thread on start-up)
			gRenderWindow = renderWindow;

			auto fnCreateRenderTexture = [](u32 width, u32 height)
			{
				const SPtr<GpuDevice> gpuDevice = GetCoreApplication().GetPrimaryGpuDevice();

				TextureCreateInformation colorTextureCreateInformation;
				colorTextureCreateInformation.Width = width;
				colorTextureCreateInformation.Height = height;
				colorTextureCreateInformation.Format = PF_RGBA8;
				colorTextureCreateInformation.Usage = TU_RENDERTARGET;

				const SPtr<Texture> colorTexture = gpuDevice->CreateTexture(colorTextureCreateInformation);

				TextureCreateInformation stencilTextureCreateInformation;
				stencilTextureCreateInformation.Width = width;
				stencilTextureCreateInformation.Height = height;
				stencilTextureCreateInformation.Format = PF_D32_S8X24;
				stencilTextureCreateInformation.Usage = TU_DEPTHSTENCIL;

				const SPtr<Texture> stencilTexture = gpuDevice->CreateTexture(stencilTextureCreateInformation);

				// Create the render surface
				RenderTextureCreateInformation renderTextureCreateInformation;
				renderTextureCreateInformation.ColorSurfaces[0].Texture = colorTexture;
				renderTextureCreateInformation.DepthStencilSurface.Texture = stencilTexture;

				return RenderTexture::Create(renderTextureCreateInformation);
			};


			gRenderTarget = fnCreateRenderTexture(kWindowWidth, kWindowHeight);

			// Create the vector path
			gTestPath = VectorPath::CreateShared();

			gTestPath->DrawRoundedRectangle(Rect2(200, 200, 200, 200), 20.0f)
				.ClosePath()
				.SetFillPaint(Color::kRed)
				.SetStrokePaint(Color::kRed)
				.DrawFill();
			
			gTestPath->DrawEllipse(Vector2(400.0f, 200.0f), Vector2(100.0f, 75.0f))
				.ClosePath()
				.SetStrokePaint(Color::kBlue)
				.DrawStroke();

			gTestPath->DrawEllipse(Vector2(400.0f, 400.0f), Vector2(100.0f, 75.0f))
				.ClosePath()
				.SetFillPaint(VectorGraphicsPaint::CreateRadialGradient(Color::kBlue, Color::kGreen, Vector2(400.0f, 400.0f), 25.0f, 75.0f))
				.DrawFill();

			// Non-convex shape
			gTestPath->SetDrawCursor(Vector2(600.0f, 200.0f))
				.DrawLineTo(Vector2(700.0f, 250.0f))
				.DrawLineTo(Vector2(700.0f, 350.0f))
				.DrawLineTo(Vector2(650.0f, 250.0f))
				.DrawLineTo(Vector2(600.0f, 300.0f))
				.DrawLineTo(Vector2(625.0f, 250.0f))
				.ClosePath()
				.SetFillPaint(Color::kGreen)
				.DrawFill();

			// Convex shape
			gTestPath->SetDrawCursor(Vector2(600.0f, 400.0f))
				.DrawLineTo(Vector2(700.0f, 450.0f))
				.DrawLineTo(Vector2(700.0f, 550.0f))
				.DrawLineTo(Vector2(650.0f, 575.0f))
				.DrawLineTo(Vector2(575.0f, 500.0f))
				.DrawLineTo(Vector2(550.0f, 450.0f))
				.ClosePath()
				.SetFillPaint(Color::kGreen)
				.DrawFill()
				.SetStrokePaint(Color::kBlack)
				.DrawStroke();

			// Shape with holes
			gTestPath->DrawRoundedRectangle(Rect2(200, 500, 200, 200), 20.0f)
				.DrawEllipse(Vector2(200.0f, 600.0f), Vector2(50.0f, 50.0f))
				.SetSolidity(VectorGraphicsPathSolidity::Hole) // Affects the previous path
				.ClosePath()
				.SetFillPaint(Color::kRed)
				.DrawFill();

			// Shapes with no AA
			gTestPath->SetDrawCursor(Vector2(800.0f, 200.0f))
				.DrawLineTo(Vector2(900.0f, 250.0f))
				.DrawLineTo(Vector2(900.0f, 350.0f))
				.DrawLineTo(Vector2(850.0f, 250.0f))
				.DrawLineTo(Vector2(800.0f, 300.0f))
				.DrawLineTo(Vector2(825.0f, 250.0f))
				.SetAntialiasShapes(false)
				.ClosePath()
				.SetFillPaint(Color::kBansheeOrange)
				.DrawFill();

			gTestPath->DrawRoundedRectangle(Rect2(800, 400, 100, 100), 20.0f)
				.ClosePath()
				.SetAntialiasShapes(false)
				.SetFillPaint(Color::kRed)
				.SetStrokePaint(Color::kRed)
				.DrawFill();
			
			gTestPath->DrawEllipse(Vector2(800.0f, 400.0f), Vector2(50.0f, 50.0f))
				.ClosePath()
				.SetAntialiasShapes(false)
				.SetStrokePaint(Color::kBlue)
				.DrawStroke();


			VectorGraphicsSettings testPathSettings;
			testPathSettings.Size = Size2((float)kWindowWidth, (float)kWindowHeight);
			testPathSettings.ScalingMode = VectorGraphicsRasterizationScaling::None;

			gTestPathRenderable = gTestPath->CreateRenderable(testPathSettings);

#if 0
			// Checkmark
			gCheckmarkPath = VectorPath::CreateShared(Size2(512.0f, 512.0f));

			gCheckmarkPath->SetDrawCursor(Vector2(17.47f, 250.9f))
				.DrawCubicBezierTo(Vector2(88.82f, 328.1f), Vector2(158.0f, 397.6f), Vector2(224.5f, 485.5f))
				.DrawCubicBezierTo(Vector2(296.8f, 341.7f), Vector2(370.8f, 197.4f), Vector2(492.9f, 41.13f))
				.DrawLineTo(Vector2(460.0f, 26.06f))
				.DrawCubicBezierTo(Vector2(356.9f, 135.4f), Vector2(276.8f, 238.9f), Vector2(207.2f, 361.9f))
				.DrawCubicBezierTo(Vector2(158.8f, 318.3f), Vector2(80.58f, 256.6f), Vector2(32.82f, 224.9f))
				.ClosePath()
				.SetFillPaint(Color::kBansheeOrange)
				.DrawFill();

			VectorGraphicsSettings checkmarkPathSettings;
			checkmarkPathSettings.Size = Size2((float)kCheckmarkWidth, (float)kCheckmarkHeight);

			gCheckmarkPathRenderable = gCheckmarkPath->CreateRenderable(checkmarkPathSettings);
#endif
		}
		

		// Render the box, called every frame
		void Render()
		{
			const SPtr<GpuDevice>& gpuDevice = GetCoreApplication().GetPrimaryGpuDevice();

			// Create a command buffer
			const SPtr<GpuCommandBufferPool>& commandBufferPool = RendererManager::Instance().GetActive()->GetCommandBufferPool();
			SPtr<GpuCommandBuffer> commandBuffer = commandBufferPool->Create(GpuCommandBufferCreateInformation::Create("VectorGraphics"));

			// Bind render surface & clear it
			commandBuffer->SetRenderTarget(gRenderTarget, 0, RT_NONE);
			commandBuffer->ClearRenderTarget(FBT_COLOR | FBT_DEPTH, Color::kWhite, 1, 0, 0xFF);

			// Draw the vector shapes
			gTestPathRenderable->Render(*commandBuffer);

			if(gCheckmarkPathRenderable != nullptr)
			{
				commandBuffer->SetViewport(Rect2(0.0f, 0.0f, (float)kCheckmarkWidth / (float)kWindowWidth, (float)kCheckmarkHeight / (float)kWindowHeight));
				commandBuffer->ClearViewport(FBT_COLOR);
				gCheckmarkPathRenderable->Render(*commandBuffer);
			}

			// Blit the image from the render texture, to the render window
			commandBuffer->SetRenderTarget(gRenderWindow);

			SPtr<Texture> colorTexture = gRenderTarget->GetColorTexture(0);

			// Use the helper RendererUtility to draw a full-screen quad of the provided texture and output it to the currently
			// bound render target. Internally this uses the same calls we used above, just with a different pipeline and mesh.
			commandBuffer->SetViewport(Rect2(0.0f, 0.0f, 1.0f, 1.0f));
			GetRendererUtility().Blit(*commandBuffer, colorTexture);

			// Submit the command buffer
			gpuDevice->SubmitCommandBuffer(commandBuffer);

			// Present the rendered image to the user
			gpuDevice->PresentRenderWindow(gRenderWindow);
		}

		// Clean up any resources
		void Shutdown()
		{
			gTestPath = nullptr;
			gTestPathRenderable = nullptr;
			gRenderTarget = nullptr;
			gRenderWindow = nullptr;
		}
	} // namespace ct
} // namespace bs
