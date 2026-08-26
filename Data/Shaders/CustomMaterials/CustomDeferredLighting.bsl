// Implement the sub-shader, letting the renderer know we want to override any shaders used by the
// 'DeferredDirectLighting' extension point
subshader DeferredDirectLighting
{
	// Override the StandardBRDF mixin
    mixin StandardBRDF
    {
		code
		{
			// And its evaluateStandardBRDF method, by replacing its implementation with a basic Lambert BRDF
			float3 evaluateStandardBRDF(float3 V, float3 L, float specLobeEnergy, SurfaceData surfaceData)
			{
				return surfaceData.albedo.rgb / 3.14f;
			}
		};
    };
};