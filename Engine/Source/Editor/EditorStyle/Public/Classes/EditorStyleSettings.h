// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Public/Styling/SlateBrush.h"
#include "EditorStyleSettings.generated.h"

/**
 * Enumerates color vision deficiency types.
 */
UENUM()
enum EColorVisionDeficiency
{
	CVD_NormalVision UMETA(DisplayName="Normal Vision"),
	CVD_Deuteranomly UMETA(DisplayName="Deuteranomly (6% of males, 0.4% of females)"),
	CVD_Deuteranopia UMETA(DisplayName="Deuteranopia (1% of males)"),
	CVD_Protanomly UMETA(DisplayName="Protanomly (1% of males, 0.01% of females)"),
	CVD_Protanopia UMETA(DisplayName="Protanopia (1% of males)"),
	CVD_Tritanomaly UMETA(DisplayName="Tritanomaly (0.01% of males and females)"),
	CVD_Tritanopia UMETA(DisplayName="Tritanopia (1% of males and females)"),
	CVD_Achromatopsia UMETA(DisplayName="Achromatopsia (Extremely Rare)"),
};


UENUM()
enum class EAssetEditorOpenLocation : uint8
{
	/** Attempts to dock asset editors into either a new window, or the main window if they were docked there. */
	Default,
	/** Docks tabs into new windows. */
	NewWindow,
	/** Docks tabs into the main window. */
	MainWindow,
	/** Docks tabs into the content browser's window. */
	ContentBrowser,
	/** Docks tabs into the last window that was docked into, or a new window if there is no last docked window. */
	LastDockedWindowOrNewWindow,
	/** Docks tabs into the last window that was docked into, or the main window if there is no last docked window. */
	LastDockedWindowOrMainWindow,
	/** Docks tabs into the last window that was docked into, or the content browser window if there is no last docked window. */
	LastDockedWindowOrContentBrowser
};

/**
 * Implements the Editor style settings.
 */
UCLASS(config=EditorPerProjectUserSettings)
class EDITORSTYLE_API UEditorStyleSettings : public UObject
{
public:

	GENERATED_UCLASS_BODY()

public:

	/** The color used to represent selection */
	UPROPERTY(EditAnywhere, config, Category=Colors, meta=(DisplayName="Selection Color"))
	FLinearColor SelectionColor;

	/** The color used to represent a pressed item */
	UPROPERTY(EditAnywhere, config, Category=Colors, meta=(DisplayName="Pressed Selection Color"))
	FLinearColor PressedSelectionColor;

	/** The color used to represent selected items that are currently inactive */
	UPROPERTY(EditAnywhere, config, Category=Colors, meta=(DisplayName="Inactive Selection Color"))
	FLinearColor InactiveSelectionColor;

	/** The color used to represent keyboard input selection focus */
	UPROPERTY(EditAnywhere, config, Category=Colors, meta=(DisplayName="Keyboard Focus Color"), AdvancedDisplay)
	FLinearColor KeyboardFocusColor;

	/** Applies a color vision deficiency filter to the entire editor */
	UPROPERTY(EditAnywhere, config, Category=Colors)
	TEnumAsByte<EColorVisionDeficiency> ColorVisionDeficiencyPreviewType;

	/** The color used to tint the editor window backgrounds */
	UPROPERTY(EditAnywhere, config, Category=Colors)
	FLinearColor EditorWindowBackgroundColor;

	/** The override for the background of the main window (if not modified, the defaults will be used) */
	UPROPERTY(EditAnywhere, config, Category=Colors)
	FSlateBrush EditorMainWindowBackgroundOverride;

	/** The override for the background of the child window (if not modified, the defaults will be used) */
	UPROPERTY(EditAnywhere, config, Category=Colors)
	FSlateBrush EditorChildWindowBackgroundOverride;

	/** Check to reset the window background settings to editor defaults */
	UPROPERTY(EditAnywhere, config, Category=Colors)
	bool bResetEditorWindowBackgroundSettings;

public:

	/** Whether to use small toolbar icons without labels or not. */
	UPROPERTY(EditAnywhere, config, Category=UserInterface)
	uint32 bUseSmallToolBarIcons:1;

	/** If true the material editor and blueprint editor will show a grid on it's background. */
	UPROPERTY(EditAnywhere, config, Category = Graphs, meta = (DisplayName = "Use Grids In The Material And Blueprint Editor"))
	uint32 bUseGrid : 1;

	/** The color used to represent regular grid lines */
	UPROPERTY(EditAnywhere, config, Category = Graphs, meta = (DisplayName = "Grid Regular Color"))
	FLinearColor RegularColor;

	/** The color used to represent ruler lines in the grid */
	UPROPERTY(EditAnywhere, config, Category = Graphs, meta = (DisplayName = "Grid Ruler Color"))
	FLinearColor RuleColor;

	/** The color used to represent the center lines in the grid */
	UPROPERTY(EditAnywhere, config, Category = Graphs, meta = (DisplayName = "Grid Center Color"))
	FLinearColor CenterColor;

	/** The custom grid snap size to use  */
	UPROPERTY(EditAnywhere, config, Category = Graphs, meta = (ClampMin = "1.0", ClampMax = "100.0", UIMin = "1.0", UIMax = "100.0"))
	uint32 GridSnapSize;

	/** Enables animated transitions for certain menus and pop-up windows.  Note that animations may be automatically disabled at low frame rates in order to improve responsiveness. */
	UPROPERTY(EditAnywhere, config, Category=UserInterface)
	uint32 bEnableWindowAnimations:1;

	/** When enabled, the C++ names for properties and functions will be displayed in a format that is easier to read */
	UPROPERTY(EditAnywhere, config, Category=UserInterface, meta=(DisplayName="Show Friendly Variable Names"))
	uint32 bShowFriendlyNames:1;

	/** When enabled, the Editor Preferences and Project Settings menu items in the main menu will be expanded with sub-menus for each settings section. */
	UPROPERTY(EditAnywhere, config, Category=UserInterface, AdvancedDisplay)
	uint32 bExpandConfigurationMenus:1;

	/** When enabled, the Editor Preferences and Project Settings menu items in the main menu will be expanded with sub-menus for each settings section. */
	UPROPERTY(config)
	uint32 bShowProjectMenus : 1;

	/** When enabled, the Launch menu items will be shown. */
	UPROPERTY(config)
	uint32 bShowLaunchMenus : 1;

	/** The color used for the background in the output log */
	UPROPERTY(EditAnywhere, config, Category="Output Log", meta=(DisplayName="Background Color"))
	FLinearColor LogBackgroundColor;

	/** The color used for the background of selected text in the output log */
	UPROPERTY(EditAnywhere, config, Category="Output Log", meta=(DisplayName="Selection Background Color"))
	FLinearColor LogSelectionBackgroundColor;

	/** The color used for normal text in the output log */
	UPROPERTY(EditAnywhere, config, Category="Output Log", meta=(DisplayName="Normal Text Color"))
	FLinearColor LogNormalColor;

	/** The color used for normal text in the output log */
	UPROPERTY(EditAnywhere, config, Category="Output Log", meta=(DisplayName="Command Text Color"))
	FLinearColor LogCommandColor;

	/** The color used for warning log lines */
	UPROPERTY(EditAnywhere, config, Category="Output Log", meta=(DisplayName="Warning Text Color"))
	FLinearColor LogWarningColor;

	/** The color used for error log lines */
	UPROPERTY(EditAnywhere, config, Category="Output Log", meta=(DisplayName="Error Text Color"))
	FLinearColor LogErrorColor;

	/** When enabled, the Advanced Details will always auto expand. */
	UPROPERTY(config)
	uint32 bShowAllAdvancedDetails : 1;

	/** When Playing or Simulating, shows all properties (even non-visible and non-editable properties), if the object belongs to a simulating world.  This is useful for debugging. */
	UPROPERTY(config)
	uint32 bShowHiddenPropertiesWhilePlaying : 1;

	/** The font size used in the output log */
	UPROPERTY(EditAnywhere, config, Category="Output Log", meta=(DisplayName="Log Font Size", ConfigRestartRequired=true))
	int32 LogFontSize;

	/** The display mode for timestamps in the output log */
	UPROPERTY(EditAnywhere, config, Category="Output Log")
	TEnumAsByte<ELogTimes::Type> LogTimestampMode;

	/** Should warnings and errors in the Output Log during "Play in Editor" be promoted to the message log? */
	UPROPERTY(EditAnywhere, config, Category="Output Log")
	bool bPromoteOutputLogWarningsDuringPIE;

	/** New asset editor tabs will open at the specified location. */
	UPROPERTY(EditAnywhere, config, Category=UserInterface)
	EAssetEditorOpenLocation AssetEditorOpenLocation;

	/** Should editor tabs be colorized according to the asset type */
	UPROPERTY(EditAnywhere, config, Category=UserInterface)
	uint32 bEnableColorizedEditorTabs : 1;

public:

	/**
	 * Returns an event delegate that is executed when a setting has changed.
	 *
	 * @return The delegate.
	 */
	DECLARE_EVENT_OneParam(UEditorStyleSettings, FSettingChangedEvent, FName /*PropertyName*/);
	FSettingChangedEvent& OnSettingChanged( ) { return SettingChangedEvent; }

	/** @return A subdued version of the users selection color (for use with inactive selection)*/
	FLinearColor GetSubduedSelectionColor() const;
protected:

	// UObject overrides

#if WITH_EDITOR
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent ) override;
#endif

private:

	// Holds an event delegate that is executed when a setting has changed.
	FSettingChangedEvent SettingChangedEvent;
};
