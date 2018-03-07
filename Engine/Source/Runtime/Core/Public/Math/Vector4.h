// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/Crc.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/UnrealString.h"
#include "Misc/Parse.h"
#include "Logging/LogMacros.h"
#include "Math/Vector2D.h"
#include "Math/Vector.h"

/**
 * A 4D homogeneous vector, 4x1 FLOATs, 16-byte aligned.
 */
MS_ALIGN(16) struct FVector4
{
public:

	/** The vector's X-component. */
	float X;

	/** The vector's Y-component. */
	float Y;

	/** The vector's Z-component. */
	float Z;

	/** The vector's W-component. */
	float W;

public:

	/**
	 * Constructor.
	 *
	 * @param InVector 3D Vector to set first three components.
	 * @param InW W Coordinate.
	 */
	FVector4(const FVector& InVector, float InW = 1.0f);

	/**
	 * Creates and initializes a new vector from a color value.
	 *
	 * @param InColour Color used to set vector.
	 */
	FVector4(const FLinearColor& InColor);

	/**
	 * Creates and initializes a new vector from the specified components.
	 *
	 * @param InX X Coordinate.
	 * @param InY Y Coordinate.
	 * @param InZ Z Coordinate.
	 * @param InW W Coordinate.
	 */
	explicit FVector4(float InX = 0.0f, float InY = 0.0f, float InZ = 0.0f, float InW = 1.0f);

	/**
	 * Creates and initializes a new vector from the specified 2D vectors.
	 *
	 * @param InXY A 2D vector holding the X- and Y-components.
	 * @param InZW A 2D vector holding the Z- and W-components.
	 */
	explicit FVector4(FVector2D InXY, FVector2D InZW);

	/**
	 * Creates and initializes a new vector to zero.
	 *
	 * @param EForceInit Force Init Enum.
	 */
	explicit FVector4(EForceInit);

public:

	/**
	 * Access a specific component of the vector.
	 *
	 * @param ComponentIndex The index of the component.
	 * @return Reference to the desired component.
	 */
	FORCEINLINE float& operator[](int32 ComponentIndex);

	/**
	 * Access a specific component of the vector.
	 *
	 * @param ComponentIndex The index of the component.
	 * @return Copy of the desired component.
	 */
	FORCEINLINE float operator[](int32 ComponentIndex) const;

	// Unary operators.

	/**
	 * Gets a negated copy of the vector.
	 *
	 * @return A negated copy of the vector.
	 */
	FORCEINLINE FVector4 operator-() const;

	/**
	 * Gets the result of adding a vector to this.
	 *
	 * @param V The vector to add.
	 * @return The result of vector addition.
	 */
	FORCEINLINE FVector4 operator+(const FVector4& V) const;

	/**
	 * Adds another vector to this one.
	 *
	 * @param V The other vector to add.
	 * @return Copy of the vector after addition.
	 */
	FORCEINLINE FVector4 operator+=(const FVector4& V);

	/**
	 * Gets the result of subtracting a vector from this.
	 *
	 * @param V The vector to subtract.
	 * @return The result of vector subtraction.
	 */
	FORCEINLINE FVector4 operator-(const FVector4& V) const;

	/**
	 * Gets the result of scaling this vector.
	 *
	 * @param Scale The scaling factor.
	 * @return The result of vector scaling.
	 */
	FORCEINLINE FVector4 operator*(float Scale) const;

	/**
	 * Gets the result of dividing this vector.
	 *
	 * @param Scale What to divide by.
	 * @return The result of division.
	 */
	FVector4 operator/(float Scale) const;

	/**
	 * Gets the result of dividing this vector.
	 *
	 * @param V What to divide by.
	 * @return The result of division.
	 */
	FVector4 operator/(const FVector4& V) const;

	/**
	 * Gets the result of multiplying a vector with this.
	 *
	 * @param V The vector to multiply.
	 * @return The result of vector multiplication.
	 */
	FVector4 operator*(const FVector4& V) const;

	/**
	 * Gets the result of multiplying a vector with another Vector (component wise).
	 *
	 * @param V The vector to multiply.
	 * @return The result of vector multiplication.
	 */
	FVector4 operator*=(const FVector4& V);

	/**
	 * Gets the result of dividing a vector with another Vector (component wise).
	 *
	 * @param V The vector to divide with.
	 * @return The result of vector multiplication.
	 */
	FVector4 operator/=(const FVector4& V);

	/**
	 * Gets the result of scaling this vector.
	 *
	 * @param Scale The scaling factor.
	 * @return The result of vector scaling.
	 */
	FVector4 operator*=(float S);

	/**
	 * Calculates 3D Dot product of two 4D vectors.
	 *
	 * @param V1 The first vector.
	 * @param V2 The second vector.
	 * @return The 3D Dot product.
	 */
	friend FORCEINLINE float Dot3(const FVector4& V1, const FVector4& V2)
	{
		return V1.X*V2.X + V1.Y*V2.Y + V1.Z*V2.Z;
	}

	/**
	 * Calculates 4D Dot product.
	 *
	 * @param V1 The first vector.
	 * @param V2 The second vector.
	 * @return The 4D Dot Product.
	 */
	friend FORCEINLINE float Dot4(const FVector4& V1, const FVector4& V2)
	{
		return V1.X*V2.X + V1.Y*V2.Y + V1.Z*V2.Z + V1.W*V2.W;
	}

	/**
	 * Scales a vector.
	 *
	 * @param Scale The scaling factor.
	 * @param V The vector to scale.
	 * @return The result of scaling.
	 */
	friend FORCEINLINE FVector4 operator*(float Scale, const FVector4& V)
	{
		return V.operator*(Scale);
	}

	/**
	 * Checks for equality against another vector.
	 *
	 * @param V The other vector.
	 * @return true if the two vectors are the same, otherwise false.
	 */
	bool operator==(const FVector4& V) const;

	/**
	 * Checks for inequality against another vector.
	 *
	 * @param V The other vector.
	 * @return true if the two vectors are different, otherwise false.
	 */
	bool operator!=(const FVector4& V) const;

	/**
	 * Calculate Cross product between this and another vector.
	 *
	 * @param V The other vector.
	 * @return The Cross product.
	 */
	FVector4 operator^(const FVector4& V) const;

public:

	// Simple functions.

	/**
	 * Gets a specific component of the vector.
	 *
	 * @param Index The index of the component.
	 * @return Reference to the component.
	 */
	float& Component(int32 Index);

	/**
	 * Error tolerant comparison.
	 *
	 * @param V Vector to compare against.
	 * @param Tolerance Error Tolerance.
	 * @return true if the two vectors are equal within specified tolerance, otherwise false.
	 */
	bool Equals(const FVector4& V, float Tolerance=KINDA_SMALL_NUMBER) const;

	/**
	 * Check if the vector is of unit length, with specified tolerance.
	 *
	 * @param LengthSquaredTolerance Tolerance against squared length.
	 * @return true if the vector is a unit vector within the specified tolerance.
	 */
	bool IsUnit3(float LengthSquaredTolerance = KINDA_SMALL_NUMBER) const;

	/**
	 * Get a textual representation of the vector.
	 *
	 * @return Text describing the vector.
	 */
	FString ToString() const;

	/**
	 * Initialize this Vector based on an FString. The String is expected to contain X=, Y=, Z=, W=.
	 * The FVector4 will be bogus when InitFromString returns false.
	 *
	 * @param InSourceString	FString containing the vector values.
	 * @return true if the X,Y,Z values were read successfully; false otherwise.
	 */
	bool InitFromString(const FString& InSourceString);

	/**
	 * Returns a normalized copy of the vector if safe to normalize.
	 *
	 * @param Tolerance Minimum squared length of vector for normalization.
	 * @return A normalized copy of the vector or a zero vector.
	 */
	FORCEINLINE FVector4 GetSafeNormal(float Tolerance = SMALL_NUMBER) const;

	/**
	 * Calculates normalized version of vector without checking if it is non-zero.
	 *
	 * @return Normalized version of vector.
	 */
	FORCEINLINE FVector4 GetUnsafeNormal3() const;

	/**
	 * Return the FRotator orientation corresponding to the direction in which the vector points.
	 * Sets Yaw and Pitch to the proper numbers, and sets roll to zero because the roll can't be determined from a vector.
	 * @return FRotator from the Vector's direction.
	 */
	CORE_API FRotator ToOrientationRotator() const;

	/**
	 * Return the Quaternion orientation corresponding to the direction in which the vector points.
	 * @return Quaternion from the Vector's direction.
	 */
	CORE_API FQuat ToOrientationQuat() const;

	/**
	 * Return the FRotator orientation corresponding to the direction in which the vector points.
	 * Sets Yaw and Pitch to the proper numbers, and sets roll to zero because the roll can't be determined from a vector.
	 * Identical to 'ToOrientationRotator()'.
	 * @return FRotator from the Vector's direction.
	 * @see ToOrientationRotator()
	 */
	CORE_API FRotator Rotation() const;

	/**
	 * Set all of the vectors coordinates.
	 *
	 * @param InX New X Coordinate.
	 * @param InY New Y Coordinate.
	 * @param InZ New Z Coordinate.
	 * @param InW New W Coordinate.
	 */
	FORCEINLINE void Set(float InX, float InY, float InZ, float InW);

	/**
	 * Get the length of this vector not taking W component into account.
	 *
	 * @return The length of this vector.
	 */
	float Size3() const;

	/**
	 * Get the squared length of this vector not taking W component into account.
	 *
	 * @return The squared length of this vector.
	 */
	float SizeSquared3() const;

	/**
	 * Get the length (magnitude) of this vector, taking the W component into account
	 *
	 * @return The length of this vector
	 */
	float Size() const;

	/**
	 * Get the squared length of this vector, taking the W component into account
	 *
	 * @return The squared length of this vector
	 */
	float SizeSquared() const;

	/** Utility to check if there are any non-finite values (NaN or Inf) in this vector. */
	bool ContainsNaN() const;

	/** Utility to check if all of the components of this vector are nearly zero given the tolerance. */
	bool IsNearlyZero3(float Tolerance = KINDA_SMALL_NUMBER) const;

	/** Reflect vector. */
	FVector4 Reflect3(const FVector4& Normal) const;

	/**
	 * Find good arbitrary axis vectors to represent U and V axes of a plane,
	 * given just the normal.
	 */
	void FindBestAxisVectors3(FVector4& Axis1, FVector4& Axis2) const;

#if ENABLE_NAN_DIAGNOSTIC
	FORCEINLINE void DiagnosticCheckNaN()
	{
		if (ContainsNaN())
		{
			logOrEnsureNanError(TEXT("FVector contains NaN: %s"), *ToString());
			*this = FVector4(FVector::ZeroVector);

		}
	}
#else
	FORCEINLINE void DiagnosticCheckNaN() { }
#endif

public:

	/**
	 * Serializer.
	 *
	 * @param Ar The Serialization Archive.
	 * @param V The vector being serialized.
	 * @return Reference to the Archive after serialization.
	 */
	friend FArchive& operator<<(FArchive& Ar, FVector4& V)
	{
		return Ar << V.X << V.Y << V.Z << V.W;
	}

	bool Serialize(FArchive& Ar)
	{
		Ar << *this;
		return true;
	}

} GCC_ALIGN(16);


/**
 * Creates a hash value from a FVector4.
 *
 * @param Vector the vector to create a hash value for
 *
 * @return The hash value from the components
 */
FORCEINLINE uint32 GetTypeHash(const FVector4& Vector)
{
	// Note: this assumes there's no padding in FVector that could contain uncompared data.
	return FCrc::MemCrc_DEPRECATED(&Vector,sizeof(Vector));
}


/* FVector4 inline functions
 *****************************************************************************/

FORCEINLINE FVector4::FVector4(const FVector& InVector,float InW)
	: X(InVector.X)
	, Y(InVector.Y)
	, Z(InVector.Z)
	, W(InW)
{
	DiagnosticCheckNaN();
}


FORCEINLINE FVector4::FVector4(const FLinearColor& InColor)
	: X(InColor.R)
	, Y(InColor.G)
	, Z(InColor.B)
	, W(InColor.A)
{
	DiagnosticCheckNaN();
}


FORCEINLINE FVector4::FVector4(float InX,float InY,float InZ,float InW)
	: X(InX)
	, Y(InY)
	, Z(InZ)
	, W(InW)
{
	DiagnosticCheckNaN();
}


FORCEINLINE FVector4::FVector4(EForceInit)
	: X(0.f)
	, Y(0.f)
	, Z(0.f)
	, W(0.f)
{
	DiagnosticCheckNaN();
}


FORCEINLINE FVector4::FVector4(FVector2D InXY, FVector2D InZW)
	: X(InXY.X)
	, Y(InXY.Y)
	, Z(InZW.X)
	, W(InZW.Y)
{
	DiagnosticCheckNaN();
}


FORCEINLINE float& FVector4::operator[](int32 ComponentIndex)
{
	return (&X)[ ComponentIndex ];
}


FORCEINLINE float FVector4::operator[](int32 ComponentIndex) const
{
	return (&X)[ ComponentIndex ];
}


FORCEINLINE void FVector4::Set(float InX, float InY, float InZ, float InW)
{
	X = InX;
	Y = InY;
	Z = InZ;
	W = InW;

	DiagnosticCheckNaN();
}


FORCEINLINE FVector4 FVector4::operator-() const
{
	return FVector4(-X, -Y, -Z, -W);
}


FORCEINLINE FVector4 FVector4::operator+(const FVector4& V) const
{
	return FVector4(X + V.X, Y + V.Y, Z + V.Z, W + V.W);
}


FORCEINLINE FVector4 FVector4::operator+=(const FVector4& V)
{
	X += V.X; Y += V.Y; Z += V.Z; W += V.W;
	DiagnosticCheckNaN();
	return *this;
}


FORCEINLINE FVector4 FVector4::operator-(const FVector4& V) const
{
	return FVector4(X - V.X, Y - V.Y, Z - V.Z, W - V.W);
}


FORCEINLINE FVector4 FVector4::operator*(float Scale) const
{
	return FVector4(X * Scale, Y * Scale, Z * Scale, W * Scale);
}


FORCEINLINE FVector4 FVector4::operator/(float Scale) const
{
	const float RScale = 1.f/Scale;
	return FVector4(X * RScale, Y * RScale, Z * RScale, W * RScale);
}


FORCEINLINE FVector4 FVector4::operator*(const FVector4& V) const
{
	return FVector4(X * V.X, Y * V.Y, Z * V.Z, W * V.W);
}


FORCEINLINE FVector4 FVector4::operator^(const FVector4& V) const
{
	return FVector4(
		Y * V.Z - Z * V.Y,
		Z * V.X - X * V.Z,
		X * V.Y - Y * V.X,
		0.0f
	);
}


FORCEINLINE float& FVector4::Component(int32 Index)
{
	return (&X)[Index];
}


FORCEINLINE bool FVector4::operator==(const FVector4& V) const
{
	return ((X == V.X) && (Y == V.Y) && (Z == V.Z) && (W == V.W));
}


FORCEINLINE bool FVector4::operator!=(const FVector4& V) const
{
	return ((X != V.X) || (Y != V.Y) || (Z != V.Z) || (W != V.W));
}


FORCEINLINE bool FVector4::Equals(const FVector4& V, float Tolerance) const
{
	return FMath::Abs(X-V.X) <= Tolerance && FMath::Abs(Y-V.Y) <= Tolerance && FMath::Abs(Z-V.Z) <= Tolerance && FMath::Abs(W-V.W) <= Tolerance;
}


FORCEINLINE FString FVector4::ToString() const
{
	return FString::Printf(TEXT("X=%3.3f Y=%3.3f Z=%3.3f W=%3.3f"), X, Y, Z, W);
}


FORCEINLINE bool FVector4::InitFromString(const FString& InSourceString)
{
	X = Y = Z = 0;
	W = 1.0f;

	// The initialization is only successful if the X, Y, and Z values can all be parsed from the string
	const bool bSuccessful = FParse::Value(*InSourceString, TEXT("X=") , X) && FParse::Value(*InSourceString, TEXT("Y="), Y) && FParse::Value(*InSourceString, TEXT("Z="), Z);

	// W is optional, so don't factor in its presence (or lack thereof) in determining initialization success
	FParse::Value(*InSourceString, TEXT("W="), W);

	return bSuccessful;
}


FORCEINLINE FVector4 FVector4::GetSafeNormal(float Tolerance) const
{
	const float SquareSum = X*X + Y*Y + Z*Z;
	if(SquareSum > Tolerance)
	{
		const float Scale = FMath::InvSqrt(SquareSum);
		return FVector4(X*Scale, Y*Scale, Z*Scale, 0.0f);
	}
	return FVector4(0.f);
}


FORCEINLINE FVector4 FVector4::GetUnsafeNormal3() const
{
	const float Scale = FMath::InvSqrt(X*X+Y*Y+Z*Z);
	return FVector4(X*Scale, Y*Scale, Z*Scale, 0.0f);
}


FORCEINLINE float FVector4::Size3() const
{
	return FMath::Sqrt(X*X + Y*Y + Z*Z);
}


FORCEINLINE float FVector4::SizeSquared3() const
{
	return X*X + Y*Y + Z*Z;
}

FORCEINLINE float FVector4::Size() const
{
	return FMath::Sqrt(X*X + Y*Y + Z*Z + W*W);
}

FORCEINLINE float FVector4::SizeSquared() const
{
	return X*X + Y*Y + Z*Z + W*W;
}


FORCEINLINE bool FVector4::IsUnit3(float LengthSquaredTolerance) const
{
	return FMath::Abs(1.0f - SizeSquared3()) < LengthSquaredTolerance;
}


FORCEINLINE bool FVector4::ContainsNaN() const
{
	return (!FMath::IsFinite(X) || 
			!FMath::IsFinite(Y) ||
			!FMath::IsFinite(Z) ||
			!FMath::IsFinite(W));
}


FORCEINLINE bool FVector4::IsNearlyZero3(float Tolerance) const
{
	return
			FMath::Abs(X)<=Tolerance
		&&	FMath::Abs(Y)<=Tolerance
		&&	FMath::Abs(Z)<=Tolerance;
}


FORCEINLINE FVector4 FVector4::Reflect3(const FVector4& Normal) const
{
	return 2.0f * Dot3(*this, Normal) * Normal - *this;
}


FORCEINLINE void FVector4::FindBestAxisVectors3(FVector4& Axis1, FVector4& Axis2) const
{
	const float NX = FMath::Abs(X);
	const float NY = FMath::Abs(Y);
	const float NZ = FMath::Abs(Z);

	// Find best basis vectors.
	if(NZ>NX && NZ>NY)	Axis1 = FVector4(1,0,0);
	else					Axis1 = FVector4(0,0,1);

	Axis1 = (Axis1 - *this * Dot3(Axis1, *this)).GetSafeNormal();
	Axis2 = Axis1 ^ *this;
}


FORCEINLINE FVector4 FVector4::operator*=(const FVector4& V)
{
	X *= V.X; Y *= V.Y; Z *= V.Z; W *= V.W;
	DiagnosticCheckNaN();
	return *this;
}


FORCEINLINE FVector4 FVector4::operator/=(const FVector4& V)
{
	X /= V.X; Y /= V.Y; Z /= V.Z; W /= V.W;
	DiagnosticCheckNaN();
	return *this;
}


FORCEINLINE FVector4 FVector4::operator*=(float S)
{
	X *= S; Y *= S; Z *= S; W *= S;
	DiagnosticCheckNaN();
	return *this;
}


FORCEINLINE FVector4 FVector4::operator/(const FVector4& V) const
{
	return FVector4(X / V.X, Y / V.Y, Z / V.Z, W / V.W);
}

template <> struct TIsPODType<FVector4> { enum { Value = true }; };


/* FVector inline functions
 *****************************************************************************/

FORCEINLINE FVector::FVector( const FVector4& V )
	: X(V.X), Y(V.Y), Z(V.Z)
{
	DiagnosticCheckNaN();
}
