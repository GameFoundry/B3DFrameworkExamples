#pragma once

#include "BsPrerequisites.h"
#include "Reflection/BsRTTIType.h"
#include "Resources/BsResources.h"
#include "Resources/BsResourceManifest.h"
#include "Mesh/BsMesh.h"
#include "Importer/BsImporter.h"
#include "Importer/BsMeshImportOptions.h"
#include "Importer/BsTextureImportOptions.h"
#include "BsExampleConfig.h"
#include "Text/BsFontImportOptions.h"
#include "FileSystem/BsFileSystem.h"
#include "Input/BsVirtualInput.h"

namespace bs
{
	/** A list of mesh assets provided with the example projects. */
	enum class ExampleMesh
	{
		Pistol,
		Cerberus
	};

	/** A list of texture assets provided with the example projects. */
	enum class ExampleTexture
	{
		PistolAlbedo,
		PistolNormal,
		PistolRoughness,
		PistolMetalness,
		EnvironmentPaperMill,
		GUIBansheeIcon,
		GUIExampleButtonNormal,
		GUIExampleButtonHover,
		GUIExampleButtonActive,
		DroneAlbedo,
		DroneNormal,
		DroneRoughness,
		DroneMetalness,
		GridPattern,
		GridPattern2,
		EnvironmentDaytime,
		EnvironmentRathaus,
		CerberusAlbedo,
		CerberusNormal,
		CerberusRoughness,
		CerberusMetalness,
		ParticleSmoke,
		DecalAlbedo,
		DecalNormal
	};

	/** A list of shader assets provided with the example projects. */
	enum class ExampleShader
	{
		CustomVertex,
		CustomDeferredSurface,
		CustomDeferredLighting,
		CustomForward
	};

	/** A list of font assets provided with the example projects. */
	enum class ExampleFont
	{
		SegoeUILight,
		SegoeUISemiBold
	};

	/** A list of assets without a speccific type provided with the example projects. */
	enum class ExampleResource
	{
		VectorField
	};

	/** Various helper functionality used throught the examples. */
	class ExampleFramework
	{
	public:
		/** Loads a manifest of all resources that were previously saved using this class. */
		static void LoadResourceManifest()
		{
			const Path dataPath = EXAMPLE_DATA_PATH;
			const Path manifestPath = dataPath + "ResourceManifest.asset";

			if(FileSystem::Exists(manifestPath))
				manifest = ResourceManifest::Load(manifestPath, dataPath);
			else
				manifest = ResourceManifest::Create("ExampleAssets");

			GetResources().RegisterResourceManifest(manifest);
		}

		/** Saves the current resource manifest. */
		static void SaveResourceManifest()
		{
			const Path dataPath = EXAMPLE_DATA_PATH;
			const Path manifestPath = dataPath + "ResourceManifest.asset";

			if(manifest)
				ResourceManifest::Save(manifest, manifestPath, dataPath);
		}

		/** Registers a common set of keys/buttons that are used for controlling the examples. */
		static void SetupInputConfig()
		{
			// Register input configuration
			// bsf allows you to use VirtualInput system which will map input device buttons and axes to arbitrary names,
			// which allows you to change input buttons without affecting the code that uses it, since the code is only
			// aware of the virtual names.  If you want more direct input, see Input class.
			auto inputConfig = GetVirtualInput().GetConfiguration();

			// Camera controls for buttons (digital 0-1 input, e.g. keyboard or gamepad button)
			inputConfig->RegisterButton("Forward", BC_W);
			inputConfig->RegisterButton("Back", BC_S);
			inputConfig->RegisterButton("Left", BC_A);
			inputConfig->RegisterButton("Right", BC_D);
			inputConfig->RegisterButton("Forward", BC_UP);
			inputConfig->RegisterButton("Back", BC_DOWN);
			inputConfig->RegisterButton("Left", BC_LEFT);
			inputConfig->RegisterButton("Right", BC_RIGHT);
			inputConfig->RegisterButton("FastMove", BC_LSHIFT);
			inputConfig->RegisterButton("RotateObj", BC_MOUSE_LEFT);
			inputConfig->RegisterButton("RotateCam", BC_MOUSE_RIGHT);

			// Camera controls for axes (analog input, e.g. mouse or gamepad thumbstick)
			// These return values in [-1.0, 1.0] range.
			inputConfig->RegisterAxis("Horizontal", VIRTUAL_AXIS_DESC((u32)InputAxis::MouseX));
			inputConfig->RegisterAxis("Vertical", VIRTUAL_AXIS_DESC((u32)InputAxis::MouseY));
		}

		/**
		 * Loads one of the builtin mesh assets. If the asset doesn't exist, the mesh will be re-imported from the source
		 * file, and then saved so it can be loaded on the next call to this method.
		 *
		 * Use the 'scale' parameter to control the size of the mesh. Note this option is only relevant when a mesh is
		 * being imported (i.e. when the asset file is missing).
		 */
		static HMesh LoadMesh(ExampleMesh type, float scale = 1.0f)
		{
			// Map from the enum to the actual file path
			static Path assetPaths[] = {
				Path(EXAMPLE_DATA_PATH) + "Pistol/Pistol01.fbx",
				Path(EXAMPLE_DATA_PATH) + "Cerberus/Cerberus.FBX",
			};

			const Path& srcAssetPath = assetPaths[(u32)type];

			// Attempt to load the previously processed asset
			Path assetPath = srcAssetPath;
			assetPath.SetExtension(srcAssetPath.GetExtension() + ".asset");

			HMesh model = GetResources().Load<Mesh>(assetPath);
			if(model == nullptr) // Mesh file doesn't exist, import from the source file.
			{
				// When importing you may specify optional import options that control how is the asset imported.
				SPtr<ImportOptions> meshImportOptions = Importer::Instance().CreateImportOptions(srcAssetPath);

				// B3DRTTIIsOfType checks if the import options are of valid type, in case the provided path is pointing to a
				// non-mesh resource. This is similar to dynamic_cast but uses Banshee internal RTTI system for type checking.
				if(B3DRTTIIsOfType<MeshImportOptions>(meshImportOptions))
				{
					MeshImportOptions* importOptions = static_cast<MeshImportOptions*>(meshImportOptions.get());

					importOptions->ImportScale = scale;
				}

				model = GetImporter().Import<Mesh>(srcAssetPath, meshImportOptions);

				// Save for later use, so we don't have to import on the next run.
				GetResources().Save(model, assetPath, true);

				// Register with manifest, if one is present. Manifest allows the engine to find the resource even after
				// the application was restarted, which is important if resource was referenced in some serialized object.
				if(manifest)
					manifest->RegisterResource(model.GetId(), assetPath);
			}

			return model;
		}

		/**
		 * Loads one of the builtin texture assets. If the asset doesn't exist, the texture will be re-imported from the
		 * source file, and then saved so it can be loaded on the next call to this method.
		 *
		 * Textures not in sRGB space (e.g. normal maps) need to be specially marked by setting 'isSRGB' to false. Also
		 * allows for conversion of texture to cubemap by setting the 'isCubemap' parameter. If the data should be imported
		 * in a floating point format, specify 'isHDR' to true. Note these options are only relevant when a texture is
		 * being imported (i.e. when asset file is missing). If 'mips' is true, mip-map levels will be generated.
		 */
		static HTexture LoadTexture(ExampleTexture type, bool isSRGB = true, bool isCubemap = false, bool isHDR = false, bool mips = true)
		{
			// Map from the enum to the actual file path
			static Path assetPaths[] = {
				Path(EXAMPLE_DATA_PATH) + "Pistol/Pistol_DFS.png",
				Path(EXAMPLE_DATA_PATH) + "Pistol/Pistol_NM.png",
				Path(EXAMPLE_DATA_PATH) + "Pistol/Pistol_RGH.png",
				Path(EXAMPLE_DATA_PATH) + "Pistol/Pistol_MTL.png",
				Path(EXAMPLE_DATA_PATH) + "Environments/PaperMill_E_3k.hdr",
				Path(EXAMPLE_DATA_PATH) + "GUI/BansheeIcon.png",
				Path(EXAMPLE_DATA_PATH) + "GUI/ExampleButtonNormal.png",
				Path(EXAMPLE_DATA_PATH) + "GUI/ExampleButtonHover.png",
				Path(EXAMPLE_DATA_PATH) + "GUI/ExampleButtonActive.png",
				Path(EXAMPLE_DATA_PATH) + "MechDrone/Drone_diff.jpg",
				Path(EXAMPLE_DATA_PATH) + "MechDrone/Drone_normal.jpg",
				Path(EXAMPLE_DATA_PATH) + "MechDrone/Drone_rough.jpg",
				Path(EXAMPLE_DATA_PATH) + "MechDrone/Drone_metal.jpg",
				Path(EXAMPLE_DATA_PATH) + "Grid/GridPattern.png",
				Path(EXAMPLE_DATA_PATH) + "Grid/GridPattern2.png",
				Path(EXAMPLE_DATA_PATH) + "Environments/daytime.hdr",
				Path(EXAMPLE_DATA_PATH) + "Environments/rathaus.hdr",
				Path(EXAMPLE_DATA_PATH) + "Cerberus/Cerberus_A.tga",
				Path(EXAMPLE_DATA_PATH) + "Cerberus/Cerberus_N.tga",
				Path(EXAMPLE_DATA_PATH) + "Cerberus/Cerberus_R.tga",
				Path(EXAMPLE_DATA_PATH) + "Cerberus/Cerberus_M.tga",
				Path(EXAMPLE_DATA_PATH) + "Particles/Smoke.png",
				Path(EXAMPLE_DATA_PATH) + "Decal/DecalAlbedo.png",
				Path(EXAMPLE_DATA_PATH) + "Decal/DecalNormal.png",
			};

			const Path& srcAssetPath = assetPaths[(u32)type];

			// Attempt to load the previously processed asset
			Path assetPath = srcAssetPath;
			assetPath.SetExtension(srcAssetPath.GetExtension() + ".asset");

			HTexture texture = GetResources().Load<Texture>(assetPath);
			if(texture == nullptr) // Texture file doesn't exist, import from the source file.
			{
				// When importing you may specify optional import options that control how is the asset imported.
				SPtr<ImportOptions> textureImportOptions = Importer::Instance().CreateImportOptions(srcAssetPath);

				// B3DRTTIIsOfType checks if the import options are of valid type, in case the provided path is pointing to a
				// non-texture resource. This is similar to dynamic_cast but uses Banshee internal RTTI system for type checking.
				if(B3DRTTIIsOfType<TextureImportOptions>(textureImportOptions))
				{
					TextureImportOptions* importOptions = static_cast<TextureImportOptions*>(textureImportOptions.get());

					// We want maximum number of mipmaps to be generated
					importOptions->GenerateMips = mips;

					// If the texture is in sRGB space the system needs to know about it
					importOptions->SRgb = isSRGB;

					// Ensures we can save the texture contents
					importOptions->CpuCached = true;

					// Import as cubemap if needed
					importOptions->Cubemap = isCubemap;

					// If importing as cubemap, assume source is a panorama
					importOptions->CubemapSourceType = CubemapSourceType::Cylindrical;

					// Importing using a HDR format if requested
					if(isHDR)
						importOptions->Format = PF_RG11B10F;
				}

				// Import texture with specified import options
				texture = GetImporter().Import<Texture>(srcAssetPath, textureImportOptions);

				// Save for later use, so we don't have to import on the next run.
				GetResources().Save(texture, assetPath, true);

				// Register with manifest, if one is present. Manifest allows the engine to find the resource even after
				// the application was restarted, which is important if resource was referenced in some serialized object.
				if(manifest)
					manifest->RegisterResource(texture.GetId(), assetPath);
			}

			return texture;
		}

		/**
		 * Loads one of the builtin shader assets. If the asset doesn't exist, the shader will be re-imported from the
		 * source file, and then saved so it can be loaded on the next call to this method.
		 */
		static HShader LoadShader(ExampleShader type)
		{
			// Map from the enum to the actual file path
			static Path assetPaths[] = {
				Path(EXAMPLE_DATA_PATH) + "Shaders/CustomVertex.bsl",
				Path(EXAMPLE_DATA_PATH) + "Shaders/CustomDeferredSurface.bsl",
				Path(EXAMPLE_DATA_PATH) + "Shaders/CustomDeferredLighting.bsl",
				Path(EXAMPLE_DATA_PATH) + "Shaders/CustomForward.bsl",
			};

			const Path& srcAssetPath = assetPaths[(u32)type];

			// Attempt to load the previously processed asset
			Path assetPath = srcAssetPath;
			assetPath.SetExtension(srcAssetPath.GetExtension() + ".asset");

			HShader shader = GetResources().Load<Shader>(assetPath);
			if(shader == nullptr) // Shader file doesn't exist, import from the source file.
			{
				shader = GetImporter().Import<Shader>(srcAssetPath);

				// Save for later use, so we don't have to import on the next run.
				GetResources().Save(shader, assetPath, true);

				// Register with manifest, if one is present. Manifest allows the engine to find the resource even after
				// the application was restarted, which is important if resource was referenced in some serialized object.
				if(manifest)
					manifest->RegisterResource(shader.GetId(), assetPath);
			}

			return shader;
		}

		/**
		 * Loads one of the builtin font assets. If the asset doesn't exist, the font will be re-imported from the
		 * source file, and then saved so it can be loaded on the next call to this method.
		 *
		 * Use the 'fontSizes' parameter to determine which sizes of this font should be imported. Note this option is only
		 * relevant when a font is being imported (i.e. when the asset file is missing).
		 */
		static HFont LoadFont(ExampleFont type, Vector<float> fontSizes)
		{
			// Map from the enum to the actual file path
			static Path assetPaths[] = {
				Path(EXAMPLE_DATA_PATH) + "GUI/segoeuil.ttf",
				Path(EXAMPLE_DATA_PATH) + "GUI/seguisb.ttf",
			};

			const Path& srcAssetPath = assetPaths[(u32)type];

			// Attempt to load the previously processed asset
			Path assetPath = srcAssetPath;
			assetPath.SetExtension(srcAssetPath.GetExtension() + ".asset");

			HFont font = GetResources().Load<Font>(assetPath);
			if(font == nullptr) // Font file doesn't exist, import from the source file.
			{
				// When importing you may specify optional import options that control how is the asset imported.
				SPtr<FontImportOptions> fontImportOptions = FontImportOptions::Create();
				fontImportOptions->FontSizes = fontSizes;

				font = GetImporter().Import<Font>(srcAssetPath, fontImportOptions);

				// Save for later use, so we don't have to import on the next run.
				GetResources().Save(font, assetPath, true);

				// Register with manifest, if one is present. Manifest allows the engine to find the resource even after
				// the application was restarted, which is important if resource was referenced in some serialized object.
				if(manifest)
				{
					manifest->RegisterResource(font.GetId(), assetPath);

					// Font has child resources, which also need to be registered
					for(auto& size : fontSizes)
					{
						SPtr<const FontBitmapInformation> fontData = font->GetBitmap(size);

						Path texPageOutputPath = Path(EXAMPLE_DATA_PATH) + "GUI/";

						u32 pageIdx = 0;
						for(const auto& page : fontData->TexturePages)
						{
							String fontName = srcAssetPath.GetFilename(false);
							texPageOutputPath.SetFilename(fontName + "_" + ToString(size) + "_texpage_" + ToString(pageIdx) + ".asset");

							GetResources().Save(page.Texture, texPageOutputPath, true);
							manifest->RegisterResource(page.Texture.GetId(), texPageOutputPath);

							pageIdx++;
						}
					}
				}
			}

			return font;
		}

		/**
		 * Loads one of the builtin non-specific assets. If the asset doesn't exist, it will be re-imported from the
		 * source file, and then saved so it can be loaded on the next call to this method.
		 */
		template <class T>
		static ResourceHandle<T> LoadResource(ExampleResource type)
		{
			// Map from the enum to the actual file path
			static Path assetPaths[] = {
				Path(EXAMPLE_DATA_PATH) + "Particles/VectorField.fga",
			};

			const Path& srcAssetPath = assetPaths[(u32)type];

			// Attempt to load the previously processed asset
			Path assetPath = srcAssetPath;
			assetPath.SetExtension(srcAssetPath.GetExtension() + ".asset");

			ResourceHandle<T> resource = GetResources().Load<T>(assetPath);
			if(resource == nullptr) // Shader file doesn't exist, import from the source file.
			{
				resource = GetImporter().Import<T>(srcAssetPath);

				// Save for later use, so we don't have to import on the next run.
				GetResources().Save(resource, assetPath, true);

				// Register with manifest, if one is present. Manifest allows the engine to find the resource even after
				// the application was restarted, which is important if resource was referenced in some serialized object.
				if(manifest)
					manifest->RegisterResource(resource.GetId(), assetPath);
			}

			return resource;
		}

	private:
		static SPtr<ResourceManifest> manifest;
	};

	SPtr<ResourceManifest> ExampleFramework::manifest;
} // namespace bs
