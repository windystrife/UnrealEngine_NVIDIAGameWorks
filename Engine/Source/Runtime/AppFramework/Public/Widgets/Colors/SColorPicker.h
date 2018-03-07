// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Input/Reply.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Framework/SlateDelegates.h"

class FColorTheme;
class SBorder;
class SColorThemesViewer;
class SComboButton;
class SThemeColorBlocksBar;


/** Called when the color picker cancel button is pressed */
DECLARE_DELEGATE_OneParam(FOnColorPickerCancelled, FLinearColor);


/**
 * Enumerates color channels (do not reorder).
 */
enum class EColorPickerChannels
{
	Red,
	Green,
	Blue,
	Alpha,
	Hue,
	Saturation,
	Value
};


/**
 * Enumerates color picker modes.
 */
enum class EColorPickerModes
{
	Spectrum,
	Wheel
};


/**
 * Struct for holding individual pointers to float values.
 */
struct FColorChannels
{
	FColorChannels()
	{
		Red = Green = Blue = Alpha = nullptr;
	}

	float* Red;
	float* Green;
	float* Blue;
	float* Alpha;
};


/**
 * Class for placing a color picker. If all you need is a standalone color picker,
 * use the functions OpenColorPicker and DestroyColorPicker, since they hold a static
 * instance of the color picker.
 */
class APPFRAMEWORK_API SColorPicker
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SColorPicker)
		: _TargetColorAttribute(FLinearColor(ForceInit))
		, _TargetFColors()
		, _TargetLinearColors()
		, _TargetColorChannels()
		, _UseAlpha(true)
		, _OnlyRefreshOnMouseUp(false)
		, _OnlyRefreshOnOk(false)
		, _OnColorCommitted()
		, _PreColorCommitted()
		, _OnColorPickerCancelled()
		, _OnColorPickerWindowClosed()
		, _OnInteractivePickBegin()
		, _OnInteractivePickEnd()
		, _ParentWindow()
		, _DisplayGamma(2.2f)
		, _sRGBOverride()
		, _DisplayInlineVersion(false)
		, _OverrideColorPickerCreation(false)
		, _ExpandAdvancedSection(false)
	{ }
		
		/** The color that is being targeted as a TAttribute */
		SLATE_ATTRIBUTE(FLinearColor, TargetColorAttribute)
		
		/** An array of color pointers this color picker targets */
		SLATE_ATTRIBUTE(TArray<FColor*>, TargetFColors)
		
		/** An array of linear color pointers this color picker targets */
		SLATE_ATTRIBUTE(TArray<FLinearColor*>, TargetLinearColors)
		
		/**
		 * An array of color pointer structs this color picker targets
		 * Only to keep compatibility with wx. Should be removed once wx is gone.
		 */
		SLATE_ATTRIBUTE(TArray<FColorChannels>, TargetColorChannels)

		/** Whether the ability to pick the alpha value is enabled */
		SLATE_ATTRIBUTE(bool, UseAlpha)
		
		/** Prevents immediate refreshs for performance reasons. */
		SLATE_ATTRIBUTE(bool, OnlyRefreshOnMouseUp)

		/** Prevents multiple refreshes when requested. */
		SLATE_ATTRIBUTE(bool, OnlyRefreshOnOk)
		
		/** The event called when the color is committed */
		SLATE_EVENT(FOnLinearColorValueChanged, OnColorCommitted)

		/** The event called before the color is committed */
		SLATE_EVENT(FOnLinearColorValueChanged, PreColorCommitted)
		
		/** The event called when the color picker cancel button is pressed */
		SLATE_EVENT(FOnColorPickerCancelled, OnColorPickerCancelled)

		/** The event called when the color picker parent window is closed */
		SLATE_EVENT(FOnWindowClosed, OnColorPickerWindowClosed)

		/** The event called when a slider drag, color wheel drag or dropper grab starts */
		SLATE_EVENT(FSimpleDelegate, OnInteractivePickBegin)

		/** The event called when a slider drag, color wheel drag or dropper grab finishes */
		SLATE_EVENT(FSimpleDelegate, OnInteractivePickEnd)

		/** A pointer to the parent window */
		SLATE_ATTRIBUTE(TSharedPtr<SWindow>, ParentWindow)

		/** Sets the display Gamma setting - used to correct colors sampled from the screen */
		SLATE_ATTRIBUTE(float, DisplayGamma)

		/** Overrides the checkbox value of the sRGB option. */
		SLATE_ARGUMENT(TOptional<bool>, sRGBOverride)

		/** If true, this color picker will be a stripped down version of the full color picker */
		SLATE_ARGUMENT(bool, DisplayInlineVersion)

		/** If true, this color picker will have non-standard creation behavior */
		SLATE_ARGUMENT(bool, OverrideColorPickerCreation)

		/** If true, the Advanced section will be expanded, regardless of the remembered state */
		SLATE_ARGUMENT(bool, ExpandAdvancedSection)

	SLATE_END_ARGS()
	
	/** A default window size for the color picker which looks nice */
	static const FVector2D DEFAULT_WINDOW_SIZE;

	/**	Destructor. */
	~SColorPicker();

public:

	/**
	 * Construct the widget
	 *
	 * @param InArgs Declaration from which to construct the widget.
	 */
	void Construct(const FArguments& InArgs);

	/** Delegate to override color picker creation behavior */
	DECLARE_DELEGATE_OneParam(FOnColorPickerCreationOverride, const TSharedRef<SColorPicker>&);
	static FOnColorPickerCreationOverride OnColorPickerNonModalCreateOverride;

	/** Delegate to override color picker destruction behavior */
	DECLARE_DELEGATE(FOnColorPickerDestructionOverride);
	static FOnColorPickerDestructionOverride OnColorPickerDestroyOverride;

protected:

	/** Backup all the colors that are being modified */
	void BackupColors();

	/** Restore all the modified colors to their original state */
	void RestoreColors();

	/** Set all the colors to this new color */
	void SetColors(const FLinearColor& InColor);

	bool ApplyNewTargetColor(bool bForceUpdate = false);

	void GenerateDefaultColorPickerContent(bool bAdvancedSectionExpanded);
	void GenerateInlineColorPickerContent();

	FLinearColor GetCurrentColor() const
	{
		return CurrentColorHSV;
	}

	/** Calls the user defined delegate for when the color changes are discarded */
	void DiscardColor();

	/** Sets new color in ether RGB or HSV */
	bool SetNewTargetColorRGB(const FLinearColor& NewValue, bool bForceUpdate = false);
	bool SetNewTargetColorHSV(const FLinearColor& NewValue, bool bForceUpdate = false);

	void UpdateColorPick();
	void UpdateColorPickMouseUp();

	void BeginAnimation(FLinearColor Start, FLinearColor End);

	void HideSmallTrash();
	void ShowSmallTrash();

	/** Cycles the color picker's mode. */
	void CycleMode();

	/**
	 * Creates a color slider widget for the specified channel.
	 *
	 * @param Channel The color channel to create the widget for.
	 * @return The new slider.
	 */
	TSharedRef<SWidget> MakeColorSlider(EColorPickerChannels Channel) const;

	/**
	 * Creates a color spin box widget for the specified channel.
	 *
	 * @param Channel The color channel to create the widget for.
	 * @return The new spin box.
	 */
	TSharedRef<SWidget> MakeColorSpinBox(EColorPickerChannels Channel) const;

	/**
	 * Creates the color preview box widget.
	 *
	 * @return The new color preview box.
	 */
	TSharedRef<SWidget> MakeColorPreviewBox() const;

private:

	// Callback for the active timer to animate the color post-construct
	EActiveTimerReturnType AnimatePostConstruct(double InCurrentTime, float InDeltaTime);

	// Callback for getting the end color of a color spin box gradient.
	FLinearColor GetGradientEndColor(EColorPickerChannels Channel) const;

	// Callback for getting the start color of a color spin box gradient.
	FLinearColor GetGradientStartColor(EColorPickerChannels Channel) const;

	// Callback for handling expansion of the 'Advanced' area.
	void HandleAdvancedAreaExpansionChanged(bool Expanded);

	// Callback for getting the visibility of the alpha channel portion in color blocks.
	EVisibility HandleAlphaColorBlockVisibility() const;

	// Callback for clicking the Cancel button.
	FReply HandleCancelButtonClicked();

	// Callback for pressing a mouse button in the color area.
	FReply HandleColorAreaMouseDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	// Callback for clicking the color picker mode button.
	FReply HandleColorPickerModeButtonClicked();

	// Callback for getting the visibility of the given color picker mode.
	EVisibility HandleColorPickerModeVisibility(EColorPickerModes Mode) const;

	// Callback for getting the end color of a color slider.
	FLinearColor HandleColorSliderEndColor(EColorPickerChannels Channel) const;

	// Callback for getting the start color of a color slider.
	FLinearColor HandleColorSliderStartColor(EColorPickerChannels Channel) const;

	// Callback for value changes in the color spectrum picker.
	void HandleColorSpectrumValueChanged(FLinearColor NewValue);

	// Callback for getting the value of a color spin box.
	float HandleColorSpinBoxValue(EColorPickerChannels Channel) const;

	// Callback for value changes in a color spin box.
	void HandleColorSpinBoxValueChanged(float NewValue, EColorPickerChannels Channel);

	// Callback for completed eye dropper interactions.
	void HandleEyeDropperButtonComplete(bool bCancelled);

	// Callback for getting the text in the hex linear box.
	FText HandleHexLinearBoxText() const;

	// Callback for getting the text in the hex sRGB box.
	FText HandleHexSRGBBoxText() const;

	// Callback for committed text in the hex input box (sRGB gamma).
	void HandleHexSRGBInputTextCommitted(const FText& Text, ETextCommit::Type CommitType);

	// Callback for committed text in the hex input box (linear gamma).
	void HandleHexLinearInputTextCommitted(const FText& Text, ETextCommit::Type CommitType);

	// Callback for changing the HSV value of the current color.
	void HandleHSVColorChanged(FLinearColor NewValue);

	// Callback for when interactive user input begins.
	void HandleInteractiveChangeBegin();

	// Callback for when interactive user input ends.
	void HandleInteractiveChangeEnd();

	// Callback for when interactive user input ends.
	void HandleInteractiveChangeEnd(float NewValue);

	// Callback for clicking the new color preview block.
	FReply HandleNewColorBlockMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bCheckAlpha);

	// Callback for clicking the OK button.
	FReply HandleOkButtonClicked();

	// Callback for clicking the old color preview block.
	FReply HandleOldColorBlockMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bCheckAlpha);

	// Callback for checking whether sRGB colors should be rendered.
	bool HandleColorPickerUseSRGB() const;

	// Callback for when the parent window has been closed.
	void HandleParentWindowClosed(const TSharedRef<SWindow>& Window);

	// Callback for changing the RGB value of the current color.
	void HandleRGBColorChanged(FLinearColor NewValue);

	// Callback for changing the checked state of the sRGB check box.
	void HandleSRGBCheckBoxCheckStateChanged(ECheckBoxState InIsChecked);

	// Callback for determining whether the sRGB check box should be checked.
	ECheckBoxState HandleSRGBCheckBoxIsChecked() const;

	// Callback for selecting a color in the color theme bar.
	void HandleThemeBarColorSelected(FLinearColor NewValue);

	// Callback for getting the theme bar's color theme.
	TSharedPtr<class FColorTheme> HandleThemeBarColorTheme() const;

	// Callback for getting the visibility of the theme bar hint text.
	EVisibility HandleThemeBarHintVisibility() const;

	// Callback for determining whether the theme bar should display the alpha channel.
	bool HandleThemeBarUseAlpha() const;

	// Callback for theme viewer changes.
	void HandleThemesViewerThemeChanged();

private:
	
	/** The color that is being targeted as a TAttribute */
	TAttribute<FLinearColor> TargetColorAttribute;

	/** The current color being picked in HSV */
	FLinearColor CurrentColorHSV;

	/** The current color being picked in RGB */
	FLinearColor CurrentColorRGB;
	
	/** The old color to be changed in HSV */
	FLinearColor OldColor;
	
	/** Color end point to animate to */
	FLinearColor ColorEnd;

	/** Color start point to animate from */
	FLinearColor ColorBegin;

	/** Holds the color picker's mode. */
	EColorPickerModes CurrentMode;

	/** Time, used for color animation */
	float CurrentTime;

	/** The max time allowed for updating before we shut off auto-updating */
	static const double MAX_ALLOWED_UPDATE_TIME;

	/** If true, then the performance is too bad to have auto-updating */
	bool bPerfIsTooSlowToUpdate;

	/** An array of color pointers this color picker targets */
	TArray<FColor*> TargetFColors;
	
	/** An array of linear color pointers this color picker targets */
	TArray<FLinearColor*> TargetLinearColors;
	
	/**
	 * An array of color pointer structs this color picker targets
	 * Only to keep compatibility with wx. Should be removed once wx is gone.
	 */
	TArray<FColorChannels> TargetColorChannels;

	/** Backups of the TargetFColors */
	TArray<FColor> OldTargetFColors;

	/** Backups of the TargetLinearColors */
	TArray<FLinearColor> OldTargetLinearColors;

	/** Backups of the TargetColorChannels */
	TArray<FLinearColor> OldTargetColorChannels;

	/** Whether or not the color uses Alpha or not */
	TAttribute<bool> bUseAlpha;

	/** Prevents immediate refresh for performance reasons. */
	bool bOnlyRefreshOnMouseUp;

	/** Prevents multiple refreshes when requested. */
	bool bOnlyRefreshOnOk;

	/** true if the picker was closed via the OK or Cancel buttons, false otherwise */
	bool bClosedViaOkOrCancel;

	/** Pointer to the parent window, so we know to destroy it when done */
	TWeakPtr<SWindow> ParentWindowPtr;

	/** The widget which holds the currently selected theme */
	TSharedPtr<SThemeColorBlocksBar> CurrentThemeBar;

	/** Widget which is either the button to show the color themes viewer, or to be a color trash */
	TSharedPtr<SBorder> ColorThemeButtonOrSmallTrash;

	/** The button to show the color themes viewer */
	TSharedPtr<SComboButton> ColorThemeComboButton;

	/** The small color trash shown in place of the combo button */
	TSharedPtr<SWidget> SmallTrash;

	/** Sets the display Gamma setting - used to correct colors sampled from the screen */
	TAttribute<float> DisplayGamma;

	/** Stores the original sRGB option if this color picker temporarily overrides the global option. */
	TOptional<bool> OriginalSRGBOption;

	/** True if this color picker is an inline color picker */
	bool bColorPickerIsInlineVersion;

	/** True if something has overridden the color picker's creation behavior */
	bool bColorPickerCreationIsOverridden;

	/** Tracks whether the user is moving a value spin box, the color wheel and the dropper */
	bool bIsInteractive;

	/** Is true if the color picker creation behavior can be overridden */
	bool bValidCreationOverrideExists;

private:

	/** Invoked when a new value is selected on the color wheel */
	FOnLinearColorValueChanged OnColorCommitted;

	/** Invoked before a new value is selected on the color wheel */
	FOnLinearColorValueChanged PreColorCommitted;

	/** Invoked when the color picker cancel button is pressed */
	FOnColorPickerCancelled OnColorPickerCancelled;

	/** Invoked when a slider drag, color wheel drag or dropper grab starts */
	FSimpleDelegate OnInteractivePickBegin;

	/** Invoked when a slider drag, color wheel drag or dropper grab finishes */
	FSimpleDelegate OnInteractivePickEnd;

	/** Invoked when the color picker window closes. */
	FOnWindowClosed OnColorPickerWindowClosed;

private:

	/** A static pointer to the global color themes viewer */
	static TWeakPtr<SColorThemesViewer> ColorThemesViewer;
};


struct FColorPickerArgs
{
	/** Whether or not the new color picker is modal. */
	bool bIsModal;

	/** The parent for the new color picker window */
	TSharedPtr<SWidget> ParentWidget;

	/** Whether or not to enable the alpha slider. */
	bool bUseAlpha;

	/** Whether to disable the refresh except on mouse up for performance reasons. */
	bool bOnlyRefreshOnMouseUp;

	/** Whether to disable the refresh until the picker closes. */
	bool bOnlyRefreshOnOk;

	/** Whether to automatically expand the Advanced section. */
	bool bExpandAdvancedSection;
	
	/** Whether to open the color picker as a menu window. */
	bool bOpenAsMenu;

	/** The current display gamma used to correct colors picked from the display. */
	TAttribute<float> DisplayGamma;

	/** If set overrides the global option for the desired setting of sRGB mode. */
	TOptional<bool> sRGBOverride;

	/** An array of FColors to target. */
	const TArray<FColor*>* ColorArray;

	/** An array of FLinearColors to target. */
	const TArray<FLinearColor*>* LinearColorArray;

	/** An array of FColorChannels to target. (deprecated now that wx is gone?) */
	const TArray<FColorChannels>* ColorChannelsArray;

	/** A delegate to be called when the color changes. */
	FOnLinearColorValueChanged OnColorCommitted;

	/** A delegate to be called before the color change is committed. */
	FOnLinearColorValueChanged PreColorCommitted;

	/** A delegate to be called when the color picker window closes. */
	FOnWindowClosed OnColorPickerWindowClosed;

	/** A delegate to be called when the color picker cancel button is pressed */
	FOnColorPickerCancelled OnColorPickerCancelled;

	/** A delegate to be called when a slider drag, color wheel drag or dropper grab starts */
	FSimpleDelegate OnInteractivePickBegin;

	/** A delegate to be called when a slider drag, color wheel drag or dropper grab finishes */
	FSimpleDelegate OnInteractivePickEnd;

	/** Overrides the initial color set on the color picker. */
	FLinearColor InitialColorOverride;

	/** Default constructor. */
	FColorPickerArgs()
		: bIsModal(false)
		, ParentWidget(nullptr)
		, bUseAlpha(false)
		, bOnlyRefreshOnMouseUp(false)
		, bOnlyRefreshOnOk(false)
		, bExpandAdvancedSection(true)
		, bOpenAsMenu(false)
		, DisplayGamma(2.2f)
		, sRGBOverride()
		, ColorArray(nullptr)
		, LinearColorArray(nullptr)
		, ColorChannelsArray(nullptr)
		, OnColorCommitted()
		, PreColorCommitted()
		, OnColorPickerWindowClosed()
		, OnColorPickerCancelled()
		, OnInteractivePickBegin()
		, OnInteractivePickEnd()
		, InitialColorOverride()
	{ }
};


/** Open up the static color picker, destroying any previously existing one. */
APPFRAMEWORK_API bool OpenColorPicker(const FColorPickerArgs& Args);

/**
 * Destroy the current color picker. Necessary if the values the color picker
 * currently targets become invalid.
 */
APPFRAMEWORK_API void DestroyColorPicker();
