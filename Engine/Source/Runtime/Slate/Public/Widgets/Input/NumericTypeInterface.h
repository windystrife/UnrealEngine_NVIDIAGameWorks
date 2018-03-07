// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/ValueOrError.h"
#include "Misc/ExpressionParserTypes.h"
#include "Math/BasicMathExpressionEvaluator.h"

enum class EUnit : uint8;

/** Interface to provide specific functionality for dealing with a numeric type. Currently includes string conversion functionality. */
template<typename NumericType>
struct INumericTypeInterface
{
	virtual ~INumericTypeInterface() {}

	/** Convert the type to/from a string */
	virtual FString ToString(const NumericType& Value) const = 0;
	virtual TOptional<NumericType> FromString(const FString& InString, const NumericType& ExistingValue) = 0;

	/** Check whether the typed character is valid */
	virtual bool IsCharacterValid(TCHAR InChar) const = 0;
};

/** Default numeric type interface */
template<typename NumericType>
struct TDefaultNumericTypeInterface : INumericTypeInterface<NumericType>
{
	/** Convert the type to/from a string */
	virtual FString ToString(const NumericType& Value) const override
	{
		return Lex::ToSanitizedString(Value);
	}
	virtual TOptional<NumericType> FromString(const FString& InString, const NumericType& InExistingValue) override
	{
		static FBasicMathExpressionEvaluator Parser;

		TValueOrError<double, FExpressionError> Result = Parser.Evaluate(*InString, double(InExistingValue));
		if (Result.IsValid())
		{
			return NumericType(Result.GetValue());
		}

		return TOptional<NumericType>();
	}

	/** Check whether the typed character is valid */
	virtual bool IsCharacterValid(TCHAR InChar) const override
	{
		static FString ValidChars(TEXT("1234567890()-+=\\/.,*^%%"));
		int32 OutUnused;
		return ValidChars.FindChar(InChar, OutUnused);
	}
};

/** Forward declaration of types defined in UnitConversion.h */
enum class EUnit : uint8;
template<typename> struct FNumericUnit;

/**
 * Numeric interface that specifies how to interact with a number in a specific unit.
 * Include NumericUnitTypeInterface.inl for symbol definitions.
 */
template<typename NumericType>
struct TNumericUnitTypeInterface : TDefaultNumericTypeInterface<NumericType>
{
	/** The underlying units which the numeric type are specfied in. */
	const EUnit UnderlyingUnits;

	/** Optional units that this type interface will be fixed on */
	TOptional<EUnit> FixedDisplayUnits;

	/** Constructor */
	TNumericUnitTypeInterface(EUnit InUnits);

	/** Convert this type to a string */
	virtual FString ToString(const NumericType& Value) const override;

	/** Attempt to parse a numeral with our units from the specified string. */
	virtual TOptional<NumericType> FromString(const FString& ValueString, const NumericType& InExistingValue) override;

	/** Check whether the specified typed character is valid */
	virtual bool IsCharacterValid(TCHAR InChar) const override;

	/** Set up this interface to use a fixed display unit based on the specified value */
	void SetupFixedDisplay(const NumericType& InValue);

private:
	/** Called when the global unit settings have changed, if this type interface is using the default input units */
	void OnGlobalUnitSettingChanged();
};
