// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Misc/Timespan.h"

#if UE_ENABLE_ICU
THIRD_PARTY_INCLUDES_START
	#include <unicode/unistr.h>
THIRD_PARTY_INCLUDES_END

namespace ICUUtilities
{
	/** 
	 * An object that can convert between FString and icu::UnicodeString
	 * Note: This object is not thread-safe.
	 */
	class FStringConverter
	{
	public:
		FStringConverter();
		~FStringConverter();

		/** Convert FString -> icu::UnicodeString */
		void ConvertString(const FString& Source, icu::UnicodeString& Destination, const bool ShouldNullTerminate = true);
		void ConvertString(const TCHAR* Source, const int32 SourceStartIndex, const int32 SourceLen, icu::UnicodeString& Destination, const bool ShouldNullTerminate = true);
		icu::UnicodeString ConvertString(const FString& Source, const bool ShouldNullTerminate = true);
		icu::UnicodeString ConvertString(const TCHAR* Source, const int32 SourceStartIndex, const int32 SourceLen, const bool ShouldNullTerminate = true);

		/** Convert icu::UnicodeString -> FString */
		void ConvertString(const icu::UnicodeString& Source, FString& Destination);
		void ConvertString(const icu::UnicodeString& Source, const int32 SourceStartIndex, const int32 SourceLen, FString& Destination);
		FString ConvertString(const icu::UnicodeString& Source);
		FString ConvertString(const icu::UnicodeString& Source, const int32 SourceStartIndex, const int32 SourceLen);

	private:
		/** Non-copyable */
		FStringConverter(const FStringConverter&);
		FStringConverter& operator=(const FStringConverter&);

		UConverter* ICUConverter;
	};

	/** Convert FString -> icu::UnicodeString */
	void ConvertString(const FString& Source, icu::UnicodeString& Destination, const bool ShouldNullTerminate = true);
	void ConvertString(const TCHAR* Source, const int32 SourceStartIndex, const int32 SourceLen, icu::UnicodeString& Destination, const bool ShouldNullTerminate = true);
	icu::UnicodeString ConvertString(const FString& Source, const bool ShouldNullTerminate = true);
	icu::UnicodeString ConvertString(const TCHAR* Source, const int32 SourceStartIndex, const int32 SourceLen, const bool ShouldNullTerminate = true);

	/** Convert icu::UnicodeString -> FString */
	void ConvertString(const icu::UnicodeString& Source, FString& Destination);
	void ConvertString(const icu::UnicodeString& Source, const int32 SourceStartIndex, const int32 SourceLen, FString& Destination);
	FString ConvertString(const icu::UnicodeString& Source);
	FString ConvertString(const icu::UnicodeString& Source, const int32 SourceStartIndex, const int32 SourceLen);

	/** Given an icu::UnicodeString, count how many characters it would be if converted into an FString (as FString may not always be UTF-16) */
	int32 GetNativeStringLength(const icu::UnicodeString& Source);
	int32 GetNativeStringLength(const icu::UnicodeString& Source, const int32 InSourceStartIndex, const int32 InSourceLength);

	/** Given an FString, count how many characters it would be if converted to an icu::UnicodeString (as FString may not always be UTF-16) */
	int32 GetUnicodeStringLength(const FString& Source);
	int32 GetUnicodeStringLength(const TCHAR* Source, const int32 InSourceStartIndex, const int32 InSourceLength);

	/** Sanitize the given culture code so that it is safe to use with ICU */
	FString SanitizeCultureCode(const FString& InCultureCode);

	/** Sanitize the given timezone code so that it is safe to use with ICU */
	FString SanitizeTimezoneCode(const FString& InTimezoneCode);

	/** Sanitize the given currency code so that it is safe to use with ICU */
	FString SanitizeCurrencyCode(const FString& InCurrencyCode);
}
#endif
