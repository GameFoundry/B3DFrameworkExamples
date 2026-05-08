// Framework includes
#include "B3DApplication.h"
#include "B3DEntry.h"
#include "Resources/B3DResources.h"
#include "Audio/B3DAudioClip.h"
#include "Audio/B3DAudioClipImportOptions.h"
#include "Audio/B3DAudio.h"
#include "Material/B3DMaterial.h"
#include "Components/B3DCamera.h"
#include "Components/B3DAudioSource.h"
#include "Components/B3DAudioListener.h"
#include "GpuBackend/B3DRenderWindow.h"
#include "Scene/B3DSceneObject.h"
#include "Importer/B3DImporter.h"
#include "Utility/B3DTime.h"
#include "Input/B3DInput.h"
#include "GUI/B3DGUIWidget.h"
#include "GUI/B3DGUIPanel.h"
#include "GUI/B3DGUILayoutY.h"
#include "GUI/B3DGUILabel.h"

// Example headers
#include "B3DExampleConfig.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This example demonstrates how to import audio clips and then play them back using a variety of settings.
//
// The example starts off by import the relevant audio clips, demonstrating various settings for streaming, compression and
// 2D/3D audio. It then sets up a camera that will be used for GUI rendering, unrelated to audio. It proceeds to
// add an AudioListener component which is required to play back 3D sounds (it determines what are sounds relative to). It
// then creates a couple of AudioSources - one that is static and used for music playback (2D audio), and another that
// moves around the listener and demonstrates 3D audio playback. Follow that, input is hooked up that lets the user switch
// between the playback of the two audio sources. It also demonstrates how to play one-shot audio clips without the
// AudioSource component. Finally, GUI is set up that lets the user know which input controls are available.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace b3d
{
	u32 kWindowWidth = 1280;
	u32 kWindowHeight = 720;

	/** Create a helper component that causes its scene object to move around in a circle around the world origin. */
	class ObjectFlyer : public Component
	{
	public:
		ObjectFlyer(const HSceneObject& parent)
			: Component(parent)
		{}

		/** Triggered once per frame. */
		void Update() override
		{
			const float time = GetTime().GetRealTimeInSeconds();

			const float x = cos(time);
			const float z = sin(time);

			SO()->SetPosition(Vector3(x, 0.0f, z));
		}
	};

	/** Import audio clips and set up the audio sources and listeners. */
	void SetUpScene()
	{
		/************************************************************************/
		/* 									ASSETS	                     		*/
		/************************************************************************/

		// First import any audio clips we plan on using

		// Set up paths to the audio file resources
		const Path exampleDataPath = EXAMPLE_DATA_PATH;
		const Path musicClipPath = exampleDataPath + "Audio/BrokeForFree-NightOwl.ogg";
		const Path environmentClipPath = exampleDataPath + "Audio/FilteredPianoAmbient.ogg";
		const Path cueClipPath = exampleDataPath + "Audio/GunShot.wav";

		// Import the music audio clip. Compress the imported data to Vorbis format to save space, at the cost of decoding
		// performance. Also since it's a longer audio clip, use streaming to avoid loading the entire clip into memory,
		// at the additional cost of performance and IO overhead.
		TShared<AudioClipImportOptions> musicImportOptions = AudioClipImportOptions::Create();
		musicImportOptions->Format = AudioFormat::VORBIS;
		musicImportOptions->ReadMode = AudioReadMode::Stream;
		musicImportOptions->Is3D = false;

		HAudioClip musicClip = GetImporter().Import<AudioClip>(musicClipPath, musicImportOptions);

		// Import a loopable environment ambient sound. Compress the imported data to Vorbis format to save space, at the
		// cost of decoding performance. Same as the music clip, this is also a longer clip, but instead of streaming we
		// load the compressed data and just uncompress on the fly. This saves on IO overhead at the cost of little
		// extra memory.
		TShared<AudioClipImportOptions> environmentImportOptions = AudioClipImportOptions::Create();
		environmentImportOptions->Format = AudioFormat::VORBIS;
		environmentImportOptions->ReadMode = AudioReadMode::LoadCompressed;
		environmentImportOptions->Is3D = true;

		HAudioClip environmentClip = GetImporter().Import<AudioClip>(environmentClipPath, environmentImportOptions);

		// Import a short audio cue. Use the uncompressed PCM audio format for fast playback, at the cost of memory.
		TShared<AudioClipImportOptions> cueImportOptions = AudioClipImportOptions::Create();
		cueImportOptions->Format = AudioFormat::PCM;
		cueImportOptions->Is3D = true;

		HAudioClip cueClip = GetImporter().Import<AudioClip>(cueClipPath, cueImportOptions);

		/************************************************************************/
		/* 									CAMERA	                     		*/
		/************************************************************************/

		// Add a camera that will be used for rendering out GUI elements

		// Like before, we create a new scene object at (0, 0, 0).
		HSceneObject sceneCameraSO = SceneObject::Create("SceneCamera");

		// Get the primary render window we need for creating the camera.
		TShared<RenderWindow> window = GetApplication().GetPrimaryWindow();

		// Add a Camera component that will output whatever it sees into that window
		// (You could also use a render texture or another window you created).
		HCamera sceneCamera = sceneCameraSO->AddComponent<Camera>();
		sceneCamera->GetViewport()->SetTarget(window);

		// Set background color
		sceneCamera->GetViewport()->SetClearColorValue(Color::kBlack);

		/************************************************************************/
		/* 									AUDIO	                     		*/
		/************************************************************************/

		// Set up an audio listener. Every sound will be played relative to this listener. We'll add it to the same
		// scene object as our main camera.
		HAudioListener listener = sceneCameraSO->AddComponent<AudioListener>();

		// Add an audio source for playing back the music. Position of the audio source is not important as it is not
		// a 3D sound.
		HSceneObject musicSourceSO = SceneObject::Create("Music");
		HAudioSource musicSource = musicSourceSO->AddComponent<AudioSource>();

		// Assign the clip we want to use for the audio source
		musicSource->SetClip(musicClip);

		// Start playing the audio clip immediately
		musicSource->Play();

		// Add an audio source for playing back an environment sound. This sound is played back on a scene object that
		// orbits the viewer.
		HSceneObject environmentSourceSO = SceneObject::Create("Environment");
		HAudioSource environmentSource = environmentSourceSO->AddComponent<AudioSource>();

		// Assign the clip we want to use for the audio source
		environmentSource->SetClip(environmentClip);

		// Make sure the sound keeps looping if it reaches the end
		environmentSource->SetIsLooping(true);

		// Make the audio source orbit the listener, by attaching an ObjectFlyer component
		environmentSourceSO->AddComponent<ObjectFlyer>();

		/************************************************************************/
		/* 									INPUT	                     		*/
		/************************************************************************/

		// Hook up input commands that toggle between the different audio sources.
		GetInput().OnButtonUp.Connect([=](const ButtonEvent& event)
									{
			switch(event.ButtonCode)
			{
			case ButtonCode::Key1:
				// Start or resume playing music, if not already playing. Stop the ambient sound playback.
				environmentSource->Stop();
				musicSource->Play();
				break;
			case ButtonCode::Key2:
				// Start playing ambient sound, if not already playing. Pause music playback.
				musicSource->Pause();
				environmentSource->Play();
				break;
			case ButtonCode::MouseLeft:
				// Play a one-shot sound at origin. We don't use an AudioSource component because it's a short sound cue
				// that we don't require additional control over.
				GetAudio().Play(cueClip, Vector3::kZero);
				break;
			default:
				break;
			} });

		/************************************************************************/
		/* 									GUI		                     		*/
		/************************************************************************/

		// Display GUI elements indicating to the user which input keys are available

		// Add a GUIWidget component we will use for rendering the GUI
		HSceneObject guiSO = SceneObject::Create("GUI");
		HGUIWidget gui = guiSO->AddComponent<GUIWidget>(sceneCamera);

		// Grab the main panel onto which to attach the GUI elements to
		GUIPanel* mainPanel = gui->GetPanel();

		// Create a vertical GUI layout to align the labels one below each other
		GUILayoutY* vertLayout = GUILayoutY::Create();

		// Create the GUI labels displaying the available input commands
		HString musicString(u8"Press 1 to play music");
		HString environmentString(u8"Press 2 to play 3D ambient sound");
		HString cueString(u8"Press left mouse button to play a gun shot sound");

		vertLayout->AddNewElement<GUILabel>(musicString);
		vertLayout->AddNewElement<GUILabel>(environmentString);
		vertLayout->AddNewElement<GUILabel>(cueString);

		// Register the layout with the main GUI panel, placing the layout in top left corner of the screen by default
		mainPanel->AddElement(vertLayout);
	}
} // namespace b3d

/** Main entry point into the application. */
int B3DMain()
{
	using namespace b3d;

	// Initializes the application and creates a window with the specified properties
	VideoMode videoMode(kWindowWidth, kWindowHeight);
	Application::StartUp(videoMode, "Example", false);

	// Custom example code goes here
	SetUpScene();

	// Runs the main loop that does most of the work. This method will exit when user closes the main
	// window or exits in some other way.
	Application::Instance().RunMainLoop();

	// When done, clean up
	Application::ShutDown();

	return 0;
}
