//************************************* B3D Framework - Copyright 2026 Marko Pintera *************************************//
//*********** Licensed under the MIT license. See LICENSE.md for full terms. This notice is not to be removed. ***********//
shader AdvancedPushConstantComputeTests
{
	variations
	{
		VALUE_COUNT = { 1, 2, 3, 4 };
	};

	code
	{
		[pushConstant]
		cbuffer PushConstants
		{
			float gValue0;
			#if VALUE_COUNT >= 2
				float gValue1;
			#endif
			#if VALUE_COUNT >= 3
				float gValue2;
			#endif
			#if VALUE_COUNT >= 4
				float gValue3;
			#endif
		};

		RWStructuredBuffer<float4> gOutput;

		[numthreads(1, 1, 1)]
		void csmain()
		{
			float4 values = float4(gValue0, 0.0f, 0.0f, 0.0f);
			#if VALUE_COUNT >= 2
				values.y = gValue1;
			#endif
			#if VALUE_COUNT >= 3
				values.z = gValue2;
			#endif
			#if VALUE_COUNT >= 4
				values.w = gValue3;
			#endif
			gOutput[0] = values;
		}
	};
};
