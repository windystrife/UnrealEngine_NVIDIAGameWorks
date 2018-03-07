// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Internationalization/FastDecimalFormat.h"

namespace FastDecimalFormat
{

namespace Internal
{

static const int32 MaxIntegralPrintLength = 20;
static const int32 MaxFractionalPrintPrecision = 18;
static const int32 MinRequiredIntegralBufferSize = (MaxIntegralPrintLength * 2) + 1; // *2 for an absolute worst case group separator scenario, +1 for null terminator

static const uint64 Pow10Table[] = {
	1,						// 10^0
	10,						// 10^1
	100,					// 10^2
	1000,					// 10^3
	10000,					// 10^4
	100000,					// 10^5
	1000000,				// 10^6
	10000000,				// 10^7
	100000000,				// 10^8
	1000000000,				// 10^9
	10000000000,			// 10^10
	100000000000,			// 10^11
	1000000000000,			// 10^12
	10000000000000,			// 10^13
	100000000000000,		// 10^14
	1000000000000000,		// 10^15
	10000000000000000,		// 10^16
	100000000000000000,		// 10^17
	1000000000000000000,	// 10^18
};

static_assert(ARRAY_COUNT(Pow10Table) - 1 >= MaxFractionalPrintPrecision, "Pow10Table must at big enough to index any value up-to MaxFractionalPrintPrecision");

void SanitizeNumberFormattingOptions(FNumberFormattingOptions& InOutFormattingOptions)
{
	// Ensure that the minimum limits are >= 0
	InOutFormattingOptions.MinimumIntegralDigits = FMath::Max(0, InOutFormattingOptions.MinimumIntegralDigits);
	InOutFormattingOptions.MinimumFractionalDigits = FMath::Max(0, InOutFormattingOptions.MinimumFractionalDigits);

	// Ensure that the maximum limits are >= the minimum limits
	InOutFormattingOptions.MaximumIntegralDigits = FMath::Max(InOutFormattingOptions.MinimumIntegralDigits, InOutFormattingOptions.MaximumIntegralDigits);
	InOutFormattingOptions.MaximumFractionalDigits = FMath::Max(InOutFormattingOptions.MinimumFractionalDigits, InOutFormattingOptions.MaximumFractionalDigits);
}

int32 IntegralToString_UInt64ToString(
	const uint64 InVal, 
	const bool InUseGrouping, const uint8 InPrimaryGroupingSize, const uint8 InSecondaryGroupingSize, const TCHAR InGroupingSeparatorCharacter, const TCHAR* InDigitCharacters, 
	const int32 InMinDigitsToPrint, const int32 InMaxDigitsToPrint, 
	TCHAR* InBufferToFill, const int32 InBufferToFillSize
	)
{
	check(InBufferToFillSize >= MinRequiredIntegralBufferSize);

	TCHAR TmpBuffer[MinRequiredIntegralBufferSize];
	int32 StringLen = 0;

	int32 DigitsPrinted = 0;
	uint8 NumUntilNextGroup = InPrimaryGroupingSize;

	if (InVal > 0)
	{
		// Perform the initial number -> string conversion
		uint64 TmpNum = InVal;
		while (DigitsPrinted < InMaxDigitsToPrint && TmpNum != 0)
		{
			if (InUseGrouping && NumUntilNextGroup-- == 0)
			{
				TmpBuffer[StringLen++] = InGroupingSeparatorCharacter;
				NumUntilNextGroup = InSecondaryGroupingSize - 1; // -1 to account for the digit we're about to print
			}

			TmpBuffer[StringLen++] = InDigitCharacters[TmpNum % 10];
			TmpNum /= 10;

			++DigitsPrinted;
		}
	}

	// Pad the string to the min digits requested
	{
		const int32 PaddingToApply = FMath::Min(InMinDigitsToPrint - DigitsPrinted, MaxIntegralPrintLength - DigitsPrinted);
		for (int32 PaddingIndex = 0; PaddingIndex < PaddingToApply; ++PaddingIndex)
		{
			if (InUseGrouping && NumUntilNextGroup-- == 0)
			{
				TmpBuffer[StringLen++] = InGroupingSeparatorCharacter;
				NumUntilNextGroup = InSecondaryGroupingSize;
			}

			TmpBuffer[StringLen++] = InDigitCharacters[0];
		}
	}

	// TmpBuffer is backwards, flip it into the final output buffer
	for (int32 FinalBufferIndex = 0; FinalBufferIndex < StringLen; ++FinalBufferIndex)
	{
		InBufferToFill[FinalBufferIndex] = TmpBuffer[StringLen - FinalBufferIndex - 1];
	}
	InBufferToFill[StringLen] = 0;

	return StringLen;
}

FORCEINLINE int32 IntegralToString_Common(const uint64 InVal, const FDecimalNumberFormattingRules& InFormattingRules, const FNumberFormattingOptions& InFormattingOptions, TCHAR* InBufferToFill, const int32 InBufferToFillSize)
{
	// Perform the initial format to a decimal string
	return IntegralToString_UInt64ToString(
		InVal, 
		InFormattingOptions.UseGrouping, 
		InFormattingRules.PrimaryGroupingSize, 
		InFormattingRules.SecondaryGroupingSize, 
		InFormattingRules.GroupingSeparatorCharacter, 
		InFormattingRules.DigitCharacters, 
		InFormattingOptions.MinimumIntegralDigits, 
		InFormattingOptions.MaximumIntegralDigits, 
		InBufferToFill, 
		InBufferToFillSize
		);
}

void FractionalToString_SplitAndRoundNumber(const bool bIsNegative, const double InValue, const int32 InNumDecimalPlaces, ERoundingMode InRoundingMode, double& OutIntegralPart, double& OutFractionalPart)
{
	const int32 DecimalPlacesToRoundTo = FMath::Min(InNumDecimalPlaces, MaxFractionalPrintPrecision);

	const bool bIsRoundingEntireNumber = DecimalPlacesToRoundTo == 0;

	// We split the value before performing the rounding to avoid losing precision during the rounding calculations
	// If we're rounding to zero decimal places, then we just apply rounding to the number as a whole
	double IntegralPart = InValue;
	double FractionalPart = (bIsRoundingEntireNumber) ? 0.0 : FMath::Modf(InValue, &IntegralPart);

	// Multiply the value to round by 10^DecimalPlacesToRoundTo - this will allow us to perform rounding calculations 
	// that correctly trim any remaining fractional parts that are outside of our rounding range
	double& ValueToRound = ((bIsRoundingEntireNumber) ? IntegralPart : FractionalPart);
	ValueToRound *= Pow10Table[DecimalPlacesToRoundTo];

	// The rounding modes here mimic those of ICU. See http://userguide.icu-project.org/formatparse/numbers/rounding-modes
	switch (InRoundingMode)
	{
	case ERoundingMode::HalfToEven:
		// Rounds to the nearest place, equidistant ties go to the value which is closest to an even value: 1.5 becomes 2, 0.5 becomes 0
		ValueToRound = FMath::RoundHalfToEven(ValueToRound);
		break;

	case ERoundingMode::HalfFromZero:
		// Rounds to nearest place, equidistant ties go to the value which is further from zero: -0.5 becomes -1.0, 0.5 becomes 1.0
		ValueToRound = FMath::RoundHalfFromZero(ValueToRound);
		break;
	
	case ERoundingMode::HalfToZero:
		// Rounds to nearest place, equidistant ties go to the value which is closer to zero: -0.5 becomes 0, 0.5 becomes 0
		ValueToRound = FMath::RoundHalfToZero(ValueToRound);
		break;

	case ERoundingMode::FromZero:
		// Rounds to the value which is further from zero, "larger" in absolute value: 0.1 becomes 1, -0.1 becomes -1
		ValueToRound = FMath::RoundFromZero(ValueToRound);
		break;
	
	case ERoundingMode::ToZero:
		// Rounds to the value which is closer to zero, "smaller" in absolute value: 0.1 becomes 0, -0.1 becomes 0
		ValueToRound = FMath::RoundToZero(ValueToRound);
		break;
	
	case ERoundingMode::ToNegativeInfinity:
		// Rounds to the value which is more negative: 0.1 becomes 0, -0.1 becomes -1
		ValueToRound = FMath::RoundToNegativeInfinity(ValueToRound);
		break;
	
	case ERoundingMode::ToPositiveInfinity:
		// Rounds to the value which is more positive: 0.1 becomes 1, -0.1 becomes 0
		ValueToRound = FMath::RoundToPositiveInfinity(ValueToRound);
		break;

	default:
		break;
	}

	// Copy to the correct output param depending on whether we were rounding to the number as a whole
	if (bIsRoundingEntireNumber)
	{
		OutIntegralPart = ValueToRound;
		OutFractionalPart = 0.0;
	}
	else
	{
		// Rounding may have caused the fractional value to overflow, and any overflow will need to be applied to the integral part and stripped from the fractional part
		const double ValueToOverflowTest = (bIsNegative) ? -ValueToRound : ValueToRound;
		if (ValueToOverflowTest >= Pow10Table[DecimalPlacesToRoundTo])
		{
			if (bIsNegative)
			{
				IntegralPart -= 1;
				ValueToRound += Pow10Table[DecimalPlacesToRoundTo];
			}
			else
			{
				IntegralPart += 1;
				ValueToRound -= Pow10Table[DecimalPlacesToRoundTo];
			}
		}

		OutIntegralPart = IntegralPart;
		OutFractionalPart = ValueToRound;
	}
}

void BuildFinalString(const bool bIsNegative, const FDecimalNumberFormattingRules& InFormattingRules, const TCHAR* InIntegralBuffer, const int32 InIntegralLen, const TCHAR* InFractionalBuffer, const int32 InFractionalLen, FString& OutString)
{
	const FString& FinalPrefixStr = (bIsNegative) ? InFormattingRules.NegativePrefixString : InFormattingRules.PositivePrefixString;
	const FString& FinalSuffixStr = (bIsNegative) ? InFormattingRules.NegativeSuffixString : InFormattingRules.PositiveSuffixString;

	OutString.Reserve(OutString.Len() + FinalPrefixStr.Len() + InIntegralLen + 1 + InFractionalLen + FinalSuffixStr.Len());

	OutString.Append(FinalPrefixStr);
	OutString.AppendChars(InIntegralBuffer, InIntegralLen);
	if (InFractionalLen > 0)
	{
		OutString.AppendChar(InFormattingRules.DecimalSeparatorCharacter);
		OutString.AppendChars(InFractionalBuffer, InFractionalLen);
	}
	OutString.Append(FinalSuffixStr);
}

void IntegralToString(const bool bIsNegative, const uint64 InVal, const FDecimalNumberFormattingRules& InFormattingRules, FNumberFormattingOptions InFormattingOptions, FString& OutString)
{
	SanitizeNumberFormattingOptions(InFormattingOptions);

	// Deal with the integral part (produces a string of the integral part, inserting group separators if requested and required, and padding as needed)
	TCHAR IntegralPartBuffer[MinRequiredIntegralBufferSize];
	const int32 IntegralPartLen = IntegralToString_Common(InVal, InFormattingRules, InFormattingOptions, IntegralPartBuffer, ARRAY_COUNT(IntegralPartBuffer));

	// Deal with any forced fractional part (produces a string zeros up to the required minimum length)
	TCHAR FractionalPartBuffer[MinRequiredIntegralBufferSize];
	int32 FractionalPartLen = 0;
	if (InFormattingOptions.MinimumFractionalDigits > 0)
	{
		const int32 PaddingToApply = FMath::Min(InFormattingOptions.MinimumFractionalDigits, MaxFractionalPrintPrecision);
		for (int32 PaddingIndex = 0; PaddingIndex < PaddingToApply; ++PaddingIndex)
		{
			FractionalPartBuffer[FractionalPartLen++] = InFormattingRules.DigitCharacters[0];
		}
	}
	FractionalPartBuffer[FractionalPartLen] = 0;

	BuildFinalString(bIsNegative, InFormattingRules, IntegralPartBuffer, IntegralPartLen, FractionalPartBuffer, FractionalPartLen, OutString);
}

void FractionalToString(const double InVal, const FDecimalNumberFormattingRules& InFormattingRules, FNumberFormattingOptions InFormattingOptions, FString& OutString)
{
	SanitizeNumberFormattingOptions(InFormattingOptions);

	if (FMath::IsNaN(InVal))
	{
		OutString.Append(InFormattingRules.NaNString);
		return;
	}

	const bool bIsNegative = FMath::IsNegativeDouble(InVal);

	double IntegralPart = 0.0;
	double FractionalPart = 0.0;
	FractionalToString_SplitAndRoundNumber(bIsNegative, InVal, InFormattingOptions.MaximumFractionalDigits, InFormattingOptions.RoundingMode, IntegralPart, FractionalPart);

	if (bIsNegative)
	{
		IntegralPart = -IntegralPart;
		FractionalPart = -FractionalPart;
	}

	// Deal with the integral part (produces a string of the integral part, inserting group separators if requested and required, and padding as needed)
	TCHAR IntegralPartBuffer[MinRequiredIntegralBufferSize];
	const int32 IntegralPartLen = IntegralToString_Common(static_cast<uint64>(IntegralPart), InFormattingRules, InFormattingOptions, IntegralPartBuffer, ARRAY_COUNT(IntegralPartBuffer));

	// Deal with the fractional part (produces a string of the fractional part, potentially padding with zeros up to InFormattingOptions.MaximumFractionalDigits)
	TCHAR FractionalPartBuffer[MinRequiredIntegralBufferSize];
	int32 FractionalPartLen = 0;
	if (FractionalPart != 0.0)
	{
		FractionalPartLen = IntegralToString_UInt64ToString(static_cast<uint64>(FractionalPart), false, 0, 0, ' ', InFormattingRules.DigitCharacters, 0, InFormattingOptions.MaximumFractionalDigits, FractionalPartBuffer, ARRAY_COUNT(FractionalPartBuffer));
	
		{
			// Pad the fractional part with any leading zeros that may have been lost when the number was split
			const int32 LeadingZerosToAdd = FMath::Min(InFormattingOptions.MaximumFractionalDigits - FractionalPartLen, MaxFractionalPrintPrecision - FractionalPartLen);
			if (LeadingZerosToAdd > 0)
			{
				FMemory::Memmove(FractionalPartBuffer + LeadingZerosToAdd, FractionalPartBuffer, FractionalPartLen * sizeof(TCHAR));

				for (int32 Index = 0; Index < LeadingZerosToAdd; ++Index)
				{
					FractionalPartBuffer[Index] = InFormattingRules.DigitCharacters[0];
				}

				FractionalPartLen += LeadingZerosToAdd;
			}
		}

		// Trim any trailing zeros back down to InFormattingOptions.MinimumFractionalDigits
		while (FractionalPartLen > InFormattingOptions.MinimumFractionalDigits && FractionalPartBuffer[FractionalPartLen - 1] == InFormattingRules.DigitCharacters[0])
		{
			--FractionalPartLen;
		}
	}
	FractionalPartBuffer[FractionalPartLen] = 0;

	// Pad the fractional part with any zeros that may have been missed so far
	{
		const int32 PaddingToApply = FMath::Min(InFormattingOptions.MinimumFractionalDigits - FractionalPartLen, MaxFractionalPrintPrecision - FractionalPartLen);
		for (int32 PaddingIndex = 0; PaddingIndex < PaddingToApply; ++PaddingIndex)
		{
			FractionalPartBuffer[FractionalPartLen++] = InFormattingRules.DigitCharacters[0];
		}
		FractionalPartBuffer[FractionalPartLen] = 0;
	}

	BuildFinalString(bIsNegative, InFormattingRules, IntegralPartBuffer, IntegralPartLen, FractionalPartBuffer, FractionalPartLen, OutString);
}

} // namespace Internal

FDecimalNumberFormattingRules GetCultureAgnosticFormattingRules()
{
	FDecimalNumberFormattingRules CultureAgnosticFormattingRules;
	CultureAgnosticFormattingRules.NaNString = TEXT("NaN");
	CultureAgnosticFormattingRules.NegativePrefixString = TEXT("-");
	CultureAgnosticFormattingRules.GroupingSeparatorCharacter = ',';
	CultureAgnosticFormattingRules.DecimalSeparatorCharacter = '.';
	CultureAgnosticFormattingRules.PrimaryGroupingSize = 3;
	CultureAgnosticFormattingRules.SecondaryGroupingSize = 3;
	return CultureAgnosticFormattingRules;
}

} // namespace FastDecimalFormat
