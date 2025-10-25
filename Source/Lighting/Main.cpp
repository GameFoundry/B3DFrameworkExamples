// Framework includes
#include "B3DApplication.h"
#include "Resources/B3DResources.h"
#include "Resources/B3DBuiltinResources.h"
#include "Material/B3DMaterial.h"
#include "Components/B3DCamera.h"
#include "Components/B3DRenderable.h"
#include "Components/B3DLight.h"
#include "Components/B3DSkybox.h"
#include "Components/B3DPlaneCollider.h"
#include "Components/B3DCharacterController.h"
#include "GUI/B3DGUIWidget.h"
#include "GUI/B3DGUIPanel.h"
#include "GUI/B3DGUILayoutY.h"
#include "GUI/B3DGUILabel.h"
#include "RenderAPI/B3DRenderWindow.h"
#include "Scene/B3DSceneObject.h"
#include "Platform/B3DCursor.h"
#include "Input/B3DInput.h"

// Example includes
#include "B3DExampleFramework.h"
#include "B3DFPSWalker.h"
#include "B3DFPSCamera.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This example demonstrates various types of non-directional lights (radial and spot lights), both with and
// without shadow casting. The scene contains a floor and multiple objects that receive lighting from different light
// sources, each with a different color to make their contribution clearly visible.
//
// The example first loads necessary resources, including textures and materials. Then it sets up the scene with a floor
// and various geometric objects (boxes and spheres). Four different lights are created:
// - Red radial light (unshadowed)
// - Blue radial light (shadowed)
// - Green spot light (unshadowed)
// - Orange spot light (shadowed)
// A character controller and FPS camera are set up to allow the user to walk around and explore the lighting effects.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace b3d
{
	constexpr float GROUND_PLANE_SCALE = 50.0f;

	u32 kWindowWidth = 1280;
	u32 kWindowHeight = 720;

	/** Set up the scene used by the example, and the camera to view the world through. */
	void SetUpScene()
	{
		/************************************************************************/
		/* 									ASSETS	                    		*/
		/************************************************************************/

		// Prepare all the resources we'll be using throughout this example

		// Grab a couple of test textures that we'll apply to the rendered objects
		HTexture gridPattern = ExampleFramework::LoadTexture(ExampleTexture::GridPattern);
		HTexture gridPattern2 = ExampleFramework::LoadTexture(ExampleTexture::GridPattern2);

		// Grab the default PBR shader
		HShader shader = GetBuiltinResources().GetBuiltinShader(BuiltinShader::Standard);

		// Create a set of materials to apply to renderables used
		HMaterial planeMaterial = Material::Create(shader);
		planeMaterial->SetTexture("gAlbedoTex", gridPattern2);

		// Tile the texture so every tile covers a 2x2m area
		planeMaterial->SetVec2("gUVTile", Vector2::kOne * GROUND_PLANE_SCALE * 0.5f);

		HMaterial boxMaterial = Material::Create(shader);
		boxMaterial->SetTexture("gAlbedoTex", gridPattern);

		HMaterial sphereMaterial = Material::Create(shader);

		// Create emissive materials for visualizing light positions
		HMaterial redEmissiveMat = Material::Create(shader);
		redEmissiveMat->SetTexture("gEmissiveMaskTex", GetBuiltinResources().GetTexture(BuiltinTexture::White));
		redEmissiveMat->SetColor("gEmissiveColor", Color::kRed * 5.0f);

		HMaterial blueEmissiveMat = Material::Create(shader);
		blueEmissiveMat->SetTexture("gEmissiveMaskTex", GetBuiltinResources().GetTexture(BuiltinTexture::White));
		blueEmissiveMat->SetColor("gEmissiveColor", Color::kBlue * 5.0f);

		HMaterial greenEmissiveMat = Material::Create(shader);
		greenEmissiveMat->SetTexture("gEmissiveMaskTex", GetBuiltinResources().GetTexture(BuiltinTexture::White));
		greenEmissiveMat->SetColor("gEmissiveColor", Color::kGreen * 5.0f);

		HMaterial yellowEmissiveMat = Material::Create(shader);
		yellowEmissiveMat->SetTexture("gEmissiveMaskTex", GetBuiltinResources().GetTexture(BuiltinTexture::White));
		yellowEmissiveMat->SetColor("gEmissiveColor", Color::kBansheeOrange * 5.0f);

		// Load meshes we'll use for our rendered objects
		HMesh boxMesh = GetBuiltinResources().GetMesh(BuiltinMesh::Box);
		HMesh planeMesh = GetBuiltinResources().GetMesh(BuiltinMesh::Quad);
		HMesh sphereMesh = GetBuiltinResources().GetMesh(BuiltinMesh::Sphere);

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

		/************************************************************************/
		/* 									OBJECTS	                    		*/
		/************************************************************************/

		// Create boxes and spheres to demonstrate lighting and shadows
		// Central area with boxes arranged in a circle
		for(u32 boxIndex = 0; boxIndex < 8; boxIndex++)
		{
			float angle = boxIndex * 45.0f;
			float radius = 3.0f;

			Vector3 position(
				radius * Math::Cos(Degree(angle)),
				0.5f,
				radius * Math::Sin(Degree(angle))
			);

			HSceneObject boxSO = SceneObject::Create("Box");
			HRenderable boxRenderable = boxSO->AddComponent<Renderable>();
			boxRenderable->SetMesh(boxMesh);
			boxRenderable->SetMaterial(boxMaterial);

			boxSO->SetPosition(position);
		}

		// Create some spheres at different heights
		for(u32 sphereIndex = 0; sphereIndex < 4; sphereIndex++)
		{
			float angle = sphereIndex * 90.0f + 45.0f;
			float radius = 5.0f;

			Vector3 position(
				radius * Math::Cos(Degree(angle)),
				1.5f,
				radius * Math::Sin(Degree(angle))
			);

			HSceneObject sphereSO = SceneObject::Create("Sphere");
			HRenderable sphereRenderable = sphereSO->AddComponent<Renderable>();
			sphereRenderable->SetMesh(sphereMesh);
			sphereRenderable->SetMaterial(sphereMaterial);

			sphereSO->SetPosition(position);
			sphereSO->SetScale(Vector3::kOne * 0.8f);
		}

		// Create a few taller boxes to cast more interesting shadows
		for(u32 tallBoxIndex = 0; tallBoxIndex < 4; tallBoxIndex++)
		{
			float angle = tallBoxIndex * 90.0f;
			float radius = 7.0f;

			Vector3 position(
				radius * Math::Cos(Degree(angle)),
				1.0f,
				radius * Math::Sin(Degree(angle))
			);

			HSceneObject tallBoxSO = SceneObject::Create("TallBox");
			HRenderable tallBoxRenderable = tallBoxSO->AddComponent<Renderable>();
			tallBoxRenderable->SetMesh(boxMesh);
			tallBoxRenderable->SetMaterial(boxMaterial);

			tallBoxSO->SetPosition(position);
			tallBoxSO->SetScale(Vector3(1.0f, 2.0f, 1.0f));
		}

		/************************************************************************/
		/* 									LIGHTS	                    		*/
		/************************************************************************/

		// Red radial light (unshadowed) - positioned to the north
		HSceneObject redLightSO = SceneObject::Create("RedRadialLight");
		redLightSO->SetPosition(Vector3(0.0f, 3.0f, -8.0f));

		HLight redLight = redLightSO->AddComponent<Light>();
		redLight->SetType(LightType::Radial);
		redLight->SetColor(Color::kRed);
		redLight->SetIntensity(100.0f);
		redLight->SetUseAutoAttenuation(false);
		redLight->SetAttenuationRadius(15.0f);
		redLight->SetCastsShadow(false);

		// Add a small sphere to visualize the light position
		HRenderable redLightSphere = redLightSO->AddComponent<Renderable>();
		redLightSphere->SetMesh(sphereMesh);
		redLightSphere->SetMaterial(redEmissiveMat);
		redLightSO->SetScale(Vector3::kOne * 0.15f);

		// Blue radial light (shadowed) - positioned to the south
		HSceneObject blueLightSO = SceneObject::Create("BlueRadialLight");
		blueLightSO->SetPosition(Vector3(0.0f, 3.0f, 8.0f));

		HLight blueLight = blueLightSO->AddComponent<Light>();
		blueLight->SetType(LightType::Radial);
		blueLight->SetColor(Color::kBlue);
		blueLight->SetIntensity(100.0f);
		blueLight->SetUseAutoAttenuation(false);
		blueLight->SetAttenuationRadius(15.0f);
		blueLight->SetCastsShadow(true);

		// Add a small sphere to visualize the light position
		HRenderable blueLightSphere = blueLightSO->AddComponent<Renderable>();
		blueLightSphere->SetMesh(sphereMesh);
		blueLightSphere->SetMaterial(blueEmissiveMat);
		blueLightSO->SetScale(Vector3::kOne * 0.15f);

		// Green spot light (unshadowed) - positioned to the east
		HSceneObject greenLightSO = SceneObject::Create("GreenSpotLight");
		greenLightSO->SetPosition(Vector3(8.0f, 4.0f, 0.0f));
		greenLightSO->SetRotation(Quaternion(Degree(0.0f), Degree(-90.0f), Degree(-45.0f)));

		HLight greenLight = greenLightSO->AddComponent<Light>();
		greenLight->SetType(LightType::Spot);
		greenLight->SetColor(Color::kGreen);
		greenLight->SetIntensity(100.0f);
		greenLight->SetUseAutoAttenuation(false);
		greenLight->SetAttenuationRadius(20.0f);
		greenLight->SetSpotAngle(Degree(45.0f));
		greenLight->SetSpotFalloffAngle(Degree(35.0f));
		greenLight->SetCastsShadow(false);

		// Add a small sphere to visualize the light position
		HRenderable greenLightSphere = greenLightSO->AddComponent<Renderable>();
		greenLightSphere->SetMesh(sphereMesh);
		greenLightSphere->SetMaterial(greenEmissiveMat);
		greenLightSO->SetScale(Vector3::kOne * 0.15f);

		// Yellow spot light (shadowed) - positioned to the west
		HSceneObject yellowLightSO = SceneObject::Create("YellowSpotLight");
		yellowLightSO->SetPosition(Vector3(-8.0f, 4.0f, 0.0f));
		yellowLightSO->SetRotation(Quaternion(Degree(0.0f), Degree(90.0f), Degree(-45.0f)));

		HLight yellowLight = yellowLightSO->AddComponent<Light>();
		yellowLight->SetType(LightType::Spot);
		yellowLight->SetColor(Color::kBansheeOrange);
		yellowLight->SetIntensity(100.0f);
		yellowLight->SetUseAutoAttenuation(false);
		yellowLight->SetAttenuationRadius(20.0f);
		yellowLight->SetSpotAngle(Degree(45.0f));
		yellowLight->SetSpotFalloffAngle(Degree(35.0f));
		yellowLight->SetCastsShadow(true);

		// Add a small sphere to visualize the light position
		HRenderable yellowLightSphere = yellowLightSO->AddComponent<Renderable>();
		yellowLightSphere->SetMesh(sphereMesh);
		yellowLightSphere->SetMaterial(yellowEmissiveMat);
		yellowLightSO->SetScale(Vector3::kOne * 0.15f);

		/************************************************************************/
		/* 									CHARACTER                    		*/
		/************************************************************************/

		// Add physics geometry and components for character movement and physics interaction
		HSceneObject characterSO = SceneObject::Create("Character");
		characterSO->SetPosition(Vector3(0.0f, 1.0f, 12.0f));

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
		SPtr<RenderWindow> window = GetApplication().GetPrimaryWindow();

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
		HFPSCamera fpsCamera = sceneCameraSO->AddComponent<FPSCamera>();

		// Set the character controller on the FPS camera, so the component can apply yaw rotation to it
		fpsCamera->SetCharacter(characterSO);

		// Make the camera a child of the character scene object, and position it roughly at eye level
		sceneCameraSO->SetParent(characterSO);
		sceneCameraSO->SetPosition(Vector3(0.0f, 1.8f * 0.5f - 0.1f, 0.0f));

		/************************************************************************/
		/* 									SKYBOX                       		*/
		/************************************************************************/

		// Load a skybox texture
		HTexture skyCubemap = ExampleFramework::LoadTexture(ExampleTexture::EnvironmentPaperMill, false, true, true);

		// Add a skybox texture for sky reflections
		HSceneObject skyboxSO = SceneObject::Create("Skybox");

		HSkybox skybox = skyboxSO->AddComponent<Skybox>();
		skybox->SetTexture(skyCubemap);

		/************************************************************************/
		/* 									CURSOR                       		*/
		/************************************************************************/

		// Hide and clip the cursor, since we only use the mouse movement for camera rotation
		Cursor::Instance().Hide();
		Cursor::Instance().ClipToWindow(*window);

		/************************************************************************/
		/* 									INPUT                       		*/
		/************************************************************************/

		// Hook up Esc key to quit
		GetInput().OnButtonUp.Connect([=](const ButtonEvent& ev)
		{
			if(ev.ButtonCode == ButtonCode::Escape)
			{
				// Quit the application when Escape key is pressed
				GetApplication().NotifyQuitRequested();
			}
		});

		/************************************************************************/
		/* 									GUI		                     		*/
		/************************************************************************/

		// Display GUI elements indicating to the user which lights are in the scene

		// Add a GUIWidget component we will use for rendering the GUI
		HSceneObject guiSO = SceneObject::Create("GUI");
		HGUIWidget gui = guiSO->AddComponent<GUIWidget>(sceneCamera);

		// Grab the main panel onto which to attach the GUI elements to
		GUIPanel* mainPanel = gui->GetPanel();

		// Create a vertical GUI layout to align the labels one below each other
		GUILayoutY* vertLayout = GUILayoutY::Create();

		// Create the GUI labels displaying the lighting setup
		HString titleString(u8"Non-Directional Lighting Demo");
		HString redLightString(u8"Red radial light (north) - unshadowed");
		HString blueLightString(u8"Blue radial light (south) - shadowed");
		HString greenLightString(u8"Green spot light (east) - unshadowed");
		HString yellowLightString(u8"Yellow spot light (west) - shadowed");
		HString controlsString(u8"Use WASD to move, mouse to look around");
		HString quitString(u8"Press Escape to quit");

		vertLayout->AddNewElement<GUILabel>(titleString);
		vertLayout->AddNewElement<GUILabel>(redLightString);
		vertLayout->AddNewElement<GUILabel>(blueLightString);
		vertLayout->AddNewElement<GUILabel>(greenLightString);
		vertLayout->AddNewElement<GUILabel>(yellowLightString);
		vertLayout->AddNewElement<GUILabel>(controlsString);
		vertLayout->AddNewElement<GUILabel>(quitString);

		// Register the layout with the main GUI panel, placing the layout in top left corner of the screen by default
		mainPanel->AddElement(vertLayout);
	}
} // namespace b3d

/** Main entry point into the application. */
#if B3D_PLATFORM == B3D_PLATFORM_ID_WIN32
#	include <windows.h>

int CALLBACK WinMain(
	_In_ HINSTANCE hInstance,
	_In_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nCmdShow)
#else
int main()
#endif
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
