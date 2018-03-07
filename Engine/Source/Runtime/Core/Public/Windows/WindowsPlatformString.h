// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/MicrosoftPlatformString.h"


/**
 * Windows string implementation.
 */
struct FWindowsPlatformString
	: public FMicrosoftPlatformString
{
	// These should be replaced with equivalent Convert and ConvertedLength functions when we properly support variable-length encodings.
/*	static void WideCharToMultiByte(const wchar_t *Source, uint32 LengthWM1, ANSICHAR *Dest, uint32 LengthA)
	{
		::WideCharToMultiByte(CP_ACP,0,Source,LengthWM1+1,Dest,LengthA,NULL,NULL);
	}
	static void MultiByteToWideChar(const ANSICHAR *Source, TCHAR *Dest, uint32 LengthM1)
	{
		::MultiByteToWideChar(CP_ACP,0,Source,LengthM1+1,Dest,LengthM1+1);
	}*/
};


typedef FWindowsPlatformString FPlatformString;
