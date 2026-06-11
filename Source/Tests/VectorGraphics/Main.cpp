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
#include "GUI/StyleSheet/B3DGUIStyleSheet.h"
#include "Image/B3DTexture.h"
#include "GpuBackend/B3DRenderTexture.h"
#include "Renderer/B3DRenderer.h"
#include "Renderer/B3DRendererManager.h"
#include "VectorGraphics/B3DVectorGraphics.h"

namespace b3d
{
	u32 kWindowWidth = 1280;
	u32 kWindowHeight = 1280;

	u32 kCheckmarkWidth = 512;
	u32 kCheckmarkHeight = 1024;

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

// Main entry point into the application
int B3DMain()
{
	using namespace b3d;

	// Define a video mode for the resolution of the primary rendering window.
	VideoMode videoMode(kWindowWidth, kWindowHeight);

	// Start-up the engine using our custom MyApplication class. This will also create the primary rendering window.
	// We provide the initial resolution of the window, its title and fullscreen state.
	Application::StartUp<MyApplication>(Application::BuildCreateInformation(videoMode, "bsf Example App", false));

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
		// Fields where we'll store the resources required during calls to Render(). These are initialized in Setup()
		// and cleaned up in ShutDown()
		TShared<b3d::VectorPath> gTestPath;
		TShared<b3d::VectorPath> gCheckmarkPath;
		TShared<VectorPathRenderable> gTestPathRenderable;
		TShared<VectorPathRenderable> gCheckmarkPathRenderable;
		TShared<GpuCommandBufferPool> gCommandBufferPool;

		TShared<RenderTexture> gRenderTarget;
		TShared<RenderWindow> gRenderWindow;

		// Initializes any resources required for rendering
		void Setup(const TShared<RenderWindow>& renderWindow)
		{
			const TShared<GpuDevice>& gpuDevice = GetApplication().GetPrimaryGpuDevice();

			// Create a pool to allocate GPU command buffer from
			GpuCommandBufferPoolCreateInformation poolCreateInformation;
			poolCreateInformation.Thread = B3D_CURRENT_THREAD_ID;
			poolCreateInformation.Type = GQT_GRAPHICS;

			gCommandBufferPool = gpuDevice->CreateGpuCommandBufferPool(poolCreateInformation);

			// This will be the primary output for our rendering (created by the main thread on start-up)
			gRenderWindow = renderWindow;

			auto fnCreateRenderTexture = [](u32 width, u32 height)
			{
				const TShared<GpuDevice> gpuDevice = GetApplication().GetPrimaryGpuDevice();

				TextureCreateInformation colorTextureCreateInformation;
				colorTextureCreateInformation.Width = width;
				colorTextureCreateInformation.Height = height;
				colorTextureCreateInformation.Format = PF_RGBA8;
				colorTextureCreateInformation.Usage = TextureUsageFlag::RenderTarget;

				const TShared<Texture> colorTexture = gpuDevice->CreateTexture(colorTextureCreateInformation);

				TextureCreateInformation stencilTextureCreateInformation;
				stencilTextureCreateInformation.Width = width;
				stencilTextureCreateInformation.Height = height;
				stencilTextureCreateInformation.Format = PF_D32_S8X24;
				stencilTextureCreateInformation.Usage = TextureUsageFlag::DepthStencil;

				const TShared<Texture> stencilTexture = gpuDevice->CreateTexture(stencilTextureCreateInformation);

				// Create the render surface
				RenderTextureCreateInformation renderTextureCreateInformation;
				renderTextureCreateInformation.ColorSurfaces[0].Texture = colorTexture;
				renderTextureCreateInformation.DepthStencilSurface.Texture = stencilTexture;

				return RenderTexture::Create(renderTextureCreateInformation);
			};


			gRenderTarget = fnCreateRenderTexture(kWindowWidth, kWindowHeight);

			// Create the vector path
			gTestPath = b3d::VectorPath::CreateShared();

			gTestPath->DrawRoundedRectangle(Area2(200, 200, 200, 200), 20.0f)
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
			gTestPath->DrawRoundedRectangle(Area2(200, 500, 200, 200), 20.0f)
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

			gTestPath->DrawRoundedRectangle(Area2(800, 400, 100, 100), 20.0f)
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

			// Rectangle with border
			gTestPath->DrawRectangle(Area2(800, 600, 149, 89))
				.ClosePath()
				.SetFillPaint(Color::kRed)
				.DrawFill()
				.SetStrokePaint(Color::kBlack)
				.SetStrokeWidth(1)
				.DrawStroke();

			// Manually drawn rounded rectangle
			{
				const float width = 150.0f;
				const float height = 90.0f;

				const float halfWidth = Math::Abs(width) * 0.5f;
				const float halfHeight = Math::Abs(height) * 0.5f;

				GUIStyleSheetRules rules;
				rules.BorderBottomLeftRadius = 15;
				rules.BorderBottomRightRadius = 15;
				rules.BorderTopLeftRadius = 15;
				rules.BorderTopRightRadius = 15;

				const Vector2 bottomLeft(
					Math::Min(rules.BorderBottomLeftRadius, halfWidth) * Math::Sign(width),
					Math::Min(rules.BorderBottomLeftRadius, halfHeight) * Math::Sign(height));

				const Vector2 bottomRight(
					Math::Min(rules.BorderBottomRightRadius, halfWidth) * Math::Sign(width),
					Math::Min(rules.BorderBottomRightRadius, halfHeight) * Math::Sign(height));

				const Vector2 topRight(
					Math::Min(rules.BorderTopRightRadius, halfWidth) * Math::Sign(width),
					Math::Min(rules.BorderTopRightRadius, halfHeight) * Math::Sign(height));

				const Vector2 topLeft(
					Math::Min(rules.BorderTopLeftRadius, halfWidth) * Math::Sign(width),
					Math::Min(rules.BorderTopLeftRadius, halfHeight) * Math::Sign(height));

				const float x = 800.0f;
				const float y = 800.0f;

				constexpr static float kKappa45 = 0.2652164900709011f;
				constexpr static float kKappa90 = 0.5522847493f; // Length proportional to radius of a cubic bezier handle for 90deg arcs.

				gTestPath->SetDrawCursor(Vector2(x, y + topLeft.Y));
				gTestPath->DrawLineTo(Vector2(x, y + height - bottomLeft.Y));
				gTestPath->DrawCubicBezierTo(Vector2(x, y + height - bottomLeft.Y * (1 - kKappa90)), Vector2(x + bottomLeft.X * (1 - kKappa90), y + height), Vector2(x + bottomLeft.X, y + height));
				gTestPath->DrawLineTo(Vector2(x + width - bottomRight.X, y + height));
				gTestPath->DrawCubicBezierTo(Vector2(x + width - bottomRight.X * (1 - kKappa90), y + height), Vector2(x + width, y + height - bottomRight.Y * (1 - kKappa90)), Vector2(x + width, y + height - bottomRight.Y));
				gTestPath->DrawLineTo(Vector2(x + width, y + topRight.Y));
				gTestPath->DrawCubicBezierTo(Vector2(x + width, y + topRight.Y * (1 - kKappa90)), Vector2(x + width - topRight.X * (1 - kKappa90), y), Vector2(x + width - topRight.X, y)),
					gTestPath->DrawLineTo(Vector2(x + topLeft.X, y));
				gTestPath->DrawCubicBezierTo(Vector2(x + topLeft.X * (1 - kKappa90), y), Vector2(x, y + topLeft.Y * (1 - kKappa90)), Vector2(x, y + topLeft.Y));
				gTestPath->ClosePath();
				gTestPath->SetAntialiasShapes(true);
				gTestPath->DrawFill();
			}

			// Button with separate styles for border sides
			{
				GUIStyleSheetRules rules;
				rules.BorderBottomLeftRadius = 15;
				rules.BorderBottomRightRadius = 15;
				rules.BorderTopLeftRadius = 15;
				rules.BorderTopRightRadius = 15;
				rules.BorderLeft.Width = 5;
				rules.BorderRight.Width = 5;
				rules.BorderTop.Width = 5;
				rules.BorderBottom.Width = 5;

				const float x = 800.0f;
				const float y = 1000.0f;

				const float width = 150.0f;
				const float height = 90.0f;

				const float leftBorderWidth = (float)rules.BorderLeft.Width;
				const float rightBorderWidth = (float)rules.BorderRight.Width;
				const float topBorderWidth = (float)rules.BorderTop.Width;
				const float bottomBorderWidth = (float)rules.BorderBottom.Width;

				const float innerX = x + leftBorderWidth;
				const float innerY = y + topBorderWidth;

				const float innerWidth = Math::Max(0.0f, width - leftBorderWidth - rightBorderWidth);
				const float innerHeight = Math::Max(0.0f, height - topBorderWidth - bottomBorderWidth);

				enum BorderCorner
				{
					BC_TopRight,
					BC_TopLeft,
					BC_BottomLeft,
					BC_BottomRight,
				};

				enum BorderSide
				{
					BS_Top,
					BS_Left,
					BS_Bottom,
					BS_Right,
				};

				constexpr BorderCorner kCornersPerSide[4][2]{
					{ BC_TopRight, BC_TopLeft },
					{ BC_TopLeft, BC_BottomLeft },
					{ BC_BottomLeft, BC_BottomRight },
					{ BC_BottomRight, BC_TopRight },
				};

				struct BorderInformation
				{
					Array<Vector2, 4> CornerCenters;
				};

				auto fnGenerateCornerInformation = [&rules](float x, float y, float width, float height) {
					const float halfWidth = Math::Abs(width) * 0.5f;
					const float halfHeight = Math::Abs(height) * 0.5f;

					const float right = x + width;
					const float bottom = y + height;

					// TODO - Known issue if the radius is larger than the half height of the inner border, border will not match
					// up with the rounded rectangle
					Vector2 borderCornerOffset[4];
					borderCornerOffset[BC_TopRight] = Vector2(
						Math::Min(rules.BorderTopRightRadius, halfWidth) * Math::Sign(width),
						Math::Min(rules.BorderTopRightRadius, halfHeight) * Math::Sign(height));

					borderCornerOffset[BC_TopLeft] = Vector2(
						Math::Min(rules.BorderTopLeftRadius, halfWidth) * Math::Sign(width),
						Math::Min(rules.BorderTopLeftRadius, halfHeight) * Math::Sign(height));

					borderCornerOffset[BC_BottomLeft] = Vector2(
						Math::Min(rules.BorderBottomLeftRadius, halfWidth) * Math::Sign(width),
						Math::Min(rules.BorderBottomLeftRadius, halfHeight) * Math::Sign(height));

					borderCornerOffset[BC_BottomRight] = Vector2(
						Math::Min(rules.BorderBottomRightRadius, halfWidth) * Math::Sign(width),
						Math::Min(rules.BorderBottomRightRadius, halfHeight) * Math::Sign(height));

					BorderInformation output;
					output.CornerCenters[BC_TopRight] = Vector2(right - borderCornerOffset[BC_TopRight].X, y + borderCornerOffset[BC_TopRight].Y);
					output.CornerCenters[BC_TopLeft] = Vector2(x + borderCornerOffset[BC_TopLeft].X, y + borderCornerOffset[BC_TopLeft].Y);
					output.CornerCenters[BC_BottomLeft] = Vector2(x + borderCornerOffset[BC_BottomLeft].X, bottom - borderCornerOffset[BC_BottomLeft].Y);
					output.CornerCenters[BC_BottomRight] = Vector2(right - borderCornerOffset[BC_BottomRight].X, bottom - borderCornerOffset[BC_BottomRight].Y);

					return output;
				};

				const BorderInformation outerBorderInformation = fnGenerateCornerInformation(x, y, width, height);
				const BorderInformation innerBorderInformation = fnGenerateCornerInformation(innerX, innerY, innerWidth, innerHeight);

				float kCornerRadii[4];
				kCornerRadii[BC_TopRight] = (float)rules.BorderTopRightRadius;
				kCornerRadii[BC_TopLeft] = (float)rules.BorderTopLeftRadius;
				kCornerRadii[BC_BottomLeft] = (float)rules.BorderBottomLeftRadius;
				kCornerRadii[BC_BottomRight] = (float)rules.BorderBottomRightRadius;

				Degree currentAngle(315.0f);
				const Degree kAngle45(45.0f);
				for(u32 side = 0; side < 4; ++side)
				{
					const u32 sideCornerA = kCornersPerSide[side][0];
					const u32 sideCornerB = kCornersPerSide[side][1];

					// Outer part of the border
					gTestPath->DrawArc(
						outerBorderInformation.CornerCenters[sideCornerA],
						kCornerRadii[sideCornerA],
						currentAngle,
						currentAngle - kAngle45, VectorGraphicsPathWinding::Counterclockwise);
					gTestPath->DrawArc(
						outerBorderInformation.CornerCenters[sideCornerB],
						kCornerRadii[sideCornerB],
						currentAngle - kAngle45,
						currentAngle - kAngle45 * 2.0f, VectorGraphicsPathWinding::Counterclockwise);

					// Inner part of the border
					gTestPath->DrawArc(
						innerBorderInformation.CornerCenters[sideCornerB],
						kCornerRadii[sideCornerB],
						currentAngle - kAngle45 * 2.0,
						currentAngle - kAngle45, VectorGraphicsPathWinding::Clockwise);
					gTestPath->DrawArc(
						innerBorderInformation.CornerCenters[sideCornerA],
						kCornerRadii[sideCornerA],
						currentAngle - kAngle45,
						currentAngle, VectorGraphicsPathWinding::Clockwise);

					gTestPath->ClosePath();
					gTestPath->SetAntialiasShapes(true);
					if(side % 2 == 0)
						gTestPath->SetFillPaint(Color::kGreen);
					else
						gTestPath->SetFillPaint(Color::kBlue);

					gTestPath->DrawFill();

					currentAngle -= kAngle45 * 2.0f;
				}

				// Fill
				gTestPath->DrawRoundedRectangle(Area2(innerX, innerY, innerWidth, innerHeight), (float)rules.BorderTopLeftRadius, (float)rules.BorderTopRightRadius, (float)rules.BorderBottomLeftRadius, (float)rules.BorderBottomRightRadius);
				gTestPath->SetFillPaint(Color::kRed);
				//gTestPath->SetAlpha(0.5f);
				gTestPath->DrawFill();
			}

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
			const TShared<GpuDevice>& gpuDevice = GetApplication().GetPrimaryGpuDevice();
			GpuWorkContext& gpuContext = GetRenderer()->GetGpuContext();

			// Create a command buffer
			TShared<GpuCommandBuffer> commandBuffer = gCommandBufferPool->Create(GpuCommandBufferCreateInformation::Create("VectorGraphics"));

			const TShared<GpuParameterSet> gpuParameters = gTestPathRenderable->Prepare();

			// Bind render surface & clear it
			RenderPassCreateInformation renderPassCreateInformation(gRenderTarget, gpuParameters, RT_NONE, RT_NONE);
			renderPassCreateInformation.ClearMask = RT_COLOR_ALL | RT_DEPTH;
			renderPassCreateInformation.ClearColor = Color::kWhite;

			commandBuffer->BeginRenderPass(RenderPassCreateInformation(gRenderTarget, gpuParameters, RT_NONE, RT_NONE));

			// Draw the vector shapes
			gTestPathRenderable->Render(*commandBuffer);

			if(gCheckmarkPathRenderable != nullptr)
			{
				commandBuffer->SetViewport(Area2(0.0f, 0.0f, (float)kCheckmarkWidth / (float)kWindowWidth, (float)kCheckmarkHeight / (float)kWindowHeight));
				commandBuffer->ClearViewport(RT_COLOR_ALL);
				gCheckmarkPathRenderable->Render(*commandBuffer);
			}

			// Blit the image from the render texture, to the render window
			commandBuffer->EndRenderPass();

			TShared<Texture> colorTexture = gRenderTarget->GetColorTexture(0);

			// Use the helper RendererUtility to draw a full-screen quad of the provided texture and output it to the currently
			// bound render target. Internally this uses the same calls we used above, just with a different pipeline and mesh.
			BlitInformation blitInformation = BlitInformation::BlitColor(colorTexture, gRenderWindow);
			blitInformation.OutputArea =Area2(0.0f, 0.0f, 1.0f, 1.0f); 

			GetRendererUtility().Blit(*commandBuffer, blitInformation);

			// Submit the command buffer through the render thread's GPU work context
			gpuContext.SubmitCommandBuffer(commandBuffer);

			// Present the rendered image to the user
			gpuDevice->PresentRenderWindow(gRenderWindow);
		}

		// Clean up any resources
		void Shutdown()
		{
			gCommandBufferPool = nullptr;
			gTestPath = nullptr;
			gTestPathRenderable = nullptr;
			gRenderTarget = nullptr;
			gRenderWindow = nullptr;
		}
	} // namespace render
} // namespace b3d
