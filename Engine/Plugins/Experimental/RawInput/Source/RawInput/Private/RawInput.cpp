// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "RawInput.h"
#include "RawInputFunctionLibrary.h"
#include "IInputDeviceModule.h"
#include "IInputDevice.h"

#if PLATFORM_WINDOWS
	#include "Windows/RawInputWindows.h"
#endif

#define LOCTEXT_NAMESPACE "RawInputPlugin"

// Generic USB controller (Wheels, flight sticks etc. These require the rawinput plugin to be enabled)
const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Axis1( "GenericUSBController_Axis1" );
const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Axis2( "GenericUSBController_Axis2" );
const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Axis3( "GenericUSBController_Axis3" );
const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Axis4( "GenericUSBController_Axis4" );
const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Axis5( "GenericUSBController_Axis5" );
const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Axis6( "GenericUSBController_Axis6" );
const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Axis7( "GenericUSBController_Axis7" );
const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Axis8( "GenericUSBController_Axis8" );

const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Button1("GenericUSBController_Button1");
const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Button2("GenericUSBController_Button2");
const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Button3("GenericUSBController_Button3");
const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Button4("GenericUSBController_Button4");
const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Button5("GenericUSBController_Button5");
const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Button6("GenericUSBController_Button6");
const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Button7("GenericUSBController_Button7");
const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Button8("GenericUSBController_Button8");
const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Button9("GenericUSBController_Button9");
const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Button10("GenericUSBController_Button10");
const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Button11("GenericUSBController_Button11");
const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Button12("GenericUSBController_Button12");
const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Button13("GenericUSBController_Button13");
const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Button14("GenericUSBController_Button14");
const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Button15("GenericUSBController_Button15");
const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Button16("GenericUSBController_Button16");
const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Button17("GenericUSBController_Button17");
const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Button18("GenericUSBController_Button18");
const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Button19("GenericUSBController_Button19");
const FGamepadKeyNames::Type FRawInputKeyNames::GenericUSBController_Button20("GenericUSBController_Button20");

// USB controller (Wheels, flight stick etc)
const FKey FRawInputKeys::GenericUSBController_Axis1(FRawInputKeyNames::GenericUSBController_Axis1);
const FKey FRawInputKeys::GenericUSBController_Axis2(FRawInputKeyNames::GenericUSBController_Axis2);
const FKey FRawInputKeys::GenericUSBController_Axis3(FRawInputKeyNames::GenericUSBController_Axis3);
const FKey FRawInputKeys::GenericUSBController_Axis4(FRawInputKeyNames::GenericUSBController_Axis4);
const FKey FRawInputKeys::GenericUSBController_Axis5(FRawInputKeyNames::GenericUSBController_Axis5);
const FKey FRawInputKeys::GenericUSBController_Axis6(FRawInputKeyNames::GenericUSBController_Axis6);
const FKey FRawInputKeys::GenericUSBController_Axis7(FRawInputKeyNames::GenericUSBController_Axis7);
const FKey FRawInputKeys::GenericUSBController_Axis8(FRawInputKeyNames::GenericUSBController_Axis8);

const FKey FRawInputKeys::GenericUSBController_Button1(FRawInputKeyNames::GenericUSBController_Button1);
const FKey FRawInputKeys::GenericUSBController_Button2(FRawInputKeyNames::GenericUSBController_Button2);
const FKey FRawInputKeys::GenericUSBController_Button3(FRawInputKeyNames::GenericUSBController_Button3);
const FKey FRawInputKeys::GenericUSBController_Button4(FRawInputKeyNames::GenericUSBController_Button4);
const FKey FRawInputKeys::GenericUSBController_Button5(FRawInputKeyNames::GenericUSBController_Button5);
const FKey FRawInputKeys::GenericUSBController_Button6(FRawInputKeyNames::GenericUSBController_Button6);
const FKey FRawInputKeys::GenericUSBController_Button7(FRawInputKeyNames::GenericUSBController_Button7);
const FKey FRawInputKeys::GenericUSBController_Button8(FRawInputKeyNames::GenericUSBController_Button8);
const FKey FRawInputKeys::GenericUSBController_Button9(FRawInputKeyNames::GenericUSBController_Button9);
const FKey FRawInputKeys::GenericUSBController_Button10(FRawInputKeyNames::GenericUSBController_Button10);
const FKey FRawInputKeys::GenericUSBController_Button11(FRawInputKeyNames::GenericUSBController_Button11);
const FKey FRawInputKeys::GenericUSBController_Button12(FRawInputKeyNames::GenericUSBController_Button12);
const FKey FRawInputKeys::GenericUSBController_Button13(FRawInputKeyNames::GenericUSBController_Button13);
const FKey FRawInputKeys::GenericUSBController_Button14(FRawInputKeyNames::GenericUSBController_Button14);
const FKey FRawInputKeys::GenericUSBController_Button15(FRawInputKeyNames::GenericUSBController_Button15);
const FKey FRawInputKeys::GenericUSBController_Button16(FRawInputKeyNames::GenericUSBController_Button16);
const FKey FRawInputKeys::GenericUSBController_Button17(FRawInputKeyNames::GenericUSBController_Button17);
const FKey FRawInputKeys::GenericUSBController_Button18(FRawInputKeyNames::GenericUSBController_Button18);
const FKey FRawInputKeys::GenericUSBController_Button19(FRawInputKeyNames::GenericUSBController_Button19);
const FKey FRawInputKeys::GenericUSBController_Button20(FRawInputKeyNames::GenericUSBController_Button20);

IRawInput::IRawInput(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
	: MessageHandler(InMessageHandler)
	, LastAssignedInputHandle(0)
{
};

TSharedPtr< class IInputDevice > FRawInputPlugin::CreateInputDevice(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
{
	RawInputDevice = MakeShareable( new FPlatformRawInput(InMessageHandler) ); 
	return RawInputDevice;
}

void FRawInputPlugin::StartupModule()
{
	IInputDeviceModule::StartupModule();

	const FName NAME_GenericUSBController(TEXT("GenericUSBController"));

	// Generic USB Controllers (Wheel, Flightstick etc.)
	EKeys::AddMenuCategoryDisplayInfo(NAME_GenericUSBController, LOCTEXT("GenericUSBControllerSubCateogry", "GenericUSBController"), TEXT("GraphEditor.KeyEvent_16x"));

	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Axis1, LOCTEXT("GenericUSBController_Axis1", "GenericUSBController Axis 1"), FKeyDetails::GamepadKey, NAME_GenericUSBController));
	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Axis2, LOCTEXT("GenericUSBController_Axis2", "GenericUSBController Axis 2"), FKeyDetails::GamepadKey, NAME_GenericUSBController));
	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Axis3, LOCTEXT("GenericUSBController_Axis3", "GenericUSBController Axis 3"), FKeyDetails::GamepadKey, NAME_GenericUSBController));
	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Axis4, LOCTEXT("GenericUSBController_Axis4", "GenericUSBController Axis 4"), FKeyDetails::GamepadKey, NAME_GenericUSBController));
	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Axis5, LOCTEXT("GenericUSBController_Axis5", "GenericUSBController Axis 5"), FKeyDetails::GamepadKey, NAME_GenericUSBController));
	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Axis6, LOCTEXT("GenericUSBController_Axis6", "GenericUSBController Axis 6"), FKeyDetails::GamepadKey, NAME_GenericUSBController));
	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Axis7, LOCTEXT("GenericUSBController_Axis7", "GenericUSBController Axis 7"), FKeyDetails::GamepadKey, NAME_GenericUSBController));
	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Axis8, LOCTEXT("GenericUSBController_Axis8", "GenericUSBController Axis 8"), FKeyDetails::GamepadKey, NAME_GenericUSBController));

	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Button1, LOCTEXT("GenericUSBController_Button1", "GenericUSBController Button 1"), FKeyDetails::GamepadKey, NAME_GenericUSBController));
	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Button2, LOCTEXT("GenericUSBController_Button2", "GenericUSBController Button 2"), FKeyDetails::GamepadKey, NAME_GenericUSBController));
	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Button3, LOCTEXT("GenericUSBController_Button3", "GenericUSBController Button 3"), FKeyDetails::GamepadKey, NAME_GenericUSBController));
	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Button4, LOCTEXT("GenericUSBController_Button4", "GenericUSBController Button 4"), FKeyDetails::GamepadKey, NAME_GenericUSBController));
	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Button5, LOCTEXT("GenericUSBController_Button5", "GenericUSBController Button 5"), FKeyDetails::GamepadKey, NAME_GenericUSBController));
	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Button6, LOCTEXT("GenericUSBController_Button6", "GenericUSBController Button 6"), FKeyDetails::GamepadKey, NAME_GenericUSBController));
	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Button7, LOCTEXT("GenericUSBController_Button7", "GenericUSBController Button 7"), FKeyDetails::GamepadKey, NAME_GenericUSBController));
	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Button8, LOCTEXT("GenericUSBController_Button8", "GenericUSBController Button 8"), FKeyDetails::GamepadKey, NAME_GenericUSBController));
	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Button9, LOCTEXT("GenericUSBController_Button9", "GenericUSBController Button 9"), FKeyDetails::GamepadKey, NAME_GenericUSBController));
	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Button10, LOCTEXT("GenericUSBController_Button10", "GenericUSBController Button 10"), FKeyDetails::GamepadKey, NAME_GenericUSBController));
	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Button11, LOCTEXT("GenericUSBController_Button11", "GenericUSBController Button 11"), FKeyDetails::GamepadKey, NAME_GenericUSBController));
	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Button12, LOCTEXT("GenericUSBController_Button12", "GenericUSBController Button 12"), FKeyDetails::GamepadKey, NAME_GenericUSBController));
	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Button13, LOCTEXT("GenericUSBController_Button13", "GenericUSBController Button 13"), FKeyDetails::GamepadKey, NAME_GenericUSBController));
	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Button14, LOCTEXT("GenericUSBController_Button14", "GenericUSBController Button 14"), FKeyDetails::GamepadKey, NAME_GenericUSBController));
	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Button15, LOCTEXT("GenericUSBController_Button15", "GenericUSBController Button 15"), FKeyDetails::GamepadKey, NAME_GenericUSBController));
	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Button16, LOCTEXT("GenericUSBController_Button16", "GenericUSBController Button 16"), FKeyDetails::GamepadKey, NAME_GenericUSBController));
	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Button17, LOCTEXT("GenericUSBController_Button17", "GenericUSBController Button 17"), FKeyDetails::GamepadKey, NAME_GenericUSBController));
	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Button18, LOCTEXT("GenericUSBController_Button18", "GenericUSBController Button 18"), FKeyDetails::GamepadKey, NAME_GenericUSBController));
	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Button19, LOCTEXT("GenericUSBController_Button19", "GenericUSBController Button 19"), FKeyDetails::GamepadKey, NAME_GenericUSBController));
	EKeys::AddKey(FKeyDetails(FRawInputKeys::GenericUSBController_Button20, LOCTEXT("GenericUSBController_Button20", "GenericUSBController Button 20"), FKeyDetails::GamepadKey, NAME_GenericUSBController));
}

IMPLEMENT_MODULE( FRawInputPlugin, RawInput)

#undef LOCTEXT_NAMESPACE
