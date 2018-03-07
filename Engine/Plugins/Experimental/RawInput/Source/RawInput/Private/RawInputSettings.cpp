// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "RawInputSettings.h"
#include "RawInputFunctionLibrary.h"

#if PLATFORM_WINDOWS
	#include "Windows/RawInputWindows.h"
#endif

FRawInputDeviceConfiguration::FRawInputDeviceConfiguration()
{
	ButtonProperties.AddDefaulted(MAX_NUM_CONTROLLER_BUTTONS);
	AxisProperties.AddDefaulted(MAX_NUM_CONTROLLER_ANALOG);

	ButtonProperties[0].Key = FRawInputKeys::GenericUSBController_Button1;
	ButtonProperties[1].Key = FRawInputKeys::GenericUSBController_Button2;
	ButtonProperties[2].Key = FRawInputKeys::GenericUSBController_Button3;
	ButtonProperties[3].Key = FRawInputKeys::GenericUSBController_Button4;
	ButtonProperties[4].Key = FRawInputKeys::GenericUSBController_Button5;
	ButtonProperties[5].Key = FRawInputKeys::GenericUSBController_Button6;
	ButtonProperties[6].Key = FRawInputKeys::GenericUSBController_Button7;
	ButtonProperties[7].Key = FRawInputKeys::GenericUSBController_Button8;
	ButtonProperties[8].Key = FRawInputKeys::GenericUSBController_Button9;
	ButtonProperties[9].Key = FRawInputKeys::GenericUSBController_Button10;
	ButtonProperties[10].Key = FRawInputKeys::GenericUSBController_Button11;
	ButtonProperties[11].Key = FRawInputKeys::GenericUSBController_Button12;
	ButtonProperties[12].Key = FRawInputKeys::GenericUSBController_Button13;
	ButtonProperties[13].Key = FRawInputKeys::GenericUSBController_Button14;
	ButtonProperties[14].Key = FRawInputKeys::GenericUSBController_Button15;
	ButtonProperties[15].Key = FRawInputKeys::GenericUSBController_Button16;
	ButtonProperties[16].Key = FRawInputKeys::GenericUSBController_Button17;
	ButtonProperties[17].Key = FRawInputKeys::GenericUSBController_Button18;
	ButtonProperties[18].Key = FRawInputKeys::GenericUSBController_Button19;
	ButtonProperties[19].Key = FRawInputKeys::GenericUSBController_Button20;

	AxisProperties[0].Key = FRawInputKeys::GenericUSBController_Axis1;
	AxisProperties[1].Key = FRawInputKeys::GenericUSBController_Axis2;
	AxisProperties[2].Key = FRawInputKeys::GenericUSBController_Axis3;
	AxisProperties[3].Key = FRawInputKeys::GenericUSBController_Axis4;
	AxisProperties[4].Key = FRawInputKeys::GenericUSBController_Axis5;
	AxisProperties[5].Key = FRawInputKeys::GenericUSBController_Axis6;
	AxisProperties[6].Key = FRawInputKeys::GenericUSBController_Axis7;
	AxisProperties[7].Key = FRawInputKeys::GenericUSBController_Axis8;
}

FName URawInputSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

#if WITH_EDITOR
FText URawInputSettings::GetSectionText() const
{
	return NSLOCTEXT("RawInputPlugin", "RawInputSettingsSection", "Raw Input");
}

void URawInputSettings::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
	
#if PLATFORM_WINDOWS
	FRawInputWindows* RawInput = static_cast<FRawInputWindows*>(static_cast<FRawInputPlugin*>(&FRawInputPlugin::Get())->GetRawInputDevice().Get());

	for (const TPair<int32, FRawWindowsDeviceEntry>& RegisteredDevice : RawInput->RegisteredDeviceList)
	{
		const FRawWindowsDeviceEntry& DeviceEntry = RegisteredDevice.Value;
		if (DeviceEntry.bIsConnected)
		{
			RawInput->SetupBindings(RegisteredDevice.Key, false);
		}
	}

#endif
}
#endif