// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GenericPlatform/GenericPlatformString.h"
#include "HAL/UnrealMemory.h"
#include "Misc/Char.h"
#include "Containers/UnrealString.h"
#include "Logging/LogMacros.h"


DEFINE_LOG_CATEGORY_STATIC(LogGenericPlatformString, Log, All);

template <> const TCHAR* FGenericPlatformString::GetEncodingTypeName<ANSICHAR>() { return TEXT("ANSICHAR"); }
template <> const TCHAR* FGenericPlatformString::GetEncodingTypeName<WIDECHAR>() { return TEXT("WIDECHAR"); }
template <> const TCHAR* FGenericPlatformString::GetEncodingTypeName<UCS2CHAR>() { return TEXT("UCS2CHAR"); }


void* FGenericPlatformString::Memcpy(void* Dest, const void* Src, SIZE_T Count)
{
	return FMemory::Memcpy(Dest, Src, Count);
}

namespace
{
	void TrimStringAndLogBogusCharsError(FString& SrcStr, const TCHAR* SourceCharName, const TCHAR* DestCharName)
	{
		SrcStr.TrimStartInline();
		// @todo: Put this back in once GLog becomes a #define, or is replaced with GLog::GetLog()
		//UE_LOG(LogGenericPlatformString, Warning, TEXT("Bad chars found when trying to convert \"%s\" from %s to %s"), *SrcStr, SourceCharName, DestCharName);
	}
}

template <typename DestEncoding, typename SourceEncoding>
void FGenericPlatformString::LogBogusChars(const SourceEncoding* Src, int32 SrcSize)
{
	FString SrcStr;
	bool    bFoundBogusChars = false;
	for (; SrcSize; --SrcSize)
	{
		SourceEncoding SrcCh = *Src++;
		if (!CanConvertChar<DestEncoding>(SrcCh))
		{
			SrcStr += FString::Printf(TEXT("[0x%X]"), (int32)SrcCh);
			bFoundBogusChars = true;
		}
		else if (CanConvertChar<TCHAR>(SrcCh))
		{
			if (TChar<SourceEncoding>::IsLinebreak(SrcCh))
			{
				if (bFoundBogusChars)
				{
					TrimStringAndLogBogusCharsError(SrcStr, GetEncodingTypeName<SourceEncoding>(), GetEncodingTypeName<DestEncoding>());
					bFoundBogusChars = false;
				}
				SrcStr.Empty();
			}
			else
			{
				SrcStr.AppendChar((TCHAR)SrcCh);
			}
		}
		else
		{
			SrcStr.AppendChar((TCHAR)'?');
		}
	}

	if (bFoundBogusChars)
	{
		TrimStringAndLogBogusCharsError(SrcStr, GetEncodingTypeName<SourceEncoding>(), GetEncodingTypeName<DestEncoding>());
	}
}

#if !UE_BUILD_DOCS
template CORE_API void FGenericPlatformString::LogBogusChars<ANSICHAR, WIDECHAR>(const WIDECHAR* Src, int32 SrcSize);
template CORE_API void FGenericPlatformString::LogBogusChars<ANSICHAR, UCS2CHAR>(const UCS2CHAR* Src, int32 SrcSize);
template CORE_API void FGenericPlatformString::LogBogusChars<WIDECHAR, ANSICHAR>(const ANSICHAR* Src, int32 SrcSize);
template CORE_API void FGenericPlatformString::LogBogusChars<WIDECHAR, UCS2CHAR>(const UCS2CHAR* Src, int32 SrcSize);
template CORE_API void FGenericPlatformString::LogBogusChars<UCS2CHAR, ANSICHAR>(const ANSICHAR* Src, int32 SrcSize);
template CORE_API void FGenericPlatformString::LogBogusChars<UCS2CHAR, WIDECHAR>(const WIDECHAR* Src, int32 SrcSize);
#endif
