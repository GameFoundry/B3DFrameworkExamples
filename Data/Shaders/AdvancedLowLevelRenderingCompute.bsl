shader AdvancedLowLevelRenderingCompute
{
	code
	{
		[pushConstant]
		cbuffer PushConstants
		{
			float4 gColorMultiplier;
		};

		RWStructuredBuffer<float4> gComputeOutput;

		[numthreads(1, 1, 1)]
		void csmain(uint3 dispatchId : SV_DispatchThreadID)
		{
			gComputeOutput[dispatchId.x] = gColorMultiplier;
		}
	};
};
