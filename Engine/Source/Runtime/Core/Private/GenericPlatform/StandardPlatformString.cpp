// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GenericPlatform/StandardPlatformString.h"

#if !PLATFORM_USE_SYSTEM_VSWPRINTF

#include "GenericPlatform/StandardPlatformString.h"
#include "HAL/UnrealMemory.h"
#include "Templates/UnrealTemplate.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"


DEFINE_LOG_CATEGORY_STATIC(LogStandardPlatformString, Log, All);

#if PLATFORM_IOS
	#define VA_LIST_REF va_list&
#else
	#define VA_LIST_REF va_list
#endif


struct FFormatInfo
{
	WIDECHAR Format[32];
	WIDECHAR LengthModifier;
	WIDECHAR Type;
	bool HasDynamicWidth;
};

static int32 GetFormattingInfo(const WIDECHAR* Format, FFormatInfo& OutInfo)
{
	const WIDECHAR* FormatStart = Format++;

	// Skip flags
	while (*Format == LITERAL(WIDECHAR, '#') || *Format == LITERAL(WIDECHAR, '0') || *Format == LITERAL(WIDECHAR, '-')
		   || *Format == LITERAL(WIDECHAR, ' ') || *Format == LITERAL(WIDECHAR, '+') || *Format == LITERAL(WIDECHAR, '\''))
	{
		Format++;
	}

	OutInfo.HasDynamicWidth = false;

	// Skip width
	while ((*Format >= LITERAL(WIDECHAR, '0') && *Format <= LITERAL(WIDECHAR, '9')) || *Format == LITERAL(WIDECHAR, '*'))
	{
		if (*Format == LITERAL(WIDECHAR, '*'))
		{
			OutInfo.HasDynamicWidth = true;
		}
		Format++;
	}

	// Skip precision
	if (*Format == LITERAL(WIDECHAR, '.'))
	{
		Format++;
		while ((*Format >= LITERAL(WIDECHAR, '0') && *Format <= LITERAL(WIDECHAR, '9')) || *Format == LITERAL(WIDECHAR, '*'))
		{
			if (*Format == LITERAL(WIDECHAR, '*'))
			{
				OutInfo.HasDynamicWidth = true;
			}
			Format++;
		}
	}

	OutInfo.LengthModifier = 0;
	if (*Format == LITERAL(WIDECHAR, 'h') || *Format == LITERAL(WIDECHAR, 'l') || *Format == LITERAL(WIDECHAR, 'j') || *Format == LITERAL(WIDECHAR, 't')
		|| *Format == LITERAL(WIDECHAR, 'z') || *Format == LITERAL(WIDECHAR, 'q') || *Format == LITERAL(WIDECHAR, 'L'))
	{
		OutInfo.LengthModifier = *Format++;
		if (*Format == LITERAL(WIDECHAR, 'h'))
		{
			OutInfo.LengthModifier = 'H';
			Format++;
		}
		else if (*Format == LITERAL(WIDECHAR, 'l'))
		{
			OutInfo.LengthModifier = 'L';
			Format++;
		}
	}

	OutInfo.Type = *Format++;

	const int32 FormatLength = Format - FormatStart;

	FMemory::Memcpy(OutInfo.Format, FormatStart, FormatLength * sizeof(WIDECHAR));
	int32 OutInfoFormatLength = FormatLength;
	if (OutInfo.HasDynamicWidth && FChar::ToLower(OutInfo.Type) == LITERAL(WIDECHAR, 's'))
	{
		OutInfo.Format[OutInfoFormatLength - 1] = 'l';
		OutInfo.Format[OutInfoFormatLength++] = 's';
	}
	OutInfo.Format[OutInfoFormatLength] = 0;

	return FormatLength;
}

template <typename T1, typename T2>
static int32 FormatString(const FFormatInfo& Info, VA_LIST_REF ArgPtr, WIDECHAR* Formatted, int32 Length)
{
	if (Info.HasDynamicWidth)
	{
		int32 Width = va_arg(ArgPtr, int32);
		if (FChar::ToLower(Info.LengthModifier) == LITERAL(WIDECHAR, 'l'))
		{
			T1 Value = va_arg(ArgPtr, T1);
			return swprintf(Formatted, Length, Info.Format, Width, Value);
		}
		else
		{
			T2 Value = va_arg(ArgPtr, T2);
			return swprintf(Formatted, Length, Info.Format, Width, Value);
		}
	}
	else
	{
		if (FChar::ToLower(Info.LengthModifier) == LITERAL(WIDECHAR, 'l'))
		{
			T1 Value = va_arg(ArgPtr, T1);
			return swprintf(Formatted, Length, Info.Format, Value);
		}
		else
		{
			T2 Value = va_arg(ArgPtr, T2);
			return swprintf(Formatted, Length, Info.Format, Value);
		}
	}
}

static const WIDECHAR* GetFormattedArgument(const FFormatInfo& Info, VA_LIST_REF ArgPtr, WIDECHAR* Formatted, int32 &InOutLength)
{
	if (FChar::ToLower(Info.Type) == LITERAL(WIDECHAR, 's'))
	{
		if (Info.HasDynamicWidth)
		{
			int32 Width = va_arg(ArgPtr, int32);
			const WIDECHAR* String = va_arg(ArgPtr, WIDECHAR*);
			if (String)
			{
				InOutLength = swprintf(Formatted, InOutLength, Info.Format, Width, String);
				return Formatted;
			}
			else
			{
				return TEXT("(null)");
			}
		}
		else
		{
			const WIDECHAR* String = va_arg(ArgPtr, WIDECHAR*);
			InOutLength = FCString::Strlen(String);
			return String ? String : TEXT("(null)");
		}
	}

	if (FChar::ToLower(Info.Type) == LITERAL(WIDECHAR, 'c'))
	{
		const WIDECHAR Char = (WIDECHAR)va_arg(ArgPtr, int32);
		Formatted[0] = Char;
		Formatted[1] = 0;
		InOutLength = 1;
		return Formatted;
	}

	if (FChar::ToLower(Info.Type) == LITERAL(WIDECHAR, 'a') || FChar::ToLower(Info.Type) == LITERAL(WIDECHAR, 'e')
		|| FChar::ToLower(Info.Type) == LITERAL(WIDECHAR, 'f') || FChar::ToLower(Info.Type) == LITERAL(WIDECHAR, 'g'))
	{
		InOutLength = FormatString<long double, double>(Info, ArgPtr, Formatted, InOutLength);
	}
	else if (Info.Type == LITERAL(WIDECHAR, 'p'))
	{
		void* Value = va_arg(ArgPtr, void*);
		InOutLength = swprintf(Formatted, InOutLength, Info.Format, Value);
	}
	else if (FChar::ToLower(Info.Type) == LITERAL(WIDECHAR, 'd') || FChar::ToLower(Info.Type) == LITERAL(WIDECHAR, 'i'))
	{
		InOutLength = FormatString<int64, int32>(Info, ArgPtr, Formatted, InOutLength);
	}
	else if (FChar::ToLower(Info.Type) == LITERAL(WIDECHAR, 'o') || FChar::ToLower(Info.Type) == LITERAL(WIDECHAR, 'u') || FChar::ToLower(Info.Type) == LITERAL(WIDECHAR, 'x'))
	{
		InOutLength = FormatString<uint64, uint32>(Info, ArgPtr, Formatted, InOutLength);
	}

	check(InOutLength != -1);

	return Formatted;
}

int32 FStandardPlatformString::GetVarArgs( WIDECHAR* Dest, SIZE_T DestSize, int32 Count, const WIDECHAR*& Fmt, va_list ArgPtr )
{
  	const WIDECHAR* Format = Fmt;
	const WIDECHAR* DestStart = Dest;

	while (Count > 1 &&  *Format)
	{
		if (*Format == LITERAL(WIDECHAR, '%'))
		{
			if (*(Format + 1) == LITERAL(WIDECHAR, '%'))
			{
				*Dest++ = *Format;
				Format += 2;
				Count--;
				continue;
			}

			FFormatInfo Info;
			Format += GetFormattingInfo(Format, Info);

			WIDECHAR Formatted[1024];
			int32 Length = ARRAY_COUNT(Formatted);
			const WIDECHAR* FormattedArg = GetFormattedArgument(Info, ArgPtr, Formatted, Length);
			if (FormattedArg && Length > 0)
			{
				if (Length < Count)
				{
					FMemory::Memcpy(Dest, FormattedArg, Length * sizeof(WIDECHAR));
					Dest += Length;
				}
				Count -= Length;
			}
		}
		else
		{
			*Dest++ = *Format++;
			Count--;
		}
	}

	*Dest = 0;

	return Count > 1 ? Dest - DestStart : -1;
}

#endif // !PLATFORM_USE_SYSTEM_VSWPRINTF
