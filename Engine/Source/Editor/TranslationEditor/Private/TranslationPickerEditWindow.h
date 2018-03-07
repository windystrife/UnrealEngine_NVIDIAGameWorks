// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "TranslationPickerEditWindow.generated.h"

#define LOCTEXT_NAMESPACE "TranslationPicker"

class SBox;
class SMultiLineEditableTextBox;
class SWindow;
class UTranslationUnit;

UCLASS(config = TranslationPickerSettings)
class UTranslationPickerSettings : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	/** Bool submit translation picker changes to Localization Service */
	UPROPERTY(config)
	bool bSubmitTranslationPickerChangesToLocalizationService;
};

class FTranslationPickerSettingsManager
{
	FTranslationPickerSettingsManager()
	{
		TranslationPickerSettingsObject = NewObject<UTranslationPickerSettings>();
		TranslationPickerSettingsObject->LoadConfig();
	}

public:
	void SaveSettings()
	{
		TranslationPickerSettingsObject->SaveConfig();
	}

	void LoadSettings()
	{
		TranslationPickerSettingsObject->LoadConfig();
	}

	UTranslationPickerSettings* GetSettings()
	{
		return TranslationPickerSettingsObject;
	}

	/**
	* Gets a reference to the translation picker manager instance.
	*
	* @return A reference to the translation picker manager instance.
	*/
	static inline TSharedPtr<FTranslationPickerSettingsManager>& Get()
	{
		if (!TranslationPickerSettingsManagerInstance.IsValid())
		{
			TranslationPickerSettingsManagerInstance = MakeShareable(new FTranslationPickerSettingsManager());
		}

		return TranslationPickerSettingsManagerInstance;
	}

private:

	static TSharedPtr<FTranslationPickerSettingsManager> TranslationPickerSettingsManagerInstance;

	/** Used to load and store settings for the Translation Picker */
	UTranslationPickerSettings* TranslationPickerSettingsObject;
};

/** Translation picker edit Widget to handle the display and editing of a single selected FText */
class STranslationPickerEditWidget : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(STranslationPickerEditWidget) {}

	SLATE_ARGUMENT(FText, PickedText)

	SLATE_ARGUMENT(bool, bAllowEditing)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Return the translation unit for this text, with any modifications */
	UTranslationUnit* GetTranslationUnitWithAnyChanges();

	/** Whether or not we can save */
	bool CanSave()
	{
		return bAllowEditing && bHasRequiredLocalizationInfoForSaving;
	}

private:

	virtual bool SupportsKeyboardFocus() const override
	{
		return true;
	}

	FReply SaveAndPreview();

	/** The FText that we are using this widget to translate */
	FText PickedText;

	/** The translation we're editing represented in a UTranslationUnit object */
	UTranslationUnit* TranslationUnit;

	/** The text box for entering/modifying a translation */
	TSharedPtr<SMultiLineEditableTextBox> TextBox;

	/** Whether or not to show the save button*/
	bool bAllowEditing;

	/** Whether or not we were able to find the necessary info for saving */
	bool bHasRequiredLocalizationInfoForSaving;
};

/** Translation picker edit window to allow you to translate selected FTexts in place */
class STranslationPickerEditWindow : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(STranslationPickerEditWindow) {}

	SLATE_ARGUMENT(TWeakPtr<SWindow>, ParentWindow)

	SLATE_ARGUMENT(TArray<FText>, PickedTexts)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// Default dimensions of the Translation Picker edit window (floating window also uses these sizes, so it matches roughly)
	static const int32 DefaultEditWindowWidth;
	static const int32 DefaultEditWindowHeight;

private:

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	FReply Close();

	/** We need to support keyboard focus to process the 'Esc' key */
	virtual bool SupportsKeyboardFocus() const override
	{
		return true;
	}

	/** Handle to the window that contains this widget */
	TWeakPtr<SWindow> ParentWindow;

	/** Contents of the window */
	TSharedPtr<SBox> WindowContents;

	/** The FTexts that we have found under the cursor */
	TArray<FText> PickedTexts;

	/** All of our current edit widgets */
	TArray<TSharedRef<STranslationPickerEditWidget>> EditWidgets;

	/** Save all translations and close */
	FReply SaveAllAndClose();


};

#undef LOCTEXT_NAMESPACE
