// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IOS/IOSPlatformInput.h"
#include "IOS/IOSInputInterface.h"

uint32 FIOSPlatformInput::GetKeyMap( uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings )
{
#define ADDKEYMAP(KeyCode, KeyName)		if (NumMappings<MaxMappings) { KeyCodes[NumMappings]=KeyCode; KeyNames[NumMappings]=KeyName; ++NumMappings; };
	
	uint32 NumMappings = 0;
	
	// we only handle a few "fake" keys from the IOS keyboard delegate stuff in IOSView.cpp
	if (KeyCodes && KeyNames && (MaxMappings > 0))
	{
		ADDKEYMAP(KEYCODE_ENTER, TEXT("Enter"));
		ADDKEYMAP(KEYCODE_BACKSPACE, TEXT("BackSpace"));
		ADDKEYMAP(KEYCODE_ESCAPE, TEXT("Escape"));
	}
	return NumMappings;

#undef ADDKEYMAP
}

uint32 FIOSPlatformInput::GetCharKeyMap(uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings)
{
	return FGenericPlatformInput::GetStandardPrintableKeyMap(KeyCodes, KeyNames, MaxMappings, true, true);
}
