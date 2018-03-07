// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/TextProperty.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "KismetTextLibrary.generated.h"

#if !CPP
/** This code is only used for meta-data extraction and the main declaration is with FText, be sure to update there as well */

/** Provides rounding modes for converting numbers into localized text */
UENUM(BlueprintType)
enum ERoundingMode
{
	/** Rounds to the nearest place, equidistant ties go to the value which is closest to an even value: 1.5 becomes 2, 0.5 becomes 0 */
	HalfToEven,
	/** Rounds to nearest place, equidistant ties go to the value which is further from zero: -0.5 becomes -1.0, 0.5 becomes 1.0 */
	HalfFromZero,
	/** Rounds to nearest place, equidistant ties go to the value which is closer to zero: -0.5 becomes 0, 0.5 becomes 0. */
	HalfToZero,
	/** Rounds to the value which is further from zero, "larger" in absolute value: 0.1 becomes 1, -0.1 becomes -1 */
	FromZero,
	/** Rounds to the value which is closer to zero, "smaller" in absolute value: 0.1 becomes 0, -0.1 becomes 0 */
	ToZero,
	/** Rounds to the value which is more negative: 0.1 becomes 0, -0.1 becomes -1 */
	ToNegativeInfinity,
	/** Rounds to the value which is more positive: 0.1 becomes 1, -0.1 becomes 0 */
	ToPositiveInfinity,
};

UENUM(BlueprintType)
enum class ETextGender : uint8
{
	Masculine,
	Feminine,
	Neuter,
};

UENUM(BlueprintType)
namespace EFormatArgumentType
{
	enum Type
	{
		Int,
		UInt,
		Float,
		Double,
		Text,
		Gender,
	};
}

/**
 * Used to pass argument/value pairs into FText::Format.
 * The full C++ struct is located here: Engine\Source\Runtime\Core\Public\Internationalization\Text.h
 */
USTRUCT(noexport, BlueprintInternalUseOnly)
struct FFormatArgumentData
{
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=ArgumentName)
	FString ArgumentName;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=ArgumentValue)
	TEnumAsByte<EFormatArgumentType::Type> ArgumentValueType;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=ArgumentValue)
	FText ArgumentValue;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=ArgumentValue)
	int32 ArgumentValueInt;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=ArgumentValue)
	float ArgumentValueFloat;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=ArgumentValue)
	ETextGender ArgumentValueGender;
};
#endif

UCLASS(meta=(BlueprintThreadSafe))
class ENGINE_API UKismetTextLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/** Converts a vector value to localized formatted text, in the form 'X= Y= Z=' */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ToText (Vector)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Utilities|Text")
	static FText Conv_VectorToText(FVector InVec);

	/** Converts a vector2d value to localized formatted text, in the form 'X= Y=' */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ToText (vector2d)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Utilities|Text")
	static FText Conv_Vector2dToText(FVector2D InVec);

	/** Converts a rotator value to localized formatted text, in the form 'P= Y= R=' */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ToText (rotator)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Utilities|Text")
	static FText Conv_RotatorToText(FRotator InRot);

	/** Converts a transform value to localized formatted text, in the form 'Translation: X= Y= Z= Rotation: P= Y= R= Scale: X= Y= Z=' */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ToText (transform)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Utilities|Text")
	static FText Conv_TransformToText(const FTransform& InTrans);

	/** Converts a UObject value to culture invariant text by calling the object's GetName method */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ToText (object)", BlueprintAutocast), Category = "Utilities|Text")
	static FText Conv_ObjectToText(class UObject* InObj);

	/** Converts a linear color value to localized formatted text, in the form '(R=,G=,B=,A=)' */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ToText (linear color)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Utilities|Text")
	static FText Conv_ColorToText(FLinearColor InColor);

	/** Converts localizable text to the string */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ToString (text)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Utilities|String")
	static FString Conv_TextToString(const FText& InText);

	/** Converts string to culture invariant text. Use Format or Make Literal Text to create localizable text */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToText (string)", BlueprintAutocast), Category="Utilities|Text")
	static FText Conv_StringToText(const FString& InString);

	/** Converts Name to culture invariant text */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToText (name)", BlueprintAutocast), Category="Utilities|Text")
	static FText Conv_NameToText(FName InName);

	/* Returns true if text is empty. */
	UFUNCTION(BlueprintPure, Category="Utilities|Text")
	static bool TextIsEmpty(const FText& InText);

	/* Returns true if text is transient. */
	UFUNCTION(BlueprintPure, Category="Utilities|Text")
	static bool TextIsTransient(const FText& InText);

	/* Returns true if text is culture invariant. */
	UFUNCTION(BlueprintPure, Category="Utilities|Text")
	static bool TextIsCultureInvariant(const FText& InText);

	/**
	 * Transforms the text to lowercase in a culture correct way.
	 * @note The returned instance is linked to the original and will be rebuilt if the active culture is changed.
	 */
	UFUNCTION(BlueprintPure, Category="Utilities|Text")
	static FText TextToLower(const FText& InText);

	/**
	 * Transforms the text to uppercase in a culture correct way.
	 * @note The returned instance is linked to the original and will be rebuilt if the active culture is changed.
	 */
	UFUNCTION(BlueprintPure, Category="Utilities|Text")
	static FText TextToUpper(const FText& InText);

	/* Removes whitespace characters from the front of the text. */
	UFUNCTION(BlueprintPure, Category="Utilities|Text")
	static FText TextTrimPreceding(const FText& InText);

	/* Removes trailing whitespace characters. */
	UFUNCTION(BlueprintPure, Category="Utilities|Text")
	static FText TextTrimTrailing(const FText& InText);

	/* Removes whitespace characters from the front and end of the text. */
	UFUNCTION(BlueprintPure, Category="Utilities|Text")
	static FText TextTrimPrecedingAndTrailing(const FText& InText);

	/* Returns an empty piece of text. */
	UFUNCTION(BlueprintPure, Category="Utilities|Text")
	static FText GetEmptyText();

	/* Attempts to find existing Text using the representation found in the loc tables for the specified namespace and key. */
	UFUNCTION(BlueprintPure, Category="Utilities|Text")
	static bool FindTextInLocalizationTable(const FString& Namespace, const FString& Key, FText& OutText);

	/* Returns true if A and B are linguistically equal (A == B). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal (text)", CompactNodeTitle = "=="), Category="Utilities|Text")
	static bool EqualEqual_TextText(const FText& A, const FText& B);

	/* Returns true if A and B are linguistically equal (A == B), ignoring case. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal, Case Insensitive (text)", CompactNodeTitle = "=="), Category="Utilities|Text")
	static bool EqualEqual_IgnoreCase_TextText(const FText& A, const FText& B);
				
	/* Returns true if A and B are linguistically not equal (A != B). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "NotEqual (text)", CompactNodeTitle = "!="), Category="Utilities|Text")
	static bool NotEqual_TextText(const FText& A, const FText& B);

	/* Returns true if A and B are linguistically not equal (A != B), ignoring case. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "NotEqual, Case Insensitive (text)", CompactNodeTitle = "!="), Category="Utilities|Text")
	static bool NotEqual_IgnoreCase_TextText(const FText& A, const FText& B);

	/** Converts a boolean value to formatted text, either 'true' or 'false' */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToText (boolean)", CompactNodeTitle = "->", BlueprintAutocast), Category="Utilities|Text")
	static FText Conv_BoolToText(bool InBool);

	/** Converts a byte value to formatted text */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToText (byte)", CompactNodeTitle = "->", BlueprintAutocast), Category="Utilities|Text")
	static FText Conv_ByteToText(uint8 Value);

	// Default values are duplicated from FNumberFormattingOptions and should be replicated in all functions and in the struct when changed!
	/* Converts a passed in integer to text based on formatting options */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToText (int)", AdvancedDisplay = "1", BlueprintAutocast), Category="Utilities|Text")
	static FText Conv_IntToText(int32 Value, bool bUseGrouping = true, int32 MinimumIntegralDigits = 1, int32 MaximumIntegralDigits = 324);

	// Default values are duplicated from FNumberFormattingOptions and should be replicated in all functions and in the struct when changed!
	/* Converts a passed in float to text based on formatting options */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToText (float)", AdvancedDisplay = "1", BlueprintAutocast), Category="Utilities|Text")
	static FText Conv_FloatToText(float Value, TEnumAsByte<ERoundingMode> RoundingMode, bool bUseGrouping = true, int32 MinimumIntegralDigits = 1, int32 MaximumIntegralDigits = 324, int32 MinimumFractionalDigits = 0, int32 MaximumFractionalDigits = 3);
	
	/**
	 * Generate an FText that represents the passed number as currency in the current culture.
	 * BaseVal is specified in the smallest fractional value of the currency and will be converted for formatting according to the selected culture.
	 * Keep in mind the CurrencyCode is completely independent of the culture it's displayed in (and they do not imply one another).
	 * For example: FText::AsCurrencyBase(650, TEXT("EUR")); would return an FText of "<EUR>6.50" in most English cultures (en_US/en_UK) and "6,50<EUR>" in Spanish (es_ES) (where <EUR> is U+20AC)
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "AsCurrency"), Category = "Utilities|Text")
	static FText AsCurrencyBase(int32 BaseValue, const FString& CurrencyCode);

	// Default values are duplicated from FNumberFormattingOptions and should be replicated in all functions and in the struct when changed!
	/* Converts a passed in integer to a text formatted as a currency */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "AsCurrency (int) - DEPRECATED (use AsCurrency)"), Category="Utilities|Text")
	static FText AsCurrency_Integer(int32 Value, TEnumAsByte<ERoundingMode> RoundingMode, bool bUseGrouping = true, int32 MinimumIntegralDigits = 1, int32 MaximumIntegralDigits = 324, int32 MinimumFractionalDigits = 0, int32 MaximumFractionalDigits = 3, const FString& CurrencyCode = TEXT(""));

	// Default values are duplicated from FNumberFormattingOptions and should be replicated in all functions and in the struct when changed!
	/* Converts a passed in float to a text formatted as a currency */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "AsCurrency (float) - DEPRECATED (use AsCurrency)"), Category="Utilities|Text")
	static FText AsCurrency_Float(float Value, TEnumAsByte<ERoundingMode> RoundingMode, bool bUseGrouping = true, int32 MinimumIntegralDigits = 1, int32 MaximumIntegralDigits = 324, int32 MinimumFractionalDigits = 0, int32 MaximumFractionalDigits = 3, const FString& CurrencyCode = TEXT(""));

	// Default values are duplicated from FNumberFormattingOptions and should be replicated in all functions and in the struct when changed!
	/* Converts a passed in float to a text, formatted as a percent */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "AsPercent", AdvancedDisplay = "1"), Category="Utilities|Text")
	static FText AsPercent_Float(float Value, TEnumAsByte<ERoundingMode> RoundingMode, bool bUseGrouping = true, int32 MinimumIntegralDigits = 1, int32 MaximumIntegralDigits = 324, int32 MinimumFractionalDigits = 0, int32 MaximumFractionalDigits = 3);

	/* Converts a passed in date & time to a text, formatted as a date using an invariant timezone. This will use the given date & time as-is, so it's assumed to already be in the correct timezone. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "AsDate", AdvancedDisplay = "1"), Category="Utilities|Text")
	static FText AsDate_DateTime(const FDateTime& InDateTime);

	/* Converts a passed in date & time to a text, formatted as a date using the given timezone (default is the local timezone). This will convert the given date & time from UTC to the given timezone (taking into account DST). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "AsDate (from UTC)", AdvancedDisplay = "1"), Category="Utilities|Text")
	static FText AsTimeZoneDate_DateTime(const FDateTime& InDateTime, const FString& InTimeZone = TEXT(""));

	/* Converts a passed in date & time to a text, formatted as a date & time using an invariant timezone. This will use the given date & time as-is, so it's assumed to already be in the correct timezone. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "AsDateTime", AdvancedDisplay = "1"), Category="Utilities|Text")
	static FText AsDateTime_DateTime(const FDateTime& In);

	/* Converts a passed in date & time to a text, formatted as a date & time using the given timezone (default is the local timezone). This will convert the given date & time from UTC to the given timezone (taking into account DST). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "AsDateTime (from UTC)", AdvancedDisplay = "1"), Category="Utilities|Text")
	static FText AsTimeZoneDateTime_DateTime(const FDateTime& InDateTime, const FString& InTimeZone = TEXT(""));

	/* Converts a passed in date & time to a text, formatted as a time using an invariant timezone. This will use the given date & time as-is, so it's assumed to already be in the correct timezone. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "AsTime", AdvancedDisplay = "1"), Category="Utilities|Text")
	static FText AsTime_DateTime(const FDateTime& In);

	/* Converts a passed in date & time to a text, formatted as a time using the given timezone (default is the local timezone). This will convert the given date & time from UTC to the given timezone (taking into account DST). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "AsTime (from UTC)", AdvancedDisplay = "1"), Category="Utilities|Text")
	static FText AsTimeZoneTime_DateTime(const FDateTime& InDateTime, const FString& InTimeZone = TEXT(""));

	/* Converts a passed in time span to a text, formatted as a time span */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "AsTimespan", AdvancedDisplay = "1"), Category="Utilities|Text")
	static FText AsTimespan_Timespan(const FTimespan& InTimespan);

	/* Used for formatting text using the FText::Format function and utilized by the UK2Node_FormatText */
	UFUNCTION(BlueprintPure, meta=(BlueprintInternalUseOnly = "true"))
	static FText Format(FText InPattern, TArray<FFormatArgumentData> InArgs);

	/** Returns true if the given text is referencing a string table. */
	UFUNCTION(BlueprintPure, Category="Utilities|Text", meta=(DisplayName="Is Text from String Table"))
	static bool TextIsFromStringTable(const FText& Text);

	/**
	 * Attempts to create a text instance from a string table ID and key.
	 * @note This exists to allow programmatic ‎look-up of a string table entry from dynamic content - you should favor setting your string table reference on a text property or pin wherever possible as it is significantly more robust (see "Make Literal Text").
	 * @return The found text, or a dummy text if the entry could not be found.
	 */
	UFUNCTION(BlueprintPure, Category="Utilities|Text", meta=(DisplayName="Make Text from String Table (Advanced)"))
	static FText TextFromStringTable(const FName TableId, const FString& Key);

	/**
	 * Attempts to find the String Table ID and key used by the given text.
	 * @return True if the String Table ID and key were found, false otherwise.
	 */
	UFUNCTION(BlueprintPure, Category="Utilities|Text", meta=(DisplayName="Find String Table ID and Key from Text"))
	static bool StringTableIdAndKeyFromText(FText Text, FName& OutTableId, FString& OutKey);
};
