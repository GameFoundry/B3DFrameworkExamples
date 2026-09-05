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

		[numthreads(32, 1, 1)]
		void csmain(uint3 dispatchId : SV_DispatchThreadID)
		{
			float activeLaneCount = (float)WaveActiveCountBits(true);
			float4 waveSum = WaveActiveSum(gColorMultiplier);
			if(WaveGetLaneIndex() == 0)
				gComputeOutput[0] = waveSum / activeLaneCount;
		}
	};
};
