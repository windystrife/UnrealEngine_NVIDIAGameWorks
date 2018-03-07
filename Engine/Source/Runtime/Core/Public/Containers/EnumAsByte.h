// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/IsPODType.h"
#include "Templates/IsEnumClass.h"

template <bool> struct TEnumAsByte_EnumClass;
template <> struct DEPRECATED(4.15, "TEnumAsByte is not intended for use with enum classes - please derive your enum class from uint8 instead.") TEnumAsByte_EnumClass<true> {};
template <> struct TEnumAsByte_EnumClass<false> {};

/**
 * Template to store enumeration values as bytes in a type-safe way.
 */
template<class TEnum>
class TEnumAsByte
{
	typedef TEnumAsByte_EnumClass<TIsEnumClass<TEnum>::Value> Check;

public:
	typedef TEnum EnumType;

	/** Default Constructor (no initialization). */
	FORCEINLINE TEnumAsByte() { }

	/**
	 * Copy constructor.
	 *
	 * @param InValue value to construct with.
	 */
	FORCEINLINE TEnumAsByte( const TEnumAsByte &InValue )
		: Value(InValue.Value)
	{ }

	/**
	 * Constructor, initialize to the enum value.
	 *
	 * @param InValue value to construct with.
	 */
	FORCEINLINE TEnumAsByte( TEnum InValue )
		: Value(static_cast<uint8>(InValue))
	{ }

	/**
	 * Constructor, initialize to the int32 value.
	 *
	 * @param InValue value to construct with.
	 */
	explicit FORCEINLINE TEnumAsByte( int32 InValue )
		: Value(static_cast<uint8>(InValue))
	{ }

	/**
	 * Constructor, initialize to the int32 value.
	 *
	 * @param InValue value to construct with.
	 */
	explicit FORCEINLINE TEnumAsByte( uint8 InValue )
		: Value(InValue)
	{ }

public:

	/**
	 * Assignment operator.
	 *
	 * @param InValue value to set.
	 * @return This instance.
	 */
	FORCEINLINE TEnumAsByte& operator=( TEnumAsByte InValue )
	{
		Value = InValue.Value;
		return *this;
	}
	/**
	 * Assignment operator.
	 *
	 * @param InValue value to set.
	 * @return This instance.
	 */
	FORCEINLINE TEnumAsByte& operator=( TEnum InValue )
	{
		Value = static_cast<uint8>(InValue);
		return *this;
	}

	/**
	 * Compares two enumeration values for equality.
	 *
	 * @param InValue The value to compare with.
	 * @return true if the two values are equal, false otherwise.
	 */
	bool operator==( TEnum InValue ) const
	{
		return static_cast<TEnum>(Value) == InValue;
	}

	/**
	 * Compares two enumeration values for equality.
	 *
	 * @param InValue The value to compare with.
	 * @return true if the two values are equal, false otherwise.
	 */
	bool operator==(TEnumAsByte InValue) const
	{
		return Value == InValue.Value;
	}

	/** Implicit conversion to TEnum. */
	operator TEnum() const
	{
		return (TEnum)Value;
	}

public:

	/**
	 * Gets the enumeration value.
	 *
	 * @return The enumeration value.
	 */
	TEnum GetValue() const
	{
		return (TEnum)Value;
	}

private:

	/** Holds the value as a byte. **/
	uint8 Value;


	FORCEINLINE friend uint32 GetTypeHash(const TEnumAsByte& Enum)
	{
		return GetTypeHash(Enum.Value);
	}
};


template<class T> struct TIsPODType<TEnumAsByte<T>> { enum { Value = true }; };
