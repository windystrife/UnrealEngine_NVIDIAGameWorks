// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Mac/MacPlatformInput.h"

uint32 FMacPlatformInput::GetKeyMap( uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings )
{
#define ADDKEYMAP(KeyCode, KeyName)		if (NumMappings<MaxMappings) { KeyCodes[NumMappings]=KeyCode; KeyNames[NumMappings]=KeyName; ++NumMappings; };

	uint32 NumMappings = 0;

	if ( KeyCodes && KeyNames && (MaxMappings > 0) )
	{
		ADDKEYMAP( kVK_Delete, TEXT("BackSpace") );
		ADDKEYMAP( kVK_Tab, TEXT("Tab") );
		ADDKEYMAP( kVK_Return, TEXT("Enter") );
		ADDKEYMAP( kVK_ANSI_KeypadEnter, TEXT("Enter") );

		ADDKEYMAP( kVK_CapsLock, TEXT("CapsLock") );
		ADDKEYMAP( kVK_Escape, TEXT("Escape") );
		ADDKEYMAP( kVK_Space, TEXT("SpaceBar") );
		ADDKEYMAP( kVK_PageUp, TEXT("PageUp") );
		ADDKEYMAP( kVK_PageDown, TEXT("PageDown") );
		ADDKEYMAP( kVK_End, TEXT("End") );
		ADDKEYMAP( kVK_Home, TEXT("Home") );

		ADDKEYMAP( kVK_LeftArrow, TEXT("Left") );
		ADDKEYMAP( kVK_UpArrow, TEXT("Up") );
		ADDKEYMAP( kVK_RightArrow, TEXT("Right") );
		ADDKEYMAP( kVK_DownArrow, TEXT("Down") );

		ADDKEYMAP( kVK_ForwardDelete, TEXT("Delete") );

		ADDKEYMAP( kVK_ANSI_Keypad0, TEXT("NumPadZero") );
		ADDKEYMAP( kVK_ANSI_Keypad1, TEXT("NumPadOne") );
		ADDKEYMAP( kVK_ANSI_Keypad2, TEXT("NumPadTwo") );
		ADDKEYMAP( kVK_ANSI_Keypad3, TEXT("NumPadThree") );
		ADDKEYMAP( kVK_ANSI_Keypad4, TEXT("NumPadFour") );
		ADDKEYMAP( kVK_ANSI_Keypad5, TEXT("NumPadFive") );
		ADDKEYMAP( kVK_ANSI_Keypad6, TEXT("NumPadSix") );
		ADDKEYMAP( kVK_ANSI_Keypad7, TEXT("NumPadSeven") );
		ADDKEYMAP( kVK_ANSI_Keypad8, TEXT("NumPadEight") );
		ADDKEYMAP( kVK_ANSI_Keypad9, TEXT("NumPadNine") );

		ADDKEYMAP( kVK_ANSI_KeypadMultiply, TEXT("Multiply") );
		ADDKEYMAP( kVK_ANSI_KeypadPlus, TEXT("Add") );
		ADDKEYMAP( kVK_ANSI_KeypadMinus, TEXT("Subtract") );
		ADDKEYMAP( kVK_ANSI_KeypadDecimal, TEXT("Decimal") );
		ADDKEYMAP( kVK_ANSI_KeypadDivide, TEXT("Divide") );

		ADDKEYMAP( kVK_F1, TEXT("F1") );
		ADDKEYMAP( kVK_F2, TEXT("F2") );
		ADDKEYMAP( kVK_F3, TEXT("F3") );
		ADDKEYMAP( kVK_F4, TEXT("F4") );
		ADDKEYMAP( kVK_F5, TEXT("F5") );
		ADDKEYMAP( kVK_F6, TEXT("F6") );
		ADDKEYMAP( kVK_F7, TEXT("F7") );
		ADDKEYMAP( kVK_F8, TEXT("F8") );
		ADDKEYMAP( kVK_F9, TEXT("F9") );
		ADDKEYMAP( kVK_F10, TEXT("F10") );
		ADDKEYMAP( kVK_F11, TEXT("F11") );
		ADDKEYMAP( kVK_F12, TEXT("F12") );

		ADDKEYMAP( MMK_RightControl, TEXT("RightControl") );
		ADDKEYMAP( MMK_LeftControl, TEXT("LeftControl") );
		ADDKEYMAP( MMK_LeftShift, TEXT("LeftShift") );
		ADDKEYMAP( MMK_CapsLock, TEXT("CapsLock") );
		ADDKEYMAP( MMK_LeftAlt, TEXT("LeftAlt") );
		ADDKEYMAP( MMK_LeftCommand, TEXT("LeftCommand") );
		ADDKEYMAP( MMK_RightShift, TEXT("RightShift") );
		ADDKEYMAP( MMK_RightAlt, TEXT("RightAlt") );
		ADDKEYMAP( MMK_RightCommand, TEXT("RightCommand") );
	}
#undef ADDKEYMAP

	check(NumMappings < MaxMappings);
	return NumMappings;
}

uint32 FMacPlatformInput::GetCharKeyMap(uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings)
{
	return FGenericPlatformInput::GetStandardPrintableKeyMap(KeyCodes, KeyNames, MaxMappings, false, true);
}

