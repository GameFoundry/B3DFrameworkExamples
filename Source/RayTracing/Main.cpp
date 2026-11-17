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
#include "GpuBackend/B3DGpuShaderBindingTable.h"
#include "GpuBackend/B3DGpuBuffer.h"
#include "GpuBackend/B3DGpuDevice.h"
#include "GpuBackend/B3DGpuParameterSetPool.h"
#include "GpuBackend/B3DVertexDescription.h"
#include "Image/B3DTexture.h"
#include "Mesh/B3DMesh.h"
#include "Mesh/B3DMeshData.h"
#include "RayTracing/B3DRayTracingGeometry.h"
#include "RayTracing/B3DRayTracingScene.h"
#include "Renderer/B3DRendererMeshData.h"
#include "Renderer/B3DRendererUtility.h"
#include "Renderer/B3DRenderer.h"
#include "FrameGraph/B3DFrameGraph.h"
#include "Utility/B3DConfigVariable.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This example renders a scene with hardware ray tracing, through the low-level rendering API. It builds on the LowLevelRendering
// example, so read that one first.
//
// Every mesh that supports ray tracing (MeshFlag::SupportsRayTracing, the default for static meshes) has a ray tracing geometry: a
// bottom level acceleration structure built from the mesh's own vertex and index buffers. A RayTracingScene places instances of these
// geometries in the world, and builds the top level acceleration structure rays are traced against. The scene below has a ground plane,
// three instances of one box (they share one bottom level structure), an imported textured mesh, and a quad with alpha tested holes. One
// of the boxes moves every frame, so the scene rebuilds its top level structure every frame.
//
// The scene is traced in two ways, each into one half of the image:
//  - Left: inline ray tracing. A compute program traces rays with RayTraceClosest() and RayTraceAny(), and runs the alpha test in an
//    AnyHit callback.
//  - Right: a ray tracing pipeline. A ray generation program traces rays with TraceRay(), and the hit groups of a shader binding table
//    pick the closest hit and any hit programs that run for each instance.
// ExampleRayTracingInline.bsl and ExampleRayTracingPipeline.bsl shade with the same code, so the halves look the same. Devices
// without ray tracing pipelines render both halves inline. Set example.RayTracingMode to 1 or 2 to use one mode for the whole image.
//
// Shaders read the vertices and indices of the surface a ray hit through the bindless descriptor heap, at the indices the geometry's
// layouts (RayTracingGeometryLayout) hold. A table of these layouts, plus a table of per-instance data indexed by the instance id, tell
// the shaders what each instance is. Ray tracing geometry needs the vertex and index formats RayTracingGeometry lists, which are the
// formats RendererMeshData produces, so the meshes below are converted with RendererMeshData::Convert().
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace b3d
{
	u32 kWindowWidth = 1280;
	u32 kWindowHeight = 720;

	TConfigVariable<u32> gRayTracingMode("example.RayTracingMode",
		"How the RayTracing example traces the scene. 0 (default) traces the left half of the image inline and the right half through a "
		"ray tracing pipeline, 1 traces the whole image inline, and 2 traces it through a ray tracing pipeline. Devices without ray tracing "
		"pipelines always trace inline.",
		0,
		ConfigVariableFlag::RenderThreadSafe);

	namespace render
	{
		void Setup(const TShared<RenderWindow>& renderWindow, const TShared<Shader>& inlineShader, const TShared<Shader>& pipelineShader,
			const TShared<Mesh>& texturedMesh, const TShared<Texture>& texturedMeshAlbedo);
		void Render();
		void Shutdown();
	} // namespace render

	class MyApplication : public Application
	{
	public:
		MyApplication(const ApplicationCreateInformation& createInformation)
			: Application(createInformation)
		{}

	private:
		void OnStartUp() override
		{
			Application::OnStartUp();

			TShared<render::RenderWindow> renderWindow = B3DGetRenderProxy(GetPrimaryWindow());

			// Load packages so we can find previously saved resources. Platforms without importers only have those.
			ExampleFramework::LoadPackages();

			// One shader traces inline, from a compute program, and the other through a ray tracing pipeline
			mInlineShader = ExampleFramework::LoadShader("ExampleRayTracingInline.bsl");
			mPipelineShader = ExampleFramework::LoadShader("ExampleRayTracingPipeline.bsl");

			// The textured mesh. Meshes support ray tracing unless their import options turn it off (MeshImportOptions::SupportsRayTracing).
			mTexturedMesh = ExampleFramework::LoadMesh(ExampleMesh::Cerberus);
			mTexturedMeshAlbedo = ExampleFramework::LoadTexture(ExampleTexture::CerberusAlbedo);

			TShared<render::Shader> inlineShader = mInlineShader.IsLoaded() ? B3DGetRenderProxy(mInlineShader) : nullptr;
			TShared<render::Shader> pipelineShader = mPipelineShader.IsLoaded() ? B3DGetRenderProxy(mPipelineShader) : nullptr;
			TShared<render::Mesh> texturedMesh = mTexturedMesh.IsLoaded() ? B3DGetRenderProxy(mTexturedMesh) : nullptr;
			TShared<render::Texture> texturedMeshAlbedo = mTexturedMeshAlbedo.IsLoaded() ? B3DGetRenderProxy(mTexturedMeshAlbedo) : nullptr;

			GetRenderThread().PostCommand([renderWindow, inlineShader, pipelineShader, texturedMesh, texturedMeshAlbedo]
			{
				render::Setup(renderWindow, inlineShader, pipelineShader, texturedMesh, texturedMeshAlbedo);
			});
		}

		void OnShutDown() override
		{
			GetRenderThread().PostCommand(&render::Shutdown);

			mInlineShader = HShader();
			mPipelineShader = HShader();
			mTexturedMesh = HMesh();
			mTexturedMeshAlbedo = HTexture();

			Application::OnShutDown();
		}

		void PreUpdate() override
		{
			GetRenderThread().PostCommand(&render::Render);
			Application::PreUpdate();
		}

		HShader mInlineShader;
		HShader mPipelineShader;
		HMesh mTexturedMesh;
		HTexture mTexturedMeshAlbedo;
	};
} // namespace b3d

/** Main entry point into the application. */
int B3DMain()
{
	using namespace b3d;

	VideoMode videoMode(kWindowWidth, kWindowHeight);
	ApplicationCreateInformation createInformation = Application::BuildCreateInformation(videoMode, "Ray Tracing", false);

	Application::StartUp<MyApplication>(createInformation);
	Application::Instance().RunMainLoop();
	Application::ShutDown();

	return 0;
}

namespace b3d
{
	namespace render
	{
		// Materials of the instances. Match the MATERIAL_* values of the shaders. Colors are linear: the shaders light in linear space, and
		// encode the result as sRGB.
		enum class ExampleMaterial : u32
		{
			Checker, // Color, darkened in every other cell of a 1x1 grid in world space
			Solid, // Color
			Cutout, // Color, with alpha tested holes in every other cell of a 4x4 grid over the texture coordinates
			Textured // The albedo texture
		};

		// Data of one instance, indexed by its instance id. Matches ObjectData of the shaders.
		struct ObjectData
		{
			u32 FirstLayout; // Index of the layout of the instance's first geometry in the layout table
			ExampleMaterial Material;
			u32 Padding[2];
			Color Color;
		};

		// Matches the Params uniform block of the shaders
		struct ParamsBlock
		{
			Vector4 CameraPosition;
			Vector4 CameraRight;
			Vector4 CameraUp;
			Vector4 CameraForward;
			Vector4 LightDirection;
			u32 ViewSize[2];
			u32 ViewOffset;
			u32 Padding;
		};

		// Hit group records of the shader binding table that the instances start at. The pipeline traces two types of rays, primary rays
		// (0) and shadow rays (1), and ray type N runs miss record N and the hit group record at the instance's offset plus N. Opaque
		// instances use records 0 and 1, and cutout instances records 2 and 3.
		constexpr u32 kOpaqueHitGroupOffset = 0;
		constexpr u32 kCutoutHitGroupOffset = 2;

		TShared<RenderWindow> gRenderWindow;
		bool gSupportsRayTracing = false;
		u32 gFrameIndex = 0;

		TShared<GpuComputePipelineState> gInlinePipeline;
		TShared<GpuRayTracingPipelineState> gRayTracingPipeline;
		TShared<GpuShaderBindingTable> gShaderBindingTable;

		Vector<TShared<Mesh>> gMeshes; // Keeps the meshes, and with them their ray tracing geometries, alive
		TShared<RayTracingScene> gScene;
		u32 gMovingInstanceSlot = 0;

		TShared<GpuBuffer> gObjectBuffer;
		TShared<GpuBuffer> gLayoutBuffer;
		TShared<Texture> gAlbedoTexture;
		TShared<SamplerState> gAlbedoSampler;
		TextureCreateInformation gOutputInformation;

#if B3D_BUILD_TYPE_DEVELOPMENT
		// Frame whose image is read back to compare the two halves
		constexpr u32 kCompareFrameIndex = 2;
		TShared<GpuBuffer> gReadbackBuffer;
#endif

		TShared<Mesh> CreateMesh(TArrayView<const Vector3> positions, TArrayView<const Vector3> normals, TArrayView<const Vector2> uvs,
			TArrayView<const u32> indices);
		TShared<Mesh> CreatePlaneMesh(float size);
		TShared<Mesh> CreateBoxMesh(float size);
		TShared<Mesh> CreateQuadMesh(float width, float height);
		Matrix4 GetMovingInstanceTransform(u32 frameIndex);
		ParamsBlock CreateParams(u32 viewOffset, u32 viewWidth);

		/** Returns the program of @p pass with entry point @p entryPoint, created on @p device, or null if the pass has none. */
		TShared<GpuProgram> CreateProgram(GpuDevice& device, const Pass& pass, const char* entryPoint)
		{
			for(const GpuProgramCreateInformation& programInformation : pass.GetInformation().Programs)
			{
				if(programInformation.EntryPoint == entryPoint)
					return device.CreateGpuProgram(programInformation);
			}

			return nullptr;
		}

		/** Returns the pass of @p shader, compiled for the active backend, or null if the shader failed to compile. */
		TShared<Pass> GetPass(const Shader& shader)
		{
			const Vector<TShared<Variation>> variations = shader.GetCompatibleVariations();
			if(variations.empty())
				return nullptr;

			const TShared<Variation>& variation = variations[0];
			if(!variation->IsCompiled())
				variation->Compile().BlockUntilComplete();

			return variation->IsCompiled() && variation->GetPassCount() > 0 ? variation->GetPass(0) : nullptr;
		}

		void Setup(const TShared<RenderWindow>& renderWindow, const TShared<Shader>& inlineShader, const TShared<Shader>& pipelineShader,
			const TShared<Mesh>& texturedMesh, const TShared<Texture>& texturedMeshAlbedo)
		{
			const TShared<GpuDevice>& gpuDevice = GetApplication().GetPrimaryGpuDevice();
			gRenderWindow = renderWindow;

			// Inline ray tracing needs acceleration structures and ray queries. The shaders read the meshes through the bindless heap.
			const GpuDeviceCapabilities& capabilities = gpuDevice->GetCapabilities();
			gSupportsRayTracing = inlineShader != nullptr && capabilities.HasCapability(RSC_ACCELERATION_STRUCTURES) &&
				capabilities.HasCapability(RSC_RAY_QUERY) && gpuDevice->GetBindlessHeaps() != nullptr;

			if(!gSupportsRayTracing)
			{
				B3D_LOG(Info, LogGeneric, "The GPU device doesn't support inline ray tracing with bindless resources. The example presents a cleared "
					"window.");
				return;
			}

			// The inline shader has one compute program
			if(const TShared<Pass> inlinePass = GetPass(*inlineShader))
			{
				GpuComputePipelineStateCreateInformation pipelineInformation;
				pipelineInformation.Program = gpuDevice->CreateGpuProgram(inlinePass->GetGpuProgramCreateInformation(GPT_COMPUTE_PROGRAM));
				gInlinePipeline = gpuDevice->CreateGpuComputePipelineState(pipelineInformation);
			}

			if(gInlinePipeline == nullptr)
			{
				B3D_LOG(Error, LogGeneric, "Failed to create the inline ray tracing pipeline.");
				gSupportsRayTracing = false;
				return;
			}

			// The pipeline shader has a ray generation program, a miss program per ray type, and the programs of the hit groups
			if(pipelineShader != nullptr && capabilities.HasCapability(RSC_RAY_TRACING_PIPELINE))
			{
				if(const TShared<Pass> pipelinePass = GetPass(*pipelineShader))
				{
					GpuRayTracingPipelineStateCreateInformation pipelineInformation;
					pipelineInformation.RayGenerationPrograms.push_back(CreateProgram(*gpuDevice, *pipelinePass, "TraceScene"));
					pipelineInformation.MissPrograms.push_back(CreateProgram(*gpuDevice, *pipelinePass, "MissPrimary"));
					pipelineInformation.MissPrograms.push_back(CreateProgram(*gpuDevice, *pipelinePass, "MissShadow"));

					// Hit group 0 shades primary rays on opaque instances, 1 shades primary rays on cutouts after their alpha test, and 2 alpha
					// tests shadow rays on cutouts
					const TShared<GpuProgram> shadeHit = CreateProgram(*gpuDevice, *pipelinePass, "ShadeHit");
					pipelineInformation.HitGroups.push_back(GpuRayTracingHitGroup(shadeHit));
					pipelineInformation.HitGroups.push_back(GpuRayTracingHitGroup(shadeHit, CreateProgram(*gpuDevice, *pipelinePass, "AlphaTestPrimary")));
					pipelineInformation.HitGroups.push_back(GpuRayTracingHitGroup(nullptr, CreateProgram(*gpuDevice, *pipelinePass, "AlphaTestShadow")));

					gRayTracingPipeline = gpuDevice->CreateGpuRayTracingPipelineState(pipelineInformation);
				}

				if(gRayTracingPipeline == nullptr)
					B3D_LOG(Error, LogGeneric, "Failed to create the ray tracing pipeline.");
				else
				{
					// One miss record per ray type, and the hit group records at the instances' offsets plus the ray type. Shadow rays on
					// opaque instances need no program: they accept the first hit and skip closest hit programs.
					GpuShaderBindingTableCreateInformation tableInformation;
					tableInformation.Pipeline = gRayTracingPipeline;
					tableInformation.RayGenerationRecords = { 0 };
					tableInformation.MissRecords = { 0, 1 };
					tableInformation.HitGroupRecords = { 0, kGpuShaderBindingTableEmptyRecord, 1, 2 };

					gShaderBindingTable = gpuDevice->CreateGpuShaderBindingTable(tableInformation);
				}
			}

			// Create the meshes. Their ray tracing geometries are created on first use, and built by the scene.
			const TShared<Mesh> planeMesh = CreatePlaneMesh(20.0f);
			const TShared<Mesh> boxMesh = CreateBoxMesh(1.0f);
			const TShared<Mesh> quadMesh = CreateQuadMesh(3.0f, 3.0f);
			gMeshes = { planeMesh, boxMesh, quadMesh };
			if(texturedMesh != nullptr)
				gMeshes.push_back(texturedMesh);

			// Put the layouts of every geometry in one table, and remember where each mesh's layouts start
			Vector<RayTracingGeometryLayout> layouts;
			UnorderedMap<const Mesh*, u32> firstLayouts;
			for(const TShared<Mesh>& mesh : gMeshes)
			{
				const TShared<RayTracingGeometry>& geometry = mesh->GetOrCreateRayTracingGeometry();
				if(geometry == nullptr)
					continue;

				firstLayouts[mesh.get()] = (u32)layouts.size();
				for(const RayTracingGeometryLayout& layout : geometry->GetGeometryLayouts())
					layouts.push_back(layout);
			}

			// Place the instances. The instance id indexes the object data.
			gScene = B3DMakeShared<RayTracingScene>(*gpuDevice, "Example scene");
			Vector<ObjectData> objects;

			const auto fnAddInstance = [&](const TShared<Mesh>& mesh, const Matrix4& transform, ExampleMaterial material, const Color& color)
			{
				const TShared<RayTracingGeometry>& geometry = mesh->GetOrCreateRayTracingGeometry();
				if(geometry == nullptr)
					return ~0u;

				ObjectData object;
				object.FirstLayout = firstLayouts[mesh.get()];
				object.Material = material;
				object.Padding[0] = object.Padding[1] = 0;
				object.Color = color;

				RayTracingGeometryInstance instance(geometry, transform, (u32)objects.size());
				objects.push_back(object);

				// Alpha tested instances run any hit programs, which only run for geometry that isn't opaque
				if(material == ExampleMaterial::Cutout)
				{
					instance.HitGroupIndexOffset = kCutoutHitGroupOffset;
					instance.Flags = GpuRayTracingInstanceFlag::ForceNonOpaque;
				}
				else
					instance.HitGroupIndexOffset = kOpaqueHitGroupOffset;

				return gScene->AddInstance(instance);
			};

			fnAddInstance(planeMesh, Matrix4::kIdentity, ExampleMaterial::Checker, Color(0.6f, 0.6f, 0.6f));
			fnAddInstance(boxMesh, Matrix4::TRS(Vector3(2.5f, 0.75f, 2.0f), Quaternion(Vector3::kUnitY, Degree(30.0f)), Vector3::kOne * 1.5f),
				ExampleMaterial::Solid, Color(0.6f, 0.04f, 0.02f));
			fnAddInstance(boxMesh, Matrix4::TRS(Vector3(4.5f, 0.5f, -0.5f), Quaternion(Vector3::kUnitY, Degree(-15.0f)), Vector3::kOne),
				ExampleMaterial::Solid, Color(0.03f, 0.45f, 0.05f));
			gMovingInstanceSlot = fnAddInstance(boxMesh, GetMovingInstanceTransform(0), ExampleMaterial::Solid, Color(0.03f, 0.1f, 0.7f));
			fnAddInstance(quadMesh, Matrix4::TRS(Vector3(-3.5f, 1.5f, 1.5f), Quaternion(Vector3::kUnitY, Degree(-20.0f)), Vector3::kOne),
				ExampleMaterial::Cutout, Color(0.9f, 0.35f, 0.01f));

			// Turn the textured mesh to show its side, scale it to fit a 3 unit box, and place it a little above the ground
			if(texturedMesh != nullptr)
			{
				const AABox bounds = texturedMesh->GetProperties().Bounds.GetBox();
				const Vector3 size = bounds.GetSize();
				const float scale = 3.0f / std::max(std::max(size.X, size.Y), std::max(size.Z, 0.001f));

				const Quaternion rotation(Vector3::kUnitY, Degree(-70.0f));
				Matrix4 transform = Matrix4::TRS(Vector3::kZero, rotation, Vector3::kOne * scale) *
					Matrix4::Translation(-bounds.GetCenter());

				AABox placedBounds = bounds;
				placedBounds.TransformAffine(transform);
				transform = Matrix4::Translation(Vector3(0.0f, 0.5f - placedBounds.Minimum.Y, 0.0f)) * transform;

				fnAddInstance(texturedMesh, transform, ExampleMaterial::Textured, Color::kWhite);
			}

			// The tables never change and every ray reads them, so they live in the memory the GPU reads fastest. GpuBufferUtility::Write()
			// uploads them through a staging buffer where the CPU can't write that memory directly.
			gObjectBuffer = gpuDevice->CreateGpuBuffer(GpuBufferCreateInformation::CreateStructuredStorage(sizeof(ObjectData), (u32)objects.size()));
			gLayoutBuffer = gpuDevice->CreateGpuBuffer(GpuBufferCreateInformation::CreateStructuredStorage(sizeof(RayTracingGeometryLayout),
				(u32)layouts.size()));

			GpuWorkContext& gpuContext = GetRenderer()->GetGpuContext();
			GpuBufferUtility::Write(gpuContext, gObjectBuffer, 0, (u32)(objects.size() * sizeof(ObjectData)), objects.data());
			GpuBufferUtility::Write(gpuContext, gLayoutBuffer, 0, (u32)(layouts.size() * sizeof(RayTracingGeometryLayout)), layouts.data());

			// Shaders always sample a texture, so use a white one if the albedo texture failed to load
			gAlbedoTexture = texturedMeshAlbedo;
			if(gAlbedoTexture == nullptr)
			{
				TShared<PixelData> pixelData = PixelData::Create(1, 1, 1, PF_RGBA8);
				pixelData->SetColorAt(Color::kWhite, 0, 0);
				gAlbedoTexture = gpuDevice->CreateTexture(pixelData);
			}

			gAlbedoSampler = gpuDevice->FindOrCreateSamplerState(SamplerStateCreateInformation());

			// The image the rays write into. The frame graph creates it every frame, in memory it reuses between frames.
			gOutputInformation.Width = kWindowWidth;
			gOutputInformation.Height = kWindowHeight;
			gOutputInformation.Format = PF_RGBA8;
			gOutputInformation.Usage = TextureUsageFlag::AllowUnorderedAccessOnTheGPU;

#if B3D_BUILD_TYPE_DEVELOPMENT
			gReadbackBuffer = gpuDevice->CreateGpuBuffer(GpuBufferCreateInformation::CreateStagingRead(kWindowWidth * kWindowHeight * 4));
#endif
		}

#if B3D_BUILD_TYPE_DEVELOPMENT
		/**
		 * Compares the two halves of the image the readback buffer holds, which the two modes traced from the same camera, and logs an error
		 * if they differ in more than a few pixels.
		 */
		void CompareHalves()
		{
			const GpuBufferMappedScope mapping = gReadbackBuffer->Map(GpuMapOption::Read);
			if(!mapping.IsValid())
				return;

			const u8* pixels = static_cast<const u8*>(mapping.GetMappedMemory());
			const u32 viewWidth = kWindowWidth / 2;

			u32 differentPixelCount = 0;
			for(u32 y = 0; y < kWindowHeight; y++)
			{
				for(u32 x = 0; x < viewWidth; x++)
				{
					const u8* left = pixels + (y * kWindowWidth + x) * 4;
					const u8* right = left + viewWidth * 4;

					for(u32 channel = 0; channel < 3; channel++)
					{
						if(std::abs((i32)left[channel] - (i32)right[channel]) > 8)
						{
							differentPixelCount++;
							break;
						}
					}
				}
			}

			// Floating point differences between the programs may flip a few pixels on the edges of triangles and alpha tested holes
			const u32 pixelCount = viewWidth * kWindowHeight;
			if(differentPixelCount * 1000 > pixelCount)
				B3D_LOG(Error, LogGeneric, "The inline and pipeline halves of the image differ in {0} of {1} pixels.", differentPixelCount, pixelCount);
			else
				B3D_LOG(Info, LogGeneric, "The inline and pipeline halves of the image differ in {0} of {1} pixels.", differentPixelCount, pixelCount);
		}
#endif

		void Render()
		{
			const TShared<GpuDevice>& gpuDevice = GetApplication().GetPrimaryGpuDevice();
			GpuWorkContext& gpuContext = GetRenderer()->GetGpuContext();

			FrameGraph frameGraph(gpuContext, "Ray tracing");
			frameGraph.ImportRenderTarget(gRenderWindow);

			if(!gSupportsRayTracing)
			{
				FrameGraphRasterPass clearPass;
				clearPass.SetRenderTarget(gRenderWindow, FrameGraphAttachmentLoad::Clear);
				frameGraph.AddRasterPass("Clear", std::move(clearPass), nullptr);

				frameGraph.Compile();
				frameGraph.Execute();
				gpuDevice->PresentRenderWindow(gRenderWindow);
				return;
			}

			// Move the box, and build what changed. The move is by frame index, not time, so a given frame always looks the same.
			gScene->SetInstanceTransform(gMovingInstanceSlot, GetMovingInstanceTransform(gFrameIndex));
			gScene->Build(frameGraph);

			const TShared<Texture> output = frameGraph.CreateTexture("Ray traced image", gOutputInformation);

			// Split the image between the modes
			const u32 mode = gRayTracingMode;
			const bool hasPipeline = gShaderBindingTable != nullptr;
			const u32 pipelineViewOffset = !hasPipeline || mode == 1 ? kWindowWidth : (mode == 2 ? 0 : kWindowWidth / 2);

			// Adds a pass that traces the view @p viewWidth pixels wide at @p viewOffset, through the pipeline if @p isPipeline is true
			const auto fnAddTracePass = [&](const char* name, u32 viewOffset, u32 viewWidth, bool isPipeline)
			{
				if(viewWidth == 0)
					return;

				const ParamsBlock params = CreateParams(viewOffset, viewWidth);
				const TShared<GpuBuffer> paramsBuffer = gpuDevice->CreateGpuBuffer(GpuBufferCreateInformation::CreateUniform(sizeof(ParamsBlock)));
				{
					const GpuBufferMappedScope mapping = paramsBuffer->Map(GpuMapOption::Write);
					std::memcpy(mapping.GetMappedMemory(), &params, sizeof(params));
				}

				const TShared<GpuPipelineParameterLayout>& parameterLayout = isPipeline ? gRayTracingPipeline->GetParameterLayout() :
					gInlinePipeline->GetParameterLayout();

				// The parameter set tells the graph what the pass reads and writes: the top level structure the scene built, and the image
				const TShared<GpuParameterSet> parameters = gpuContext.GetParameterSetPool().Create(parameterLayout->GetSet(0), 0);
				parameters->SetUniformBuffer("Params", paramsBuffer);
				parameters->SetAccelerationStructure("gScene", gScene->GetAccelerationStructure());
				parameters->SetStorageBuffer("gObjects", gObjectBuffer);
				parameters->SetStorageBuffer("gLayouts", gLayoutBuffer);
				parameters->SetSampledTexture("gTextureAlbedo", gAlbedoTexture);
				parameters->SetSamplerState("gTextureSampler", gAlbedoSampler);
				parameters->SetStorageTexture("gOutput", output, TextureSurface::kComplete);

				FrameGraphComputePass pass;
				pass.AddParameterSet(parameters);
				frameGraph.AddComputePass(name, std::move(pass), [parameters, viewWidth, isPipeline](FrameGraphPassContext& passContext)
				{
					GpuCommandBuffer& commandBuffer = passContext.GetCommandBuffer();
					commandBuffer.SetGpuParameterSet(parameters);

					// A ray tracing dispatch binds the pipeline of its table, and runs the ray generation program once per pixel
					if(isPipeline)
						commandBuffer.DispatchRays(gShaderBindingTable, 0, viewWidth, kWindowHeight);
					else
					{
						commandBuffer.SetGpuComputePipelineState(gInlinePipeline);
						commandBuffer.DispatchCompute((viewWidth + 7) / 8, (kWindowHeight + 7) / 8);
					}
				});
			};

			fnAddTracePass("Trace inline", 0, pipelineViewOffset, false);
			if(hasPipeline)
				fnAddTracePass("Trace through the pipeline", pipelineViewOffset, kWindowWidth - pipelineViewOffset, true);

#if B3D_BUILD_TYPE_DEVELOPMENT
			// Read the image back once, to check that both modes traced the same image
			const bool compareHalves = gFrameIndex == kCompareFrameIndex && hasPipeline && mode == 0;
			if(compareHalves)
			{
				frameGraph.ImportBuffer(gReadbackBuffer);

				FrameGraphCopyPass readPass;
				readPass.AddSource(output);
				readPass.AddDestination(gReadbackBuffer);
				frameGraph.AddCopyPass("Read the image", std::move(readPass), [output](FrameGraphPassContext& passContext)
				{
					passContext.GetCommandBuffer().CopyTextureToBuffer(output, gReadbackBuffer, 0, 0);
				});
			}
#endif

			// Copy the image into the window, as in the LowLevelRendering example
			BlitMat* const blitMaterial = BlitMat::GetVariation(1, true, false);

			FrameGraphRasterPass blitPass;
			blitPass.SetRenderTarget(gRenderWindow, FrameGraphAttachmentLoad::DontCare);

			const TShared<GpuParameterSet> blitParameters = blitMaterial->Prepare(blitPass, output);
			frameGraph.AddRasterPass("Blit to window", std::move(blitPass), [blitMaterial, blitParameters](FrameGraphPassContext& passContext)
			{
				blitMaterial->Execute(passContext, blitParameters, Area2(0.0f, 0.0f, (float)kWindowWidth, (float)kWindowHeight), false);
			});

			frameGraph.Compile();
			frameGraph.Execute();

#if B3D_BUILD_TYPE_DEVELOPMENT
			if(compareHalves)
			{
				gpuDevice->WaitUntilIdle();
				CompareHalves();
			}
#endif

			gpuDevice->PresentRenderWindow(gRenderWindow);
			gFrameIndex++;
		}

		void Shutdown()
		{
			gShaderBindingTable = nullptr;
			gRayTracingPipeline = nullptr;
			gInlinePipeline = nullptr;
			gScene = nullptr;
			gMeshes.clear();
			gObjectBuffer = nullptr;
			gLayoutBuffer = nullptr;
			gAlbedoTexture = nullptr;
			gAlbedoSampler = nullptr;
			gRenderWindow = nullptr;

#if B3D_BUILD_TYPE_DEVELOPMENT
			gReadbackBuffer = nullptr;
#endif
		}

		/////////////////////////////////////////////////////////////////////////////////////
		//////////////////////////////////HELPER METHODS/////////////////////////////////////
		/////////////////////////////////////////////////////////////////////////////////////

		/** Returns the transform of the moving box in frame @p frameIndex: it circles the textured mesh once every 240 frames. */
		Matrix4 GetMovingInstanceTransform(u32 frameIndex)
		{
			const Radian angle = Degree(frameIndex * 1.5f + 35.0f);
			const Vector3 position(std::cos(angle.GetValueInRadians()) * 3.5f, 0.5f, std::sin(angle.GetValueInRadians()) * 3.5f);

			return Matrix4::TRS(position, Quaternion(Vector3::kUnitY, angle), Vector3::kOne);
		}

		/** Returns the uniform block of a view @p viewWidth pixels wide, at @p viewOffset in the image. */
		ParamsBlock CreateParams(u32 viewOffset, u32 viewWidth)
		{
			const Vector3 cameraPosition(0.0f, 4.5f, -9.0f);
			const Vector3 forward = Vector3::Normalize(Vector3(0.0f, 1.0f, 0.0f) - cameraPosition);
			const Vector3 right = Vector3::Normalize(Vector3::kUnitY.Cross(forward));
			const Vector3 up = forward.Cross(right);

			const float verticalScale = Math::Tan(Degree(30.0f));
			const float horizontalScale = verticalScale * (float)viewWidth / (float)kWindowHeight;

			ParamsBlock params;
			params.CameraPosition = Vector4(cameraPosition, 0.0f);
			params.CameraRight = Vector4(right * horizontalScale, 0.0f);
			params.CameraUp = Vector4(up * verticalScale, 0.0f);
			params.CameraForward = Vector4(forward, 0.0f);
			params.LightDirection = Vector4(Vector3::Normalize(Vector3(-0.4f, -1.0f, -0.6f)), 0.0f);
			params.ViewSize[0] = viewWidth;
			params.ViewSize[1] = kWindowHeight;
			params.ViewOffset = viewOffset;
			params.Padding = 0;
			return params;
		}

		/** Creates a static mesh that supports ray tracing, with one triangle list sub-mesh. */
		TShared<Mesh> CreateMesh(TArrayView<const Vector3> positions, TArrayView<const Vector3> normals, TArrayView<const Vector2> uvs,
			TArrayView<const u32> indices)
		{
			TInlineArray<VertexElement, 8> vertexElements;
			vertexElements.Add(VertexElement(VET_FLOAT3, VES_POSITION));
			vertexElements.Add(VertexElement(VET_FLOAT3, VES_NORMAL));
			vertexElements.Add(VertexElement(VET_FLOAT2, VES_TEXCOORD));

			const TShared<MeshData> meshData = B3DMakeShared<MeshData>((u32)positions.size(), (u32)indices.size(),
				B3DMakeShared<VertexDescription>(vertexElements));
			meshData->SetVertexData(VES_POSITION, (void*)positions.data(), (u32)(positions.size() * sizeof(Vector3)));
			meshData->SetVertexData(VES_NORMAL, (void*)normals.data(), (u32)(normals.size() * sizeof(Vector3)));
			meshData->SetVertexData(VES_TEXCOORD, (void*)uvs.data(), (u32)(uvs.size() * sizeof(Vector2)));
			std::memcpy(meshData->GetIndices32(), indices.data(), indices.size() * sizeof(u32));

			return Mesh::Create(RendererMeshData::Convert(meshData), MeshFlag::Static | MeshFlag::SupportsRayTracing);
		}

		/** Creates a square @p size wide, lying on the XZ plane and centered on the origin. */
		TShared<Mesh> CreatePlaneMesh(float size)
		{
			const float half = size * 0.5f;
			const Vector3 positions[] = { Vector3(-half, 0.0f, -half), Vector3(half, 0.0f, -half), Vector3(half, 0.0f, half), Vector3(-half, 0.0f, half) };
			const Vector3 normals[] = { Vector3::kUnitY, Vector3::kUnitY, Vector3::kUnitY, Vector3::kUnitY };
			const Vector2 uvs[] = { Vector2(0.0f, 0.0f), Vector2(1.0f, 0.0f), Vector2(1.0f, 1.0f), Vector2(0.0f, 1.0f) };
			const u32 indices[] = { 0, 2, 1, 0, 3, 2 };

			return CreateMesh(TArrayView<const Vector3>(positions, std::size(positions)), TArrayView<const Vector3>(normals, std::size(normals)),
				TArrayView<const Vector2>(uvs, std::size(uvs)), TArrayView<const u32>(indices, std::size(indices)));
		}

		/** Creates a rectangle @p width by @p height, on the XY plane and centered on the origin. */
		TShared<Mesh> CreateQuadMesh(float width, float height)
		{
			const float halfWidth = width * 0.5f;
			const float halfHeight = height * 0.5f;
			const Vector3 positions[] = { Vector3(-halfWidth, -halfHeight, 0.0f), Vector3(halfWidth, -halfHeight, 0.0f), Vector3(halfWidth, halfHeight, 0.0f),
				Vector3(-halfWidth, halfHeight, 0.0f) };
			const Vector3 normals[] = { -Vector3::kUnitZ, -Vector3::kUnitZ, -Vector3::kUnitZ, -Vector3::kUnitZ };
			const Vector2 uvs[] = { Vector2(0.0f, 1.0f), Vector2(1.0f, 1.0f), Vector2(1.0f, 0.0f), Vector2(0.0f, 0.0f) };
			const u32 indices[] = { 0, 2, 1, 0, 3, 2 };

			return CreateMesh(TArrayView<const Vector3>(positions, std::size(positions)), TArrayView<const Vector3>(normals, std::size(normals)),
				TArrayView<const Vector2>(uvs, std::size(uvs)), TArrayView<const u32>(indices, std::size(indices)));
		}

		/** Creates a cube @p size wide, centered on the origin, with four vertices of its own per face. */
		TShared<Mesh> CreateBoxMesh(float size)
		{
			const float half = size * 0.5f;
			const Vector3 faceNormals[] = { Vector3::kUnitX, -Vector3::kUnitX, Vector3::kUnitY, -Vector3::kUnitY, Vector3::kUnitZ, -Vector3::kUnitZ };

			Vector3 positions[24];
			Vector3 normals[24];
			Vector2 uvs[24];
			u32 indices[36];

			for(u32 face = 0; face < 6; face++)
			{
				// Two axes along the face, perpendicular to its normal
				const Vector3 normal = faceNormals[face];
				const Vector3 tangent = std::abs(normal.Y) > 0.5f ? Vector3::kUnitX : Vector3::kUnitY;
				const Vector3 bitangent = normal.Cross(tangent);

				const Vector2 corners[] = { Vector2(-1.0f, -1.0f), Vector2(1.0f, -1.0f), Vector2(1.0f, 1.0f), Vector2(-1.0f, 1.0f) };
				for(u32 corner = 0; corner < 4; corner++)
				{
					const u32 vertex = face * 4 + corner;
					positions[vertex] = (normal + tangent * corners[corner].X + bitangent * corners[corner].Y) * half;
					normals[vertex] = normal;
					uvs[vertex] = (corners[corner] + Vector2::kOne) * 0.5f;
				}

				const u32 faceIndices[] = { 0, 1, 2, 0, 2, 3 };
				for(u32 index = 0; index < 6; index++)
					indices[face * 6 + index] = face * 4 + faceIndices[index];
			}

			return CreateMesh(TArrayView<const Vector3>(positions, std::size(positions)), TArrayView<const Vector3>(normals, std::size(normals)),
				TArrayView<const Vector2>(uvs, std::size(uvs)), TArrayView<const u32>(indices, std::size(indices)));
		}
	} // namespace render
} // namespace b3d
