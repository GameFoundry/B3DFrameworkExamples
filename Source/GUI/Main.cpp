// Framework includes
#include "B3DApplication.h"
#include "Resources/B3DResources.h"
#include "Resources/B3DBuiltinResources.h"
#include "Material/B3DMaterial.h"
#include "Components/B3DCamera.h"
#include "GUI/B3DGUIWidget.h"
#include "GUI/B3DGUIPanel.h"
#include "GUI/B3DGUILayoutX.h"
#include "GUI/B3DGUILayoutY.h"
#include "GUI/B3DGUILabel.h"
#include "GUI/B3DGUIButton.h"
#include "GUI/B3DGUIInputBox.h"
#include "GUI/B3DGUIListBox.h"
#include "GUI/B3DGUIToggle.h"
#include "GUI/B3DGUIScrollArea.h"
#include "GUI/B3DGUISpace.h"
#include "GpuBackend/B3DRenderWindow.h"
#include "Scene/B3DSceneObject.h"
#include "B3DExampleFramework.h"
#include "GUI/StyleSheet/B3DGUIStyleSheet.h"
#include "Image/B3DSpriteTexture.h"
#include "B3DEntry.h"

#include "Reflection/B3DRTTIIteratorField.h"
#include "Renderer/B3DRenderSettings.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This example demonstrates how to set up a graphical user interface. It demoes a variety of common GUI elements, as well
// as demonstrating the capability of layouts. It also shows how to customize the look of GUI elements.
//
// The example starts off by setting up a camera, which is required for any kind of rendering, including GUI. It then
// proceeds to demonstrate a set of basic controls, while using manual positioning. It then shows how to create a custom
// style and apply it to a GUI element. It follows to demonstrate the concept of layouts that automatically position
// and size elements, as well as scroll areas. Finally, it demonstrates a more complex example of creating a custom style,
// by creating a button with custom textures and font.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace b3d
{
	u32 kWindowWidth = 1280;
	u32 kWindowHeight = 720;

	/** Set up the GUI elements and the camera. */
	void SetUpGui()
	{
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

		// Pick a prettier background color
		Color gray = Color(51 / 255.0f, 51 / 255.0f, 51 / 255.0f);
		sceneCamera->GetViewport()->SetClearColorValue(gray);

		// Let the camera know it will be used for overlay rendering only. This stops the renderer from running potentially
		// expensive effects that ultimately don't effect anything. It also allows us to use a linear-space color for the
		// camera background (normal rendering expects colors in gamma space, which is unintuitive for aspects such as
		// GUI).
		const TShared<RenderSettings>& renderSettings = sceneCamera->GetRenderSettings();
		renderSettings->OverlayOnly = true;
		sceneCamera->SetRenderSettings(renderSettings);

		/************************************************************************/
		/* 									GUI		                     		*/
		/************************************************************************/

		// Add a GUIWidget component we will use for rendering the GUI
		HSceneObject guiSO = SceneObject::Create("GUI");
		HGUIWidget gui = guiSO->AddComponent<GUIWidget>(sceneCamera);

		// Load the custom style sheet file and apply it to the widget
		TShared<GUIStyleSheetCascade> customStyleSheetCascade = B3DMakeShared<GUIStyleSheetCascade>();
		customStyleSheetCascade->RegisterStyleSheet(GetBuiltinResources().GetDefaultGUIStyleSheet(), GUIStyleSheet::kBuiltinImportance); // Anything not defined in the custom style sheet file will use the default style sheet

		HGUIStyleSheet customStyleSheet = GUIStyleSheet::Parse(Path(EXAMPLE_DATA_PATH) + "GUI.css");
		customStyleSheetCascade->RegisterStyleSheet(customStyleSheet, GUIStyleSheet::kDeveloperImportance);

		// Register the style sheet with the GUI widget
		gui->SetStyleSheetCascade(customStyleSheetCascade);

		// Retrieve the primary panel onto which to attach GUI elements to. Panels allow free placement of elements in
		// them (unlike layouts), and can also have depth, meaning you can overlay multiple panels over one another.
		GUIPanel* mainPanel = gui->GetPanel();

		////////////////////// Add a variety of GUI controls /////////////////////
		// Clickable button with a textual label
		GUIButton* button = mainPanel->AddNewElement<GUIButton>(HString("Click me!"));
		button->OnClick.Connect([]()
								{
			// Log a message when the user clicks the button
			B3D_LOG(Info, LogUncategorized, "Button clicked!"); });

		button->SetPosition(10, 50);
		button->SetSize(GUILogicalSize(100, 30));

		// Toggleable button
		GUIToggle* toggle = mainPanel->AddNewElement<GUIToggle>();
		toggle->OnToggled.Connect([](bool enabled)
								  {
			// Log a message when the user toggles the button
			if(enabled)
			{
				B3D_LOG(Info, LogUncategorized, "Toggle turned on");
			}
			else
			{
				B3D_LOG(Info, LogUncategorized, "Toggle turned off");
			} });

		toggle->SetPosition(10, 90);

		// Add non-interactable label next to the toggle
		GUILabel* toggleLabel = mainPanel->AddNewElement<GUILabel>(HString("Toggle me!"));
		toggleLabel->SetPosition(30, 92);

		// Single-line box in which user can input text into
		GUIInputBox* inputBox = mainPanel->AddNewElement<GUIInputBox>();
		inputBox->OnValueChanged.Connect([](const String& value)
										 {
			// Log a message when the user enters new text in the input box
			B3D_LOG(Info, LogUncategorized, "User entered: \"" + value + "\""); });

		inputBox->SetText("Type in me...\nand some more\nand more\nand even more\nand the last line");
		inputBox->SetPosition(10, 115);
		inputBox->SetWidth(100);
		inputBox->SetHeight(40);

		// List box allowing you to select one of the specified elements
		Vector<HString> listBoxElements = {
			HString("Blue"),
			HString("Black"),
			HString("Green"),
			HString("Orange")
		};

		GUIListBox* listBox = mainPanel->AddNewElement<GUIListBox>(listBoxElements);
		listBox->OnSelectionToggled.Connect([listBoxElements](u32 idx, bool enabled)
											{
												// Log a message when the user selects a new element
												B3D_LOG(Info, LogUncategorized, "User selected element: \"" + listBoxElements[idx].GetValue() + "\"");
											});

		listBox->SetPosition(10, 180);
		listBox->SetWidth(100);

		// Add a button with an image
		HTexture icon = ExampleFramework::LoadTexture(ExampleTexture::GUIBansheeIcon, ExampleTextureType::Default, false);
		HSpriteImage iconSprite = SpriteTexture::Create(icon);

		// Create a GUI content object that contains an icon to display on the button. Also an optional text and tooltip.
		GUIContent buttonContent(iconSprite);
		GUIButton* iconButton = mainPanel->AddNewElement<GUIButton>(buttonContent);

		iconButton->SetPosition(10, 210);
		iconButton->SetSize(GUILogicalSize(70, 70));

		/////////////////////////// Header label /////////////////////////////////
		// Add a header for the controls we added in the previous section.

		// Create and position the label
		GUILabel* basicControlsLbl = mainPanel->AddNewElement<GUILabel>(HString("Basic controls"), "HeaderLabelStyle");
		basicControlsLbl->SetPosition(10, 10);

		///////////////////////////  vertical layout /////////////////////////
		// Use a vertical layout to automatically position GUI elements. This is unlike above where we position and
		// sized all elements manually.
		GUILayoutY* vertLayout = mainPanel->AddNewElement<GUILayoutY>();

		// Add five buttons to the layout
		for(u32 i = 0; i < 5; i++)
		{
			vertLayout->AddNewElement<GUIButton>(HString("Click me!"));

			// Add a 10 pixel spacing between each button
			vertLayout->AddNewElement<GUIFixedSpace>(10);
		}

		// Add a flexible space ensuring all the elements get pushed to the top of the layout
		vertLayout->AddNewElement<GUIFlexibleSpace>();

		// Position the layout relative to the main panel, and limit width to 100 pixels
		vertLayout->SetPosition(350, 50);
		vertLayout->SetWidth(100);

		// Add a header
		GUILabel* vertLayoutLbl = mainPanel->AddNewElement<GUILabel>(HString("Vertical layout"), "HeaderLabelStyle");
		vertLayoutLbl->SetPosition(300, 10);

		////////////////////////// Horizontal layout ///////////////////////
		// Use a horizontal layout to automatically position GUI elements
		GUILayoutX* horzLayout = mainPanel->AddNewElement<GUILayoutX>();
		horzLayout->AddNewElement<GUIFlexibleSpace>();

		// Add five buttons to the layout
		for(u32 i = 0; i < 5; i++)
		{
			horzLayout->AddNewElement<GUIButton>(HString("Click me!"));
			horzLayout->AddNewElement<GUIFlexibleSpace>();
		}

		// Position the layout relative to the main panel, and limit the height to 30 pixels
		horzLayout->SetPosition(0, 340);
		horzLayout->SetHeight(30);

		// Add a header
		GUILabel* horzLayoutLbl = mainPanel->AddNewElement<GUILabel>(HString("Horizontal layout"), "HeaderLabelStyle");
		horzLayoutLbl->SetPosition(10, 300);

		//////////////////////////// Scroll area ///////////////////////
		// Container GUI element that allows scrolling if the number of elements inside the area are larger than the visible
		// area
		GUIScrollArea* scrollArea = mainPanel->AddNewElement<GUIScrollArea>();

		// Scroll areas have a vertical layout we can append elements to, same as with a normal layout
		GUILayout* scrollLayout = scrollArea->GetLayout();

		for(u32 i = 0; i < 15; i++)
			scrollLayout->AddNewElement<GUIButton>(HString("Click me!"));

		scrollArea->SetPosition(565, 50);
		scrollArea->SetSize(GUILogicalSize(130, 200));

		// Add a header
		GUILabel* scrollAreaLbl = mainPanel->AddNewElement<GUILabel>(HString("Scroll area"), "HeaderLabelStyle");
		scrollAreaLbl->SetPosition(550, 10);

		///////////////////////////// Button using a custom style ///////////////////

		// Custom style defined in Examples/Data/GUI.css

		// Create the button that uses the custom style
		GUIButton* customButton = mainPanel->AddNewElement<GUIButton>(HString("Click me!"), "CustomButtonStyle");
		customButton->SetPosition(800, 50);

		// Add a header
		GUILabel* customButtonLbl = mainPanel->AddNewElement<GUILabel>(HString("Custom button"), "HeaderLabelStyle");
		customButtonLbl->SetPosition(800, 10);
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

	// Set up the GUI elements
	SetUpGui();

	// Runs the main loop that does most of the work. This method will exit when user closes the main
	// window or exits in some other way.
	Application::Instance().RunMainLoop();

	// When done, clean up
	Application::ShutDown();

	return 0;
}
