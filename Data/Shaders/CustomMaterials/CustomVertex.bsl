#include "$ENGINE$\PerCameraData.bslinc"
#include "$ENGINE$\PerObjectData.bslinc"
#include "$ENGINE$\PerFrameData.bslinc"
#include "$ENGINE$\VertexInput.bslinc"
#include "$ENGINE$\GBufferOutput.bslinc"

shader VertexTransform
{
	// Default set of mixins required by the vertex shader
    mixin PerCameraData;
    mixin PerObjectData;
    mixin VertexInput;
	mixin GBufferOutput;
	
	// Additional mixin that provides values that are constant over an entire frame (in this case, gTime)
	mixin PerFrameData;
	
    code
    {
		// Entry point to the vertex shader
        VStoFS vsmain(VertexInput input)
        {
            VStoFS output;
			
			// Generate a default set of intermediate values and world position. Internally this handles
			// skinned & morph animation as well.
            VertexIntermediate intermediate = getVertexIntermediate(input);
            float4 worldPosition = getVertexWorldPosition(input, intermediate);
			
			// Apply the wobble by modifying the world position
			worldPosition.y += sin(gTime + worldPosition.z) * 0.1f;
			
			// Finalize the output structure by writing the world position, clip space position, and remaining values
			// (this is the same as the default vertex shader)
            output.worldPosition = worldPosition.xyz;
            output.position = mul(gMatViewProj, worldPosition);
            populateVertexOutput(input, intermediate, output);
			
            return output;
        }
		
		// When overriding the vertex shader you must also override the surface shader. The code below just
		// implements the default surface shader.
		SamplerState gAlbedoSamp;
		SamplerState gNormalSamp;
		SamplerState gRoughnessSamp;
		SamplerState gMetalnessSamp;
		
		Texture2D gAlbedoTex;
		Texture2D gNormalTex;
		Texture2D gRoughnessTex;
		Texture2D gMetalnessTex;
		
		void fsmain(
			in VStoFS input, 
			out float4 OutGBufferA : SV_Target0,
			out float4 OutGBufferB : SV_Target1,
			out float2 OutGBufferC : SV_Target2)
		{
			float3 normal = normalize(gNormalTex.Sample(gNormalSamp, input.uv0) * 2.0f - float3(1, 1, 1));
			float3 worldNormal = calcWorldNormal(input, normal);
		
			SurfaceData surfaceData;
			surfaceData.albedo = gAlbedoTex.Sample(gAlbedoSamp, input.uv0);
			surfaceData.worldNormal.xyz = worldNormal;
			surfaceData.roughness = gRoughnessTex.Sample(gRoughnessSamp, input.uv0).x;
			surfaceData.metalness = gMetalnessTex.Sample(gMetalnessSamp, input.uv0).x;
			
			encodeGBuffer(surfaceData, OutGBufferA, OutGBufferB, OutGBufferC);
		}			
    };
};