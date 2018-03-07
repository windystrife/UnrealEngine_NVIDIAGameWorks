// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "Widgets/SWidget.h"
#include "IDetailCustomization.h"
#include "TargetPlatformAudioCustomization.h"

class IDetailLayoutBuilder;
class IPropertyHandle;
enum class ECheckBoxState : uint8;

/* FTargetShaderFormatsPropertyDetails
*****************************************************************************/

/**
* Helper which implements details panel customizations for a device profiles parent property
*/
class FTargetShaderFormatsPropertyDetails
	: public TSharedFromThis<FTargetShaderFormatsPropertyDetails>
{

public:

	/**
	 * Constructor for the parent property details view
	 *
	 * @param InDetailsBuilder - Where we are adding our property view to
	 */
	FTargetShaderFormatsPropertyDetails(IDetailLayoutBuilder* InDetailBuilder);


	/**
	 * Create the UI to select which windows shader formats we are targetting
	 */
	void CreateTargetShaderFormatsPropertyView();

private:

	// Supported/Targeted RHI check boxes
	void OnTargetedRHIChanged(ECheckBoxState InNewValue, FName InRHIName);
	ECheckBoxState IsTargetedRHIChecked(FName InRHIName) const;

private:

	/** A handle to the detail view builder */
	IDetailLayoutBuilder* DetailBuilder;

	/** Access to the Parent Property */
	TSharedPtr<IPropertyHandle> TargetShaderFormatsPropertyHandle;
};


/**
 * Manages the Transform section of a details view                    
 */
class FWindowsTargetSettingsDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

private:
	/** Delegate handler for before an icon is copied */
	bool HandlePreExternalIconCopy(const FString& InChosenImage);

	/** Delegate handler to get the path to start picking from */
	FString GetPickerPath();

	/** Delegate handler to set the path to start picking from */
	bool HandlePostExternalIconCopy(const FString& InChosenImage);

	/** Handles when a new audio device is selected from list of available audio devices. */
	void HandleAudioDeviceSelected(FString AudioDeviceName, TSharedPtr<IPropertyHandle> PropertyHandle);

	/** Handles changing the foreground color of the audio device box. */
	FSlateColor HandleAudioDeviceBoxForegroundColor(TSharedPtr<IPropertyHandle> PropertyHandle) const;

	/** Handles getting text of the audio device list text block. */
	FText HandleAudioDeviceTextBoxText(TSharedPtr<IPropertyHandle> PropertyHandle) const;

	/** Handles text changes in the audio device list text block. */
	void HandleAudioDeviceTextBoxTextChanged(const FText& InText, TSharedPtr<IPropertyHandle> PropertyHandle);

	/** Handles committing changes in the audio list text block. */
	void HandleAudioDeviceTextBoxTextComitted(const FText& InText, ETextCommit::Type CommitType, TSharedPtr<IPropertyHandle> PropertyHandle);

protected:

	/** Checks if the device name is valid. */
	bool IsValidAudioDeviceName(const FString& InDeviceName) const;

	/** Creates a widget for the audio device picker. */
	TSharedRef<SWidget> MakeAudioDeviceMenu(const TSharedPtr<IPropertyHandle>& PropertyHandle);

private:
	/** Reference to the target shader formats property view */
	TSharedPtr<FTargetShaderFormatsPropertyDetails> TargetShaderFormatsDetails;
	FAudioPluginWidgetManager AudioPluginWidgetManager;
};
