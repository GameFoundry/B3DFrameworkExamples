// Framework includes
#include "B3DApplication.h"
#include "B3DEntry.h"
#include "Resources/B3DResources.h"
#include "Resources/B3DBuiltinResources.h"
#include "Material/B3DMaterial.h"
#include "Components/B3DCamera.h"
#include "Components/B3DRenderable.h"
#include "Components/B3DSkybox.h"
#include "Components/B3DLight.h"
#include "GUI/B3DGUIWidget.h"
#include "GUI/B3DGUIPanel.h"
#include "GUI/B3DGUILayoutY.h"
#include "GUI/B3DGUILabel.h"
#include "GpuBackend/B3DRenderWindow.h"
#include "Scene/B3DSceneObject.h"
#include "Renderer/B3DRenderer.h"
#include "Material/B3DShader.h"

// Example includes
#include "B3DCameraFlyer.h"
#include "B3DObjectRotator.h"
#include "B3DExampleFramework.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This example renders an object using a variety of custom materials, showing you how you can customize the rendering of
// your objects if the built-in materials are not adequate. The example is structuraly very similar to the
// PhysicallyBasedShading example, with the addition of custom materials. The most important part of this example are in
// fact the shaders that it uses, so make sure to also study the BSL code of the shaders we import below.
//
// The example first loads necessary resources, including a mesh and textures to use for rendering. Then it imports a set
// of custom shaders and creates a set of materials based on those shaders. It then proceeds to register the relevant keys
// used for controling the camera and the rendered object, as well as a key to switch between different materials. It then
// sets up the 3D scene using the mesh, textures, and the initial material, as well as a camera, along with CameraFlyer and
// ObjectRotator components that allow the user to fly around the scene and rotate the 3D model. Finally it hooks up a
// callback that switches between the materials when the user presses the relevant key, and adds a bit of GUI to let the
// user know which key to press.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace b3d
{
	u32 kWindowWidth = 1280;
	u32 kWindowHeight = 720;

	/** Container for all resources used by the example. */
	struct Assets
	{
		HMesh Sphere;
		HTexture ExampleAlbedoTex;
		HTexture ExampleNormalsTex;
		HTexture ExampleRoughnessTex;
		HTexture ExampleMetalnessTex;
		HTexture SkyTex;

		HMaterial StandardMaterial;
		HMaterial VertexMaterial;
		HMaterial DeferredSurfaceMaterial;
		HMaterial ForwardMaterial;

		HShader DeferredLightingShader;
	};

	Assets gAssets;

	/** Helper method that creates a material from the provided shader, and assigns the relevant PBR textures. */
	HMaterial CreatePbrMaterial(const HShader& shader, const Assets& assets)
	{
		HMaterial material = Material::Create(shader);

		material->SetTexture("gAlbedoTex", assets.ExampleAlbedoTex);
		material->SetTexture("gNormalTex", assets.ExampleNormalsTex);
		material->SetTexture("gRoughnessTex", assets.ExampleRoughnessTex);
		material->SetTexture("gMetalnessTex", assets.ExampleMetalnessTex);

		return material;
	}

	/** Load the resources we'll be using throughout the example. */
	Assets LoadAssets()
	{
		Assets assets;

		// Load a 3D model
		assets.Sphere = ExampleFramework::LoadMesh(ExampleMesh::Pistol, 10.0f);

		// Load PBR textures for the 3D model
		assets.ExampleAlbedoTex = ExampleFramework::LoadTexture(ExampleTexture::PistolAlbedo, ExampleTextureType::Default);
		assets.ExampleNormalsTex = ExampleFramework::LoadTexture(ExampleTexture::PistolNormal, ExampleTextureType::NormalMap);
		assets.ExampleRoughnessTex = ExampleFramework::LoadTexture(ExampleTexture::PistolRoughness, ExampleTextureType::Linear);
		assets.ExampleMetalnessTex = ExampleFramework::LoadTexture(ExampleTexture::PistolMetalness, ExampleTextureType::Linear);

		// Create a set of materials we'll be using for rendering the object
		//// Create a standard PBR material
		HShader standardShader = GetBuiltinResources().GetBuiltinShader(BuiltinShader::Standard);
		assets.StandardMaterial = CreatePbrMaterial(standardShader, assets);

		//// Create a material that overrides the vertex transform of the rendered model. This creates a wobble in the model
		//// geometry, but doesn't otherwise change the lighting properties (i.e. it still uses the PBR lighting model).
		HShader vertexShader = ExampleFramework::LoadShader(ExampleShader::CustomVertex);
		assets.VertexMaterial = CreatePbrMaterial(vertexShader, assets);

		//// Create a material that overrides the surface data that gets used by the lighting evaluation. The material
		//// ignores the albedo texture provided, and instead uses a noise function to generate the albedo values.
		HShader deferredSurfaceShader = ExampleFramework::LoadShader(ExampleShader::CustomDeferredSurface);
		assets.DeferredSurfaceMaterial = CreatePbrMaterial(deferredSurfaceShader, assets);

		//// Create a material that overrides the lighting calculation by implementing a custom BRDF function, in this case
		//// using a basic Lambert BRDF. Note that lighting calculations for the deferred pipeline are done globally, so
		//// this material is created and used differently than others in this example. Instead of being assigned to
		//// Renderable it is instead applied globally and will affect all objects using the deferred pipeline.
		assets.DeferredLightingShader = ExampleFramework::LoadShader(ExampleShader::CustomDeferredLighting);

		//// Creates a material that uses the forward rendering pipeline, while all previous materials have used the
		//// deferred rendering pipeline. Forward rendering is required when the shader is used for rendering transparent
		//// geometry, as this is not supported by the deferred pipeline. Forward rendering shader contains both the surface
		//// and lighting portions in a single shader (unlike with deferred). This custom shader overrides both, using a
		//// noise function for generating the surface albedo, and overriding the PBR BRDF with a basic Lambert BRDF.
		HShader forwardSurfaceAndLighting = ExampleFramework::LoadShader(ExampleShader::CustomForward);
		assets.ForwardMaterial = CreatePbrMaterial(forwardSurfaceAndLighting, assets);

		// Load an environment map
		assets.SkyTex = ExampleFramework::LoadTexture(ExampleTexture::EnvironmentPaperMill, ExampleTextureType::HDRI);

		return assets;
	}

	HRenderable gRenderable;
	HGUIWidget gGUI;
	u32 gMaterialIdx = 0;

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
		HSceneObject pistolSO = SceneObject::Create("Pistol");

		// Attach the Renderable component and hook up the mesh we loaded, and the material we created.
		gRenderable = pistolSO->AddComponent<Renderable>();
		gRenderable->SetMesh(assets.Sphere);
		gRenderable->SetMaterial(assets.StandardMaterial);

		// Add a rotator component so we can rotate the object during runtime
		pistolSO->AddComponent<ObjectRotator>();

		/************************************************************************/
		/* 									LIGHT		                  		*/
		/************************************************************************/

		// Add a light so we can actually see the object
		HSceneObject lightSO = SceneObject::Create("Light");

		HLight light = lightSO->AddComponent<Light>();
		light->SetIntensity(100.0f);

		lightSO->SetPosition(Vector3(1.0f, 0.5f, 0.0f));

		/************************************************************************/
		/* 									SKYBOX                       		*/
		/************************************************************************/

		// Add a skybox texture for sky reflections
		HSceneObject skyboxSO = SceneObject::Create("Skybox");

		HSkybox skybox = skyboxSO->AddComponent<Skybox>();
		skybox->SetTexture(assets.SkyTex);

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

		// Add a CameraFlyer component that allows us to move the camera. See CameraFlyer for more information.
		sceneCameraSO->AddComponent<CameraFlyer>();

		// Position and orient the camera scene object
		sceneCameraSO->SetPosition(Vector3(2.0f, 1.0f, 2.0f));
		sceneCameraSO->LookAt(Vector3(-0.4f, 0, 0));

		/************************************************************************/
		/* 									GUI		                     		*/
		/************************************************************************/

		// Add a GUIWidget component we will use for rendering the GUI
		HSceneObject guiSO = SceneObject::Create("GUI");
		gGUI = guiSO->AddComponent<GUIWidget>(sceneCamera);
	}

	/** Sets up or rebuilds any GUI elements used by the example. */
	void UpdateGui()
	{
		GUIPanel* mainPanel = gGUI->GetPanel();

		// Clear any existing elements, in case this is not the first time we're calling this function
		mainPanel->Clear();

		// Set up strings to display
		String materialNameLookup[] = {
			"Standard",
			"Vertex wobble (Deferred)",
			"Surface noise (Deferred)",
			"Lambert BRDF (Deferred)",
			"Surface noise & Lambert BRDF (Forward)"
		};

		HString toggleString("Press Q to toggle between materials");
		HString currentMaterialString("Current material: {0}");

		currentMaterialString.SetParameter(0, materialNameLookup[gMaterialIdx]);

		// Create a vertical GUI layout to align the two labels one below each other
		GUILayoutY* vertLayout = GUILayoutY::Create();

		// Create a couple of GUI labels displaying the two strings we created above
		vertLayout->AddNewElement<GUILabel>(toggleString);
		vertLayout->AddNewElement<GUILabel>(currentMaterialString);

		// Register the layout with the main GUI panel, placing the layout in top left corner of the screen by default
		mainPanel->AddElement(vertLayout);
	}

	/** Switches the material used for rendering the renderable object. */
	void SwitchMaterial()
	{
		HMaterial materialLookup[] = {
			gAssets.StandardMaterial,
			gAssets.VertexMaterial,
			gAssets.DeferredSurfaceMaterial,
			gAssets.ForwardMaterial
		};

		gMaterialIdx = (gMaterialIdx + 1) % 5;

		// Apply the newly selected material
		switch(gMaterialIdx)
		{
		case 0:
			// Standard material, simply apply to renderable
			gRenderable->SetMaterial(gAssets.StandardMaterial);
			break;
		case 1:
			// Deferred vertex material, simply apply to renderable
			gRenderable->SetMaterial(gAssets.VertexMaterial);
			break;
		case 2:
			// Deferred surface material, simply apply to renderable
			gRenderable->SetMaterial(gAssets.DeferredSurfaceMaterial);
			break;
		case 3:
			// Deferred lighting material. Apply it globally and reset the surface material back to standard.
			gRenderable->SetMaterial(gAssets.StandardMaterial);
			break;
		case 4:
			// Forward surface/lighting material. Simply apply to renderable. Also clear the deferred lighting material
			// override from the last material.
			gRenderable->SetMaterial(gAssets.ForwardMaterial);
			break;
		}

		// Update GUI with current material name
		UpdateGui();
	}

	/** Register relevant mouse/keyboard buttons used for controlling the example. */
	void SetupInput()
	{
		// Registers a default set of input controls
		ExampleFramework::SetupInputConfig();

		VirtualButton switchMaterialButton = VirtualInput::GetOrCreateVirtualButton("SwitchMaterial");

		// Register a key for toggling between different materials
		auto inputConfig = GetVirtualInput().GetConfiguration();
		inputConfig->RegisterButton("SwitchMaterial", ButtonCode::Q);

		GetVirtualInput().OnButtonUp.Connect(
			[switchMaterialButton](const VirtualButton& btn, u32 deviceIdx)
			{
				if(btn == switchMaterialButton)
					SwitchMaterial();
			});
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

	// Register buttons for controlling the example
	SetupInput();

	// Load a model and textures, create materials
	gAssets = LoadAssets();

	// Set up the scene with an object to render and a camera
	SetUp3DScene(gAssets);

	// Sets up any GUI elements used by the example.
	UpdateGui();

	// Runs the main loop that does most of the work. This method will exit when user closes the main
	// window or exits in some other way.
	Application::Instance().RunMainLoop();

	// When done, clean up
	Application::ShutDown();

	return 0;
}
