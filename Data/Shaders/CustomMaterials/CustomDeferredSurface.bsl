#include "$ENGINE$\BasePass.bslinc"
#include "$ENGINE$\GBufferOutput.bslinc"
#include "Data\Shaders\Noise.bslinc"

shader Surface
{
	// Default set of mixins required by the surface shader
    mixin BasePass;
    mixin GBufferOutput;
	
	// Helper mixins used to generate the noise
	mixin Noise;
	
    code
    {	
		// PBR texture inputs and their sampler states
		SamplerState gAlbedoSamp;
		SamplerState gNormalSamp;
		SamplerState gRoughnessSamp;
		SamplerState gMetalnessSamp;
		
		Texture2D gAlbedoTex;
		Texture2D gNormalTex;
		Texture2D gRoughnessTex;
		Texture2D gMetalnessTex;
		
		// Surface shader entry point
        void fsmain(
            in VStoFS input, 
            out float4 OutGBufferA : SV_Target0,
            out float4 OutGBufferB : SV_Target1,
            out float2 OutGBufferC : SV_Target2)
        {
			// First decode the local normal from the normal texture, then convert it to world space
			float3 normal = normalize(gNormalTex.Sample(gNormalSamp, input.uv0) * 2.0f - float3(1, 1, 1));
			float3 worldNormal = calcWorldNormal(input, normal);
		
			// Calculate a set of texture coordinates to use for noise. We could just use UV coordinates but
			// the effect doesn't look as good due to UV seams. Note that this portion of the shader is
			// dependant on exact mesh size and position, meaning it won't work properly if it is used with
			// a different mesh, or a mesh at a different position. This is not something you want to do usually
			// but instead just used to keep the example simple.
			float2 st;
			st.x = (input.worldPosition.z + 1.0f) / 2.65f;
			st.y = (input.worldPosition.y + 1.1f) / 2.1f;
			
			// Generate surface data. Everything is generated as in the default surface shader, except we apply a
			// red tint to places where noise function returns black.
			float4 noise = getAlbedoNoise(st);
			float4 albedo = gAlbedoTex.Sample(gAlbedoSamp, input.uv0);
		
			SurfaceData surfaceData;
			surfaceData.albedo = albedo * lerp(float4(1, 0, 0, 1), float4(1, 1, 1, 1), noise);
			surfaceData.worldNormal.xyz = worldNormal;
			surfaceData.roughness = gRoughnessTex.Sample(gRoughnessSamp, input.uv0).x;
			surfaceData.metalness = gMetalnessTex.Sample(gMetalnessSamp, input.uv0).x;
			
			// Write the surface data to the GBuffer
			encodeGBuffer(surfaceData, OutGBufferA, OutGBufferB, OutGBufferC);
        }   
    };
};