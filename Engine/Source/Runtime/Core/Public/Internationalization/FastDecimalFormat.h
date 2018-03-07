// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Internationalization/Text.h"

/** Rules used to format a decimal number */
struct FDecimalNumberFormattingRules
{
	FDecimalNumberFormattingRules()
		: GroupingSeparatorCharacter(0)
		, DecimalSeparatorCharacter(0)
		, PrimaryGroupingSize(0)
		, SecondaryGroupingSize(0)
	{
		DigitCharacters[0] = '0';
		DigitCharacters[1] = '1';
		DigitCharacters[2] = '2';
		DigitCharacters[3] = '3';
		DigitCharacters[4] = '4';
		DigitCharacters[5] = '5';
		DigitCharacters[6] = '6';
		DigitCharacters[7] = '7';
		DigitCharacters[8] = '8';
		DigitCharacters[9] = '9';
	}

	/** Number formatting rules, typically extracted from the ICU decimal formatter for a given culture */
	FString NaNString;
	FString NegativePrefixString;
	FString NegativeSuffixString;
	FString PositivePrefixString;
	FString PositiveSuffixString;
	TCHAR GroupingSeparatorCharacter;
	TCHAR DecimalSeparatorCharacter;
	uint8 PrimaryGroupingSize;
	uint8 SecondaryGroupingSize;
	TCHAR DigitCharacters[10];

	/** Default number formatting options for a given culture */
	FNumberFormattingOptions CultureDefaultFormattingOptions;
};

/**
 * Provides efficient and culture aware number formatting.
 * You would call FastDecimalFormat::NumberToString to convert a number to the correct decimal representation based on the given formatting rules and options.
 * The primary consumer of this is FText, however you can use it for other things. GetCultureAgnosticFormattingRules can provide formatting rules for cases where you don't care about culture.
 * @note If you use the version that takes an output string, the formatted number will be appended to the existing contents of the string.
 */
namespace FastDecimalFormat
{

namespace Internal
{

CORE_API void IntegralToString(const bool bIsNegative, const uint64 InVal, const FDecimalNumberFormattingRules& InFormattingRules, FNumberFormattingOptions InFormattingOptions, FString& OutString);
CORE_API void FractionalToString(const double InVal, const FDecimalNumberFormattingRules& InFormattingRules, FNumberFormattingOptions InFormattingOptions, FString& OutString);

} // namespace Internal

#define FAST_DECIMAL_FORMAT_SIGNED_IMPL(NUMBER_TYPE)																																				\
	FORCEINLINE void NumberToString(const NUMBER_TYPE InVal, const FDecimalNumberFormattingRules& InFormattingRules, const FNumberFormattingOptions& InFormattingOptions, FString& OutString)		\
	{																																																\
		const bool bIsNegative = InVal < 0;																																							\
		Internal::IntegralToString(bIsNegative, static_cast<uint64>((bIsNegative) ? -InVal : InVal), InFormattingRules, InFormattingOptions, OutString);											\
	}																																																\
	FORCEINLINE FString NumberToString(const NUMBER_TYPE InVal, const FDecimalNumberFormattingRules& InFormattingRules, const FNumberFormattingOptions& InFormattingOptions)						\
	{																																																\
		FString Result;																																												\
		NumberToString(InVal, InFormattingRules, InFormattingOptions, Result);																														\
		return Result;																																												\
	}

#define FAST_DECIMAL_FORMAT_UNSIGNED_IMPL(NUMBER_TYPE)																																				\
	FORCEINLINE void NumberToString(const NUMBER_TYPE InVal, const FDecimalNumberFormattingRules& InFormattingRules, const FNumberFormattingOptions& InFormattingOptions, FString& OutString)		\
	{																																																\
		Internal::IntegralToString(false, static_cast<uint64>(InVal), InFormattingRules, InFormattingOptions, OutString);																			\
	}																																																\
	FORCEINLINE FString NumberToString(const NUMBER_TYPE InVal, const FDecimalNumberFormattingRules& InFormattingRules, const FNumberFormattingOptions& InFormattingOptions)						\
	{																																																\
		FString Result;																																												\
		NumberToString(InVal, InFormattingRules, InFormattingOptions, Result);																														\
		return Result;																																												\
	}

#define FAST_DECIMAL_FORMAT_FRACTIONAL_IMPL(NUMBER_TYPE)																																			\
	FORCEINLINE void NumberToString(const NUMBER_TYPE InVal, const FDecimalNumberFormattingRules& InFormattingRules, const FNumberFormattingOptions& InFormattingOptions, FString& OutString)		\
	{																																																\
		Internal::FractionalToString(static_cast<double>(InVal), InFormattingRules, InFormattingOptions, OutString);																				\
	}																																																\
	FORCEINLINE FString NumberToString(const NUMBER_TYPE InVal, const FDecimalNumberFormattingRules& InFormattingRules, const FNumberFormattingOptions& InFormattingOptions)						\
	{																																																\
		FString Result;																																												\
		NumberToString(InVal, InFormattingRules, InFormattingOptions, Result);																														\
		return Result;																																												\
	}

FAST_DECIMAL_FORMAT_SIGNED_IMPL(int8)
FAST_DECIMAL_FORMAT_SIGNED_IMPL(int16)
FAST_DECIMAL_FORMAT_SIGNED_IMPL(int32)
FAST_DECIMAL_FORMAT_SIGNED_IMPL(int64)

FAST_DECIMAL_FORMAT_UNSIGNED_IMPL(uint8)
FAST_DECIMAL_FORMAT_UNSIGNED_IMPL(uint16)
FAST_DECIMAL_FORMAT_UNSIGNED_IMPL(uint32)
FAST_DECIMAL_FORMAT_UNSIGNED_IMPL(uint64)

FAST_DECIMAL_FORMAT_FRACTIONAL_IMPL(float)
FAST_DECIMAL_FORMAT_FRACTIONAL_IMPL(double)

#undef FAST_DECIMAL_FORMAT_SIGNED_IMPL
#undef FAST_DECIMAL_FORMAT_UNSIGNED_IMPL
#undef FAST_DECIMAL_FORMAT_FRACTIONAL_IMPL

/**
 * Get the formatting rules to use when you don't care about culture.
 */
CORE_API FDecimalNumberFormattingRules GetCultureAgnosticFormattingRules();

} // namespace FastDecimalFormat
