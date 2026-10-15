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
#include "GpuBackend/B3DGpuPushConstants.h"
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
#include "FrameGraph/B3DFrameGraph.h"
#include "String/B3DStringFormat.h"

/**
 * Low-level rendering test bed for newly introduced GPU features. It currently verifies graphics and compute push
 * constants at every portable size through readback, plus wave/lane operations and compute-to-graphics propagation.
 */
namespace b3d
{
	constexpr u32 kWindowWidth = 1280;
	constexpr u32 kWindowHeight = 720;
	float gFrameTimeSeconds = 0.0f;

	namespace render
	{
		void Setup(const TShared<RenderWindow>& renderWindow, const TShared<Shader>& graphicsShader,
			const TShared<Shader>& computeShader, const TShared<Shader>& pushConstantShader, const TShared<Shader>& pushConstantComputeShader);
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
			mPushConstantShader = ExampleFramework::LoadShader("AdvancedPushConstantTests.bsl");
			mPushConstantComputeShader = ExampleFramework::LoadShader("AdvancedPushConstantComputeTests.bsl");
			if(!mGraphicsShader || !mComputeShader || !mPushConstantShader || !mPushConstantComputeShader)
				B3D_LOG(Fatal, LogGeneric, "AdvancedLowLevelRendering requires all four test shaders.");

			const TShared<render::Shader> graphicsShader = B3DGetRenderProxy(mGraphicsShader);
			const TShared<render::Shader> computeShader = B3DGetRenderProxy(mComputeShader);
			const TShared<render::Shader> pushConstantShader = B3DGetRenderProxy(mPushConstantShader);
			const TShared<render::Shader> pushConstantComputeShader = B3DGetRenderProxy(mPushConstantComputeShader);

			GetRenderThread().PostCommand([renderWindow, graphicsShader, computeShader, pushConstantShader, pushConstantComputeShader]
			{
				render::Setup(renderWindow, graphicsShader, computeShader, pushConstantShader, pushConstantComputeShader);
			});
		}

		void OnShutDown() override
		{
			GetRenderThread().PostCommand(&render::Shutdown);
			mGraphicsShader = HShader();
			mComputeShader = HShader();
			mPushConstantShader = HShader();
			mPushConstantComputeShader = HShader();

			Application::OnShutDown();
		}

		void PreUpdate() override
		{
			GetRenderThread().PostCommand(&render::Render);
			Application::PreUpdate();
		}

		HShader mGraphicsShader;
		HShader mComputeShader;
		HShader mPushConstantShader;
		HShader mPushConstantComputeShader;
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
	TextureCreateInformation gColorInformation;
	TextureCreateInformation gDepthInformation;
	TShared<RenderWindow> gRenderWindow;

	constexpr u32 kVertexCount = 24;
	constexpr u32 kIndexCount = 36;

	struct UniformBlock
	{
		Matrix4 GMatWvp;
	};

	void WriteBoxVertices(const AABox& box, u8* positions, u8* uvs, u32 stride);
	void WriteBoxIndices(u32* indices);
	Matrix4 CreateWorldViewProjectionMatrix();

	void Setup(const TShared<RenderWindow>& renderWindow, const TShared<Shader>& graphicsShader, const TShared<Shader>& computeShader, const TShared<Shader>& pushConstantShader, const TShared<Shader>& pushConstantComputeShader)
	{
		const TShared<GpuDevice>& gpuDevice = GetApplication().GetPrimaryGpuDevice();
		GpuWorkContext& gpuContext = GetRenderer()->GetGpuContext();
		gRenderWindow = renderWindow;

		// Compile pipelines from loaded bytecode
		Array<TShared<GpuGraphicsPipelineState>, 4> testGraphicsPipelines;
		Array<TShared<GpuComputePipelineState>, 4> testComputePipelines;
		for(const TShared<Shader>& testShader : { pushConstantShader, pushConstantComputeShader })
		{
			for(const TShared<Variation>& variation : testShader->GetCompatibleVariations())
			{
				ShaderVariationParameters variationParameters = variation->GetVariationParameters();
				const u32 valueCount = (u32)variationParameters.GetI32("VALUE_COUNT");
				if(valueCount < 1 || valueCount > kMaxPushConstantValueCount)
					B3D_LOG(Fatal, LogGeneric, "Invalid push-constant test variation.");

				if(!variation->IsCompiled())
					variation->Compile().BlockUntilComplete();

				if(!variation->IsCompiled() || variation->GetPassCount() != 1)
					B3D_LOG(Fatal, LogGeneric, "Missing compiled push-constant test pass.");

				const TShared<Pass> testPass = variation->GetPass(0);
				if(testPass == nullptr)
					B3D_LOG(Fatal, LogGeneric, "Missing push-constant test pass.");

				const PassInformation& pass = testPass->GetInformation();
				if(testShader == pushConstantComputeShader)
				{
					if(!pass.ComputeProgramCreateInformation.Bytecode || !pass.ComputeProgramCreateInformation.Bytecode->Instructions.Data)
						B3D_LOG(Fatal, LogGeneric, "Missing compute push-constant test bytecode.");

					GpuComputePipelineStateCreateInformation pipelineInformation;
					pipelineInformation.Program = gpuDevice->CreateGpuProgram(pass.ComputeProgramCreateInformation);

					testComputePipelines[valueCount - 1] = gpuDevice->CreateGpuComputePipelineState(pipelineInformation);
				}
				else
				{
					if(!pass.VertexProgramCreateInformation.Bytecode || !pass.VertexProgramCreateInformation.Bytecode->Instructions.Data || !pass.FragmentProgramCreateInformation.Bytecode || !pass.FragmentProgramCreateInformation.Bytecode->Instructions.Data)
						B3D_LOG(Fatal, LogGeneric, "Missing graphics push-constant test bytecode.");

					GpuGraphicsPipelineStateCreateInformation pipelineInformation;
					pipelineInformation.VertexProgram = gpuDevice->CreateGpuProgram(pass.VertexProgramCreateInformation);
					pipelineInformation.FragmentProgram = gpuDevice->CreateGpuProgram(pass.FragmentProgramCreateInformation);
					pipelineInformation.DepthStencilState = pass.DepthStencilStateInformation;
					pipelineInformation.RasterizerState.CullMode = CULL_NONE;

					testGraphicsPipelines[valueCount - 1] = gpuDevice->CreateGpuGraphicsPipelineState(pipelineInformation);
				}
			}
		}

		TextureCreateInformation testTextureInformation;
		testTextureInformation.Width = 1;
		testTextureInformation.Height = 1;
		testTextureInformation.Format = PF_RGBA32F;
		testTextureInformation.Usage = TextureUsageFlag::RenderTarget;

		const TShared<Texture> testTexture = gpuDevice->CreateTexture(testTextureInformation);
		const ImageSubresourcePitch testPitch = testTexture->GetStagingBufferPitchForSubresource(0, 0);
		const TShared<GpuBuffer> graphicsReadback = gpuDevice->CreateGpuBuffer(GpuBufferCreateInformation::CreateStagingRead(testPitch.RowPitch * testPitch.SliceHeight * sizeof(float) * 4));
		const TShared<GpuBuffer> computeReadback = gpuDevice->CreateGpuBuffer(GpuBufferCreateInformation::CreateStagingRead(sizeof(float) * 4));
		const TShared<GpuBuffer> testOutput = gpuDevice->CreateGpuBuffer(GpuBufferCreateInformation::CreateStructuredStorage(sizeof(float) * 4, 1, GpuBufferFlag::StoreOnGPU | GpuBufferFlag::AllowUnorderedAccessOnTheGPU));
		const TShared<VertexDescription> emptyVertexDescription = B3DMakeShared<VertexDescription>(TInlineArray<VertexElement, 8>());

		// One cached payload survives partial writes, smaller pipeline layouts, and graphics/compute transitions.
		float expectedValues[4];
		for(u32 valueCount = 1; valueCount <= kMaxPushConstantValueCount; valueCount++)
		{
			const TShared<GpuGraphicsPipelineState>& graphicsPipeline = testGraphicsPipelines[valueCount - 1];
			const TShared<GpuComputePipelineState>& computePipeline = testComputePipelines[valueCount - 1];
			if(!graphicsPipeline || !computePipeline || graphicsPipeline->GetParameterLayout()->GetPushConstantBufferSize() != valueCount * sizeof(float) || computePipeline->GetParameterLayout()->GetPushConstantBufferSize() != valueCount * sizeof(float))
				B3D_LOG(Fatal, LogGeneric, "Missing or incorrectly sized push-constant test pipeline.");

			const TShared<GpuParameterSet> parameters = gpuContext.GetParameterSetPool().Create(computePipeline->GetParameterLayout()->GetSet(0), 0);
			parameters->SetStorageBuffer("gOutput", testOutput);
			for(u32 valueIndex = 0; valueIndex < kMaxPushConstantValueCount; valueIndex++)
				expectedValues[valueIndex] = (float)(valueCount * 10 + valueIndex + 1);

			// Repeat with fresh graphs and writes before a pipeline is selected. A graph records its passes in the order they were added,
			// into one command buffer, so the payload one pass leaves is the payload the next pass starts with.
			for(u32 phase = 0; phase < 3; phase++)
			{
				Array<float, 4> firstValues;
				std::copy(expectedValues, expectedValues + 4, firstValues.begin());

				if(phase != 0)
					expectedValues[valueCount - 1] += 100.0f;

				const float updatedLastValue = expectedValues[valueCount - 1];
				const float computeLastValue = expectedValues[valueCount - 1] + 1000.0f;

				FrameGraph graph(gpuContext, "Push-constant values");
				graph.ImportTexture(testTexture);
				graph.ImportBuffer(testOutput);
				graph.ImportBuffer(graphicsReadback);
				graph.ImportBuffer(computeReadback);

				// Every dispatch writes the output, so each one is a pass of its own, with a barrier before it. Passes that bind nothing
				// dispatch with the pipeline and set the pass before them bound.
				const auto fnAddDispatchPass = [&graph, &parameters](const char* name, Function<void(GpuCommandBuffer&)> record)
				{
					FrameGraphComputePass pass;
					pass.AddParameterSet(parameters);
					graph.AddComputePass(name, std::move(pass), [record = std::move(record)](FrameGraphPassContext& passContext)
					{
						GpuCommandBuffer& commands = passContext.GetCommandBuffer();
						record(commands);
						commands.DispatchCompute(1);
					});
				};

				fnAddDispatchPass("Compute values", [computePipeline, parameters, firstValues, valueCount](GpuCommandBuffer& commands)
				{
					commands.SetPushConstants(0, valueCount * sizeof(float), firstValues.data());
					commands.SetGpuComputePipelineState(computePipeline);
					commands.SetGpuParameterSet(parameters);
				});

				if(phase != 0)
				{
					fnAddDispatchPass("Compute partial values", [updatedLastValue, valueCount](GpuCommandBuffer& commands)
					{
						commands.SetPushConstants((valueCount - 1) * sizeof(float), sizeof(float), &updatedLastValue);
					});
				}

				if(phase == 2)
				{
					fnAddDispatchPass("Compute with a smaller layout", [&testComputePipelines, parameters](GpuCommandBuffer& commands)
					{
						commands.SetGpuComputePipelineState(testComputePipelines[0]);
						commands.SetGpuParameterSet(parameters);
					});

					fnAddDispatchPass("Compute values again", [computePipeline, parameters](GpuCommandBuffer& commands)
					{
						commands.SetGpuComputePipelineState(computePipeline);
						commands.SetGpuParameterSet(parameters);
					});
				}

				FrameGraphRasterPass drawPass;
				drawPass.SetColorAttachment(0, testTexture, FrameGraphAttachmentLoad::Clear);
				graph.AddRasterPass("Draw values", std::move(drawPass), [graphicsPipeline, emptyVertexDescription](FrameGraphPassContext& passContext)
				{
					GpuCommandBuffer& commands = passContext.GetCommandBuffer();
					commands.SetViewport(Area2(0.0f, 0.0f, 1.0f, 1.0f));
					commands.SetGpuGraphicsPipelineState(graphicsPipeline);
					commands.SetVertexDescription(emptyVertexDescription);
					commands.SetDrawOperation(DOT_TRIANGLE_LIST);
					commands.Draw(0, 3, 1);
				});

				if(phase == 2)
				{
					// Loads the target rather than clearing it, so the graph keeps the pass before it. Every draw covers the whole target.
					FrameGraphRasterPass replayPass;
					replayPass.SetColorAttachment(0, testTexture, FrameGraphAttachmentLoad::Load);
					graph.AddRasterPass("Draw values again", std::move(replayPass), [&testGraphicsPipelines, graphicsPipeline](FrameGraphPassContext& passContext)
					{
						GpuCommandBuffer& commands = passContext.GetCommandBuffer();
						commands.SetViewport(Area2(0.0f, 0.0f, 1.0f, 1.0f));
						commands.SetGpuGraphicsPipelineState(testGraphicsPipelines[0]);
						commands.Draw(0, 3, 1);
						commands.SetGpuGraphicsPipelineState(graphicsPipeline);
						commands.Draw(0, 3, 1);
					});
				}

				FrameGraphCopyPass graphicsCopyPass;
				graphicsCopyPass.AddSource(testTexture);
				graphicsCopyPass.AddDestination(graphicsReadback);
				graph.AddCopyPass("Read graphics values", std::move(graphicsCopyPass), [testTexture, graphicsReadback](FrameGraphPassContext& passContext)
				{
					passContext.GetCommandBuffer().CopyTextureToBuffer(testTexture, graphicsReadback, 0, 0);
				});

				FrameGraphComputePass lastValuePass;
				lastValuePass.AddParameterSet(parameters);
				graph.AddComputePass("Compute last value", std::move(lastValuePass), [computePipeline, parameters, computeLastValue, valueCount](FrameGraphPassContext& passContext)
				{
					GpuCommandBuffer& commands = passContext.GetCommandBuffer();
					commands.SetGpuComputePipelineState(computePipeline);
					commands.SetGpuParameterSet(parameters);
					commands.SetPushConstants((valueCount - 1) * sizeof(float), sizeof(float), &computeLastValue);
					commands.DispatchCompute(1);
				});

				FrameGraphCopyPass computeCopyPass;
				computeCopyPass.AddSource(testOutput);
				computeCopyPass.AddDestination(computeReadback);
				graph.AddCopyPass("Read compute values", std::move(computeCopyPass), [testOutput, computeReadback](FrameGraphPassContext& passContext)
				{
					passContext.GetCommandBuffer().CopyBufferToBuffer(testOutput, computeReadback, 0, 0, sizeof(float) * 4);
				});

				graph.Compile();
				graph.Execute();
				gpuDevice->WaitUntilIdle();

				const GpuBufferMappedScope computeMapping = computeReadback->Map(GpuMapOption::Read);
				const GpuBufferMappedScope graphicsMapping = graphicsReadback->Map(GpuMapOption::Read);
				if(!computeMapping.IsValid() || !graphicsMapping.IsValid())
					B3D_LOG(Fatal, LogGeneric, "Push-constant test readback mapping failed.");

				for(u32 valueIndex = 0; valueIndex < kMaxPushConstantValueCount; valueIndex++)
				{
					const float expected = valueIndex < valueCount ? expectedValues[valueIndex] : 0.0f;
					const float expectedCompute = valueIndex == valueCount - 1 ? computeLastValue : expected;
					const float computeValue = static_cast<const float*>(computeMapping.GetMappedMemory())[valueIndex];
					const float graphicsValue = static_cast<const float*>(graphicsMapping.GetMappedMemory())[valueIndex];

					if(computeValue != expectedCompute || graphicsValue != expected * 3.0f)
						B3D_LOG(Fatal, LogGeneric, "Push-constant readback failed: {0} bytes, phase {1}, component {2}: compute {3}, graphics {4}, expected {5}/{6}.", valueCount * sizeof(float), phase, valueIndex, computeValue, graphicsValue, expectedCompute, expected * 3.0f);
				}
			}

			B3D_LOG(Info, LogGeneric, "Push-constant readback passed: {0} bytes, vertex/fragment/compute, full/partial/replay.", valueCount * sizeof(float));
		}

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

		B3D_ASSERT(gpuDevice->GetCapabilities().MaximumPushConstantSize >= kMaxPushConstantSizeInBytes);
		B3D_ASSERT(gGraphicsPipelineState->GetParameterLayout()->GetPushConstantBufferSize() == kMaxPushConstantSizeInBytes);
		B3D_ASSERT(gComputePipelineState->GetParameterLayout()->GetPushConstantBufferSize() == kMaxPushConstantSizeInBytes);

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

		// Off-screen color and depth, created by each frame's graph
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

		// The window and the compute output are resources the graph did not create, so they are imported
		FrameGraph frameGraph(gpuContext, "AdvancedLowLevelRendering");
		frameGraph.ImportRenderTarget(gRenderWindow);
		frameGraph.ImportBuffer(gComputeOutput);

		const TShared<Texture> colorTexture = frameGraph.CreateTexture("Cube color", gColorInformation);
		const TShared<Texture> depthTexture = frameGraph.CreateTexture("Cube depth", gDepthInformation);

		// Exercise partial compute push-constant writes and publish their result for the graphics pass.
		FrameGraphComputePass computePass;
		computePass.AddParameterSet(gComputeParameters);
		frameGraph.AddComputePass("Compute tint", std::move(computePass), [](FrameGraphPassContext& passContext)
		{
			GpuCommandBuffer& commandBuffer = passContext.GetCommandBuffer();

			const float computeConstants[4] = { 0.8f, 0.9f, 1.0f, 1.0f };
			commandBuffer.SetGpuComputePipelineState(gComputePipelineState);
			commandBuffer.SetGpuParameterSet(gComputeParameters);
			commandBuffer.SetPushConstants(0, sizeof(float) * 2, computeConstants);
			commandBuffer.SetPushConstants(sizeof(float) * 2, sizeof(float) * 2, computeConstants + 2);
			commandBuffer.DispatchCompute(1);
		});

		// Draw two cubes with distinct graphics push constants; the right cube consumes the compute result.
		FrameGraphRasterPass cubePass;
		cubePass.SetColorAttachment(0, colorTexture, FrameGraphAttachmentLoad::Clear);
		cubePass.SetDepthStencilAttachment(depthTexture, FrameGraphDepthStencilMode::Write, FrameGraphAttachmentLoad::Clear);
		cubePass.AddParameterSet(gGraphicsParameters);
		frameGraph.AddRasterPass("Cubes", std::move(cubePass), [](FrameGraphPassContext& passContext)
		{
			GpuCommandBuffer& commandBuffer = passContext.GetCommandBuffer();
			commandBuffer.SetViewport(Area2(0.0f, 0.0f, 1.0f, 1.0f));
			commandBuffer.SetGpuGraphicsPipelineState(gGraphicsPipelineState);
			commandBuffer.SetVertexBuffers(0, &gVertexBuffer, 1);
			commandBuffer.SetIndexBuffer(gIndexBuffer);
			commandBuffer.SetVertexDescription(gVertexDescription);
			commandBuffer.SetDrawOperation(DOT_TRIANGLE_LIST);
			commandBuffer.SetGpuParameterSet(gGraphicsParameters);

			const float leftConstants[4] = { -0.45f, 1.0f, 0.2f, 0.2f };
			commandBuffer.SetPushConstants(0, sizeof(leftConstants), leftConstants);
			commandBuffer.DrawIndexed(0, kIndexCount, 0, kVertexCount, 1, 0);

			const float rightOffset = 0.45f;
			const float rightTint[3] = { 0.2f, 1.0f, 0.2f };
			commandBuffer.SetPushConstants(0, sizeof(rightOffset), &rightOffset);
			commandBuffer.SetPushConstants(sizeof(rightOffset), sizeof(rightTint), rightTint);
			commandBuffer.DrawIndexed(0, kIndexCount, 0, kVertexCount, 1, 0);
		});

		// Copy the cubes into the window. The copy covers the whole window, so its previous contents are not loaded.
		BlitMat* const blitMaterial = BlitMat::GetVariation(1, true, false);

		FrameGraphRasterPass blitPass;
		blitPass.SetRenderTarget(gRenderWindow, FrameGraphAttachmentLoad::DontCare);
		const TShared<GpuParameterSet> blitParameters = blitMaterial->Prepare(blitPass, colorTexture);

		frameGraph.AddRasterPass("Blit to window", std::move(blitPass), [blitMaterial, blitParameters](FrameGraphPassContext& passContext)
		{
			passContext.GetCommandBuffer().SetViewport(Area2(0.0f, 0.0f, 1.0f, 1.0f));
			blitMaterial->Execute(passContext, blitParameters, Area2(0.0f, 0.0f, (float)kWindowWidth, (float)kWindowHeight), false);
		});

		frameGraph.Compile();
		frameGraph.Execute();

		gpuDevice->PresentRenderWindow(gRenderWindow);
	}

	void Shutdown()
	{
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
