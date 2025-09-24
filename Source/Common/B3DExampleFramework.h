#pragma once

#include "BsPrerequisites.h"
#include "Reflection/BsRTTIType.h"
#include "Resources/BsResources.h"
#include "Mesh/BsMesh.h"
#include "Importer/BsImporter.h"
#include "Importer/BsMeshImportOptions.h"
#include "Importer/BsTextureImportOptions.h"
#include "BsExampleConfig.h"
#include "Text/BsFontImportOptions.h"
#include "FileSystem/BsFileSystem.h"
#include "Input/BsVirtualInput.h"

namespace b3d
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
		/** Contains a location in which the asset is located in (folder) and the name of the asset. */
		struct AssetFolderAndName
		{
			AssetFolderAndName(const Path& folder, const String& name)
				: Folder(folder), Name(name)
			{ }

			Path Folder;
			String Name;
		};

		/** Virtual path prefix that may be used for accessing all resources managed by this class. */
		static constexpr const char* kVirtualPathPrefix = "/B3D/ExampleData/";

		/** Loads all packages containing example data. */
		static void LoadPackages()
		{
			GetPackageManager().LoadPackages(EXAMPLE_DATA_PATH, true, kVirtualPathPrefix, true);
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
			inputConfig->RegisterButton("Forward", ButtonCode::W);
			inputConfig->RegisterButton("Back", ButtonCode::S);
			inputConfig->RegisterButton("Left", ButtonCode::A);
			inputConfig->RegisterButton("Right", ButtonCode::D);
			inputConfig->RegisterButton("Forward", ButtonCode::ArrowUp);
			inputConfig->RegisterButton("Back", ButtonCode::ArrowDown);
			inputConfig->RegisterButton("Left", ButtonCode::ArrowLeft);
			inputConfig->RegisterButton("Right", ButtonCode::ArrowRight);
			inputConfig->RegisterButton("FastMove", ButtonCode::LeftShift);
			inputConfig->RegisterButton("RotateObj", ButtonCode::MouseLeft);
			inputConfig->RegisterButton("RotateCam", ButtonCode::MouseRight);

			// Camera controls for axes (analog input, e.g. mouse or gamepad thumbstick)
			// These return values in [-1.0, 1.0] range.
			inputConfig->RegisterAxis("Horizontal", VirtualAxisCreateInformation((u32)InputAxis::MouseX));
			inputConfig->RegisterAxis("Vertical", VirtualAxisCreateInformation((u32)InputAxis::MouseY));
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
			static AssetFolderAndName assetPaths[] = {
				AssetFolderAndName("Pistol/", "Pistol01.fbx"),
				AssetFolderAndName("Cerberus/", "Cerberus.FBX")
			};

			const AssetFolderAndName& assetFolderAndName = assetPaths[(u32)type];
			const Path& virtualAssetPath = Path::Combine(kVirtualPathPrefix, assetFolderAndName.Folder, assetFolderAndName.Name);

			// Attempt to load the previously processed asset
			if(GetResources().Exists(virtualAssetPath))
				return GetResources().Load<Mesh>(virtualAssetPath, ResourceLoadOptions(false));
			else // Mesh file doesn't exist, import from the source file.
			{
				// When importing you may specify optional import options that control how is the asset imported.
				const Path& sourceAssetPath = Path::Combine(EXAMPLE_DATA_PATH, assetFolderAndName.Folder, assetFolderAndName.Name);
				SPtr<ImportOptions> meshImportOptions = Importer::Instance().CreateImportOptions(sourceAssetPath);

				// B3DRTTIIsOfType checks if the import options are of valid type, in case the provided path is pointing to a
				// non-mesh resource. This is similar to dynamic_cast but uses Banshee internal RTTI system for type checking.
				if(B3DRTTIIsOfType<MeshImportOptions>(meshImportOptions))
				{
					MeshImportOptions* importOptions = static_cast<MeshImportOptions*>(meshImportOptions.get());

					importOptions->ImportScale = scale;
				}

				HMesh mesh = GetImporter().Import<Mesh>(sourceAssetPath, meshImportOptions);

				// Save for later use, so we don't have to import on the next run.
				ResourceSaveOptions resourceSaveOptions;
				resourceSaveOptions.VirtualPathPrefix = Path::Combine(kVirtualPathPrefix, assetFolderAndName.Folder);

				GetResources().SaveAsSinglePackage(mesh, Path::Combine(EXAMPLE_DATA_PATH, assetFolderAndName.Folder), assetFolderAndName.Name, resourceSaveOptions);
				return mesh;
			}
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
			static AssetFolderAndName assetPaths[] = {
				AssetFolderAndName("Pistol/", "Pistol_DFS.png"),
				AssetFolderAndName("Pistol/", "Pistol_NM.png"),
				AssetFolderAndName("Pistol/", "Pistol_RGH.png"),
				AssetFolderAndName("Pistol/", "Pistol_MTL.png"),
				AssetFolderAndName("Environments/", "PaperMill_E_3k.hdr"),
				AssetFolderAndName("GUI/", "BansheeIcon.png"),
				AssetFolderAndName("MechDrone/", "Drone_diff.jpg"),
				AssetFolderAndName("MechDrone/", "Drone_normal.jpg"),
				AssetFolderAndName("MechDrone/", "Drone_rough.jpg"),
				AssetFolderAndName("MechDrone/", "Drone_metal.jpg"),
				AssetFolderAndName("Grid/", "GridPattern.png"),
				AssetFolderAndName("Grid/", "GridPattern2.png"),
				AssetFolderAndName("Environments/", "daytime.hdr"),
				AssetFolderAndName("Environments/", "rathaus.hdr"),
				AssetFolderAndName("Cerberus/", "Cerberus_A.tga"),
				AssetFolderAndName("Cerberus/", "Cerberus_N.tga"),
				AssetFolderAndName("Cerberus/", "Cerberus_R.tga"),
				AssetFolderAndName("Cerberus/", "Cerberus_M.tga"),
				AssetFolderAndName("Particles/", "Smoke.png"),
				AssetFolderAndName("Decal/", "DecalAlbedo.png"),
				AssetFolderAndName("Decal/", "DecalNormal.png"),
			};

			const AssetFolderAndName& assetFolderAndName = assetPaths[(u32)type];
			const Path& virtualAssetPath = Path::Combine(kVirtualPathPrefix, assetFolderAndName.Folder, assetFolderAndName.Name);

			// Attempt to load the previously processed asset
			if(GetResources().Exists(virtualAssetPath))
				return GetResources().Load<Texture>(virtualAssetPath, ResourceLoadOptions(false));
			else // Texture file doesn't exist, import from the source file.
			{
				// When importing you may specify optional import options that control how is the asset imported.
				const Path& sourceAssetPath = Path::Combine(EXAMPLE_DATA_PATH, assetFolderAndName.Folder, assetFolderAndName.Name);
				SPtr<ImportOptions> textureImportOptions = Importer::Instance().CreateImportOptions(sourceAssetPath);

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
				HTexture texture = GetImporter().Import<Texture>(sourceAssetPath, textureImportOptions);

				// Save for later use, so we don't have to import on the next run.
				ResourceSaveOptions resourceSaveOptions;
				resourceSaveOptions.VirtualPathPrefix = Path::Combine(kVirtualPathPrefix, assetFolderAndName.Folder);

				GetResources().SaveAsSinglePackage(texture, Path::Combine(EXAMPLE_DATA_PATH, assetFolderAndName.Folder), assetFolderAndName.Name, resourceSaveOptions);
				return texture;
			}
		}

		/**
		 * Loads one of the builtin shader assets. If the asset doesn't exist, the shader will be re-imported from the
		 * source file, and then saved so it can be loaded on the next call to this method.
		 */
		static HShader LoadShader(ExampleShader type)
		{
			// Map from the enum to the actual file path
			static AssetFolderAndName assetPaths[] = {
				AssetFolderAndName("Shaders/", "CustomVertex.bsl"),
				AssetFolderAndName("Shaders/", "CustomDeferredSurface.bsl"),
				AssetFolderAndName("Shaders/", "CustomDeferredLighting.bsl"),
				AssetFolderAndName("Shaders/", "CustomForward.bsl"),
			};

			const AssetFolderAndName& assetFolderAndName = assetPaths[(u32)type];
			const Path& virtualAssetPath = Path::Combine(kVirtualPathPrefix, assetFolderAndName.Folder, assetFolderAndName.Name);

			// Attempt to load the previously processed asset
			if(GetResources().Exists(virtualAssetPath))
				return GetResources().Load<Shader>(virtualAssetPath, ResourceLoadOptions(false));
			else // Shader file doesn't exist, import from the source file.
			{
				const Path& sourceAssetPath = Path::Combine(EXAMPLE_DATA_PATH, assetFolderAndName.Folder, assetFolderAndName.Name);
				HShader shader = GetImporter().Import<Shader>(sourceAssetPath);

				// Save for later use, so we don't have to import on the next run.
				ResourceSaveOptions resourceSaveOptions;
				resourceSaveOptions.VirtualPathPrefix = Path::Combine(kVirtualPathPrefix, assetFolderAndName.Folder);

				GetResources().SaveAsSinglePackage(shader, Path::Combine(EXAMPLE_DATA_PATH, assetFolderAndName.Folder), assetFolderAndName.Name, resourceSaveOptions);
				return shader;
			}
		}

		/**
		 * Loads one of the builtin font assets. If the asset doesn't exist, the font will be re-imported from the
		 * source file, and then saved so it can be loaded on the next call to this method.
		 */
		static HFont LoadFont(ExampleFont type)
		{
			// Map from the enum to the actual file path
			static AssetFolderAndName assetPaths[] = {
				AssetFolderAndName("GUI/", "segoeuil.ttf"),
				AssetFolderAndName("GUI/", "seguisb.ttf")
			};

			const AssetFolderAndName& assetFolderAndName = assetPaths[(u32)type];
			const Path& virtualAssetPath = Path::Combine(kVirtualPathPrefix, assetFolderAndName.Folder, assetFolderAndName.Name);

			// Attempt to load the previously processed asset
			if(GetResources().Exists(virtualAssetPath))
				return GetResources().Load<Font>(virtualAssetPath, ResourceLoadOptions(false));
			else // Font file doesn't exist, import from the source file.
			{
				const Path& sourceAssetPath = Path::Combine(EXAMPLE_DATA_PATH, assetFolderAndName.Folder, assetFolderAndName.Name);

				// When importing you may specify optional import options that control how is the asset imported.
				SPtr<FontImportOptions> fontImportOptions = FontImportOptions::Create();
				HFont font = GetImporter().Import<Font>(sourceAssetPath, fontImportOptions);

				// Save for later use, so we don't have to import on the next run.
				ResourceSaveOptions resourceSaveOptions;
				resourceSaveOptions.VirtualPathPrefix = Path::Combine(kVirtualPathPrefix, assetFolderAndName.Folder);

				GetResources().SaveAsSinglePackage(font, Path::Combine(EXAMPLE_DATA_PATH, assetFolderAndName.Folder), assetFolderAndName.Name, resourceSaveOptions);
				return font;
			}
		}

		/**
		 * Loads one of the builtin non-specific assets. If the asset doesn't exist, it will be re-imported from the
		 * source file, and then saved so it can be loaded on the next call to this method.
		 */
		template <class T>
		static TResourceHandle<T> LoadResource(ExampleResource type)
		{
			// Map from the enum to the actual file path
			static AssetFolderAndName assetPaths[] = {
				AssetFolderAndName("Particles/", "VectorField.fga")
			};

			const AssetFolderAndName& assetFolderAndName = assetPaths[(u32)type];
			const Path& virtualAssetPath = Path::Combine(kVirtualPathPrefix, assetFolderAndName.Folder, assetFolderAndName.Name);

			// Attempt to load the previously processed asset
			TResourceHandle<T> resource = GetResources().Load<T>(virtualAssetPath, ResourceLoadOptions(false));
			if(resource == nullptr) // Shader file doesn't exist, import from the source file.
			{
				const Path& sourceAssetPath = Path::Combine(EXAMPLE_DATA_PATH, assetFolderAndName.Folder, assetFolderAndName.Name);
				resource = GetImporter().Import<T>(sourceAssetPath);

				// Save for later use, so we don't have to import on the next run.
				ResourceSaveOptions resourceSaveOptions;
				resourceSaveOptions.VirtualPathPrefix = Path::Combine(kVirtualPathPrefix, assetFolderAndName.Folder);

				GetResources().SaveAsSinglePackage(resource, Path::Combine(EXAMPLE_DATA_PATH, assetFolderAndName.Folder), assetFolderAndName.Name, resourceSaveOptions);
			}

			return resource;
		}
	};

} // namespace b3d
