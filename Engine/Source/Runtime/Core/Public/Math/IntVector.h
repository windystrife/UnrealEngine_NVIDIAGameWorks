// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/Crc.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/UnrealString.h"

/**
 * Structure for integer vectors in 3-d space.
 */
struct FIntVector
{
	/** Holds the point's x-coordinate. */
	int32 X;
	
	/** Holds the point's y-coordinate. */
	int32 Y;

	/**  Holds the point's z-coordinate. */
	int32 Z;

public:

	/** An int point with zeroed values. */
	CORE_API static const FIntVector ZeroValue;

	/** An int point with INDEX_NONE values. */
	CORE_API static const FIntVector NoneValue;

public:

	/**
	 * Default constructor (no initialization).
	 */
	FIntVector();

	/**
	 * Creates and initializes a new instance with the specified coordinates.
	 *
	 * @param InX The x-coordinate.
	 * @param InY The y-coordinate.
	 * @param InZ The z-coordinate.
	 */
	FIntVector( int32 InX, int32 InY, int32 InZ );

	/**
	 * Constructor
	 *
	 * @param InValue replicated to all components
	 */
	explicit FIntVector( int32 InValue );

	/**
	 * Constructor
	 *
	 * @param InVector float vector converted to int
	 */
	explicit FIntVector( FVector InVector  );

	/**
	 * Constructor
	 *
	 * @param EForceInit Force init enum
	 */
	explicit FORCEINLINE FIntVector( EForceInit );

public:

	/**
	 * Gets specific component of a point.
	 *
	 * @param ComponentIndex Index of point component.
	 * @return const reference to component.
	 */
	const int32& operator()( int32 ComponentIndex ) const;

	/**
	 * Gets specific component of a point.
	 *
	 * @param ComponentIndex Index of point component.
	 * @return reference to component.
	 */
	int32& operator()( int32 ComponentIndex );

	/**
	 * Gets specific component of a point.
	 *
	 * @param ComponentIndex Index of point component.
	 * @return const reference to component.
	 */
	const int32& operator[]( int32 ComponentIndex ) const;

	/**
	 * Gets specific component of a point.
	 *
	 * @param ComponentIndex Index of point component.
	 * @return reference to component.
	 */
	int32& operator[]( int32 ComponentIndex );

	/**
	 * Compares points for equality.
	 *
	 * @param Other The other int point being compared.
	 * @return true if the points are equal, false otherwise..
	 */
	bool operator==( const FIntVector& Other ) const;

	/**
	 * Compares points for inequality.
	 *
	 * @param Other The other int point being compared.
	 * @return true if the points are not equal, false otherwise..
	 */
	bool operator!=( const FIntVector& Other ) const;

	/**
	 * Scales this point.
	 *
	 * @param Scale What to multiply the point by.
	 * @return Reference to this point after multiplication.
	 */
	FIntVector& operator*=( int32 Scale );

	/**
	 * Divides this point.
	 *
	 * @param Divisor What to divide the point by.
	 * @return Reference to this point after division.
	 */
	FIntVector& operator/=( int32 Divisor );

	/**
	 * Adds to this point.
	 *
	 * @param Other The point to add to this point.
	 * @return Reference to this point after addition.
	 */
	FIntVector& operator+=( const FIntVector& Other );

	/**
	 * Subtracts from this point.
	 *
	 * @param Other The point to subtract from this point.
	 * @return Reference to this point after subtraction.
	 */
	FIntVector& operator-=( const FIntVector& Other );

	/**
	 * Assigns another point to this one.
	 *
	 * @param Other The point to assign this point from.
	 * @return Reference to this point after assignment.
	 */
	FIntVector& operator=( const FIntVector& Other );

	/**
	 * Gets the result of scaling on this point.
	 *
	 * @param Scale What to multiply the point by.
	 * @return A new scaled int point.
	 */
	FIntVector operator*( int32 Scale ) const;

	/**
	 * Gets the result of division on this point.
	 *
	 * @param Divisor What to divide the point by.
	 * @return A new divided int point.
	 */
	FIntVector operator/( int32 Divisor ) const;

	/**
	 * Gets the result of addition on this point.
	 *
	 * @param Other The other point to add to this.
	 * @return A new combined int point.
	 */
	FIntVector operator+( const FIntVector& Other ) const;

	/**
	 * Gets the result of subtraction from this point.
	 *
	 * @param Other The other point to subtract from this.
	 * @return A new subtracted int point.
	 */
	FIntVector operator-( const FIntVector& Other ) const;

	/**
	 * Is vector equal to zero.
	 * @return is zero
	*/
	bool IsZero() const;

public:

	/**
	 * Gets the maximum value in the point.
	 *
	 * @return The maximum value in the point.
	 */
	float GetMax() const;

	/**
	 * Gets the minimum value in the point.
	 *
	 * @return The minimum value in the point.
	 */
	float GetMin() const;

	/**
	 * Gets the distance of this point from (0,0,0).
	 *
	 * @return The distance of this point from (0,0,0).
	 */
	int32 Size() const;

	/**
	 * Get a textual representation of this vector.
	 *
	 * @return A string describing the vector.
	 */
	FString ToString() const;

public:

	/**
	 * Divide an int point and round up the result.
	 *
	 * @param lhs The int point being divided.
	 * @param Divisor What to divide the int point by.
	 * @return A new divided int point.
	 */
	static FIntVector DivideAndRoundUp( FIntVector lhs, int32 Divisor );

	/**
	 * Gets the number of components a point has.
	 *
	 * @return Number of components point has.
	 */
	static int32 Num();

public:

	/**
	 * Serializes the Rectangle.
	 *
	 * @param Ar The archive to serialize into.
	 * @param Vector The vector to serialize.
	 * @return Reference to the Archive after serialization.
	 */
	friend FArchive& operator<<( FArchive& Ar, FIntVector& Vector )
	{
		return Ar << Vector.X << Vector.Y << Vector.Z;
	}

	bool Serialize( FArchive& Ar )
	{
		Ar << *this;
		return true;
	}
};


/* FIntVector inline functions
 *****************************************************************************/

FORCEINLINE FIntVector::FIntVector()
{ }


FORCEINLINE FIntVector::FIntVector( int32 InX, int32 InY, int32 InZ )
	: X(InX)
	, Y(InY)
	, Z(InZ)
{ }


FORCEINLINE FIntVector::FIntVector( int32 InValue )
	: X(InValue)
	, Y(InValue)
	, Z(InValue)
{ }


FORCEINLINE FIntVector::FIntVector( EForceInit )
	: X(0)
	, Y(0)
	, Z(0)
{ }


FORCEINLINE const int32& FIntVector::operator()( int32 ComponentIndex ) const
{
	return (&X)[ComponentIndex];
}


FORCEINLINE int32& FIntVector::operator()( int32 ComponentIndex )
{
	return (&X)[ComponentIndex];
}


FORCEINLINE const int32& FIntVector::operator[]( int32 ComponentIndex ) const
{
	return (&X)[ComponentIndex];
}


FORCEINLINE int32& FIntVector::operator[]( int32 ComponentIndex )
{
	return (&X)[ComponentIndex];
}

FORCEINLINE bool FIntVector::operator==( const FIntVector& Other ) const
{
	return X==Other.X && Y==Other.Y && Z==Other.Z;
}


FORCEINLINE bool FIntVector::operator!=( const FIntVector& Other ) const
{
	return X!=Other.X || Y!=Other.Y || Z!=Other.Z;
}


FORCEINLINE FIntVector& FIntVector::operator*=( int32 Scale )
{
	X *= Scale;
	Y *= Scale;
	Z *= Scale;

	return *this;
}


FORCEINLINE FIntVector& FIntVector::operator/=( int32 Divisor )
{
	X /= Divisor;
	Y /= Divisor;
	Z /= Divisor;

	return *this;
}


FORCEINLINE FIntVector& FIntVector::operator+=( const FIntVector& Other )
{
	X += Other.X;
	Y += Other.Y;
	Z += Other.Z;

	return *this;
}


FORCEINLINE FIntVector& FIntVector::operator-=( const FIntVector& Other )
{
	X -= Other.X;
	Y -= Other.Y;
	Z -= Other.Z;

	return *this;
}


FORCEINLINE FIntVector& FIntVector::operator=( const FIntVector& Other )
{
	X = Other.X;
	Y = Other.Y;
	Z = Other.Z;

	return *this;
}


FORCEINLINE FIntVector FIntVector::operator*( int32 Scale ) const
{
	return FIntVector(*this) *= Scale;
}


FORCEINLINE FIntVector FIntVector::operator/( int32 Divisor ) const
{
	return FIntVector(*this) /= Divisor;
}


FORCEINLINE FIntVector FIntVector::operator+( const FIntVector& Other ) const
{
	return FIntVector(*this) += Other;
}

FORCEINLINE FIntVector FIntVector::operator-( const FIntVector& Other ) const
{
	return FIntVector(*this) -= Other;
}


FORCEINLINE FIntVector FIntVector::DivideAndRoundUp( FIntVector lhs, int32 Divisor )
{
	return FIntVector(FMath::DivideAndRoundUp(lhs.X, Divisor), FMath::DivideAndRoundUp(lhs.Y, Divisor), FMath::DivideAndRoundUp(lhs.Z, Divisor));
}


FORCEINLINE float FIntVector::GetMax() const
{
	return FMath::Max(FMath::Max(X, Y), Z);
}


FORCEINLINE float FIntVector::GetMin() const
{
	return FMath::Min(FMath::Min(X, Y), Z);
}


FORCEINLINE int32 FIntVector::Num()
{
	return 3;
}


FORCEINLINE int32 FIntVector::Size() const
{
	int64 X64 = (int64)X;
	int64 Y64 = (int64)Y;
	int64 Z64 = (int64)Z;
	return int32(FMath::Sqrt(float(X64 * X64 + Y64 * Y64 + Z64 * Z64)));
}

FORCEINLINE bool FIntVector::IsZero() const
{
	return *this == ZeroValue;
}


FORCEINLINE FString FIntVector::ToString() const
{
	return FString::Printf(TEXT("X=%d Y=%d Z=%d"), X, Y, Z);
}

FORCEINLINE uint32 GetTypeHash(const FIntVector& Vector)
{
	return FCrc::MemCrc_DEPRECATED(&Vector,sizeof(FIntVector));
}

struct FIntVector4
{
	int32 X, Y, Z, W;

	FORCEINLINE FIntVector4()
	{
	}

	FORCEINLINE FIntVector4(int32 InX, int32 InY, int32 InZ, int32 InW)
		: X(InX)
		, Y(InY)
		, Z(InZ)
		, W(InW)
	{
	}

	FORCEINLINE explicit FIntVector4(int32 InValue)
		: X(InValue)
		, Y(InValue)
		, Z(InValue)
		, W(InValue)
	{
	}

	FORCEINLINE FIntVector4(EForceInit)
		: X(0)
		, Y(0)
		, Z(0)
		, W(0)
	{
	}

	FORCEINLINE const int32& operator[](int32 ComponentIndex) const
	{
		return (&X)[ComponentIndex];
	}


	FORCEINLINE int32& operator[](int32 ComponentIndex)
	{
		return (&X)[ComponentIndex];
	}

	FORCEINLINE bool operator==(const FIntVector4& Other) const
	{
		return X==Other.X && Y==Other.Y && Z==Other.Z && W==Other.W;
	}


	FORCEINLINE bool operator!=(const FIntVector4& Other) const
	{
		return X!=Other.X || Y!=Other.Y || Z!=Other.Z || W!=Other.W;
	}
};

struct FUintVector4
{
	uint32 X, Y, Z, W;

	FORCEINLINE FUintVector4()
	{
	}

	FORCEINLINE FUintVector4(uint32 InX, uint32 InY, uint32 InZ, uint32 InW)
		: X(InX)
		, Y(InY)
		, Z(InZ)
		, W(InW)
	{
	}

	FORCEINLINE explicit FUintVector4(uint32 InValue)
		: X(InValue)
		, Y(InValue)
		, Z(InValue)
		, W(InValue)
	{
	}

	FORCEINLINE FUintVector4(EForceInit)
		: X(0)
		, Y(0)
		, Z(0)
		, W(0)
	{
	}

	FORCEINLINE const uint32& operator[](int32 ComponentIndex) const
	{
		return (&X)[ComponentIndex];
	}


	FORCEINLINE uint32& operator[](int32 ComponentIndex)
	{
		return (&X)[ComponentIndex];
	}

	FORCEINLINE bool operator==(const FUintVector4& Other) const
	{
		return X==Other.X && Y==Other.Y && Z==Other.Z && W==Other.W;
	}


	FORCEINLINE bool operator!=(const FUintVector4& Other) const
	{
		return X!=Other.X || Y!=Other.Y || Z!=Other.Z || W!=Other.W;
	}
};

template <> struct TIsPODType<FIntVector> { enum { Value = true }; };
template <> struct TIsPODType<FIntVector4> { enum { Value = true }; };
template <> struct TIsPODType<FUintVector4> { enum { Value = true }; };
