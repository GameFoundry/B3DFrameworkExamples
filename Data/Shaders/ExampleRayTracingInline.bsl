//************************************* B3D Framework - Copyright 2026 Marko Pintera *************************************//
//*********** Licensed under the MIT license. See LICENSE.md for full terms. This notice is not to be removed. ***********//
#include "$ENGINE$\RayTracingGeometryAccess.bslinc"

// Traces the scene of the RayTracing example with inline ray tracing, from a compute program. ExampleRayTracingPipeline.bsl traces it
// through a ray tracing pipeline. A shader's variations share their entry points, so the two modes are two shaders.
// The code up to the entry points is the same in ExampleRayTracingInline.bsl and ExampleRayTracingPipeline.bsl, so both modes shade the
// same way. Keep them in sync.
shader ExampleRayTracingInline
{
	mixin RayTracingGeometryAccess;

	code
	{
		// Data of one instance, indexed by its instance id. Matches ObjectData in Main.cpp.
		struct ObjectData
		{
			uint FirstLayout; // Index of the layout of the instance's first geometry in gLayouts
			uint Material; // One of the MATERIAL_* values
			uint2 Padding;
			float4 Color; // Linear albedo of the checker, solid and cutout materials
		};

		static const uint MATERIAL_CHECKER = 0;
		static const uint MATERIAL_SOLID = 1;
		static const uint MATERIAL_CUTOUT = 2;
		static const uint MATERIAL_TEXTURED = 3;

		cbuffer Params
		{
			float4 gCameraPosition;
			float4 gCameraRight; // Scaled by the tangent of half the horizontal field of view
			float4 gCameraUp; // Scaled by the tangent of half the vertical field of view
			float4 gCameraForward;
			float4 gLightDirection; // Direction the light travels in
			uint2 gViewSize; // Size of the view in pixels
			uint gViewOffset; // Horizontal offset of the view in gOutput
			uint gParamsPadding;
		};

		RaytracingAccelerationStructure gScene;
		StructuredBuffer<ObjectData> gObjects;
		StructuredBuffer<RayTracingGeometryLayout> gLayouts;
		Texture2D gTextureAlbedo;
		SamplerState gTextureSampler;
		[layout(rgba8)]
		RWTexture2D<float4> gOutput;

		/** Returns the ray of the camera through the center of @p pixel of the view. */
		RayDesc GetCameraRay(uint2 pixel)
		{
			float2 ndc = (float2(pixel) + 0.5f) / float2(gViewSize) * 2.0f - 1.0f;

			RayDesc ray;
			ray.Origin = gCameraPosition.xyz;
			ray.TMin = 0.0f;
			ray.Direction = normalize(gCameraForward.xyz + ndc.x * gCameraRight.xyz - ndc.y * gCameraUp.xyz);
			ray.TMax = 1000.0f;
			return ray;
		}

		/** Returns the ray from @p position, on a surface with @p normal, towards the light. */
		RayDesc GetShadowRay(float3 position, float3 normal)
		{
			RayDesc ray;
			ray.Origin = position + normal * 0.01f;
			ray.TMin = 0.0f;
			ray.Direction = -gLightDirection.xyz;
			ray.TMax = 1000.0f;
			return ray;
		}

		/** Returns false where a cutout instance has a hole: every other cell of a 4x4 grid over its texture coordinates. */
		bool PassesAlphaTest(uint instanceId, uint geometryIndex, uint primitiveIndex, float2 barycentrics)
		{
			ObjectData object = gObjects[instanceId];
			if(object.Material != MATERIAL_CUTOUT)
				return true;

			RayTracingGeometryLayout layout = gLayouts[object.FirstLayout + geometryIndex];
			float2 uv = InterpolateUv(layout, LoadTriangleIndices(layout, primitiveIndex), barycentrics);
			uint2 cell = uint2(floor(uv * 4.0f));
			return ((cell.x + cell.y) & 1) == 0;
		}

		struct Surface
		{
			float3 Albedo;
			float3 Normal; // In world space, facing the ray
		};

		/**
		 * Reads the surface a ray in @p rayDirection hit at @p position: the normal from the instance's geometry, and the albedo from its
		 * material.
		 */
		Surface ReadSurface(uint instanceId, uint geometryIndex, uint primitiveIndex, float2 barycentrics, float3x4 objectToWorld, float3 position,
			float3 rayDirection)
		{
			ObjectData object = gObjects[instanceId];
			RayTracingGeometryLayout layout = gLayouts[object.FirstLayout + geometryIndex];
			uint3 indices = LoadTriangleIndices(layout, primitiveIndex);

			float3 objectNormal = InterpolateNormal(layout, indices, barycentrics);
			float3 normal = normalize(mul(objectToWorld, float4(objectNormal, 0.0f)));
			if(dot(normal, rayDirection) > 0.0f)
				normal = -normal;

			Surface surface;
			surface.Normal = normal;
			surface.Albedo = object.Color.rgb;

			if(object.Material == MATERIAL_CHECKER)
			{
				int2 cell = int2(floor(position.xz));
				surface.Albedo *= ((cell.x + cell.y) & 1) == 0 ? 1.0f : 0.6f;
			}
			else if(object.Material == MATERIAL_TEXTURED)
			{
				float2 uv = InterpolateUv(layout, indices, barycentrics);
				surface.Albedo = gTextureAlbedo.SampleLevel(gTextureSampler, uv, 0).rgb;
			}

			return surface;
		}

		/** Returns the color of the sky seen in @p direction. Linear. */
		float3 GetSkyColor(float3 direction)
		{
			return lerp(float3(0.6f, 0.7f, 0.8f), float3(0.05f, 0.17f, 0.6f), saturate(direction.y));
		}

		/** Returns the color of @p surface, lit by the light if @p isLit is true, and by a constant ambient term. Linear. */
		float3 Light(Surface surface, bool isLit)
		{
			float diffuse = isLit ? saturate(dot(surface.Normal, -gLightDirection.xyz)) : 0.0f;
			return surface.Albedo * (diffuse * 0.9f + 0.1f);
		}

		/** Encodes linear @p color as sRGB, for gOutput. An approximation of the sRGB curve. */
		float4 EncodeOutput(float3 color)
		{
			return float4(pow(saturate(color), 1.0f / 2.2f), 1.0f);
		}

		/** Ignores the hits that fall into the holes of cutout instances. */
		struct AlphaTest
		{
			uint Unused;

			bool AnyHit(RayCandidate candidate)
			{
				return PassesAlphaTest(candidate.InstanceID, candidate.GeometryIndex, candidate.PrimitiveIndex, candidate.Barycentrics);
			}
		};

		// Traces the primary ray of a pixel, then a shadow ray from what it hit
		[numthreads(8, 8, 1)]
		void csmain(uint3 threadId : SV_DispatchThreadID)
		{
			if(any(threadId.xy >= gViewSize))
				return;

			RayDesc ray = GetCameraRay(threadId.xy);

			AlphaTest alphaTest;
			alphaTest.Unused = 0;

			float3 color = GetSkyColor(ray.Direction);
			RayHit hit = RayTraceClosest(gScene, RAY_FLAG_NONE, 0xFF, ray, alphaTest);
			if(hit.IsHit)
			{
				float3 position = ray.Origin + ray.Direction * hit.T;
				Surface surface = ReadSurface(hit.InstanceID, hit.GeometryIndex, hit.PrimitiveIndex, hit.Barycentrics, hit.ObjectToWorld, position,
					ray.Direction);

				RayHit shadowHit = RayTraceAny(gScene, RAY_FLAG_NONE, 0xFF, GetShadowRay(position, surface.Normal), alphaTest);
				color = Light(surface, !shadowHit.IsHit);
			}

			gOutput[uint2(threadId.x + gViewOffset, threadId.y)] = EncodeOutput(color);
		}
	};
};
