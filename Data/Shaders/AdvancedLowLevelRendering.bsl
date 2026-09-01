shader AdvancedLowLevelRendering
{
	depth
	{
		read = false;
		write = false;
	};

	blend
	{
		target
		{
			enabled = true;
			color = { srcA, srcIA, add };
			writemask = RGB;
		};
	};

	code
	{
		[pushConstant]
		cbuffer PushConstants
		{
			float4 gDrawConstants;
		};

		cbuffer Params
		{
			float4x4 gMatWVP;
		};

		StructuredBuffer<float4> gComputeOutput;
		SamplerState gMainTexSamp;
		Texture2D gMainTexture;

		struct VertexInput
		{
			float3 position : POSITION;
			float2 uv0 : TEXCOORD0;
		};

		struct VStoFS
		{
			float4 position : SV_Position;
			float2 uv0 : TEXCOORD0;
		};

		VStoFS vsmain(VertexInput input)
		{
			VStoFS output;
			output.position = mul(gMatWVP, float4(input.position, 1.0f));
			output.position.x += gDrawConstants.x * output.position.w;
			output.uv0 = input.uv0;
			return output;
		}

		float4 fsmain(VStoFS input) : SV_Target0
		{
			float3 tint = gDrawConstants.x < 0.0f ? gDrawConstants.yzw : gComputeOutput[0].rgb;
			return gMainTexture.Sample(gMainTexSamp, input.uv0) * float4(tint, 1.0f);
		}
	};
};
