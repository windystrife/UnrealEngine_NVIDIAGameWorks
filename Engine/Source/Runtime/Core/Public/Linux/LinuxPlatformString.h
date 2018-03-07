// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	LinuxPlatformString.h: Linux platform string classes, mostly implemented with ANSI C++
==============================================================================================*/

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/StandardPlatformString.h"
/**
* Linux string implementation
**/
struct FLinuxPlatformString : public FStandardPlatformString
{
	static void WideCharToMultiByte(const TCHAR *Source, uint32 LengthWM1, ANSICHAR *Dest, uint32 LengthA)
	{
		int ret = wcstombs(Dest, Source, LengthA);
		if(ret == LengthA) Dest[LengthA - 1] = '\0';
	}
	static void MultiByteToWideChar(const ANSICHAR *Source, TCHAR *Dest, uint32 LengthM1)
	{
		int ret = mbstowcs(Dest, Source, LengthM1);
		if(ret == LengthM1) Dest[LengthM1] = '\0';
	}

	static const ANSICHAR* GetEncodingName()
	{
		return "UTF-32LE";
	}

	static const bool IsUnicodeEncoded = true;
};

typedef FLinuxPlatformString FPlatformString;
