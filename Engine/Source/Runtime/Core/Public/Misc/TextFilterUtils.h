// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"

/** Defines the comparison operators that can be used for a complex (key->value) comparison */
enum class ETextFilterComparisonOperation : uint8
{
	Equal,
	NotEqual,
	Less,
	LessOrEqual,
	Greater,
	GreaterOrEqual,
};

/** Defines the different ways that a string can be compared while evaluating the expression */
enum class ETextFilterTextComparisonMode : uint8
{
	Exact,
	Partial,
	StartsWith,
	EndsWith,
};

/**
 * String used by the text filter.
 * The given string will be stored as uppercase since filter text always performs case-insensitive string comparisons, so this will minimize ToUpper calls.
 */
class CORE_API FTextFilterString
{
public:
	/** Default constructor */
	FTextFilterString();

	/** Move and copy constructors */
	FTextFilterString(const FTextFilterString& Other);
	FTextFilterString(FTextFilterString&& Other);

	/** Construct from a string */
	FTextFilterString(const FString& InString);
	FTextFilterString(FString&& InString);
	FTextFilterString(const TCHAR* InString);

	/** Construct from a name */
	FTextFilterString(const FName& InName);

	/** Move and copy assignment */
	FTextFilterString& operator=(const FTextFilterString& Other);
	FTextFilterString& operator=(FTextFilterString&& Other);

	/** Compare this string against the other, using the text comparison mode provided */
	bool CompareText(const FTextFilterString& InOther, const ETextFilterTextComparisonMode InTextComparisonMode) const;

	/** Are the two given strings able to be compared numberically? */
	bool CanCompareNumeric(const FTextFilterString& InOther) const;

	/** Compare this string against the other, converting them to numbers and using the comparison operator provided - you should have tested CanCompareNumeric first! */
	bool CompareNumeric(const FTextFilterString& InOther, const ETextFilterComparisonOperation InComparisonOperation) const;

	/** Get the internal uppercase string of this filter string */
	FORCEINLINE const FString& AsString() const
	{
		return InternalString;
	}

	/** Get the internal uppercase string of this filter string as an FName */
	FORCEINLINE FName AsName() const
	{
		return FName(*InternalString);
	}

	/** Is the internal string empty? */
	FORCEINLINE bool IsEmpty() const
	{
		return InternalString.IsEmpty();
	}

private:
	/** Inline convert our internal string to uppercase */
	void UppercaseInternalString();

	/** The uppercase string to use for comparisons */
	FString InternalString;
};

namespace TextFilterUtils
{
	/** Utility function to perform a basic string test for the given values */
	CORE_API bool TestBasicStringExpression(const FTextFilterString& InValue1, const FTextFilterString& InValue2, const ETextFilterTextComparisonMode InTextComparisonMode);

	/** Utility function to perform a complex expression test for the given values */
	CORE_API bool TestComplexExpression(const FTextFilterString& InValue1, const FTextFilterString& InValue2, const ETextFilterComparisonOperation InComparisonOperation, const ETextFilterTextComparisonMode InTextComparisonMode);
}
