// Framework includes
#include "B3DApplication.h"
#include "B3DEntry.h"
#include "Resources/B3DResources.h"
#include "Resources/B3DBuiltinResources.h"
#include "Material/B3DMaterial.h"
#include "Components/B3DCamera.h"
#include "Components/B3DRenderable.h"
#include "Components/B3DSkybox.h"
#include "Components/B3DPlaneCollider.h"
#include "Components/B3DBoxCollider.h"
#include "Components/B3DCharacterController.h"
#include "Components/B3DRigidbody.h"
#include "Physics/B3DPhysicsMaterial.h"
#include "GpuBackend/B3DRenderWindow.h"
#include "Scene/B3DSceneObject.h"
#include "Input/B3DInput.h"
#include "Components/B3DDecal.h"

// Example includes
#include "B3DExampleFramework.h"
#include "B3DFPSWalker.h"
#include "B3DFPSCamera.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This example sets up a simple environment consisting of a floor and cube, and a decal projecting on both surfaces. The
// example demonstrates how to set up decals, how decals are not shown on surfaces perpendicular to the decal direction,
// and optionally how to use masking to only project a decal onto a certain set of surfaces.
//
// It also sets up necessary physical objects for collision, as well as the character collider and necessary components
// for walking around the environment.
//
// The example first sets up the scene consisting of a floor, box and a skybox. Character controller is created next,
// as well as the camera. Components for moving the character controller and the camera are attached to allow the user to
// control the character. It then loads the required decal textures, sets up a decal material and initializes the actual
// decal component. Finally the cursor is hidden and quit on Esc key press hooked up.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace b3d
{
	constexpr float GROUND_PLANE_SCALE = 50.0f;

	u32 kWindowWidth = 1280;
	u32 kWindowHeight = 720;

	TShared<Decal> decal;

	/** Set up the scene used by the example, and the camera to view the world through. */
	void SetUpScene()
	{
		/************************************************************************/
		/* 									ASSETS	                    		*/
		/************************************************************************/

		// Prepare all the resources we'll be using throughout this example

		// Grab a couple of test textures that we'll apply to the rendered objects
		HTexture gridPattern = ExampleFramework::LoadTexture(ExampleTexture::GridPattern, ExampleTextureType::Default);
		HTexture gridPattern2 = ExampleFramework::LoadTexture(ExampleTexture::GridPattern2, ExampleTextureType::Default);

		// Grab the default PBR shader
		HShader shader = GetBuiltinResources().GetBuiltinShader(BuiltinShader::Standard);

		// Create a set of materials to apply to renderables used
		HMaterial planeMaterial = Material::Create(shader);
		planeMaterial->SetTexture("gAlbedoTex", gridPattern2);

		// Tile the texture so every tile covers a 2x2m area
		planeMaterial->SetVec2("gUVTile", Vector2::kOne * GROUND_PLANE_SCALE * 0.5f);

		// Load meshes we'll used for our rendered objects
		HMesh planeMesh = GetBuiltinResources().GetMesh(BuiltinMesh::Quad);

		/************************************************************************/
		/* 									FLOOR	                    		*/
		/************************************************************************/

		// Set up renderable geometry for the floor plane
		HSceneObject floorSO = SceneObject::Create("Floor");
		HRenderable floorRenderable = floorSO->AddComponent<Renderable>();
		floorRenderable->SetMesh(planeMesh);
		floorRenderable->SetMaterial(planeMaterial);

		floorSO->SetScale(Vector3(GROUND_PLANE_SCALE, 1.0f, GROUND_PLANE_SCALE));

		// Add a plane collider that will prevent physical objects going through the floor
		HPlaneCollider planeCollider = floorSO->AddComponent<PlaneCollider>();

		HMaterial boxMaterial = Material::Create(shader);
		boxMaterial->SetTexture("gAlbedoTex", gridPattern);

		HMaterial sphereMaterial = Material::Create(shader);

		// Load meshes we'll used for our rendered objects
		HMesh boxMesh = GetBuiltinResources().GetMesh(BuiltinMesh::Box);
		HSceneObject boxSO = SceneObject::Create("Box");

		HRenderable boxRenderable = boxSO->AddComponent<Renderable>();
		boxRenderable->SetMesh(boxMesh);
		boxRenderable->SetMaterial(boxMaterial);

		// Set a non-default layer for the box, so we can use it for masking on which surfaces should the decal be
		// projected onto
		boxRenderable->SetLayer(1 << 1);

		boxSO->SetPosition(Vector3(0.0f, 0.5f, 0.5f));

		/************************************************************************/
		/* 									CHARACTER                    		*/
		/************************************************************************/

		// Add physics geometry and components for character movement and physics interaction
		HSceneObject characterSO = SceneObject::Create("Character");
		characterSO->SetPosition(Vector3(0.0f, 1.0f, 5.0f));

		// Add a character controller, representing the physical geometry of the character
		HCharacterController charController = characterSO->AddComponent<CharacterController>();

		// Make the character about 1.8m high, with 0.4m radius (controller represents a capsule)
		charController->SetHeight(1.0f); // + 0.4 * 2 radius = 1.8m height
		charController->SetRadius(0.4f);

		// FPS walker uses default input controls to move the character controller attached to the same object
		characterSO->AddComponent<FPSWalker>();

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

		// Add a component that allows the camera to be rotated using the mouse
		sceneCameraSO->SetRotation(Quaternion(Degree(-10.0f), Degree(0.0f), Degree(0.0f)));
		HFPSCamera fpsCamera = sceneCameraSO->AddComponent<FPSCamera>();

		// Set the character controller on the FPS camera, so the component can apply yaw rotation to it
		fpsCamera->SetCharacter(characterSO);

		// Make the camera a child of the character scene object, and position it roughly at eye level
		sceneCameraSO->SetParent(characterSO);
		sceneCameraSO->SetPosition(Vector3(0.0f, 1.8f * 0.5f - 0.1f, -2.0f));

		/************************************************************************/
		/* 									SKYBOX                       		*/
		/************************************************************************/

		// Load a skybox texture
		HTexture skyCubemap = ExampleFramework::LoadTexture(ExampleTexture::EnvironmentDaytime, ExampleTextureType::HDRI);

		// Add a skybox texture for sky reflections
		HSceneObject skyboxSO = SceneObject::Create("Skybox");

		HSkybox skybox = skyboxSO->AddComponent<Skybox>();
		skybox->SetTexture(skyCubemap);

		/************************************************************************/
		/* 									DECAL                       		*/
		/************************************************************************/

		// Load the decal textures
		HTexture decalAlbedoTex = ExampleFramework::LoadTexture(ExampleTexture::DecalAlbedo, ExampleTextureType::Default);
		HTexture decalNormalTex = ExampleFramework::LoadTexture(ExampleTexture::DecalNormal, ExampleTextureType::NormalMap);

		// Create a material using the built-in decal shader and assign the textures
		HShader decalShader = GetBuiltinResources().GetBuiltinShader(BuiltinShader::Decal);
		HMaterial decalMaterial = Material::Create(decalShader);
		decalMaterial->SetTexture("gAlbedoTex", decalAlbedoTex);
		decalMaterial->SetTexture("gNormalTex", decalNormalTex);

		decalMaterial->SetVariation(ShaderVariationParameters(
			{ // Use the default, transparent blend mode that uses traditional PBR textures to project. Normally no need
			  // to set the default explicitly but it's done here for example purposes. See the manual for all available
			  // modes
			  ShaderVariationParameter("BLEND_MODE", 0) }));

		// Create the decal scene object, position and orient it, facing down
		HSceneObject decalSO = SceneObject::Create("Decal");
		decalSO->SetPosition(Vector3(0.0f, 6.0f, 1.0f));
		decalSO->LookAt(Vector3(0.0f, 0.0f, 1.0f));

		// Set the material to project
		HDecal decal = decalSO->AddComponent<Decal>();
		decal->SetMaterial(decalMaterial);

		// Optionally set a mask to only project onto elements with layer 1 set (in this case this is the floor since we
		// changed the default layer for the box)
		// decal->setLayerMask(1);

		/************************************************************************/
		/* 									INPUT                       		*/
		/************************************************************************/

		// Hook up input that launches a sphere when user clicks the mouse, and Esc key to quit
		GetInput().OnButtonUp.Connect([=](const ButtonEvent& ev)
									{
			if(ev.ButtonCode == ButtonCode::Escape)
			{
				// Quit the application when Escape key is pressed
				GetApplication().NotifyQuitRequested();
			} });
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

	// Set up the scene with an object to render and a camera
	SetUpScene();

	// Runs the main loop that does most of the work. This method will exit when user closes the main
	// window or exits in some other way.
	Application::Instance().RunMainLoop();

	// When done, clean up
	Application::ShutDown();

	return 0;
}
