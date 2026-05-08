// Framework includes
#include "B3DApplication.h"
#include "B3DEntry.h"
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
#include "Renderer/B3DRenderSettings.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This is a playground example for testing the GUI culling functionality.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace b3d
{
	u32 kWindowWidth = 1600;
	u32 kWindowHeight = 600;

	static constexpr u32 kItemCount = 100;

	/** Set up the GUI elements and the camera. */
	void SetUpGui()
	{
		// Set up camera & GUI widget
		const TShared<RenderWindow> applicationWindow = GetApplication().GetPrimaryWindow();
		HSceneObject cameraSceneObject = SceneObject::Create("SceneCamera");

		HCamera camera = cameraSceneObject->AddComponent<Camera>();
		camera->GetViewport()->SetTarget(applicationWindow);

		const Color gray = Color(51 / 255.0f, 51 / 255.0f, 51 / 255.0f);
		camera->GetViewport()->SetClearColorValue(gray);

		const TShared<RenderSettings>& renderSettings = camera->GetRenderSettings();
		renderSettings->OverlayOnly = true;
		camera->SetRenderSettings(renderSettings);

		HSceneObject guiWidgetSceneObject = SceneObject::Create("GUI");
		HGUIWidget guiWidget = guiWidgetSceneObject->AddComponent<GUIWidget>(camera);
		GUIPanel* mainPanel = guiWidget->GetPanel();

		// Create a vertical GUI layout with items
#if 1
		GUILayoutY* itemLayout = mainPanel->AddNewElement<GUILayoutY>();
		itemLayout->SetEnableCulling(true);

		for(u32 itemIndex = 0; itemIndex < kItemCount; itemIndex++)
		{
			itemLayout->AddNewElement<GUIButton>(HString("Click me!"));
		}

		// Add a flexible space ensuring all the elements get pushed to the top of the layout
		itemLayout->AddNewElement<GUIFlexibleSpace>();

		itemLayout->SetWidth(300);
		itemLayout->SetHeight(200);
#endif

		// Create a scroll area with items
#if 1
		GUIScrollArea* scrollArea = GUIScrollArea::Create(GUIScrollAreaContent(ScrollBarType::AlwaysShow, ScrollBarType::NeverShow));
		mainPanel->AddElement(scrollArea);
		scrollArea->SetEnableCulling(true);

		for(u32 itemIndex = 0; itemIndex < kItemCount; itemIndex++)
			scrollArea->GetLayout()->AddNewElement<GUIButton>(HString("Click me!"));

		scrollArea->SetPosition(400, 0);
		scrollArea->SetWidth(300);
		scrollArea->SetHeight(200);
#endif

		// Create a draggable scroll area with items
#if 1
		GUIScrollArea* draggableScrollArea = GUIScrollArea::Create(GUIScrollAreaContent(ScrollBarType::NeverShow, ScrollBarType::NeverShow, ScrollAreaLayoutType::Panel));
		mainPanel->AddElement(draggableScrollArea);
		draggableScrollArea->SetEnableCulling(true);

		static constexpr GUILogicalUnit kItemSpacing = 10;
		static constexpr GUILogicalSize kItemSize(100, 20);
		for(u32 itemIndexY = 0; itemIndexY < kItemCount; itemIndexY++)
		{
			for(u32 itemIndexX = 0; itemIndexX < kItemCount; itemIndexX++)
			{
				const GUILogicalPoint itemPosition((kItemSize.Width + kItemSpacing) * itemIndexX, (kItemSize.Height + kItemSpacing) * itemIndexY);
				GUIButton* const button = draggableScrollArea->GetLayout()->AddNewElement<GUIButton>(HString("Click me!"));
				button->SetPosition(itemPosition);
				button->SetSize(kItemSize);
			}
		}

		draggableScrollArea->SetPosition(800, 0);
		draggableScrollArea->SetWidth(300);
		draggableScrollArea->SetHeight(200);
#endif
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
