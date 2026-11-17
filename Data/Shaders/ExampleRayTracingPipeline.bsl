//************************************* B3D Framework - Copyright 2026 Marko Pintera *************************************//
//*********** Licensed under the MIT license. See LICENSE.md for full terms. This notice is not to be removed. ***********//
#include "$ENGINE$\RayTracingGeometryAccess.bslinc"

// Traces the scene of the RayTracing example through a ray tracing pipeline. ExampleRayTracingInline.bsl traces it with inline ray
// tracing.
// The code up to the entry points is the same in ExampleRayTracingInline.bsl and ExampleRayTracingPipeline.bsl, so both modes shade the
// same way. Keep them in sync.
shader ExampleRayTracingPipeline
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

		// TODO(metal) - Ray tracing pipelines come with phase 5.4
	#if !defined(METAL)
		// Ray types, which pick the miss record and, added to the hit group offset of the instance, the hit group record
		static const uint RAY_PRIMARY = 0;
		static const uint RAY_SHADOW = 1;

		struct PrimaryPayload
		{
			float3 Albedo;
			float T;
			float3 Normal;
			uint IsHit;
		};

		struct ShadowPayload
		{
			uint IsVisible;
		};

		// Traces the primary ray of a pixel, then a shadow ray from what it hit
		[shader("raygeneration")]
		void TraceScene()
		{
			uint2 pixel = DispatchRaysIndex().xy;
			RayDesc ray = GetCameraRay(pixel);

			PrimaryPayload payload;
			payload.Albedo = float3(0.0f, 0.0f, 0.0f);
			payload.T = 0.0f;
			payload.Normal = float3(0.0f, 0.0f, 0.0f);
			payload.IsHit = 0;

			TraceRay(gScene, RAY_FLAG_NONE, 0xFF, RAY_PRIMARY, 0, RAY_PRIMARY, ray, payload);

			float3 color = GetSkyColor(ray.Direction);
			if(payload.IsHit != 0)
			{
				Surface surface;
				surface.Albedo = payload.Albedo;
				surface.Normal = payload.Normal;

				float3 position = ray.Origin + ray.Direction * payload.T;

				// The miss program marks the light as visible. Shadow rays need no closest hit, and end at the first hit.
				ShadowPayload shadowPayload;
				shadowPayload.IsVisible = 0;
				TraceRay(gScene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, 0xFF, RAY_SHADOW, 0, RAY_SHADOW,
					GetShadowRay(position, surface.Normal), shadowPayload);

				color = Light(surface, shadowPayload.IsVisible != 0);
			}

			gOutput[uint2(pixel.x + gViewOffset, pixel.y)] = EncodeOutput(color);
		}

		[shader("miss")]
		void MissPrimary(inout PrimaryPayload payload)
		{
		}

		[shader("miss")]
		void MissShadow(inout ShadowPayload payload)
		{
			payload.IsVisible = 1;
		}

		// Closest hit of primary rays on every instance
		[shader("closesthit")]
		void ShadeHit(inout PrimaryPayload payload, in BuiltInTriangleIntersectionAttributes attributes)
		{
			float3 position = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
			Surface surface = ReadSurface(InstanceID(), GeometryIndex(), PrimitiveIndex(), attributes.barycentrics, ObjectToWorld3x4(), position,
				WorldRayDirection());

			payload.Albedo = surface.Albedo;
			payload.T = RayTCurrent();
			payload.Normal = surface.Normal;
			payload.IsHit = 1;
		}

		// Any hit of primary rays on cutout instances
		[shader("anyhit")]
		void AlphaTestPrimary(inout PrimaryPayload payload, in BuiltInTriangleIntersectionAttributes attributes)
		{
			if(!PassesAlphaTest(InstanceID(), GeometryIndex(), PrimitiveIndex(), attributes.barycentrics))
				IgnoreHit();
		}

		// Any hit of shadow rays on cutout instances
		[shader("anyhit")]
		void AlphaTestShadow(inout ShadowPayload payload, in BuiltInTriangleIntersectionAttributes attributes)
		{
			if(!PassesAlphaTest(InstanceID(), GeometryIndex(), PrimitiveIndex(), attributes.barycentrics))
				IgnoreHit();
		}
	#endif
	};
};
