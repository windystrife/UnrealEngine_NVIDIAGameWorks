// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct INPUTCORE_API FGenericPlatformInput
{
public:
	FORCEINLINE static uint32 GetKeyMap( uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings )
	{
		return 0;
	}

	FORCEINLINE static uint32 GetCharKeyMap(uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings)
	{
		return 0;
	}

protected:
	/**
	* Retrieves some standard key code mappings (usually called by a subclass's GetCharKeyMap)
	*
	* @param OutKeyMap Key map to add to.
	* @param bMapUppercaseKeys If true, will map A, B, C, etc to EKeys::A, EKeys::B, EKeys::C
	* @param bMapLowercaseKeys If true, will map a, b, c, etc to EKeys::A, EKeys::B, EKeys::C
	*/
	static uint32 GetStandardPrintableKeyMap(uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings, bool bMapUppercaseKeys, bool bMapLowercaseKeys);
};
