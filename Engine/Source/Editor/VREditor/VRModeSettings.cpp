// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VRModeSettings.h"
#include "Dialogs.h"
#include "UnrealType.h"
#include "HAL/IConsoleManager.h"

#define LOCTEXT_NAMESPACE "VREditor"

namespace VREd
{
	static FAutoConsoleVariable SettingsTriggerPressedThreshold_Vive(TEXT("VREd.SettingsTriggerPressedThreshold_Vive"), 0.33f, TEXT("The amount (between 0-1) you have to depress the Vive controller trigger to register a press"));
	static FAutoConsoleVariable SettingsTriggerPressedThreshold_Rift(TEXT("VREd.SettingsTriggerPressedThreshold_Rift"), 0.5f, TEXT("The amount (between 0-1) you have to depress the Oculus Touch controller trigger to register a press "));
}

UVRModeSettings::UVRModeSettings()
	: Super()
{
	bEnableAutoVREditMode = false;
	InteractorHand = EInteractorHand::Right;
	bShowWorldMovementGrid = true;
	bShowWorldMovementPostProcess = true;
	bShowWorldScaleProgressBar = true;
	UIBrightness = 1.5f;
	GizmoScale = 0.8f;
	DoubleClickTime = 0.25f;
	TriggerPressedThreshold_Vive = VREd::SettingsTriggerPressedThreshold_Vive->GetFloat();
	TriggerPressedThreshold_Rift = VREd::SettingsTriggerPressedThreshold_Rift->GetFloat();
}

#if WITH_EDITOR
void UVRModeSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property
		&& PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UVRModeSettings, bEnableAutoVREditMode)
		&& bEnableAutoVREditMode == true)
	{
		FSuppressableWarningDialog::FSetupInfo SetupInfo(LOCTEXT("VRModeEntry_Message", "VR Mode enables you to work on your project in virtual reality using motion controllers. This feature is still under development, so you may experience bugs or crashes while using it."),
			LOCTEXT("VRModeEntry_Title", "Entering VR Mode - Experimental"), "Warning_VRModeEntry", GEditorSettingsIni);

		SetupInfo.ConfirmText = LOCTEXT("VRModeEntry_ConfirmText", "Continue");
		SetupInfo.CancelText = LOCTEXT("VRModeEntry_CancelText", "Cancel");
		SetupInfo.bDefaultToSuppressInTheFuture = true;
		FSuppressableWarningDialog VRModeEntryWarning(SetupInfo);
		bEnableAutoVREditMode = (VRModeEntryWarning.ShowModal() != FSuppressableWarningDialog::Cancel) ? true : false;
	}
}
#endif

#undef LOCTEXT_NAMESPACE