//************************************* B3D Framework - Copyright 2026 Marko Pintera *************************************//
//*********** Licensed under the MIT license. See LICENSE.md for full terms. This notice is not to be removed. ***********//
shader AdvancedPushConstantTests
{
	variations
	{
		VALUE_COUNT = { 1, 2, 3, 4 };
	};

	depth
	{
		read = false;
		write = false;
	};

	output
	{
		target
		{
			index = 0;
			format = RGBA32F;
		};
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

		float4 ReadConstants()
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
			return values;
		}

		struct VertexOutput
		{
			float4 Position : SV_Position;
			float4 Values : TEXCOORD0;
		};

		VertexOutput vsmain(uint vertexId : SV_VertexID)
		{
			VertexOutput output;
			output.Position = float4(vertexId == 1 ? 3.0f : -1.0f, vertexId == 2 ? 3.0f : -1.0f, 0.0f, 1.0f);
			output.Values = ReadConstants();
			return output;
		}

		float4 fsmain(VertexOutput input) : SV_Target0
		{
			return input.Values + 2.0f * ReadConstants();
		}
	};
};
