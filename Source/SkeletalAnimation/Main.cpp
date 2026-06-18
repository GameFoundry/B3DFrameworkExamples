// Framework includes
#include "B3DApplication.h"
#include "B3DEntry.h"
#include "Resources/B3DResources.h"
#include "Resources/B3DBuiltinResources.h"
#include "Material/B3DMaterial.h"
#include "Components/B3DCamera.h"
#include "Components/B3DRenderable.h"
#include "Components/B3DAnimation.h"
#include "Components/B3DSkybox.h"
#include "GpuBackend/B3DRenderWindow.h"
#include "Scene/B3DSceneObject.h"

// Example includes
#include "B3DCameraFlyer.h"
#include "B3DExampleFramework.h"
#include "Renderer/B3DRenderSettings.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This example demonstrates how to animate a 3D model using skeletal animation. Aside from animation this example is
// structurally similar to PhysicallyBasedShading example.
//
// The example first loads necessary resources, including a mesh and textures to use for rendering, as well as an animation
// clip. The animation clip is imported from the same file as the 3D model. Special import options are used to tell the
// importer to import data required for skeletal animation. It then proceeds to register the relevant keys used for
// controling the camera. Next it sets up the 3D scene using the mesh, textures, material and adds an animation
// component. The animation component start playing the animation clip we imported earlier. Finally it sets up a camera,
// along with CameraFlyer component that allows the user to fly around the scene.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace b3d
{
	u32 kWindowWidth = 1280;
	u32 kWindowHeight = 720;

	/** Container for all resources used by the example. */
	struct Assets
	{
		HMesh ExampleModel;
		HAnimationClip ExampleAnimClip;
		HTexture ExampleAlbedoTex;
		HTexture ExampleNormalsTex;
		HTexture ExampleRoughnessTex;
		HTexture ExampleMetalnessTex;
		HTexture ExampleSkyCubemap;
		HMaterial ExampleMaterial;
	};

	/** Load the resources we'll be using throughout the example. */
	Assets LoadAssets()
	{
		Assets assets;

		// Load the 3D model and the animation clip

		// Set up a path to the model resource
		const Path exampleDataPath = EXAMPLE_DATA_PATH;
		const Path modelPath = exampleDataPath + "MechDrone/Drone.FBX";

		// Set up mesh import options so that we import information about the skeleton and the skin, as well as any
		// animation clips the model might have.
		TShared<MeshImportOptions> meshImportOptions = MeshImportOptions::Create();
		meshImportOptions->ImportSkin = true;
		meshImportOptions->ImportAnimation = true;

		// The FBX file contains multiple resources (a mesh and an animation clip), therefore we use importAll() method,
		// which imports all resources in a file.
		TShared<MultiResource> modelResources = GetImporter().ImportAll(modelPath, meshImportOptions);
		for(auto& entry : modelResources->Entries)
		{
			if(B3DRTTIIsOfType<Mesh>(entry.Value.Get()))
				assets.ExampleModel = B3DStaticResourceCast<Mesh>(entry.Value);
			else if(B3DRTTIIsOfType<AnimationClip>(entry.Value.Get()))
				assets.ExampleAnimClip = B3DStaticResourceCast<AnimationClip>(entry.Value);
		}

		// Load PBR textures for the 3D model
		assets.ExampleAlbedoTex = ExampleFramework::LoadTexture(ExampleTexture::DroneAlbedo, ExampleTextureType::Default);
		assets.ExampleNormalsTex = ExampleFramework::LoadTexture(ExampleTexture::DroneNormal, ExampleTextureType::NormalMap);
		assets.ExampleRoughnessTex = ExampleFramework::LoadTexture(ExampleTexture::DroneRoughness, ExampleTextureType::Linear);
		assets.ExampleMetalnessTex = ExampleFramework::LoadTexture(ExampleTexture::DroneMetalness, ExampleTextureType::Linear);

		// Create a material using the default physically based shader, and apply the PBR textures we just loaded
		HShader shader = GetBuiltinResources().GetBuiltinShader(BuiltinShader::Standard);
		assets.ExampleMaterial = Material::Create(shader);

		assets.ExampleMaterial->SetTexture("gAlbedoTex", assets.ExampleAlbedoTex);
		assets.ExampleMaterial->SetTexture("gNormalTex", assets.ExampleNormalsTex);
		assets.ExampleMaterial->SetTexture("gRoughnessTex", assets.ExampleRoughnessTex);
		assets.ExampleMaterial->SetTexture("gMetalnessTex", assets.ExampleMetalnessTex);

		// Load an environment map
		assets.ExampleSkyCubemap = ExampleFramework::LoadTexture(ExampleTexture::EnvironmentRathaus, ExampleTextureType::HDRI);

		return assets;
	}

	/** Set up the 3D object used by the example, and the camera to view the world through. */
	void SetUp3DScene(const Assets& assets)
	{
		/************************************************************************/
		/* 									RENDERABLE                  		*/
		/************************************************************************/

		// Now we create a scene object that has a position, orientation, scale and optionally components to govern its
		// logic. In this particular case we are creating a SceneObject with a Renderable component which will render a
		// mesh at the position of the scene object with the provided material.

		// Create new scene object at (0, 0, 0)
		HSceneObject droneSO = SceneObject::Create("Drone");

		// Attach the Renderable component and hook up the mesh we loaded, and the material we created.
		HRenderable renderable = droneSO->AddComponent<Renderable>();
		renderable->SetMesh(assets.ExampleModel);
		renderable->SetMaterial(assets.ExampleMaterial);

		/************************************************************************/
		/* 									ANIMATION	                  		*/
		/************************************************************************/

		// Add an animation component to the same scene object we added Renderable to.
		HAnimation animation = droneSO->AddComponent<Animation>();

		// Start playing the animation clip we imported
		animation->Play(assets.ExampleAnimClip);

		/************************************************************************/
		/* 									SKYBOX                       		*/
		/************************************************************************/

		// Add a skybox texture for sky reflections
		HSceneObject skyboxSO = SceneObject::Create("Skybox");

		HSkybox skybox = skyboxSO->AddComponent<Skybox>();
		skybox->SetTexture(assets.ExampleSkyCubemap);

		/************************************************************************/
		/* 									CAMERA	                     		*/
		/************************************************************************/

		// In order something to render on screen we need at least one camera.

		// Like before, we create a new scene object at (0, 0, 0).
		HSceneObject sceneCameraSO = SceneObject::Create("SceneCamera");

		// Get the primary render window we need for creating the camera.
		TShared<RenderWindow> window = GetApplication().GetPrimaryWindow();

		// Add a Camera component that will output whatever it sees into that window
		// (You could also use a render texture or another window you created).
		HCamera sceneCamera = sceneCameraSO->AddComponent<Camera>();
		sceneCamera->GetViewport()->SetTarget(window);

		// Set up camera component properties

		// Set closest distance that is visible. Anything below that is clipped.
		sceneCamera->SetNearClipDistance(0.005f);

		// Set farthest distance that is visible. Anything above that is clipped.
		sceneCamera->SetFarClipDistance(1000);

		// Set aspect ratio depending on the current resolution
		sceneCamera->SetAspectRatio(kWindowWidth / (float)kWindowHeight);

		// Enable indirect lighting so we get accurate diffuse lighting from the skybox environment map
		const TShared<RenderSettings>& renderSettings = sceneCamera->GetRenderSettings();
		renderSettings->EnableIndirectLighting = true;

		sceneCamera->SetRenderSettings(renderSettings);

		// Add a CameraFlyer component that allows us to move the camera. See CameraFlyer for more information.
		sceneCameraSO->AddComponent<CameraFlyer>();

		// Position and orient the camera scene object
		sceneCameraSO->SetPosition(Vector3(0.0f, 2.5f, -4.0f) * 0.65f);
		sceneCameraSO->LookAt(Vector3(0, 1.5f, 0));
	}
} // namespace b3d

/** Main entry point into the application. */
int B3DMain()
{
	using namespace b3d;

	// Initializes the application and creates a window with the specified properties
	VideoMode videoMode(kWindowWidth, kWindowHeight);
	Application::StartUp(videoMode, "Example", false);

	// Load packages so we can find previously saved resources
	ExampleFramework::LoadPackages();

	// Registers a default set of input controls
	ExampleFramework::SetupInputConfig();

	// Load a model and textures, create materials
	Assets assets = LoadAssets();

	// Set up the scene with an object to render and a camera
	SetUp3DScene(assets);

	// Runs the main loop that does most of the work. This method will exit when user closes the main
	// window or exits in some other way.
	Application::Instance().RunMainLoop();

	// When done, clean up
	Application::ShutDown();

	return 0;
}
