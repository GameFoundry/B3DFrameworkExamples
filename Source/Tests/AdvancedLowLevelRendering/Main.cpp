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
#include "Image/B3DTexture.h"
#include "GpuBackend/B3DRenderTexture.h"
#include "Renderer/B3DRenderer.h"
#include "Renderer/B3DRendererManager.h"
#include "GpuBackend/B3DGpuParameterSetPool.h"

/**
 * Low-level rendering test bed for newly introduced GPU features. It currently verifies graphics and compute push
 * constants, including partial updates and propagation of compute results into a subsequent graphics pass.
 */
namespace b3d
{
	constexpr u32 kWindowWidth = 1280;
	constexpr u32 kWindowHeight = 720;
	float gFrameTimeSeconds = 0.0f;

	namespace render
	{
		void Setup(const TShared<RenderWindow>& renderWindow, const TShared<Shader>& graphicsShader,
			const TShared<Shader>& computeShader);
		void Render();
		void Shutdown();
	}

	class AdvancedLowLevelRenderingApplication : public Application
	{
	public:
		AdvancedLowLevelRenderingApplication(const ApplicationCreateInformation& createInformation)
			: Application(createInformation)
		{ }

	private:
		void OnStartUp() override
		{
			Application::OnStartUp();

			const TShared<render::RenderWindow> renderWindow = B3DGetRenderProxy(GetPrimaryWindow());
			mGraphicsShader = ExampleFramework::LoadShader("AdvancedLowLevelRendering.bsl");
			mComputeShader = ExampleFramework::LoadShader("AdvancedLowLevelRenderingCompute.bsl");
			const TShared<render::Shader> graphicsShader = B3DGetRenderProxy(mGraphicsShader);
			const TShared<render::Shader> computeShader = B3DGetRenderProxy(mComputeShader);

			GetRenderThread().PostCommand([renderWindow, graphicsShader, computeShader]
			{
				render::Setup(renderWindow, graphicsShader, computeShader);
			});
		}

		void OnShutDown() override
		{
			GetRenderThread().PostCommand(&render::Shutdown);
			mGraphicsShader = HShader();
			mComputeShader = HShader();

			Application::OnShutDown();
		}

		void PreUpdate() override
		{
			GetRenderThread().PostCommand(&render::Render);
			Application::PreUpdate();
		}

		HShader mGraphicsShader;
		HShader mComputeShader;
	};
}

int B3DMain()
{
	using namespace b3d;

	const VideoMode videoMode(kWindowWidth, kWindowHeight);
	const ApplicationCreateInformation createInformation = Application::BuildCreateInformation(videoMode, "Advanced Low Level Rendering", false);

	Application::StartUp<AdvancedLowLevelRenderingApplication>(createInformation);
	Application::Instance().RunMainLoop();
	Application::ShutDown();

	return 0;
}

namespace b3d::render
{
	TShared<GpuGraphicsPipelineState> gGraphicsPipelineState;
	TShared<GpuComputePipelineState> gComputePipelineState;
	TShared<Texture> gSurfaceTexture;
	TShared<SamplerState> gSurfaceSampler;
	TShared<GpuParameterSet> gGraphicsParameters;
	TShared<GpuParameterSet> gComputeParameters;
	TShared<VertexDescription> gVertexDescription;
	TShared<GpuBuffer> gVertexBuffer;
	TShared<GpuBuffer> gIndexBuffer;
	TShared<GpuBuffer> gComputeOutput;
	TShared<RenderTexture> gRenderTarget;
	TShared<RenderWindow> gRenderWindow;
	TShared<GpuCommandBufferPool> gCommandBufferPool;

	constexpr u32 kVertexCount = 24;
	constexpr u32 kIndexCount = 36;

	struct UniformBlock
	{
		Matrix4 GMatWvp;
	};

	void WriteBoxVertices(const AABox& box, u8* positions, u8* uvs, u32 stride);
	void WriteBoxIndices(u32* indices);
	Matrix4 CreateWorldViewProjectionMatrix();

	void Setup(const TShared<RenderWindow>& renderWindow, const TShared<Shader>& graphicsShader, const TShared<Shader>& computeShader)
	{
		const TShared<GpuDevice>& gpuDevice = GetApplication().GetPrimaryGpuDevice();
		GpuWorkContext& gpuContext = GetRenderer()->GetGpuContext();

		// Command recording and pipelines.
		GpuCommandBufferPoolCreateInformation poolCreateInformation;
		poolCreateInformation.Thread = B3D_CURRENT_THREAD_ID;
		poolCreateInformation.Type = GQT_GRAPHICS;
		gCommandBufferPool = gpuDevice->CreateGpuCommandBufferPool(poolCreateInformation);
		gRenderWindow = renderWindow;

		const TShared<Variation> graphicsVariation = graphicsShader->GetCompatibleVariations()[0];
		if(!graphicsVariation->IsCompiled())
			graphicsVariation->Compile().BlockUntilComplete();

		const TShared<Variation> computeVariation = computeShader->GetCompatibleVariations()[0];
		if(!computeVariation->IsCompiled())
			computeVariation->Compile().BlockUntilComplete();

		const PassInformation& graphicsPass = graphicsVariation->GetPass(0)->GetInformation();
		GpuGraphicsPipelineStateCreateInformation graphicsPipelineCreateInformation;
		graphicsPipelineCreateInformation.VertexProgram = gpuDevice->CreateGpuProgram(graphicsPass.VertexProgramCreateInformation);
		graphicsPipelineCreateInformation.FragmentProgram = gpuDevice->CreateGpuProgram(graphicsPass.FragmentProgramCreateInformation);
		graphicsPipelineCreateInformation.BlendState = graphicsPass.BlendStateInformation;
		graphicsPipelineCreateInformation.RasterizerState = graphicsPass.RasterizerStateInformation;
		graphicsPipelineCreateInformation.DepthStencilState = graphicsPass.DepthStencilStateInformation;

		gGraphicsPipelineState = gpuDevice->CreateGpuGraphicsPipelineState(graphicsPipelineCreateInformation);

		const PassInformation& computePass = computeVariation->GetPass(0)->GetInformation();
		GpuComputePipelineStateCreateInformation computePipelineCreateInformation;
		computePipelineCreateInformation.Program = gpuDevice->CreateGpuProgram(computePass.ComputeProgramCreateInformation);
		gComputePipelineState = gpuDevice->CreateGpuComputePipelineState(computePipelineCreateInformation);

		B3D_ASSERT(gGraphicsPipelineState->GetParameterLayout()->GetPushConstantBufferSize() == 16);
		B3D_ASSERT(gComputePipelineState->GetParameterLayout()->GetPushConstantBufferSize() == 16);

		// Shared compute output and parameter sets.
		GpuParameterSetPool& parameterSetPool = gpuContext.GetParameterSetPool();
		gGraphicsParameters = parameterSetPool.Create(gGraphicsPipelineState->GetParameterLayout()->GetSet(0), 0);
		gComputeParameters = parameterSetPool.Create(gComputePipelineState->GetParameterLayout()->GetSet(0), 0);

		const GpuBufferFlags computeBufferFlags = GpuBufferFlag::StoreOnGPU | GpuBufferFlag::AllowUnorderedAccessOnTheGPU;
		gComputeOutput = gpuDevice->CreateGpuBuffer(GpuBufferCreateInformation::CreateStructuredStorage(sizeof(float) * 4, 1, computeBufferFlags));
		gGraphicsParameters->SetStorageBuffer("gComputeOutput", gComputeOutput);
		gComputeParameters->SetStorageBuffer("gComputeOutput", gComputeOutput);

		// Cube geometry.
		TInlineArray<VertexElement, 8> vertexElements;
		vertexElements.Add(VertexElement(VET_FLOAT3, VES_POSITION));
		vertexElements.Add(VertexElement(VET_FLOAT2, VES_TEXCOORD));
		gVertexDescription = B3DMakeShared<VertexDescription>(vertexElements);

		const u32 vertexStride = gVertexDescription->GetVertexStride();
		gVertexBuffer = gpuDevice->CreateGpuBuffer(GpuBufferCreateInformation::CreateVertex(vertexStride, kVertexCount));
		u8* vertexData = (u8*)B3DStackAllocate(vertexStride * kVertexCount);
		u8* positions = vertexData + gVertexDescription->GetElementOffsetFromStream(VES_POSITION);
		u8* uvs = vertexData + gVertexDescription->GetElementOffsetFromStream(VES_TEXCOORD);
		WriteBoxVertices(AABox(Vector3::kOne * -10.0f, Vector3::kOne * 10.0f), positions, uvs, vertexStride);
		GpuBufferUtility::Write(gpuContext, gVertexBuffer, 0, vertexStride * kVertexCount, vertexData, GpuBufferWriteFlag::Discard);
		B3DStackFree(vertexData);

		gIndexBuffer = gpuDevice->CreateGpuBuffer(GpuBufferCreateInformation::CreateIndex(IT_32BIT, kIndexCount));
		u32* indexData = (u32*)B3DStackAllocate(kIndexCount * sizeof(u32));
		WriteBoxIndices(indexData);
		GpuBufferUtility::Write(gpuContext, gIndexBuffer, 0, kIndexCount * sizeof(u32), indexData, GpuBufferWriteFlag::Discard);
		B3DStackFree(indexData);

		// Surface texture, sampler and off-screen render target.
		const TShared<PixelData> pixelData = PixelData::Create(2, 2, 1, PF_RGBA8);
		pixelData->SetColorAt(Color::kWhite, 0, 0);
		pixelData->SetColorAt(Color::kBlack, 1, 0);
		pixelData->SetColorAt(Color::kWhite, 1, 1);
		pixelData->SetColorAt(Color::kBlack, 0, 1);
		gSurfaceTexture = gpuDevice->CreateTexture(pixelData);

		SamplerStateCreateInformation samplerCreateInformation;
		samplerCreateInformation.MinFilter = FO_POINT;
		samplerCreateInformation.MagFilter = FO_POINT;
		gSurfaceSampler = gpuDevice->FindOrCreateSamplerState(samplerCreateInformation);

		TextureCreateInformation colorCreateInformation;
		colorCreateInformation.Width = kWindowWidth;
		colorCreateInformation.Height = kWindowHeight;
		colorCreateInformation.Format = PF_RGBA8;
		colorCreateInformation.Usage = TextureUsageFlag::RenderTarget;
		colorCreateInformation.ClearColor = Color::kBlue;

		TextureCreateInformation depthCreateInformation;
		depthCreateInformation.Width = kWindowWidth;
		depthCreateInformation.Height = kWindowHeight;
		depthCreateInformation.Format = PF_D32;
		depthCreateInformation.Usage = TextureUsageFlag::DepthStencil;

		RenderTextureCreateInformation renderTargetCreateInformation;
		renderTargetCreateInformation.ColorSurfaces[0].Texture = gpuDevice->CreateTexture(colorCreateInformation);
		renderTargetCreateInformation.DepthStencilSurface.Texture = gpuDevice->CreateTexture(depthCreateInformation);
		gRenderTarget = RenderTexture::Create(renderTargetCreateInformation);
	}

	void Render()
	{
		const TShared<GpuDevice>& gpuDevice = GetApplication().GetPrimaryGpuDevice();
		GpuWorkContext& gpuContext = GetRenderer()->GetGpuContext();

		// Per-frame transform data retained from the original LowLevelRendering example.
		const UniformBlock uniformBlock{ CreateWorldViewProjectionMatrix() };
		const TShared<GpuBuffer> uniformBuffer = gpuDevice->CreateGpuBuffer(GpuBufferCreateInformation::CreateUniform(sizeof(UniformBlock)));
		{
			GpuBufferMappedScope mappedBuffer = uniformBuffer->Map(GpuMapOption::Write);
			memcpy(mappedBuffer.GetMappedMemory(), &uniformBlock, sizeof(uniformBlock));
		}

		gGraphicsParameters->SetUniformBuffer("Params", uniformBuffer);
		gGraphicsParameters->SetSampledTexture("gMainTexture", gSurfaceTexture);
		gGraphicsParameters->SetSamplerState("gMainTexSamp", gSurfaceSampler);

		const TShared<GpuCommandBuffer> commandBuffer = gCommandBufferPool->Create(GpuCommandBufferCreateInformation::Create("AdvancedLowLevelRendering"));

		// Exercise partial compute push-constant writes and publish their result for the graphics pass.
		const float computeConstants[4] = { 0.8f, 0.9f, 1.0f, 1.0f };
		commandBuffer->SetGpuComputePipelineState(gComputePipelineState);
		commandBuffer->SetGpuParameterSet(gComputeParameters);
		commandBuffer->SetPushConstants(0, sizeof(float) * 2, computeConstants);
		commandBuffer->SetPushConstants(sizeof(float) * 2, sizeof(float) * 2, computeConstants + 2);
		commandBuffer->DispatchCompute(1);

		// Draw two cubes with distinct graphics push constants; the right cube consumes the compute result.
		RenderPassCreateInformation renderPassCreateInformation(gRenderTarget, gGraphicsParameters, RT_NONE, RT_NONE);
		renderPassCreateInformation.ClearMask = RT_COLOR_ALL | RT_DEPTH;
		commandBuffer->BeginRenderPass(renderPassCreateInformation);
		commandBuffer->SetGpuGraphicsPipelineState(gGraphicsPipelineState);
		commandBuffer->SetVertexBuffers(0, &gVertexBuffer, 1);
		commandBuffer->SetIndexBuffer(gIndexBuffer);
		commandBuffer->SetVertexDescription(gVertexDescription);
		commandBuffer->SetDrawOperation(DOT_TRIANGLE_LIST);
		commandBuffer->SetGpuParameterSet(gGraphicsParameters);

		const float leftConstants[4] = { -0.45f, 1.0f, 0.2f, 0.2f };
		commandBuffer->SetPushConstants(0, sizeof(leftConstants), leftConstants);
		commandBuffer->DrawIndexed(0, kIndexCount, 0, kVertexCount, 1, 0);

		const float rightOffset = 0.45f;
		const float rightTint[3] = { 0.2f, 1.0f, 0.2f };
		commandBuffer->SetPushConstants(0, sizeof(rightOffset), &rightOffset);
		commandBuffer->SetPushConstants(sizeof(rightOffset), sizeof(rightTint), rightTint);
		commandBuffer->DrawIndexed(0, kIndexCount, 0, kVertexCount, 1, 0);

		commandBuffer->EndRenderPass();
		const TShared<Texture> colorTexture = gRenderTarget->GetColorSurfaceInformation(0).Texture;
		GetRendererUtility().Blit(*commandBuffer, BlitInformation::BlitColor(colorTexture, gRenderWindow));
		gpuContext.SubmitCommandBuffer(commandBuffer);
		gpuDevice->PresentRenderWindow(gRenderWindow);
	}

	void Shutdown()
	{
		gCommandBufferPool = nullptr;
		gGraphicsPipelineState = nullptr;
		gComputePipelineState = nullptr;
		gSurfaceTexture = nullptr;
		gSurfaceSampler = nullptr;
		gGraphicsParameters = nullptr;
		gComputeParameters = nullptr;
		gVertexDescription = nullptr;
		gVertexBuffer = nullptr;
		gIndexBuffer = nullptr;
		gComputeOutput = nullptr;
		gRenderTarget = nullptr;
		gRenderWindow = nullptr;
	}

	void WriteBoxVertices(const AABox& box, u8* positions, u8* uvs, u32 stride)
	{
		const AABox::Corner vertexOrder[] = {
			AABox::NearLeftBottom, AABox::NearRightBottom, AABox::NearRightTop, AABox::NearLeftTop,
			AABox::FarRightBottom, AABox::FarLeftBottom, AABox::FarLeftTop, AABox::FarRightTop,
			AABox::FarLeftBottom, AABox::NearLeftBottom, AABox::NearLeftTop, AABox::FarLeftTop,
			AABox::NearRightBottom, AABox::FarRightBottom, AABox::FarRightTop, AABox::NearRightTop,
			AABox::FarLeftTop, AABox::NearLeftTop, AABox::NearRightTop, AABox::FarRightTop,
			AABox::FarLeftBottom, AABox::FarRightBottom, AABox::NearRightBottom, AABox::NearLeftBottom
		};

		for(const AABox::Corner corner : vertexOrder)
		{
			const Vector3 position = box.GetCorner(corner);
			memcpy(positions, &position, sizeof(position));
			positions += stride;
		}

		for(u32 face = 0; face < 6; face++)
		{
			const Vector2 faceUvs[] = {
				Vector2(0.0f, 1.0f), Vector2(1.0f, 1.0f), Vector2(1.0f, 0.0f), Vector2(0.0f, 0.0f)
			};
			for(const Vector2& uv : faceUvs)
			{
				memcpy(uvs, &uv, sizeof(uv));
				uvs += stride;
			}
		}
	}

	void WriteBoxIndices(u32* indices)
	{
		for(u32 face = 0; face < 6; face++)
		{
			const u32 vertexOffset = face * 4;
			indices[face * 6 + 0] = vertexOffset + 2;
			indices[face * 6 + 1] = vertexOffset + 1;
			indices[face * 6 + 2] = vertexOffset;
			indices[face * 6 + 3] = vertexOffset;
			indices[face * 6 + 4] = vertexOffset + 3;
			indices[face * 6 + 5] = vertexOffset + 2;
		}
	}

	Matrix4 CreateWorldViewProjectionMatrix()
	{
		Matrix4 projection = Matrix4::ProjectionPerspective(Degree(75.0f), 16.0f / 9.0f, 0.05f, 1000.0f);
		if(const TShared<GpuDevice> gpuDevice = GetApplication().GetPrimaryGpuDevice())
			gpuDevice->ConvertProjectionMatrix(projection, projection);

		const Vector3 cameraPosition(0.0f, -20.0f, 50.0f);
		Quaternion cameraRotation(kIdentityTag);
		cameraRotation.LookRotation(-Vector3::Normalize(cameraPosition));
		const Matrix4 view = Matrix4::View(cameraPosition, cameraRotation);

		gFrameTimeSeconds += GetTime().GetFrameDelta();
		const Quaternion rotation(Vector3::kUnitY, Degree(gFrameTimeSeconds * 90.0f));
		const Matrix4 world = Matrix4::TRS(Vector3::kZero, rotation, Vector3::kOne);
		return (projection * view * world).Transpose();
	}
}
