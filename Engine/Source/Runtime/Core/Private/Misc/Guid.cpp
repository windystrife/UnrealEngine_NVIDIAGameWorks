// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "UObject/PropertyPortFlags.h"


/* FGuid interface
 *****************************************************************************/

bool FGuid::ExportTextItem(FString& ValueStr, FGuid const& DefaultValue, UObject* Parent, int32 PortFlags, class UObject* ExportRootScope) const
{
	if (0 != (PortFlags & EPropertyPortFlags::PPF_ExportCpp))
	{
		return false;
	}

	ValueStr += ToString();

	return true;
}


bool FGuid::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, class UObject* Parent, FOutputDevice* ErrorText)
{
	if (FPlatformString::Strlen(Buffer) < 32)
	{
		return false;
	}

	if (!ParseExact(FString(Buffer).Left(32), EGuidFormats::Digits, *this))
	{
		return false;
	}

	Buffer += 32;

	return true;
}


FString FGuid::ToString(EGuidFormats Format) const
{
	switch (Format)
	{
	case EGuidFormats::DigitsWithHyphens:
		return FString::Printf(TEXT("%08X-%04X-%04X-%04X-%04X%08X"), A, B >> 16, B & 0xFFFF, C >> 16, C & 0xFFFF, D);

	case EGuidFormats::DigitsWithHyphensInBraces:
		return FString::Printf(TEXT("{%08X-%04X-%04X-%04X-%04X%08X}"), A, B >> 16, B & 0xFFFF, C >> 16, C & 0xFFFF, D);

	case EGuidFormats::DigitsWithHyphensInParentheses:
		return FString::Printf(TEXT("(%08X-%04X-%04X-%04X-%04X%08X)"), A, B >> 16, B & 0xFFFF, C >> 16, C & 0xFFFF, D);

	case EGuidFormats::HexValuesInBraces:
		return FString::Printf(TEXT("{0x%08X,0x%04X,0x%04X,{0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,0x%02X}}"), A, B >> 16, B & 0xFFFF, C >> 24, (C >> 16) & 0xFF, (C >> 8) & 0xFF, C & 0XFF, D >> 24, (D >> 16) & 0XFF, (D >> 8) & 0XFF, D & 0XFF);

	case EGuidFormats::UniqueObjectGuid:
		return FString::Printf(TEXT("%08X-%08X-%08X-%08X"), A, B, C, D);

	default:
		return FString::Printf(TEXT("%08X%08X%08X%08X"), A, B, C, D);
	}
}


/* FGuid static interface
 *****************************************************************************/

FGuid FGuid::NewGuid()
{
	FGuid Result(0, 0, 0, 0);
	FPlatformMisc::CreateGuid(Result);

	return Result;
}


bool FGuid::Parse(const FString& GuidString, FGuid& OutGuid)
{
	if (GuidString.Len() == 32)
	{
		return ParseExact(GuidString, EGuidFormats::Digits, OutGuid);
	}
	
	if (GuidString.Len() == 36)
	{
		return ParseExact(GuidString, EGuidFormats::DigitsWithHyphens, OutGuid);
	}
	
	if (GuidString.Len() == 38)
	{
		if (GuidString.StartsWith("{"))
		{
			return ParseExact(GuidString, EGuidFormats::DigitsWithHyphensInBraces, OutGuid);
		}
		
		return ParseExact(GuidString, EGuidFormats::DigitsWithHyphensInParentheses, OutGuid);
	}
	
	if (GuidString.Len() == 68)
	{
		return ParseExact(GuidString, EGuidFormats::HexValuesInBraces, OutGuid);
	}

	if (GuidString.Len() == 35)
	{
		return ParseExact(GuidString, EGuidFormats::UniqueObjectGuid, OutGuid);
	}

	return false;
}


bool FGuid::ParseExact(const FString& GuidString, EGuidFormats Format, FGuid& OutGuid)
{
	FString NormalizedGuidString;

	NormalizedGuidString.Empty(32);

	if (Format == EGuidFormats::Digits)
	{
		NormalizedGuidString = GuidString;
	}
	else if (Format == EGuidFormats::DigitsWithHyphens)
	{
		if ((GuidString[8] != TCHAR('-')) ||
			(GuidString[13] != TCHAR('-')) ||
			(GuidString[18] != TCHAR('-')) ||
			(GuidString[23] != TCHAR('-')))
		{
			return false;
		}

		NormalizedGuidString += GuidString.Mid(0, 8);
		NormalizedGuidString += GuidString.Mid(9, 4);
		NormalizedGuidString += GuidString.Mid(14, 4);
		NormalizedGuidString += GuidString.Mid(19, 4);
		NormalizedGuidString += GuidString.Mid(24, 12);
	}
	else if (Format == EGuidFormats::DigitsWithHyphensInBraces)
	{
		if ((GuidString[0] != TCHAR('{')) ||
			(GuidString[9] != TCHAR('-')) ||
			(GuidString[14] != TCHAR('-')) ||
			(GuidString[19] != TCHAR('-')) ||
			(GuidString[24] != TCHAR('-')) ||
			(GuidString[37] != TCHAR('}')))
		{
			return false;
		}

		NormalizedGuidString += GuidString.Mid(1, 8);
		NormalizedGuidString += GuidString.Mid(10, 4);
		NormalizedGuidString += GuidString.Mid(15, 4);
		NormalizedGuidString += GuidString.Mid(20, 4);
		NormalizedGuidString += GuidString.Mid(25, 12);
	}
	else if (Format == EGuidFormats::DigitsWithHyphensInParentheses)
	{
		if ((GuidString[0] != TCHAR('(')) ||
			(GuidString[9] != TCHAR('-')) ||
			(GuidString[14] != TCHAR('-')) ||
			(GuidString[19] != TCHAR('-')) ||
			(GuidString[24] != TCHAR('-')) ||
			(GuidString[37] != TCHAR(')')))
		{
			return false;
		}

		NormalizedGuidString += GuidString.Mid(1, 8);
		NormalizedGuidString += GuidString.Mid(10, 4);
		NormalizedGuidString += GuidString.Mid(15, 4);
		NormalizedGuidString += GuidString.Mid(20, 4);
		NormalizedGuidString += GuidString.Mid(25, 12);
	}
	else if (Format == EGuidFormats::HexValuesInBraces)
	{
		if ((GuidString[0] != TCHAR('{')) ||
			(GuidString[1] != TCHAR('0')) ||
			(GuidString[2] != TCHAR('x')) ||
			(GuidString[11] != TCHAR(',')) ||
			(GuidString[12] != TCHAR('0')) ||
			(GuidString[13] != TCHAR('x')) ||
			(GuidString[18] != TCHAR(',')) ||
			(GuidString[19] != TCHAR('0')) ||
			(GuidString[20] != TCHAR('x')) ||
			(GuidString[25] != TCHAR(',')) ||
			(GuidString[26] != TCHAR('{')) ||
			(GuidString[27] != TCHAR('0')) ||
			(GuidString[28] != TCHAR('x')) ||
			(GuidString[31] != TCHAR(',')) ||
			(GuidString[32] != TCHAR('0')) ||
			(GuidString[33] != TCHAR('x')) ||
			(GuidString[36] != TCHAR(',')) ||
			(GuidString[37] != TCHAR('0')) ||
			(GuidString[38] != TCHAR('x')) ||
			(GuidString[41] != TCHAR(',')) ||
			(GuidString[42] != TCHAR('0')) ||
			(GuidString[43] != TCHAR('x')) ||
			(GuidString[46] != TCHAR(',')) ||
			(GuidString[47] != TCHAR('0')) ||
			(GuidString[48] != TCHAR('x')) ||
			(GuidString[51] != TCHAR(',')) ||
			(GuidString[52] != TCHAR('0')) ||
			(GuidString[53] != TCHAR('x')) ||
			(GuidString[56] != TCHAR(',')) ||
			(GuidString[57] != TCHAR('0')) ||
			(GuidString[58] != TCHAR('x')) ||
			(GuidString[61] != TCHAR(',')) ||
			(GuidString[62] != TCHAR('0')) ||
			(GuidString[63] != TCHAR('x')) ||
			(GuidString[66] != TCHAR('}')) ||
			(GuidString[67] != TCHAR('}')))
		{
			return false;
		}

		NormalizedGuidString += GuidString.Mid(3, 8);
		NormalizedGuidString += GuidString.Mid(14, 4);
		NormalizedGuidString += GuidString.Mid(21, 4);
		NormalizedGuidString += GuidString.Mid(29, 2);
		NormalizedGuidString += GuidString.Mid(34, 2);
		NormalizedGuidString += GuidString.Mid(39, 2);
		NormalizedGuidString += GuidString.Mid(44, 2);
		NormalizedGuidString += GuidString.Mid(49, 2);
		NormalizedGuidString += GuidString.Mid(54, 2);
		NormalizedGuidString += GuidString.Mid(59, 2);
		NormalizedGuidString += GuidString.Mid(64, 2);
	}
	else if (Format == EGuidFormats::UniqueObjectGuid)
	{
		if ((GuidString[8] != TCHAR('-')) ||
			(GuidString[17] != TCHAR('-')) ||
			(GuidString[26] != TCHAR('-')))
		{
			return false;
		}

		NormalizedGuidString += GuidString.Mid(0, 8);
		NormalizedGuidString += GuidString.Mid(9, 8);
		NormalizedGuidString += GuidString.Mid(18, 8);
		NormalizedGuidString += GuidString.Mid(27, 8);
	}

	for (int32 Index = 0; Index < NormalizedGuidString.Len(); ++Index)
	{
		if (!FChar::IsHexDigit(NormalizedGuidString[Index]))
		{
			return false;
		}
	}

	OutGuid = FGuid(
		FParse::HexNumber(*NormalizedGuidString.Mid(0, 8)),
		FParse::HexNumber(*NormalizedGuidString.Mid(8, 8)),
		FParse::HexNumber(*NormalizedGuidString.Mid(16, 8)),
		FParse::HexNumber(*NormalizedGuidString.Mid(24, 8))
	);

	return true;
}
