// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/UnrealString.h"
#include "Templates/TypeHash.h"

/**
 * Structure for integer points in 2-d space.
 *
 * @todo Docs: The operators need better documentation, i.e. what does it mean to divide a point?
 */
struct FIntPoint
{
	/** Holds the point's x-coordinate. */
	int32 X;
	
	/** Holds the point's y-coordinate. */
	int32 Y;

public:

	/** An integer point with zeroed values. */
	CORE_API static const FIntPoint ZeroValue;

	/** An integer point with INDEX_NONE values. */
	CORE_API static const FIntPoint NoneValue;

public:

	/** Default constructor (no initialization). */
	FIntPoint();

	/**
	 * Create and initialize a new instance with the specified coordinates.
	 *
	 * @param InX The x-coordinate.
	 * @param InY The y-coordinate.
	 */
	FIntPoint(int32 InX, int32 InY);

	/**
	 * Create and initialize a new instance to zero.
	 *
	 * @param EForceInit Force init enum
	 */
	explicit FORCEINLINE FIntPoint(EForceInit);

public:

	/**
	 * Get specific component of a point.
	 *
	 * @param PointIndex Index of point component.
	 * @return const reference to component.
	 */
	const int32& operator()(int32 PointIndex) const;

	/**
	 * Get specific component of a point.
	 *
	 * @param PointIndex Index of point component
	 * @return reference to component.
	 */
	int32& operator()(int32 PointIndex);

	/**
	 * Compare two points for equality.
	 *
	 * @param Other The other int point being compared.
	 * @return true if the points are equal, false otherwise.
	 */
	bool operator==(const FIntPoint& Other) const;

	/**
	 * Compare two points for inequality.
	 *
	 * @param Other The other int point being compared.
	 * @return true if the points are not equal, false otherwise.
	 */
	bool operator!=(const FIntPoint& Other) const;

	/**
	 * Scale this point.
	 *
	 * @param Scale What to multiply the point by.
	 * @return Reference to this point after multiplication.
	 */
	FIntPoint& operator*=(int32 Scale);

	/**
	 * Divide this point by a scalar.
	 *
	 * @param Divisor What to divide the point by.
	 * @return Reference to this point after division.
	 */
	FIntPoint& operator/=(int32 Divisor);

	/**
	 * Add another point component-wise to this point.
	 *
	 * @param Other The point to add to this point.
	 * @return Reference to this point after addition.
	 */
	FIntPoint& operator+=(const FIntPoint& Other);
	
	/**
	 * Subtract another point component-wise from this point.
	 *
	 * @param Other The point to subtract from this point.
	 * @return Reference to this point after subtraction.
	 */
	FIntPoint& operator-=(const FIntPoint& Other);

	/**
	 * Divide this point component-wise by another point.
	 *
	 * @param Other The point to divide with.
	 * @return Reference to this point after division.
	 */
	FIntPoint& operator/=(const FIntPoint& Other);

	/**
	 * Assign another point to this one.
	 *
	 * @param Other The point to assign this point from.
	 * @return Reference to this point after assignment.
	 */
	FIntPoint& operator=(const FIntPoint& Other);

	/**
	 * Get the result of scaling on this point.
	 *
	 * @param Scale What to multiply the point by.
	 * @return A new scaled int point.
	 */
	FIntPoint operator*(int32 Scale) const;

	/**
	 * Get the result of division on this point.
	 *
	 * @param Divisor What to divide the point by.
	 * @return A new divided int point.
	 */
	FIntPoint operator/(int32 Divisor) const;

	/**
	 * Get the result of addition on this point.
	 *
	 * @param Other The other point to add to this.
	 * @return A new combined int point.
	 */
	FIntPoint operator+(const FIntPoint& Other) const;

	/**
	 * Get the result of subtraction from this point.
	 *
	 * @param Other The other point to subtract from this.
	 * @return A new subtracted int point.
	 */
	FIntPoint operator-(const FIntPoint& Other) const;

	/**
	 * Get the result of division on this point.
	 *
	 * @param Other The other point to subtract from this.
	 * @return A new subtracted int point.
	 */
	FIntPoint operator/(const FIntPoint& Other) const;

	/**
	* Get specific component of the point.
	*
	* @param Index the index of point component
	* @return reference to component.
	*/
	int32& operator[](int32 Index);

	/**
	* Get specific component of the point.
	*
	* @param Index the index of point component
	* @return copy of component value.
	*/
	int32 operator[](int32 Index) const;

public:

	/**
	 * Get the component-wise min of two points.
	 *
	 * @see ComponentMax, GetMax
	 */
	FORCEINLINE FIntPoint ComponentMin(const FIntPoint& Other) const;

	/**
	 * Get the component-wise max of two points.
	 *
	 * @see ComponentMin, GetMin
	 */
	FORCEINLINE FIntPoint ComponentMax(const FIntPoint& Other) const;

	/**
	 * Get the larger of the point's two components.
	 *
	 * @return The maximum component of the point.
	 * @see GetMin, Size, SizeSquared
	 */
	int32 GetMax() const;

	/**
	 * Get the smaller of the point's two components.
	 *
	 * @return The minimum component of the point.
	 * @see GetMax, Size, SizeSquared
	 */
	int32 GetMin() const;

	/**
	 * Get the distance of this point from (0,0).
	 *
	 * @return The distance of this point from (0,0).
	 * @see GetMax, GetMin, SizeSquared
	 */
	int32 Size() const;

	/**
	 * Get the squared distance of this point from (0,0).
	 *
	 * @return The squared distance of this point from (0,0).
	 * @see GetMax, GetMin, Size
	 */
	int32 SizeSquared() const;
	
	/**
	 * Get a textual representation of this point.
	 *
	 * @return A string describing the point.
	 */
	FString ToString() const;

public:

	/**
	 * Divide an int point and round up the result.
	 *
	 * @param lhs The int point being divided.
	 * @param Divisor What to divide the int point by.
	 * @return A new divided int point.
	 * @see DivideAndRoundDown
	 */
	static FIntPoint DivideAndRoundUp(FIntPoint lhs, int32 Divisor);
	static FIntPoint DivideAndRoundUp(FIntPoint lhs, FIntPoint Divisor);

	/**
	 * Divide an int point and round down the result.
	 *
	 * @param lhs The int point being divided.
	 * @param Divisor What to divide the int point by.
	 * @return A new divided int point.
	 * @see DivideAndRoundUp
	 */
	static FIntPoint DivideAndRoundDown(FIntPoint lhs, int32 Divisor);

	/**
	 * Get number of components point has.
	 *
	 * @return number of components point has.
	 */
	static int32 Num();

public:

	/**
	 * Serialize the point.
	 *
	 * @param Ar The archive to serialize into.
	 * @param Point The point to serialize.
	 * @return Reference to the Archive after serialization.
	 */
	friend FArchive& operator<<(FArchive& Ar, FIntPoint& Point)
	{
		return Ar << Point.X << Point.Y;
	}

	/**
	 * Serialize the point.
	 *
	 * @param Ar The archive to serialize into.
	 * @return true on success, false otherwise.
	 */
	bool Serialize(FArchive& Ar)
	{
		Ar << *this;
		return true;
	}
};


/* FIntPoint inline functions
 *****************************************************************************/

FORCEINLINE FIntPoint::FIntPoint() { }


FORCEINLINE FIntPoint::FIntPoint(int32 InX, int32 InY)
	: X(InX)
	, Y(InY)
{ }


FORCEINLINE FIntPoint::FIntPoint(EForceInit)
	: X(0)
	, Y(0)
{ }


FORCEINLINE const int32& FIntPoint::operator()(int32 PointIndex) const
{
	return (&X)[PointIndex];
}


FORCEINLINE int32& FIntPoint::operator()(int32 PointIndex)
{
	return (&X)[PointIndex];
}


FORCEINLINE int32 FIntPoint::Num()
{
	return 2;
}


FORCEINLINE bool FIntPoint::operator==(const FIntPoint& Other) const
{
	return X==Other.X && Y==Other.Y;
}


FORCEINLINE bool FIntPoint::operator!=(const FIntPoint& Other) const
{
	return (X != Other.X) || (Y != Other.Y);
}


FORCEINLINE FIntPoint& FIntPoint::operator*=(int32 Scale)
{
	X *= Scale;
	Y *= Scale;

	return *this;
}


FORCEINLINE FIntPoint& FIntPoint::operator/=(int32 Divisor)
{
	X /= Divisor;
	Y /= Divisor;

	return *this;
}


FORCEINLINE FIntPoint& FIntPoint::operator+=(const FIntPoint& Other)
{
	X += Other.X;
	Y += Other.Y;

	return *this;
}


FORCEINLINE FIntPoint& FIntPoint::operator-=(const FIntPoint& Other)
{
	X -= Other.X;
	Y -= Other.Y;

	return *this;
}


FORCEINLINE FIntPoint& FIntPoint::operator/=(const FIntPoint& Other)
{
	X /= Other.X;
	Y /= Other.Y;

	return *this;
}


FORCEINLINE FIntPoint& FIntPoint::operator=(const FIntPoint& Other)
{
	X = Other.X;
	Y = Other.Y;

	return *this;
}


FORCEINLINE FIntPoint FIntPoint::operator*(int32 Scale) const
{
	return FIntPoint(*this) *= Scale;
}


FORCEINLINE FIntPoint FIntPoint::operator/(int32 Divisor) const
{
	return FIntPoint(*this) /= Divisor;
}


FORCEINLINE int32& FIntPoint::operator[](int32 Index)
{
	check(Index >= 0 && Index < 2);
	return ((Index == 0) ? X : Y);
}


FORCEINLINE int32 FIntPoint::operator[](int32 Index) const
{
	check(Index >= 0 && Index < 2);
	return ((Index == 0) ? X : Y);
}


FORCEINLINE FIntPoint FIntPoint::ComponentMin(const FIntPoint& Other) const
{
	return FIntPoint(FMath::Min(X, Other.X), FMath::Min(Y, Other.Y));
}


FORCEINLINE FIntPoint FIntPoint::ComponentMax(const FIntPoint& Other) const
{
	return FIntPoint(FMath::Max(X, Other.X), FMath::Max(Y, Other.Y));
}

FORCEINLINE FIntPoint FIntPoint::DivideAndRoundUp(FIntPoint lhs, int32 Divisor)
{
	return FIntPoint(FMath::DivideAndRoundUp(lhs.X, Divisor), FMath::DivideAndRoundUp(lhs.Y, Divisor));
}

FORCEINLINE FIntPoint FIntPoint::DivideAndRoundUp(FIntPoint lhs, FIntPoint Divisor)
{
	return FIntPoint(FMath::DivideAndRoundUp(lhs.X, Divisor.X), FMath::DivideAndRoundUp(lhs.Y, Divisor.Y));
}	

FORCEINLINE FIntPoint FIntPoint::DivideAndRoundDown(FIntPoint lhs, int32 Divisor)
{
	return FIntPoint(FMath::DivideAndRoundDown(lhs.X, Divisor), FMath::DivideAndRoundDown(lhs.Y, Divisor));
}	


FORCEINLINE FIntPoint FIntPoint::operator+(const FIntPoint& Other) const
{
	return FIntPoint(*this) += Other;
}


FORCEINLINE FIntPoint FIntPoint::operator-(const FIntPoint& Other) const
{
	return FIntPoint(*this) -= Other;
}


FORCEINLINE FIntPoint FIntPoint::operator/(const FIntPoint& Other) const
{
	return FIntPoint(*this) /= Other;
}


FORCEINLINE int32 FIntPoint::GetMax() const
{
	return FMath::Max(X, Y);
}


FORCEINLINE int32 FIntPoint::GetMin() const
{
	return FMath::Min(X,Y);
}

FORCEINLINE uint32 GetTypeHash(const FIntPoint& InPoint)
{
	return HashCombine(GetTypeHash(InPoint.X), GetTypeHash(InPoint.Y));
}


FORCEINLINE int32 FIntPoint::Size() const
{
	int64 X64 = (int64)X;
	int64 Y64 = (int64)Y;
	return int32(FMath::Sqrt(float(X64 * X64 + Y64 * Y64)));
}

FORCEINLINE int32 FIntPoint::SizeSquared() const
{
	return X*X + Y*Y;
}

FORCEINLINE FString FIntPoint::ToString() const
{
	return FString::Printf(TEXT("X=%d Y=%d"), X, Y);
}

template <> struct TIsPODType<FIntPoint> { enum { Value = true }; };
