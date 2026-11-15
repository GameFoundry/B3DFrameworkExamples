// Framework includes
#include "B3DApplication.h"
#include "Resources/B3DBuiltinResources.h"
#include "Components/B3DCamera.h"
#include "GUI/B3DGUIWidget.h"
#include "GUI/B3DGUIPanel.h"
#include "GUI/B3DGUILabel.h"
#include "GUI/B3DGUIButton.h"
#include "GpuBackend/B3DRenderWindow.h"
#include "Scene/B3DSceneObject.h"
#include "B3DExampleFramework.h"
#include "GUI/StyleSheet/B3DGUIStyleSheet.h"
#include "Renderer/B3DRenderSettings.h"
#include "B3DEntry.h"
#include "Utility/B3DCommandLine.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Renders a small set of buttons and text blocks that mirror Artifacts/TextRenderQuality/reference.html, so text and
// rounded-rectangle rendering can be compared against a web browser pixel for pixel.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace b3d
{
	constexpr u32 kWindowWidth = 480;
	constexpr u32 kWindowHeight = 360;

	/** Rows of text over colored backgrounds, mirroring Artifacts/TextRenderQuality/contrast.html. */
	void SetUpContrastScene(GUIPanel* panel)
	{
		static constexpr const char* kRowLabels[] = {
			"Mid grey on black", "Mid grey on white", "White on blue", "White on red", "Black on green",
			"Dark on amber", "Light on purple", "Low contrast grey", "Yellow on dark"
		};

		constexpr i32 kRowHeight = 40;
		constexpr i32 kTextInset = 12;

		// Sibling elements in one panel don't paint in insertion order, so the row backgrounds go into a panel behind the text
		GUIPanel* backgroundPanel = panel->AddNewElement<GUIPanel>(GUIPanelContent(1));
		for(u32 rowIndex = 0; rowIndex < (u32)std::size(kRowLabels); rowIndex++)
		{
			const i32 rowY = (i32)rowIndex * kRowHeight;

			GUILabel* background = backgroundPanel->AddNewElement<GUILabel>(HString(""), StringUtility::Format("ContrastRow{0}", rowIndex));
			background->SetPosition(0, rowY);
			background->SetSize(GUILogicalSize(kWindowWidth, kRowHeight));

			const String text = StringUtility::Format("{0}: Hamburgefonstiv quick brown fox AV Wa 0123", kRowLabels[rowIndex]);
			GUILabel* label = panel->AddNewElement<GUILabel>(HString(text), StringUtility::Format("ContrastText{0}", rowIndex));
			label->SetPosition(kTextInset, rowY + kTextInset);
			label->SetSize(GUILogicalSize(kWindowWidth - kTextInset, 20));
		}
	}

	void SetUpGui()
	{
		HSceneObject sceneCameraSO = SceneObject::Create("SceneCamera");
		TShared<RenderWindow> window = GetApplication().GetPrimaryWindow();

		HCamera sceneCamera = sceneCameraSO->AddComponent<Camera>();
		sceneCamera->GetViewport()->SetTarget(window);
		sceneCamera->GetViewport()->SetBackgroundColor(Color(0x1E / 255.0f, 0x1F / 255.0f, 0x22 / 255.0f));

		const TShared<RenderSettings>& renderSettings = sceneCamera->GetRenderSettings();
		renderSettings->OverlayOnly = true;
		sceneCamera->SetRenderSettings(renderSettings);

		HSceneObject guiSO = SceneObject::Create("GUI");
		HGUIWidget gui = guiSO->AddComponent<GUIWidget>(sceneCamera);

		TShared<GUIStyleSheetCascade> styleSheetCascade = B3DMakeShared<GUIStyleSheetCascade>();
		styleSheetCascade->RegisterStyleSheet(GetBuiltinResources().GetDefaultGUIStyleSheet(), GUIStyleSheet::kBuiltinImportance);
		styleSheetCascade->RegisterStyleSheet(GUIStyleSheet::Parse(TEXT_QUALITY_CSS_PATH), GUIStyleSheet::kDeveloperImportance);
		gui->SetStyleSheetCascade(styleSheetCascade);

		GUIPanel* panel = gui->GetPanel();

		// Text coverage correction edge cases: text over backgrounds that aren't the luminance inverse of the text
		if(CommandLine::GetParameterValue("scene", "main") == "contrast")
		{
			SetUpContrastScene(panel);
			return;
		}

		// Sibling elements in one panel don't paint in insertion order, so the backdrop goes into a panel behind the rest
		GUIPanel* backgroundPanel = panel->AddNewElement<GUIPanel>(GUIPanelContent(1));
		GUILabel* lightBackground = backgroundPanel->AddNewElement<GUILabel>(HString(""), "LightBg");
		lightBackground->SetPosition(0, 240);
		lightBackground->SetSize(GUILogicalSize(480, 120));

		// Optionally cover the camera clear color with an opaque GUI backdrop of the same color
		if(CommandLine::HasParameter("dark-backdrop"))
		{
			GUILabel* darkBackground = backgroundPanel->AddNewElement<GUILabel>(HString(""), "DarkBg");
			darkBackground->SetPosition(0, 0);
			darkBackground->SetSize(GUILogicalSize(480, 240));
		}

		auto addButton = [panel](const char* text, const char* style, i32 x, i32 y, u32 width, u32 height)
		{
			GUIButton* button = panel->AddNewElement<GUIButton>(HString(text), style);
			button->SetPosition(x, y);
			button->SetSize(GUILogicalSize(width, height));
		};

		auto addLabel = [panel](const char* text, const char* style, i32 x, i32 y, u32 width, u32 height)
		{
			GUILabel* label = panel->AddNewElement<GUILabel>(HString(text), style);
			label->SetPosition(x, y);
			label->SetSize(GUILogicalSize(width, height));
		};

		addButton("Save changes", "Primary", 24, 24, 140, 36);
		addButton("Cancel", "Secondary", 180, 24, 120, 36);
		addButton("Rounded pill", "Pill", 316, 24, 140, 36);

		addLabel("AVATAR Wave To. LT Ty Yo fi ffl", "Kern", 24, 76, 432, 28);
		addLabel("The quick brown fox jumps over the lazy dog. Sphinx of black\n"
				 "quartz, judge my vow. Pack my box with five dozen liquor jugs.\n"
				 "0123456789 (),.;:!? Hamburgefonstiv",
			"Para", 24, 108, 432, 60);
		addLabel("Small 11px text: Illegal1|lI 0O rn m vv w", "Small", 24, 212, 432, 16);

		addButton("Light button", "LightBtn", 24, 256, 140, 32);
		addLabel("Dark text on a light background: The quick brown fox\n"
				 "jumps over the lazy dog. AV Wa To",
			"LightText", 24, 300, 432, 40);
	}
} // namespace b3d

int B3DMain()
{
	using namespace b3d;

	VideoMode videoMode(kWindowWidth, kWindowHeight);
	Application::StartUp(videoMode, "TextQuality", false);

	// The layout is in logical pixels, so enlarge the window to match on HiDPI displays (headless: --window.HeadlessDPIScale=1.5)
	const TShared<RenderWindow> window = GetApplication().GetPrimaryWindow();
	const float dpiScale = window->GetProperties().DPIScale;
	if(dpiScale != 1.0f)
		window->Resize((u32)Math::RoundToI32(kWindowWidth * dpiScale), (u32)Math::RoundToI32(kWindowHeight * dpiScale));

	ExampleFramework::LoadPackages();
	SetUpGui();

	Application::Instance().RunMainLoop();
	Application::ShutDown();

	return 0;
}
