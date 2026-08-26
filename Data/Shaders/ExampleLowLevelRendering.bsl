shader ExampleLowLevelRendering
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
		cbuffer Params
		{
			float4x4 gMatWVP;
			float4 gTint;
		};

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
			output.uv0 = input.uv0;

			return output;
		}

		float4 fsmain(VStoFS input) : SV_Target0
		{
			float4 color = gMainTexture.Sample(gMainTexSamp, input.uv0);
			return color * gTint;
		}
	};
};
