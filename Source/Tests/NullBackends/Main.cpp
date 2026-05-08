//************************************ B3D Framework - Copyright 2026 Marko Pintera **************************************//
//*********** Licensed under the MIT license. See LICENSE.md for full terms. This notice is not to be removed. ***********//

// Framework includes
#include "B3DApplication.h"
#include "B3DEntry.h"
#include "Scene/B3DSceneObject.h"
#include "Components/B3DCamera.h"
#include "GpuBackend/B3DRenderWindow.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Null backends snapshot test.
//
// Boots the application with null backends (render API, renderer, audio, physics), runs for a few
// frames, and shuts down. The test validates that the null libraries load correctly and that the
// engine lifecycle (startup, frame loop, shutdown) completes without crashing.
//
// The rendered output will be blank since the null renderer produces no visual output - that is
// expected and correct.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** Main entry point into the application. */
int B3DMain()
{
	using namespace b3d;

	VideoMode videoMode(320, 240);

	ApplicationCreateInformation createInformation =
		Application::BuildCreateInformation(videoMode, "NullBackendsTest", false);

	// Override default plugins with null backends so this test works regardless of which
	// backends the build was configured with (e.g. when using B3D_BUILD_ALL_PLUGINS)
	createInformation.GpuBackend = "bsfNullGpuBackend";
	createInformation.Renderer = "bsfNullRenderer";
	createInformation.Audio = "bsfNullAudio";
	createInformation.Physics = "bsfNullPhysics";
	createInformation.PhysicsCooking = false;

	Application::StartUp(createInformation);

	// Create a minimal scene with a camera so the engine exercises its full frame pipeline
	const TShared<RenderWindow> window = GetApplication().GetPrimaryWindow();
	HSceneObject cameraSceneObject = SceneObject::Create("TestCamera");
	HCamera camera = cameraSceneObject->AddComponent<Camera>();
	camera->GetViewport()->SetTarget(window);

	// Run the main loop (should run with --exit-after-n-frames as otherwise you can't exit the app)
	GetApplication().RunMainLoop();

	Application::ShutDown();

	return 0;
}
