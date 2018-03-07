// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "InputCoreTypes.h"
#include "UObject/UnrealType.h"
#include "UObject/PropertyPortFlags.h"
#include "HAL/PlatformInput.h"

DEFINE_LOG_CATEGORY(LogInput);

#define LOCTEXT_NAMESPACE "InputKeys"

const FKey EKeys::AnyKey("AnyKey");

const FKey EKeys::MouseX("MouseX");
const FKey EKeys::MouseY("MouseY");
const FKey EKeys::MouseScrollUp("MouseScrollUp");
const FKey EKeys::MouseScrollDown("MouseScrollDown");
const FKey EKeys::MouseWheelAxis("MouseWheelAxis");

const FKey EKeys::LeftMouseButton("LeftMouseButton");
const FKey EKeys::RightMouseButton("RightMouseButton");
const FKey EKeys::MiddleMouseButton("MiddleMouseButton");
const FKey EKeys::ThumbMouseButton("ThumbMouseButton");
const FKey EKeys::ThumbMouseButton2("ThumbMouseButton2");

const FKey EKeys::BackSpace("BackSpace");
const FKey EKeys::Tab("Tab");
const FKey EKeys::Enter("Enter");
const FKey EKeys::Pause("Pause");

const FKey EKeys::CapsLock("CapsLock");
const FKey EKeys::Escape("Escape");
const FKey EKeys::SpaceBar("SpaceBar");
const FKey EKeys::PageUp("PageUp");
const FKey EKeys::PageDown("PageDown");
const FKey EKeys::End("End");
const FKey EKeys::Home("Home");

const FKey EKeys::Left("Left");
const FKey EKeys::Up("Up");
const FKey EKeys::Right("Right");
const FKey EKeys::Down("Down");

const FKey EKeys::Insert("Insert");
const FKey EKeys::Delete("Delete");

const FKey EKeys::Zero("Zero");
const FKey EKeys::One("One");
const FKey EKeys::Two("Two");
const FKey EKeys::Three("Three");
const FKey EKeys::Four("Four");
const FKey EKeys::Five("Five");
const FKey EKeys::Six("Six");
const FKey EKeys::Seven("Seven");
const FKey EKeys::Eight("Eight");
const FKey EKeys::Nine("Nine");

const FKey EKeys::A("A");
const FKey EKeys::B("B");
const FKey EKeys::C("C");
const FKey EKeys::D("D");
const FKey EKeys::E("E");
const FKey EKeys::F("F");
const FKey EKeys::G("G");
const FKey EKeys::H("H");
const FKey EKeys::I("I");
const FKey EKeys::J("J");
const FKey EKeys::K("K");
const FKey EKeys::L("L");
const FKey EKeys::M("M");
const FKey EKeys::N("N");
const FKey EKeys::O("O");
const FKey EKeys::P("P");
const FKey EKeys::Q("Q");
const FKey EKeys::R("R");
const FKey EKeys::S("S");
const FKey EKeys::T("T");
const FKey EKeys::U("U");
const FKey EKeys::V("V");
const FKey EKeys::W("W");
const FKey EKeys::X("X");
const FKey EKeys::Y("Y");
const FKey EKeys::Z("Z");

const FKey EKeys::NumPadZero("NumPadZero");
const FKey EKeys::NumPadOne("NumPadOne");
const FKey EKeys::NumPadTwo("NumPadTwo");
const FKey EKeys::NumPadThree("NumPadThree");
const FKey EKeys::NumPadFour("NumPadFour");
const FKey EKeys::NumPadFive("NumPadFive");
const FKey EKeys::NumPadSix("NumPadSix");
const FKey EKeys::NumPadSeven("NumPadSeven");
const FKey EKeys::NumPadEight("NumPadEight");
const FKey EKeys::NumPadNine("NumPadNine");

const FKey EKeys::Multiply("Multiply");
const FKey EKeys::Add("Add");
const FKey EKeys::Subtract("Subtract");
const FKey EKeys::Decimal("Decimal");
const FKey EKeys::Divide("Divide");

const FKey EKeys::F1("F1");
const FKey EKeys::F2("F2");
const FKey EKeys::F3("F3");
const FKey EKeys::F4("F4");
const FKey EKeys::F5("F5");
const FKey EKeys::F6("F6");
const FKey EKeys::F7("F7");
const FKey EKeys::F8("F8");
const FKey EKeys::F9("F9");
const FKey EKeys::F10("F10");
const FKey EKeys::F11("F11");
const FKey EKeys::F12("F12");

const FKey EKeys::NumLock("NumLock");

const FKey EKeys::ScrollLock("ScrollLock");

const FKey EKeys::LeftShift("LeftShift");
const FKey EKeys::RightShift("RightShift");
const FKey EKeys::LeftControl("LeftControl");
const FKey EKeys::RightControl("RightControl");
const FKey EKeys::LeftAlt("LeftAlt");
const FKey EKeys::RightAlt("RightAlt");
const FKey EKeys::LeftCommand("LeftCommand");
const FKey EKeys::RightCommand("RightCommand");

const FKey EKeys::Semicolon("Semicolon");
const FKey EKeys::Equals("Equals");
const FKey EKeys::Comma("Comma");
const FKey EKeys::Underscore("Underscore");
const FKey EKeys::Hyphen("Hyphen");
const FKey EKeys::Period("Period");
const FKey EKeys::Slash("Slash");
const FKey EKeys::Tilde("Tilde");
const FKey EKeys::LeftBracket("LeftBracket");
const FKey EKeys::LeftParantheses("LeftParantheses");
const FKey EKeys::Backslash("Backslash");
const FKey EKeys::RightBracket("RightBracket");
const FKey EKeys::RightParantheses("RightParantheses");
const FKey EKeys::Apostrophe("Apostrophe");
const FKey EKeys::Quote("Quote");

const FKey EKeys::Asterix("Asterix");
const FKey EKeys::Ampersand("Ampersand");
const FKey EKeys::Caret("Caret");
const FKey EKeys::Dollar("Dollar");
const FKey EKeys::Exclamation("Exclamation");
const FKey EKeys::Colon("Colon");

const FKey EKeys::A_AccentGrave("A_AccentGrave");
const FKey EKeys::E_AccentGrave("E_AccentGrave");
const FKey EKeys::E_AccentAigu("E_AccentAigu");
const FKey EKeys::C_Cedille("C_Cedille");
const FKey EKeys::Section("Section");


// Setup platform specific keys
#if PLATFORM_MAC
const FKey EKeys::Platform_Delete = EKeys::BackSpace;
#else
const FKey EKeys::Platform_Delete = EKeys::Delete;
#endif

// Ensure that the Gamepad_ names match those in GenericApplication.cpp
const FKey EKeys::Gamepad_LeftX("Gamepad_LeftX");
const FKey EKeys::Gamepad_LeftY("Gamepad_LeftY");
const FKey EKeys::Gamepad_RightX("Gamepad_RightX");
const FKey EKeys::Gamepad_RightY("Gamepad_RightY");
const FKey EKeys::Gamepad_LeftTriggerAxis("Gamepad_LeftTriggerAxis");
const FKey EKeys::Gamepad_RightTriggerAxis("Gamepad_RightTriggerAxis");

const FKey EKeys::Gamepad_LeftThumbstick("Gamepad_LeftThumbstick");
const FKey EKeys::Gamepad_RightThumbstick("Gamepad_RightThumbstick");
const FKey EKeys::Gamepad_Special_Left("Gamepad_Special_Left");
const FKey EKeys::Gamepad_Special_Left_X("Gamepad_Special_Left_X");
const FKey EKeys::Gamepad_Special_Left_Y("Gamepad_Special_Left_Y");
const FKey EKeys::Gamepad_Special_Right("Gamepad_Special_Right");
const FKey EKeys::Gamepad_FaceButton_Bottom("Gamepad_FaceButton_Bottom");
const FKey EKeys::Gamepad_FaceButton_Right("Gamepad_FaceButton_Right");
const FKey EKeys::Gamepad_FaceButton_Left("Gamepad_FaceButton_Left");
const FKey EKeys::Gamepad_FaceButton_Top("Gamepad_FaceButton_Top");
const FKey EKeys::Gamepad_LeftShoulder("Gamepad_LeftShoulder");
const FKey EKeys::Gamepad_RightShoulder("Gamepad_RightShoulder");
const FKey EKeys::Gamepad_LeftTrigger("Gamepad_LeftTrigger");
const FKey EKeys::Gamepad_RightTrigger("Gamepad_RightTrigger");
const FKey EKeys::Gamepad_DPad_Up("Gamepad_DPad_Up");
const FKey EKeys::Gamepad_DPad_Down("Gamepad_DPad_Down");
const FKey EKeys::Gamepad_DPad_Right("Gamepad_DPad_Right");
const FKey EKeys::Gamepad_DPad_Left("Gamepad_DPad_Left");

// Virtual key codes used for input axis button press/release emulation
const FKey EKeys::Gamepad_LeftStick_Up("Gamepad_LeftStick_Up");
const FKey EKeys::Gamepad_LeftStick_Down("Gamepad_LeftStick_Down");
const FKey EKeys::Gamepad_LeftStick_Right("Gamepad_LeftStick_Right");
const FKey EKeys::Gamepad_LeftStick_Left("Gamepad_LeftStick_Left");

const FKey EKeys::Gamepad_RightStick_Up("Gamepad_RightStick_Up");
const FKey EKeys::Gamepad_RightStick_Down("Gamepad_RightStick_Down");
const FKey EKeys::Gamepad_RightStick_Right("Gamepad_RightStick_Right");
const FKey EKeys::Gamepad_RightStick_Left("Gamepad_RightStick_Left");

// const FKey EKeys::Vector axes (FVector("Vector axes (FVector"); not float)
const FKey EKeys::Tilt("Tilt");
const FKey EKeys::RotationRate("RotationRate");
const FKey EKeys::Gravity("Gravity");
const FKey EKeys::Acceleration("Acceleration");

// Fingers
const FKey EKeys::TouchKeys[NUM_TOUCH_KEYS];

static struct FKeyInitializer
{
	FKeyInitializer()
	{
		for (int TouchIndex = 0; TouchIndex < (EKeys::NUM_TOUCH_KEYS - 1); TouchIndex++)
		{
			const_cast<FKey&>(EKeys::TouchKeys[TouchIndex]) = FKey(*FString::Printf(TEXT("Touch%d"), TouchIndex + 1));
		}
	}

} KeyInitializer;

// Gestures
const FKey EKeys::Gesture_Pinch("Gesture_Pinch");
const FKey EKeys::Gesture_Flick("Gesture_Flick");
const FKey EKeys::Gesture_Rotate("Gesture_Rotate");

// Motion Controllers
//		Left Controller
const FKey EKeys::MotionController_Left_FaceButton1("MotionController_Left_FaceButton1");
const FKey EKeys::MotionController_Left_FaceButton2("MotionController_Left_FaceButton2");
const FKey EKeys::MotionController_Left_FaceButton3("MotionController_Left_FaceButton3");
const FKey EKeys::MotionController_Left_FaceButton4("MotionController_Left_FaceButton4");
const FKey EKeys::MotionController_Left_FaceButton5("MotionController_Left_FaceButton5");
const FKey EKeys::MotionController_Left_FaceButton6("MotionController_Left_FaceButton6");
const FKey EKeys::MotionController_Left_FaceButton7("MotionController_Left_FaceButton7");
const FKey EKeys::MotionController_Left_FaceButton8("MotionController_Left_FaceButton8");

const FKey EKeys::MotionController_Left_Shoulder("MotionController_Left_Shoulder");
const FKey EKeys::MotionController_Left_Trigger("MotionController_Left_Trigger");

const FKey EKeys::MotionController_Left_Grip1("MotionController_Left_Grip1");
const FKey EKeys::MotionController_Left_Grip2("MotionController_Left_Grip2");

const FKey EKeys::MotionController_Left_Thumbstick("MotionController_Left_Thumbstick");
const FKey EKeys::MotionController_Left_Thumbstick_Up("MotionController_Left_Thumbstick_Up");
const FKey EKeys::MotionController_Left_Thumbstick_Down("MotionController_Left_Thumbstick_Down");
const FKey EKeys::MotionController_Left_Thumbstick_Left("MotionController_Left_Thumbstick_Left");
const FKey EKeys::MotionController_Left_Thumbstick_Right("MotionController_Left_Thumbstick_Right");

//		Right Controller
const FKey EKeys::MotionController_Right_FaceButton1("MotionController_Right_FaceButton1");
const FKey EKeys::MotionController_Right_FaceButton2("MotionController_Right_FaceButton2");
const FKey EKeys::MotionController_Right_FaceButton3("MotionController_Right_FaceButton3");
const FKey EKeys::MotionController_Right_FaceButton4("MotionController_Right_FaceButton4");
const FKey EKeys::MotionController_Right_FaceButton5("MotionController_Right_FaceButton5");
const FKey EKeys::MotionController_Right_FaceButton6("MotionController_Right_FaceButton6");
const FKey EKeys::MotionController_Right_FaceButton7("MotionController_Right_FaceButton7");
const FKey EKeys::MotionController_Right_FaceButton8("MotionController_Right_FaceButton8");

const FKey EKeys::MotionController_Right_Shoulder("MotionController_Right_Shoulder");
const FKey EKeys::MotionController_Right_Trigger("MotionController_Right_Trigger");

const FKey EKeys::MotionController_Right_Grip1("MotionController_Right_Grip1");
const FKey EKeys::MotionController_Right_Grip2("MotionController_Right_Grip2");

const FKey EKeys::MotionController_Right_Thumbstick("MotionController_Right_Thumbstick");
const FKey EKeys::MotionController_Right_Thumbstick_Up("MotionController_Right_Thumbstick_Up");
const FKey EKeys::MotionController_Right_Thumbstick_Down("MotionController_Right_Thumbstick_Down");
const FKey EKeys::MotionController_Right_Thumbstick_Left("MotionController_Right_Thumbstick_Left");
const FKey EKeys::MotionController_Right_Thumbstick_Right("MotionController_Right_Thumbstick_Right");

//   Motion Controller Axes
//		Left Controller
const FKey EKeys::MotionController_Left_Thumbstick_X("MotionController_Left_Thumbstick_X");
const FKey EKeys::MotionController_Left_Thumbstick_Y("MotionController_Left_Thumbstick_Y");
const FKey EKeys::MotionController_Left_TriggerAxis("MotionController_Left_TriggerAxis");
const FKey EKeys::MotionController_Left_Grip1Axis( "MotionController_Left_Grip1Axis" );
const FKey EKeys::MotionController_Left_Grip2Axis( "MotionController_Left_Grip2Axis" );

//		Right Controller
const FKey EKeys::MotionController_Right_Thumbstick_X("MotionController_Right_Thumbstick_X");
const FKey EKeys::MotionController_Right_Thumbstick_Y("MotionController_Right_Thumbstick_Y");
const FKey EKeys::MotionController_Right_TriggerAxis("MotionController_Right_TriggerAxis");
const FKey EKeys::MotionController_Right_Grip1Axis( "MotionController_Right_Grip1Axis" );
const FKey EKeys::MotionController_Right_Grip2Axis( "MotionController_Right_Grip2Axis" );

// PS4-specific
const FKey EKeys::PS4_Special("PS4_Special");

// Steam Controller Specific
const FKey EKeys::Steam_Touch_0("Steam_Touch_0");
const FKey EKeys::Steam_Touch_1("Steam_Touch_1");
const FKey EKeys::Steam_Touch_2("Steam_Touch_2");
const FKey EKeys::Steam_Touch_3("Steam_Touch_3");
const FKey EKeys::Steam_Back_Left("Steam_Back_Left");
const FKey EKeys::Steam_Back_Right("Steam_Back_Right");

// Xbox One global speech commands
const FKey EKeys::Global_Menu("Global_Menu");
const FKey EKeys::Global_View("Global_View");
const FKey EKeys::Global_Pause("Global_Pause");
const FKey EKeys::Global_Play("Global_Play");
const FKey EKeys::Global_Back("Global_Back");

// Android-specific
const FKey EKeys::Android_Back("Android_Back");
const FKey EKeys::Android_Volume_Up("Android_Volume_Up");
const FKey EKeys::Android_Volume_Down("Android_Volume_Down");
const FKey EKeys::Android_Menu("Android_Menu");

const FKey EKeys::Invalid(NAME_None);

const FName EKeys::NAME_GamepadCategory("Gamepad");
const FName EKeys::NAME_MouseCategory("Mouse");
const FName EKeys::NAME_KeyboardCategory("Key");

#if PLATFORM_USES_FACE_BUTTON_RIGHT_FOR_ACCEPT
const FKey EKeys::Virtual_Accept = EKeys::Gamepad_FaceButton_Right;
const FKey EKeys::Virtual_Back = EKeys::Gamepad_FaceButton_Bottom;
#else
const FKey EKeys::Virtual_Accept = EKeys::Gamepad_FaceButton_Bottom;
const FKey EKeys::Virtual_Back = EKeys::Gamepad_FaceButton_Right;
#endif

bool EKeys::bInitialized = false;
TMap<FKey, TSharedPtr<FKeyDetails> > EKeys::InputKeys;
TMap<FName, EKeys::FCategoryDisplayInfo> EKeys::MenuCategoryDisplayInfo;

FKeyDetails::FKeyDetails(const FKey InKey, const TAttribute<FText>& InDisplayName, const uint8 InKeyFlags, const FName InMenuCategory)
	: Key(InKey)
	, DisplayName(InDisplayName)
	, MenuCategory(InMenuCategory)
	, bIsModifierKey((InKeyFlags & EKeyFlags::ModifierKey) != 0)
	, bIsGamepadKey((InKeyFlags & EKeyFlags::GamepadKey) != 0)
	, bIsMouseButton((InKeyFlags & EKeyFlags::MouseButton) != 0)
	, bIsBindableInBlueprints((~InKeyFlags & EKeyFlags::NotBlueprintBindableKey) != 0)
	, bShouldUpdateAxisWithoutSamples((InKeyFlags & EKeyFlags::UpdateAxisWithoutSamples) != 0)
	, AxisType(EInputAxisType::None)
{
	if ((InKeyFlags & EKeyFlags::FloatAxis) != 0)
	{
		ensure((InKeyFlags & EKeyFlags::VectorAxis) == 0);
		AxisType = EInputAxisType::Float;
	}
	else if ((InKeyFlags & EKeyFlags::VectorAxis) != 0)
	{
		AxisType = EInputAxisType::Vector;
	}

	// Set up default menu categories
	if (MenuCategory.IsNone())
	{
		if (IsGamepadKey())
		{
			MenuCategory = EKeys::NAME_GamepadCategory;
		}
		else if (IsMouseButton())
		{
			MenuCategory = EKeys::NAME_MouseCategory;
		}
		else
		{
			MenuCategory = EKeys::NAME_KeyboardCategory;
		}
	}
}

PRAGMA_DISABLE_OPTIMIZATION
void EKeys::Initialize()
{
	if (bInitialized) return;
	bInitialized = true;

	AddMenuCategoryDisplayInfo(NAME_GamepadCategory, LOCTEXT("GamepadSubCategory", "Gamepad"), TEXT("GraphEditor.PadEvent_16x"));
	AddMenuCategoryDisplayInfo(NAME_MouseCategory, LOCTEXT("MouseSubCategory", "Mouse"), TEXT("GraphEditor.MouseEvent_16x"));
	AddMenuCategoryDisplayInfo(NAME_KeyboardCategory, LOCTEXT("KeyboardSubCategory", "Keyboard"), TEXT("GraphEditor.KeyEvent_16x"));

	AddKey(FKeyDetails(EKeys::AnyKey, LOCTEXT("AnyKey", "Any Key")));

	AddKey(FKeyDetails(EKeys::MouseX, LOCTEXT("MouseX", "Mouse X"), FKeyDetails::FloatAxis | FKeyDetails::MouseButton | FKeyDetails::UpdateAxisWithoutSamples));
	AddKey(FKeyDetails(EKeys::MouseY, LOCTEXT("MouseY", "Mouse Y"), FKeyDetails::FloatAxis | FKeyDetails::MouseButton | FKeyDetails::UpdateAxisWithoutSamples));
	AddKey(FKeyDetails(EKeys::MouseWheelAxis, LOCTEXT("MouseWheelAxis", "Mouse Wheel Axis"), FKeyDetails::FloatAxis | FKeyDetails::MouseButton | FKeyDetails::UpdateAxisWithoutSamples));
	AddKey(FKeyDetails(EKeys::MouseScrollUp, LOCTEXT("MouseScrollUp", "Mouse Wheel Up"), FKeyDetails::MouseButton));
	AddKey(FKeyDetails(EKeys::MouseScrollDown, LOCTEXT("MouseScrollDown", "Mouse Wheel Down"), FKeyDetails::MouseButton));
	
	AddKey(FKeyDetails(EKeys::LeftMouseButton, LOCTEXT("LeftMouseButton", "Left Mouse Button"), FKeyDetails::MouseButton));
	AddKey(FKeyDetails(EKeys::RightMouseButton, LOCTEXT("RightMouseButton", "Right Mouse Button"), FKeyDetails::MouseButton));
	AddKey(FKeyDetails(EKeys::MiddleMouseButton, LOCTEXT("MiddleMouseButton", "Middle Mouse Button"), FKeyDetails::MouseButton));
	AddKey(FKeyDetails(EKeys::ThumbMouseButton, LOCTEXT("ThumbMouseButton", "Thumb Mouse Button"), FKeyDetails::MouseButton));
	AddKey(FKeyDetails(EKeys::ThumbMouseButton2, LOCTEXT("ThumbMouseButton2", "Thumb Mouse Button 2"), FKeyDetails::MouseButton));
    
	AddKey(FKeyDetails(EKeys::Tab, LOCTEXT("Tab", "Tab")));
	AddKey(FKeyDetails(EKeys::Enter, LOCTEXT("Enter", "Enter")));
	AddKey(FKeyDetails(EKeys::Pause, LOCTEXT("Pause", "Pause")));

	AddKey(FKeyDetails(EKeys::CapsLock, LOCTEXT("CapsLock", "Caps Lock")));
	AddKey(FKeyDetails(EKeys::Escape, LOCTEXT("Escape", "Escape")));
	AddKey(FKeyDetails(EKeys::SpaceBar, LOCTEXT("SpaceBar", "Space Bar")));
	AddKey(FKeyDetails(EKeys::PageUp, LOCTEXT("PageUp", "Page Up")));
	AddKey(FKeyDetails(EKeys::PageDown, LOCTEXT("PageDown", "Page Down")));
	AddKey(FKeyDetails(EKeys::End, LOCTEXT("End", "End")));
	AddKey(FKeyDetails(EKeys::Home, LOCTEXT("Home", "Home")));

	AddKey(FKeyDetails(EKeys::Left, LOCTEXT("Left", "Left")));
	AddKey(FKeyDetails(EKeys::Up, LOCTEXT("Up", "Up")));
	AddKey(FKeyDetails(EKeys::Right, LOCTEXT("Right", "Right")));
	AddKey(FKeyDetails(EKeys::Down, LOCTEXT("Down", "Down")));

	AddKey(FKeyDetails(EKeys::Insert, LOCTEXT("Insert", "Insert")));
    
#if PLATFORM_MAC
    AddKey(FKeyDetails(EKeys::BackSpace, LOCTEXT("Delete", "Delete")));
    AddKey(FKeyDetails(EKeys::Delete, LOCTEXT("ForwardDelete", "Fn+Delete")));
#else
    AddKey(FKeyDetails(EKeys::BackSpace, LOCTEXT("BackSpace", "Backspace")));
    AddKey(FKeyDetails(EKeys::Delete, LOCTEXT("Delete", "Delete")));
#endif

	AddKey(FKeyDetails(EKeys::Zero, FText::FromString("0")));
	AddKey(FKeyDetails(EKeys::One, FText::FromString("1")));
	AddKey(FKeyDetails(EKeys::Two, FText::FromString("2")));
	AddKey(FKeyDetails(EKeys::Three, FText::FromString("3")));
	AddKey(FKeyDetails(EKeys::Four, FText::FromString("4")));
	AddKey(FKeyDetails(EKeys::Five, FText::FromString("5")));
	AddKey(FKeyDetails(EKeys::Six, FText::FromString("6")));
	AddKey(FKeyDetails(EKeys::Seven, FText::FromString("7")));
	AddKey(FKeyDetails(EKeys::Eight, FText::FromString("8")));
	AddKey(FKeyDetails(EKeys::Nine, FText::FromString("9")));

	AddKey(FKeyDetails(EKeys::A, FText::FromString("A")));
	AddKey(FKeyDetails(EKeys::B, FText::FromString("B")));
	AddKey(FKeyDetails(EKeys::C, FText::FromString("C")));
	AddKey(FKeyDetails(EKeys::D, FText::FromString("D")));
	AddKey(FKeyDetails(EKeys::E, FText::FromString("E")));
	AddKey(FKeyDetails(EKeys::F, FText::FromString("F")));
	AddKey(FKeyDetails(EKeys::G, FText::FromString("G")));
	AddKey(FKeyDetails(EKeys::H, FText::FromString("H")));
	AddKey(FKeyDetails(EKeys::I, FText::FromString("I")));
	AddKey(FKeyDetails(EKeys::J, FText::FromString("J")));
	AddKey(FKeyDetails(EKeys::K, FText::FromString("K")));
	AddKey(FKeyDetails(EKeys::L, FText::FromString("L")));
	AddKey(FKeyDetails(EKeys::M, FText::FromString("M")));
	AddKey(FKeyDetails(EKeys::N, FText::FromString("N")));
	AddKey(FKeyDetails(EKeys::O, FText::FromString("O")));
	AddKey(FKeyDetails(EKeys::P, FText::FromString("P")));
	AddKey(FKeyDetails(EKeys::Q, FText::FromString("Q")));
	AddKey(FKeyDetails(EKeys::R, FText::FromString("R")));
	AddKey(FKeyDetails(EKeys::S, FText::FromString("S")));
	AddKey(FKeyDetails(EKeys::T, FText::FromString("T")));
	AddKey(FKeyDetails(EKeys::U, FText::FromString("U")));
	AddKey(FKeyDetails(EKeys::V, FText::FromString("V")));
	AddKey(FKeyDetails(EKeys::W, FText::FromString("W")));
	AddKey(FKeyDetails(EKeys::X, FText::FromString("X")));
	AddKey(FKeyDetails(EKeys::Y, FText::FromString("Y")));
	AddKey(FKeyDetails(EKeys::Z, FText::FromString("Z")));

	AddKey(FKeyDetails(EKeys::NumPadZero, LOCTEXT("NumPadZero", "Num 0")));
	AddKey(FKeyDetails(EKeys::NumPadOne, LOCTEXT("NumPadOne", "Num 1")));
	AddKey(FKeyDetails(EKeys::NumPadTwo, LOCTEXT("NumPadTwo", "Num 2")));
	AddKey(FKeyDetails(EKeys::NumPadThree, LOCTEXT("NumPadThree", "Num 3")));
	AddKey(FKeyDetails(EKeys::NumPadFour, LOCTEXT("NumPadFour", "Num 4")));
	AddKey(FKeyDetails(EKeys::NumPadFive, LOCTEXT("NumPadFive", "Num 5")));
	AddKey(FKeyDetails(EKeys::NumPadSix, LOCTEXT("NumPadSix", "Num 6")));
	AddKey(FKeyDetails(EKeys::NumPadSeven, LOCTEXT("NumPadSeven", "Num 7")));
	AddKey(FKeyDetails(EKeys::NumPadEight, LOCTEXT("NumPadEight", "Num 8")));
	AddKey(FKeyDetails(EKeys::NumPadNine, LOCTEXT("NumPadNine", "Num 9")));

	AddKey(FKeyDetails(EKeys::Multiply, LOCTEXT("Multiply", "Num *")));
	AddKey(FKeyDetails(EKeys::Add, LOCTEXT("Add", "Num +")));
	AddKey(FKeyDetails(EKeys::Subtract, LOCTEXT("Subtract", "Num -")));
	AddKey(FKeyDetails(EKeys::Decimal, LOCTEXT("Decimal", "Num .")));
	AddKey(FKeyDetails(EKeys::Divide, LOCTEXT("Divide", "Num /")));

	AddKey(FKeyDetails(EKeys::F1, LOCTEXT("F1", "F1")));
	AddKey(FKeyDetails(EKeys::F2, LOCTEXT("F2", "F2")));
	AddKey(FKeyDetails(EKeys::F3, LOCTEXT("F3", "F3")));
	AddKey(FKeyDetails(EKeys::F4, LOCTEXT("F4", "F4")));
	AddKey(FKeyDetails(EKeys::F5, LOCTEXT("F5", "F5")));
	AddKey(FKeyDetails(EKeys::F6, LOCTEXT("F6", "F6")));
	AddKey(FKeyDetails(EKeys::F7, LOCTEXT("F7", "F7")));
	AddKey(FKeyDetails(EKeys::F8, LOCTEXT("F8", "F8")));
	AddKey(FKeyDetails(EKeys::F9, LOCTEXT("F9", "F9")));
	AddKey(FKeyDetails(EKeys::F10, LOCTEXT("F10", "F10")));
	AddKey(FKeyDetails(EKeys::F11, LOCTEXT("F11", "F11")));
	AddKey(FKeyDetails(EKeys::F12, LOCTEXT("F12", "F12")));

	AddKey(FKeyDetails(EKeys::NumLock, LOCTEXT("NumLock", "Num Lock")));
	AddKey(FKeyDetails(EKeys::ScrollLock, LOCTEXT("ScrollLock", "Scroll Lock")));

	AddKey(FKeyDetails(EKeys::LeftShift, LOCTEXT("LeftShift", "Left Shift"), FKeyDetails::ModifierKey));
	AddKey(FKeyDetails(EKeys::RightShift, LOCTEXT("RightShift", "Right Shift"), FKeyDetails::ModifierKey));
	AddKey(FKeyDetails(EKeys::LeftControl, LOCTEXT("LeftControl", "Left Ctrl"), FKeyDetails::ModifierKey));
	AddKey(FKeyDetails(EKeys::RightControl, LOCTEXT("RightControl", "Right Ctrl"), FKeyDetails::ModifierKey));
	AddKey(FKeyDetails(EKeys::LeftAlt, LOCTEXT("LeftAlt", "Left Alt"), FKeyDetails::ModifierKey));
	AddKey(FKeyDetails(EKeys::RightAlt, LOCTEXT("RightAlt", "Right Alt"), FKeyDetails::ModifierKey));
	AddKey(FKeyDetails(EKeys::LeftCommand, LOCTEXT("LeftCommand", "Left Cmd"), FKeyDetails::ModifierKey));
	AddKey(FKeyDetails(EKeys::RightCommand, LOCTEXT("RightCommand", "Right Cmd"), FKeyDetails::ModifierKey));

	AddKey(FKeyDetails(EKeys::Semicolon, LOCTEXT("Semicolon", "Semicolon")));
	AddKey(FKeyDetails(EKeys::Equals, FText::FromString("=")));
	AddKey(FKeyDetails(EKeys::Comma, LOCTEXT("Comma", "Comma")));
	AddKey(FKeyDetails(EKeys::Hyphen, LOCTEXT("Hyphen", "Hyphen")));
	AddKey(FKeyDetails(EKeys::Underscore, LOCTEXT("Underscore", "Underscore")));
	AddKey(FKeyDetails(EKeys::Period, LOCTEXT("Period", "Period")));
	AddKey(FKeyDetails(EKeys::Slash, FText::FromString("/")));
	AddKey(FKeyDetails(EKeys::Tilde, FText::FromString("`"))); // Yes this is not actually a tilde, it is a long, sad, and old story
	AddKey(FKeyDetails(EKeys::LeftBracket, FText::FromString("[")));
	AddKey(FKeyDetails(EKeys::Backslash, FText::FromString("\\")));
	AddKey(FKeyDetails(EKeys::RightBracket, FText::FromString("]")));
	AddKey(FKeyDetails(EKeys::Apostrophe, LOCTEXT("Apostrophe", "Apostrophe")));
	AddKey(FKeyDetails(EKeys::Quote, FText::FromString("\"")));

	AddKey(FKeyDetails(EKeys::LeftParantheses, FText::FromString("(")));
	AddKey(FKeyDetails(EKeys::RightParantheses, FText::FromString(")")));
	AddKey(FKeyDetails(EKeys::Ampersand, FText::FromString("&")));
	AddKey(FKeyDetails(EKeys::Asterix, LOCTEXT("Asterix", "Asterisk")));
	AddKey(FKeyDetails(EKeys::Caret, FText::FromString("^")));
	AddKey(FKeyDetails(EKeys::Dollar, FText::FromString("$")));
	AddKey(FKeyDetails(EKeys::Exclamation, FText::FromString("!")));
	AddKey(FKeyDetails(EKeys::Colon, LOCTEXT("Colon", "Colon")));

	AddKey(FKeyDetails(EKeys::A_AccentGrave, FText::FromString(FString::Chr(224))));
	AddKey(FKeyDetails(EKeys::E_AccentGrave, FText::FromString(FString::Chr(232))));
	AddKey(FKeyDetails(EKeys::E_AccentAigu, FText::FromString(FString::Chr(233))));
	AddKey(FKeyDetails(EKeys::C_Cedille, FText::FromString(FString::Chr(231))));
	AddKey(FKeyDetails(EKeys::Section, FText::FromString(FString::Chr(167))));


	// Setup Gamepad keys
	AddKey(FKeyDetails(EKeys::Gamepad_LeftX, LOCTEXT("Gamepad_LeftX", "Gamepad Left Thumbstick X-Axis"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	AddKey(FKeyDetails(EKeys::Gamepad_LeftY, LOCTEXT("Gamepad_LeftY", "Gamepad Left Thumbstick Y-Axis"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	AddKey(FKeyDetails(EKeys::Gamepad_RightX, LOCTEXT("Gamepad_RightX", "Gamepad Right Thumbstick X-Axis"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	AddKey(FKeyDetails(EKeys::Gamepad_RightY, LOCTEXT("Gamepad_RightY", "Gamepad Right Thumbstick Y-Axis"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));

	AddKey(FKeyDetails(EKeys::Gamepad_DPad_Up, LOCTEXT("Gamepad_DPad_Up", "Gamepad D-pad Up"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::Gamepad_DPad_Down, LOCTEXT("Gamepad_DPad_Down", "Gamepad D-pad Down"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::Gamepad_DPad_Right, LOCTEXT("Gamepad_DPad_Right", "Gamepad D-pad Right"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::Gamepad_DPad_Left, LOCTEXT("Gamepad_DPad_Left", "Gamepad D-pad Left"), FKeyDetails::GamepadKey));

	// Virtual key codes used for input axis button press/release emulation
	AddKey(FKeyDetails(EKeys::Gamepad_LeftStick_Up, LOCTEXT("Gamepad_LeftStick_Up", "Gamepad Left Thumbstick Up"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::Gamepad_LeftStick_Down, LOCTEXT("Gamepad_LeftStick_Down", "Gamepad Left Thumbstick Down"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::Gamepad_LeftStick_Right, LOCTEXT("Gamepad_LeftStick_Right", "Gamepad Left Thumbstick Right"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::Gamepad_LeftStick_Left, LOCTEXT("Gamepad_LeftStick_Left", "Gamepad Left Thumbstick Left"), FKeyDetails::GamepadKey));

	AddKey(FKeyDetails(EKeys::Gamepad_RightStick_Up, LOCTEXT("Gamepad_RightStick_Up", "Gamepad Right Thumbstick Up"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::Gamepad_RightStick_Down, LOCTEXT("Gamepad_RightStick_Down", "Gamepad Right Thumbstick Down"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::Gamepad_RightStick_Right, LOCTEXT("Gamepad_RightStick_Right", "Gamepad Right Thumbstick Right"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::Gamepad_RightStick_Left, LOCTEXT("Gamepad_RightStick_Left", "Gamepad Right Thumbstick Left"), FKeyDetails::GamepadKey));

	AddKey(FKeyDetails(EKeys::Gamepad_Special_Left, TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateStatic(&EKeys::GetGamepadDisplayName, EKeys::Gamepad_Special_Left)), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::Gamepad_Special_Right, TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateStatic(&EKeys::GetGamepadDisplayName, EKeys::Gamepad_Special_Right)), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::Gamepad_FaceButton_Bottom, TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateStatic(&EKeys::GetGamepadDisplayName, EKeys::Gamepad_FaceButton_Bottom)), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::Gamepad_FaceButton_Right, TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateStatic(&EKeys::GetGamepadDisplayName, EKeys::Gamepad_FaceButton_Right)), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::Gamepad_FaceButton_Left, TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateStatic(&EKeys::GetGamepadDisplayName, EKeys::Gamepad_FaceButton_Left)), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::Gamepad_FaceButton_Top, TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateStatic(&EKeys::GetGamepadDisplayName, EKeys::Gamepad_FaceButton_Top)), FKeyDetails::GamepadKey));

	AddKey(FKeyDetails(EKeys::Gamepad_LeftTriggerAxis, TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateStatic(&EKeys::GetGamepadDisplayName, EKeys::Gamepad_LeftTriggerAxis)), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	AddKey(FKeyDetails(EKeys::Gamepad_RightTriggerAxis, TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateStatic(&EKeys::GetGamepadDisplayName, EKeys::Gamepad_RightTriggerAxis)), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));

	AddKey(FKeyDetails(EKeys::Gamepad_LeftShoulder, TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateStatic(&EKeys::GetGamepadDisplayName, EKeys::Gamepad_LeftShoulder)), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::Gamepad_RightShoulder, TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateStatic(&EKeys::GetGamepadDisplayName, EKeys::Gamepad_RightShoulder)), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::Gamepad_LeftTrigger, TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateStatic(&EKeys::GetGamepadDisplayName, EKeys::Gamepad_LeftTrigger)), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::Gamepad_RightTrigger, TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateStatic(&EKeys::GetGamepadDisplayName, EKeys::Gamepad_RightTrigger)), FKeyDetails::GamepadKey));

	AddKey(FKeyDetails(EKeys::Gamepad_LeftThumbstick, TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateStatic(&EKeys::GetGamepadDisplayName, EKeys::Gamepad_LeftThumbstick)), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::Gamepad_RightThumbstick, TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateStatic(&EKeys::GetGamepadDisplayName, EKeys::Gamepad_RightThumbstick)), FKeyDetails::GamepadKey));

	AddMenuCategoryDisplayInfo("Motion", LOCTEXT("MotionSubCateogry", "Motion"), TEXT("GraphEditor.KeyEvent_16x"));

	AddKey(FKeyDetails(EKeys::Tilt, LOCTEXT("Tilt", "Tilt"), FKeyDetails::VectorAxis, "Motion"));
	AddKey(FKeyDetails(EKeys::RotationRate, LOCTEXT("RotationRate", "Rotation Rate"), FKeyDetails::VectorAxis, "Motion"));
	AddKey(FKeyDetails(EKeys::Gravity, LOCTEXT("Gravity", "Gravity"), FKeyDetails::VectorAxis, "Motion"));
	AddKey(FKeyDetails(EKeys::Acceleration, LOCTEXT("Acceleration", "Acceleration"), FKeyDetails::VectorAxis, "Motion"));

	// Fingers
	AddMenuCategoryDisplayInfo("Touch", LOCTEXT("TouchSubCateogry", "Touch"), TEXT("GraphEditor.TouchEvent_16x"));

	static_assert(EKeys::NUM_TOUCH_KEYS == ETouchIndex::MAX_TOUCHES, "The number of touch keys should be equal to the max number of TouchIndexes");
	for (int TouchIndex = 0; TouchIndex < (EKeys::NUM_TOUCH_KEYS - 1); TouchIndex++)
	{
		AddKey(FKeyDetails(EKeys::TouchKeys[TouchIndex], FText::Format(LOCTEXT("TouchFormat", "Touch {0}"), FText::AsNumber(TouchIndex + 1)), 0, "Touch"));
	}

	// Make sure the Touch key for the cursor pointer is invalid, we don't want there to be an FKey for "Touch (Mouse Index)".
	check(!EKeys::TouchKeys[(int32)ETouchIndex::CursorPointerIndex].IsValid());

	// Gestures
	AddMenuCategoryDisplayInfo("Gesture", LOCTEXT("GestureSubCateogry", "Gesture"), TEXT("GraphEditor.KeyEvent_16x"));

	AddKey(FKeyDetails(EKeys::Gesture_Pinch, LOCTEXT("Gesture_Pinch", "Pinch"), 0, "Gesture"));
	AddKey(FKeyDetails(EKeys::Gesture_Flick, LOCTEXT("Gesture_Flick", "Flick"), 0, "Gesture"));
	AddKey(FKeyDetails(EKeys::Gesture_Rotate, LOCTEXT("Gesture_Rotate", "Rotate"), 0, "Gesture"));

	// Motion Controllers
	AddMenuCategoryDisplayInfo("MotionController", LOCTEXT("MotionControllerSubCategory", "Motion Controller"), TEXT("GraphEditor.PadEvent_16x"));

	//	Left Controller
	AddKey(FKeyDetails(EKeys::MotionController_Left_FaceButton1, LOCTEXT("MotionController_Left_FaceButton1", "MotionController (L) FaceButton1"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::MotionController_Left_FaceButton2, LOCTEXT("MotionController_Left_FaceButton2", "MotionController (L) FaceButton2"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::MotionController_Left_FaceButton3, LOCTEXT("MotionController_Left_FaceButton3", "MotionController (L) FaceButton3"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::MotionController_Left_FaceButton4, LOCTEXT("MotionController_Left_FaceButton4", "MotionController (L) FaceButton4"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::MotionController_Left_FaceButton5, LOCTEXT("MotionController_Left_FaceButton5", "MotionController (L) FaceButton5"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::MotionController_Left_FaceButton6, LOCTEXT("MotionController_Left_FaceButton6", "MotionController (L) FaceButton6"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::MotionController_Left_FaceButton7, LOCTEXT("MotionController_Left_FaceButton7", "MotionController (L) FaceButton7"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::MotionController_Left_FaceButton8, LOCTEXT("MotionController_Left_FaceButton8", "MotionController (L) FaceButton8"), FKeyDetails::GamepadKey));
	
	AddKey(FKeyDetails(EKeys::MotionController_Left_Shoulder, LOCTEXT("MotionController_Left_Shoulder", "MotionController (L) Shoulder"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::MotionController_Left_Trigger, LOCTEXT("MotionController_Left_Trigger", "MotionController (L) Trigger"), FKeyDetails::GamepadKey));
	
	AddKey(FKeyDetails(EKeys::MotionController_Left_Grip1, LOCTEXT("MotionController_Left_Grip1", "MotionController (L) Grip1"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::MotionController_Left_Grip2, LOCTEXT("MotionController_Left_Grip2", "MotionController (L) Grip2"), FKeyDetails::GamepadKey));
	
	AddKey(FKeyDetails(EKeys::MotionController_Left_Thumbstick, LOCTEXT("MotionController_Left_Thumbstick", "MotionController (L) Thumbstick"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::MotionController_Left_Thumbstick_Up, LOCTEXT("MotionController_Left_Thumbstick_Up", "MotionController (L) Thumbstick Up"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::MotionController_Left_Thumbstick_Down, LOCTEXT("MotionController_Left_Thumbstick_Down", "MotionController (L) Thumbstick Down"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::MotionController_Left_Thumbstick_Left, LOCTEXT("MotionController_Left_Thumbstick_Left", "MotionController (L) Thumbstick Left"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::MotionController_Left_Thumbstick_Right, LOCTEXT("MotionController_Left_Thumbstick_Right", "MotionController (L) Thumbstick Right"), FKeyDetails::GamepadKey));
	
	//	Right Controller
	AddKey(FKeyDetails(EKeys::MotionController_Right_FaceButton1, LOCTEXT("MotionController_Right_FaceButton1", "MotionController (R) FaceButton1"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::MotionController_Right_FaceButton2, LOCTEXT("MotionController_Right_FaceButton2", "MotionController (R) FaceButton2"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::MotionController_Right_FaceButton3, LOCTEXT("MotionController_Right_FaceButton3", "MotionController (R) FaceButton3"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::MotionController_Right_FaceButton4, LOCTEXT("MotionController_Right_FaceButton4", "MotionController (R) FaceButton4"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::MotionController_Right_FaceButton5, LOCTEXT("MotionController_Right_FaceButton5", "MotionController (R) FaceButton5"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::MotionController_Right_FaceButton6, LOCTEXT("MotionController_Right_FaceButton6", "MotionController (R) FaceButton6"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::MotionController_Right_FaceButton7, LOCTEXT("MotionController_Right_FaceButton7", "MotionController (R) FaceButton7"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::MotionController_Right_FaceButton8, LOCTEXT("MotionController_Right_FaceButton8", "MotionController (R) FaceButton8"), FKeyDetails::GamepadKey));
	
	AddKey(FKeyDetails(EKeys::MotionController_Right_Shoulder, LOCTEXT("MotionController_Right_Shoulder", "MotionController (R) Shoulder"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::MotionController_Right_Trigger, LOCTEXT("MotionController_Right_Trigger", "MotionController (R) Trigger"), FKeyDetails::GamepadKey));
	
	AddKey(FKeyDetails(EKeys::MotionController_Right_Grip1, LOCTEXT("MotionController_Right_Grip1", "MotionController (R) Grip1"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::MotionController_Right_Grip2, LOCTEXT("MotionController_Right_Grip2", "MotionController (R) Grip2"), FKeyDetails::GamepadKey));
	
	AddKey(FKeyDetails(EKeys::MotionController_Right_Thumbstick, LOCTEXT("MotionController_Right_Thumbstick", "MotionController (R) Thumbstick"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::MotionController_Right_Thumbstick_Up, LOCTEXT("MotionController_Right_Thumbstick_Up", "MotionController (R) Thumbstick Up"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::MotionController_Right_Thumbstick_Down, LOCTEXT("MotionController_Right_Thumbstick_Down", "MotionController (R) Thumbstick Down"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::MotionController_Right_Thumbstick_Left, LOCTEXT("MotionController_Right_Thumbstick_Left", "MotionController (R) Thumbstick Left"), FKeyDetails::GamepadKey));
	AddKey(FKeyDetails(EKeys::MotionController_Right_Thumbstick_Right, LOCTEXT("MotionController_Right_Thumbstick_Right", "MotionController (R) Thumbstick Right"), FKeyDetails::GamepadKey));
	
	//   Motion Controller Axes
	//	Left Controller
	AddKey(FKeyDetails(EKeys::MotionController_Left_Thumbstick_X, LOCTEXT("MotionController_Left_Thumbstick_X", "MotionController (L) Thumbstick X"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	AddKey(FKeyDetails(EKeys::MotionController_Left_Thumbstick_Y, LOCTEXT("MotionController_Left_Thumbstick_Y", "MotionController (L) Thumbstick Y"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	AddKey(FKeyDetails(EKeys::MotionController_Left_TriggerAxis, LOCTEXT("MotionController_Left_TriggerAxis", "MotionController (L) TriggerAxis"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	AddKey(FKeyDetails(EKeys::MotionController_Left_Grip1Axis, LOCTEXT("MotionController_Left_Grip1Axis", "MotionController (L) Grip1 Axis"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	AddKey(FKeyDetails(EKeys::MotionController_Left_Grip2Axis, LOCTEXT("MotionController_Left_Grip2Axis", "MotionController (L) Grip2 Axis"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));

	//	Right Controller
	AddKey(FKeyDetails(EKeys::MotionController_Right_Thumbstick_X, LOCTEXT("MotionController_Right_Thumbstick_X", "MotionController (R) Thumbstick X"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	AddKey(FKeyDetails(EKeys::MotionController_Right_Thumbstick_Y, LOCTEXT("MotionController_Right_Thumbstick_Y", "MotionController (R) Thumbstick Y"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	AddKey(FKeyDetails(EKeys::MotionController_Right_TriggerAxis, LOCTEXT("MotionController_Right_TriggerAxis", "MotionController (R) TriggerAxis"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	AddKey(FKeyDetails(EKeys::MotionController_Right_Grip1Axis, LOCTEXT("MotionController_Right_Grip1Axis", "MotionController (R) Grip1 Axis"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));
	AddKey(FKeyDetails(EKeys::MotionController_Right_Grip2Axis, LOCTEXT("MotionController_Right_Grip2Axis", "MotionController (R) Grip2 Axis"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis));

	// PS4-specific
	AddMenuCategoryDisplayInfo("PS4", LOCTEXT("PS4SubCategory", "PS4"), TEXT("GraphEditor.PadEvent_16x"));
	AddKey(FKeyDetails(EKeys::PS4_Special, LOCTEXT("PS4_Special", "PS4_Special"), FKeyDetails::NotBlueprintBindableKey, "PS4"));
	AddKey(FKeyDetails(EKeys::Gamepad_Special_Left_X, LOCTEXT("PS4_Gamepad_Special_Left_X", "PS4 Touchpad Button X Axis"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis, "PS4"));
	AddKey(FKeyDetails(EKeys::Gamepad_Special_Left_Y, LOCTEXT("PS4_Gamepad_Special_Left_Y", "PS4 Touchpad Button Y Axis"), FKeyDetails::GamepadKey | FKeyDetails::FloatAxis, "PS4"));


	// Steam Controller specific
	AddMenuCategoryDisplayInfo("Steam", LOCTEXT("SteamSubCateogry", "Steam"), TEXT("GraphEditor.PadEvent_16x"));

	AddKey(FKeyDetails(EKeys::Steam_Touch_0, LOCTEXT("Steam_Touch_0", "Steam Touch 0"), FKeyDetails::GamepadKey, "Steam"));
	AddKey(FKeyDetails(EKeys::Steam_Touch_1, LOCTEXT("Steam_Touch_1", "Steam Touch 1"), FKeyDetails::GamepadKey, "Steam"));
	AddKey(FKeyDetails(EKeys::Steam_Touch_2, LOCTEXT("Steam_Touch_2", "Steam Touch 2"), FKeyDetails::GamepadKey, "Steam"));
	AddKey(FKeyDetails(EKeys::Steam_Touch_3, LOCTEXT("Steam_Touch_3", "Steam Touch 3"), FKeyDetails::GamepadKey, "Steam"));
	AddKey(FKeyDetails(EKeys::Steam_Back_Left, LOCTEXT("Steam_Back_Left", "Steam Back Left"), FKeyDetails::GamepadKey, "Steam"));
	AddKey(FKeyDetails(EKeys::Steam_Back_Right, LOCTEXT("Steam_Back_Right", "Steam Back Right"), FKeyDetails::GamepadKey, "Steam"));

	// Xbox One global speech commands
	AddMenuCategoryDisplayInfo("XBoxOne", LOCTEXT("XBoxOneSubCateogry", "XBox One"), TEXT("GraphEditor.PadEvent_16x"));

	AddKey(FKeyDetails(EKeys::Global_Menu, LOCTEXT("Global_Menu", "Global Menu"), FKeyDetails::GamepadKey, "XBoxOne"));
	AddKey(FKeyDetails(EKeys::Global_View, LOCTEXT("Global_View", "Global View"), FKeyDetails::GamepadKey, "XBoxOne"));
	AddKey(FKeyDetails(EKeys::Global_Pause, LOCTEXT("Global_Pause", "Global Pause"), FKeyDetails::GamepadKey, "XBoxOne"));
	AddKey(FKeyDetails(EKeys::Global_Play, LOCTEXT("Global_Play", "Global Play"), FKeyDetails::GamepadKey, "XBoxOne"));
	AddKey(FKeyDetails(EKeys::Global_Back, LOCTEXT("Global_Back", "Global Back"), FKeyDetails::GamepadKey, "XBoxOne"));

	// Android-specific
	AddMenuCategoryDisplayInfo("Android", LOCTEXT("AndroidSubCateogry", "Android"), TEXT("GraphEditor.KeyEvent_16x"));

	AddKey(FKeyDetails(EKeys::Android_Back, LOCTEXT("Android_Back", "Android Back"), FKeyDetails::GamepadKey, "Android"));
	AddKey(FKeyDetails(EKeys::Android_Volume_Up, LOCTEXT("Android_Volume_Up", "Android Volume Up"), FKeyDetails::GamepadKey, "Android"));
	AddKey(FKeyDetails(EKeys::Android_Volume_Down, LOCTEXT("Android_Volume_Down", "Android Volume Down"), FKeyDetails::GamepadKey, "Android"));
	AddKey(FKeyDetails(EKeys::Android_Menu, LOCTEXT("Android_Menu", "Android Menu"), FKeyDetails::GamepadKey, "Android"));

	// Initialize the input key manager.  This will cause any additional OEM keys to get added
	FInputKeyManager::Get();
}
PRAGMA_ENABLE_OPTIMIZATION

void EKeys::AddKey(const FKeyDetails& KeyDetails)
{
	const FKey& Key = KeyDetails.GetKey();
	ensureMsgf(!InputKeys.Contains(Key), TEXT("Adding duplicate key '%s'"), *Key.ToString());
	Key.KeyDetails = MakeShareable(new FKeyDetails(KeyDetails));
	InputKeys.Add(Key, Key.KeyDetails);
}

void EKeys::GetAllKeys(TArray<FKey>& OutKeys)
{
	InputKeys.GetKeys(OutKeys);
}

TSharedPtr<FKeyDetails> EKeys::GetKeyDetails(const FKey Key)
{
	TSharedPtr<FKeyDetails>* KeyDetails = InputKeys.Find(Key);
	if (KeyDetails == NULL)
	{
		return TSharedPtr<FKeyDetails>();
	}
	return *KeyDetails;
}

void EKeys::AddMenuCategoryDisplayInfo(const FName CategoryName, const FText DisplayName, const FName PaletteIcon)
{
	if (MenuCategoryDisplayInfo.Contains(CategoryName))
	{
		UE_LOG(LogInput, Warning, TEXT("Category %s already has menu display info that is being replaced."), *CategoryName.ToString());
	}
	FCategoryDisplayInfo DisplayInfo;
	DisplayInfo.DisplayName = DisplayName;
	DisplayInfo.PaletteIcon = PaletteIcon;
	MenuCategoryDisplayInfo.FindOrAdd(CategoryName) = DisplayInfo;
}

FText EKeys::GetMenuCategoryDisplayName(const FName CategoryName)
{
	if (FCategoryDisplayInfo* DisplayInfo = MenuCategoryDisplayInfo.Find(CategoryName))
	{
		return DisplayInfo->DisplayName;
	}
	return MenuCategoryDisplayInfo[NAME_KeyboardCategory].DisplayName;
}

FName EKeys::GetMenuCategoryPaletteIcon(const FName CategoryName)
{
	if (FCategoryDisplayInfo* DisplayInfo = MenuCategoryDisplayInfo.Find(CategoryName))
	{
		return DisplayInfo->PaletteIcon;
	}
	return MenuCategoryDisplayInfo[NAME_KeyboardCategory].PaletteIcon;
}

EConsoleForGamepadLabels::Type EKeys::ConsoleForGamepadLabels = EConsoleForGamepadLabels::None;

PRAGMA_DISABLE_OPTIMIZATION
FText EKeys::GetGamepadDisplayName(const FKey Key)
{
	switch (ConsoleForGamepadLabels)
	{
	case EConsoleForGamepadLabels::PS4:
		if (Key == EKeys::Gamepad_FaceButton_Bottom)
		{
			return LOCTEXT("PS4_Gamepad_FaceButton_Bottom", "Gamepad X");
		}
		else if (Key == EKeys::Gamepad_FaceButton_Right)
		{
			return LOCTEXT("PS4_Gamepad_FaceButton_Right", "Gamepad Circle");
		}
		else if (Key == EKeys::Gamepad_FaceButton_Left)
		{
			return LOCTEXT("PS4_Gamepad_FaceButton_Left", "Gamepad Square");
		}
		else if (Key == EKeys::Gamepad_FaceButton_Top)
		{
			return LOCTEXT("PS4_Gamepad_FaceButton_Top", "Gamepad Triangle");
		}
		else if (Key == EKeys::Gamepad_Special_Left)
		{
			return LOCTEXT("PS4_Gamepad_Special_Left", "Gamepad Touchpad Button");
		}
		else if (Key == EKeys::Gamepad_Special_Right)
		{
			return LOCTEXT("PS4_Gamepad_Special_Right", "Gamepad Options");
		}
		else if (Key == EKeys::Gamepad_LeftShoulder)
		{
			return LOCTEXT("PS4_Gamepad_LeftShoulder", "Gamepad L1");
		}
		else if (Key == EKeys::Gamepad_RightShoulder)
		{
			return LOCTEXT("PS4_Gamepad_RightShoulder", "Gamepad R1");
		}
		else if (Key == EKeys::Gamepad_LeftTrigger)
		{
			return LOCTEXT("PS4_Gamepad_LeftTrigger", "Gamepad L2");
		}
		else if (Key == EKeys::Gamepad_RightTrigger)
		{
			return LOCTEXT("PS4_Gamepad_RightTrigger", "Gamepad R2");
		}
		else if (Key == EKeys::Gamepad_LeftTriggerAxis)
		{
			return LOCTEXT("PS4_Gamepad_LeftTriggerAxis", "Gamepad L2 Axis");
		}
		else if (Key == EKeys::Gamepad_RightTriggerAxis)
		{
			return LOCTEXT("PS4_Gamepad_RightTriggerAxis", "Gamepad R2 Axis");
		}
		else if (Key == EKeys::Gamepad_LeftThumbstick)
		{
			return LOCTEXT("PS4_Gamepad_LeftThumbstick", "Gamepad L3");
		}
		else if (Key == EKeys::Gamepad_RightThumbstick)
		{
			return LOCTEXT("PS4_Gamepad_RightThumbstick", "Gamepad R3");
		}
		break;

	case EConsoleForGamepadLabels::XBoxOne:
		if (Key == EKeys::Gamepad_FaceButton_Bottom)
		{
			return LOCTEXT("XBoxOne_Gamepad_FaceButton_Bottom", "Gamepad A");
		}
		else if (Key == EKeys::Gamepad_FaceButton_Right)
		{
			return LOCTEXT("XBoxOne_Gamepad_FaceButton_Right", "Gamepad B");
		}
		else if (Key == EKeys::Gamepad_FaceButton_Left)
		{
			return LOCTEXT("XBoxOne_Gamepad_FaceButton_Left", "Gamepad X");
		}
		else if (Key == EKeys::Gamepad_FaceButton_Top)
		{
			return LOCTEXT("XBoxOne_Gamepad_FaceButton_Top", "Gamepad Y");
		}
		else if (Key == EKeys::Gamepad_Special_Left)
		{
			return LOCTEXT("XBoxOne_Gamepad_Special_Left", "Gamepad Back");
		}
		else if (Key == EKeys::Gamepad_Special_Right)
		{
			return LOCTEXT("XBoxOne_Gamepad_Special_Right", "Gamepad Start");
		}
		else if (Key == EKeys::Gamepad_LeftShoulder)
		{
			return LOCTEXT("Gamepad_LeftShoulder", "Gamepad Left Shoulder");
		}
		else if (Key == EKeys::Gamepad_RightShoulder)
		{
			return LOCTEXT("Gamepad_RightShoulder", "Gamepad Right Shoulder");
		}
		else if (Key == EKeys::Gamepad_LeftTrigger)
		{
			return LOCTEXT("Gamepad_LeftTrigger", "Gamepad Left Trigger");
		}
		else if (Key == EKeys::Gamepad_RightTrigger)
		{
			return LOCTEXT("Gamepad_RightTrigger", "Gamepad Right Trigger");
		}
		else if (Key == EKeys::Gamepad_LeftTriggerAxis)
		{
			return LOCTEXT("Gamepad_LeftTriggerAxis", "Gamepad Left Trigger Axis");
		}
		else if (Key == EKeys::Gamepad_RightTriggerAxis)
		{
			return LOCTEXT("Gamepad_RightTriggerAxis", "Gamepad Right Trigger Axis");
		}
		else if (Key == EKeys::Gamepad_LeftThumbstick)
		{
			return LOCTEXT("Gamepad_LeftThumbstick", "Gamepad Left Thumbstick Button");
		}
		else if (Key == EKeys::Gamepad_RightThumbstick)
		{
			return LOCTEXT("Gamepad_RightThumbstick", "Gamepad Right Thumbstick Button");
		}
		break;

	default:
		if (Key == EKeys::Gamepad_FaceButton_Bottom)
		{
			return LOCTEXT("Gamepad_FaceButton_Bottom", "Gamepad Face Button Bottom");
		}
		else if (Key == EKeys::Gamepad_FaceButton_Right)
		{
			return LOCTEXT("Gamepad_FaceButton_Right", "Gamepad Face Button Right");
		}
		else if (Key == EKeys::Gamepad_FaceButton_Left)
		{
			return LOCTEXT("Gamepad_FaceButton_Left", "Gamepad Face Button Left");
		}
		else if (Key == EKeys::Gamepad_FaceButton_Top)
		{
			return LOCTEXT("Gamepad_FaceButton_Top", "Gamepad Face Button Top");
		}
		else if (Key == EKeys::Gamepad_Special_Left)
		{
			return LOCTEXT("Gamepad_Special_Left", "Gamepad Special Left");
		}
		else if (Key == EKeys::Gamepad_Special_Right)
		{
			return LOCTEXT("Gamepad_Special_Right", "Gamepad Special Right");
		}
		else if (Key == EKeys::Gamepad_LeftShoulder)
		{
			return LOCTEXT("Gamepad_LeftShoulder", "Gamepad Left Shoulder");
		}
		else if (Key == EKeys::Gamepad_RightShoulder)
		{
			return LOCTEXT("Gamepad_RightShoulder", "Gamepad Right Shoulder");
		}
		else if (Key == EKeys::Gamepad_LeftTrigger)
		{
			return LOCTEXT("Gamepad_LeftTrigger", "Gamepad Left Trigger");
		}
		else if (Key == EKeys::Gamepad_RightTrigger)
		{
			return LOCTEXT("Gamepad_RightTrigger", "Gamepad Right Trigger");
		}
		else if (Key == EKeys::Gamepad_LeftTriggerAxis)
		{
			return LOCTEXT("Gamepad_LeftTriggerAxis", "Gamepad Left Trigger Axis");
		}
		else if (Key == EKeys::Gamepad_RightTriggerAxis)
		{
			return LOCTEXT("Gamepad_RightTriggerAxis", "Gamepad Right Trigger Axis");
		}
		else if (Key == EKeys::Gamepad_LeftThumbstick)
		{
			return LOCTEXT("Gamepad_LeftThumbstick", "Gamepad Left Thumbstick Button");
		}
		else if (Key == EKeys::Gamepad_RightThumbstick)
		{
			return LOCTEXT("Gamepad_RightThumbstick", "Gamepad Right Thumbstick Button");
		}
		break;
	}

	ensureMsgf(false, TEXT("Unexpected key %s using EKeys::GetGamepadDisplayName"), *Key.ToString());
	return FText::FromString(Key.ToString());
}
PRAGMA_ENABLE_OPTIMIZATION

bool FKey::IsValid() const 
{ 
	if (KeyName != NAME_None)
	{
		ConditionalLookupKeyDetails();
		return KeyDetails.IsValid();
	}
	return false;
}

FString FKey::ToString() const
{
	return KeyName.ToString(); 
}

FName FKey::GetFName() const
{
	return KeyName;
}

bool FKey::IsModifierKey() const 
{
	ConditionalLookupKeyDetails();
	return (KeyDetails.IsValid() ? KeyDetails->IsModifierKey() : false);
}

bool FKey::IsGamepadKey() const
{
	ConditionalLookupKeyDetails();
	return (KeyDetails.IsValid() ? KeyDetails->IsGamepadKey() : false);
}

bool FKey::IsMouseButton() const
{
	ConditionalLookupKeyDetails();
	return (KeyDetails.IsValid() ? KeyDetails->IsMouseButton() : false);
}

bool FKey::IsFloatAxis() const
{
	ConditionalLookupKeyDetails();
	return (KeyDetails.IsValid() ? KeyDetails->IsFloatAxis() : false);
}

bool FKey::IsVectorAxis() const
{
	ConditionalLookupKeyDetails();
	return (KeyDetails.IsValid() ? KeyDetails->IsVectorAxis() : false);
}

bool FKey::IsBindableInBlueprints() const
{
	ConditionalLookupKeyDetails();
	return (KeyDetails.IsValid() ? KeyDetails->IsBindableInBlueprints() : false);
}

bool FKey::ShouldUpdateAxisWithoutSamples() const
{
	ConditionalLookupKeyDetails();
	return (KeyDetails.IsValid() ? KeyDetails->ShouldUpdateAxisWithoutSamples() : false);
}

FText FKeyDetails::GetDisplayName() const
{
	return DisplayName.Get();
}

FText FKey::GetDisplayName() const
{
	ConditionalLookupKeyDetails();
	return (KeyDetails.IsValid() ? KeyDetails->GetDisplayName() : FText::FromName(KeyName));
}

FName FKey::GetMenuCategory() const
{
	ConditionalLookupKeyDetails();
	return (KeyDetails.IsValid() ? KeyDetails->GetMenuCategory() : EKeys::NAME_KeyboardCategory);
}

void FKey::ConditionalLookupKeyDetails() const
{
	if (!KeyDetails.IsValid())
	{
		KeyDetails = EKeys::GetKeyDetails(*this);
	}
}

bool FKey::SerializeFromMismatchedTag(struct FPropertyTag const& Tag, FArchive& Ar)
{
	if (Tag.Type == NAME_ByteProperty && Tag.EnumName == TEXT("EKeys"))
	{
		Ar << KeyName;
		const FString KeyNameString(KeyName.ToString());
		const int32 FindIndex(KeyNameString.Find(TEXT("EKeys::")));
		if (FindIndex != INDEX_NONE)
		{
			KeyName = *KeyNameString.RightChop(FindIndex + 7);
			return true;
		}
	}
	else if (Tag.Type == NAME_NameProperty)
	{
		Ar << KeyName;
	}

	return false;
}

bool FKey::ExportTextItem(FString& ValueStr, FKey const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	if (0 != (PortFlags & EPropertyPortFlags::PPF_ExportCpp))
	{
		ValueStr += FString::Printf(TEXT("FKey(TEXT(\"%s\"))"), *KeyName.ToString());
		return true;
	}

	ValueStr += KeyName.ToString();
	return true;
}

bool FKey::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
{
	FString Temp;
	const TCHAR* NewBuffer = UPropertyHelpers::ReadToken(Buffer, Temp);
	if (!NewBuffer)
	{
		return false;
	}
	Buffer = NewBuffer;
	KeyName = *Temp;
	KeyDetails.Reset();
	return true;
}

void FKey::PostSerialize(const FArchive& Ar)
{
	KeyDetails.Reset();
}

TSharedPtr<FInputKeyManager> FInputKeyManager::Instance;

/**
 * Returns the instance of the input key manager                   
 */
FInputKeyManager& FInputKeyManager::Get()
{
	if( !Instance.IsValid() )
	{
		Instance = MakeShareable( new FInputKeyManager() );
	}
	return *Instance;
}

void FInputKeyManager::InitKeyMappings()
{
	static const uint32 MAX_KEY_MAPPINGS(256);
	uint32 KeyCodes[MAX_KEY_MAPPINGS], CharCodes[MAX_KEY_MAPPINGS];
	FString KeyNames[MAX_KEY_MAPPINGS], CharKeyNames[MAX_KEY_MAPPINGS];

	uint32 const CharKeyMapSize(FPlatformInput::GetCharKeyMap(CharCodes, CharKeyNames, MAX_KEY_MAPPINGS));
	uint32 const KeyMapSize(FPlatformInput::GetKeyMap(KeyCodes, KeyNames, MAX_KEY_MAPPINGS));

	for (uint32 Idx=0; Idx<KeyMapSize; ++Idx)
	{
		FKey Key(*KeyNames[Idx]);
		
		if (!Key.IsValid())
		{
			EKeys::AddKey(FKeyDetails(Key, Key.GetDisplayName()));
		}

		KeyMapVirtualToEnum.Add(KeyCodes[Idx], Key);
	}
	for (uint32 Idx=0; Idx<CharKeyMapSize; ++Idx)
	{
		// repeated linear search here isn't ideal, but it's just once at startup
		const FKey Key(*CharKeyNames[Idx]);

		if (ensureMsgf(Key.IsValid(), TEXT("Failed to get key for name %s"), *CharKeyNames[Idx]))
		{
			KeyMapCharToEnum.Add(CharCodes[Idx], Key);
		}
	}
}

/**
 * Retrieves the key mapped to the specified key or character code.
 *
 * @param	KeyCode	the key code to get the name for
 */
FKey FInputKeyManager::GetKeyFromCodes( const uint32 KeyCode, const uint32 CharCode ) const
{
	const FKey* KeyPtr(KeyMapVirtualToEnum.Find(KeyCode));
	if (KeyPtr == NULL)
	{
		KeyPtr = KeyMapCharToEnum.Find(CharCode);
	}
	return KeyPtr ? *KeyPtr : EKeys::Invalid;
}

void FInputKeyManager::GetCodesFromKey(const FKey Key, const uint32*& KeyCode, const uint32*& CharCode) const
{
	CharCode = KeyMapCharToEnum.FindKey(Key);
	KeyCode = KeyMapVirtualToEnum.FindKey(Key);
}

#undef LOCTEXT_NAMESPACE
