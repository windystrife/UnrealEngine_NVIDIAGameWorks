// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"
#include "UObject/ObjectVersion.h"

/**
 * Structure for three dimensional planes.
 *
 * Stores the coeffecients as Xx+Yy+Zz=W.
 * Note that this is different from many other Plane classes that use Xx+Yy+Zz+W=0.
 */
MS_ALIGN(16) struct FPlane
	: public FVector
{
public:

	/** The w-component. */
	float W;

public:

	/** Default constructor (no initialization). */
	FORCEINLINE FPlane();

	/**
	 * Copy Constructor.
	 *
	 * @param P Plane to copy from.
	 */
	FORCEINLINE FPlane(const FPlane& P);

	/**
	 * Constructor.
	 *
	 * @param V 4D vector to set up plane.
	 */
	FORCEINLINE FPlane(const FVector4& V);

	/**
	 * Constructor.
	 *
	 * @param InX X-coefficient.
	 * @param InY Y-coefficient.
	 * @param InZ Z-coefficient.
	 * @param InW W-coefficient.
	 */
	FORCEINLINE FPlane(float InX, float InY, float InZ, float InW);

	/**
	 * Constructor.
	 *
	 * @param InNormal Plane Normal Vector.
	 * @param InW Plane W-coefficient.
	 */
	FORCEINLINE FPlane(FVector InNormal, float InW);

	/**
	 * Constructor.
	 *
	 * @param InBase Base point in plane.
	 * @param InNormal Plane Normal Vector.
	 */
	FORCEINLINE FPlane(FVector InBase, const FVector &InNormal);

	/**
	 * Constructor.
	 *
	 * @param A First point in the plane.
	 * @param B Second point in the plane.
	 * @param C Third point in the plane.
	 */
	FPlane(FVector A, FVector B, FVector C);

	/**
	 * Constructor
	 *
	 * @param EForceInit Force Init Enum.
	 */
	explicit FORCEINLINE FPlane(EForceInit);

	// Functions.

	/**
	 * Calculates distance between plane and a point.
	 *
	 * @param P The other point.
	 * @return >0: point is in front of the plane, <0: behind, =0: on the plane.
	 */
	FORCEINLINE float PlaneDot(const FVector &P) const;

	/**
	 * Get a flipped version of the plane.
	 *
	 * @return A flipped version of the plane.
	 */
	FPlane Flip() const;

	/**
	 * Get the result of transforming the plane by a Matrix.
	 *
	 * @param M The matrix to transform plane with.
	 * @return The result of transform.
	 */
	FPlane TransformBy(const FMatrix& M) const;

	/**
	 * You can optionally pass in the matrices transpose-adjoint, which save it recalculating it.
	 * MSM: If we are going to save the transpose-adjoint we should also save the more expensive
	 * determinant.
	 *
	 * @param M The Matrix to transform plane with.
	 * @param DetM Determinant of Matrix.
	 * @param TA Transpose-adjoint of Matrix.
	 * @return The result of transform.
	 */
	FPlane TransformByUsingAdjointT(const FMatrix& M, float DetM, const FMatrix& TA) const;

	/**
	 * Check if two planes are identical.
	 *
	 * @param V The other plane.
	 * @return true if planes are identical, otherwise false.
	 */
	bool operator==(const FPlane& V) const;

	/**
	 * Check if two planes are different.
	 *
	 * @param V The other plane.
	 * @return true if planes are different, otherwise false.
	 */
	bool operator!=(const FPlane& V) const;

	/**
	 * Checks whether two planes are equal within specified tolerance.
	 *
	 * @param V The other plane.
	 * @param Tolerance Error Tolerance.
	 * @return true if the two planes are equal within specified tolerance, otherwise false.
	 */
	bool Equals(const FPlane& V, float Tolerance=KINDA_SMALL_NUMBER) const;

	/**
	 * Calculates dot product of two planes.
	 *
	 * @param V The other plane.
	 * @return The dot product.
	 */
	FORCEINLINE float operator|(const FPlane& V) const;

	/**
	 * Gets result of adding a plane to this.
	 *
	 * @param V The other plane.
	 * @return The result of adding a plane to this.
	 */
	FPlane operator+(const FPlane& V) const;

	/**
	 * Gets result of subtracting a plane from this.
	 *
	 * @param V The other plane.
	 * @return The result of subtracting a plane from this.
	 */
	FPlane operator-(const FPlane& V) const;

	/**
	 * Gets result of dividing a plane.
	 *
	 * @param Scale What to divide by.
	 * @return The result of division.
	 */
	FPlane operator/(float Scale) const;

	/**
	 * Gets result of scaling a plane.
	 *
	 * @param Scale The scaling factor.
	 * @return The result of scaling.
	 */
	FPlane operator*(float Scale) const;

	/**
	 * Gets result of multiplying a plane with this.
	 *
	 * @param V The other plane.
	 * @return The result of multiplying a plane with this.
	 */
	FPlane operator*(const FPlane& V);

	/**
	 * Add another plane to this.
	 *
	 * @param V The other plane.
	 * @return Copy of plane after addition.
	 */
	FPlane operator+=(const FPlane& V);

	/**
	 * Subtract another plane from this.
	 *
	 * @param V The other plane.
	 * @return Copy of plane after subtraction.
	 */
	FPlane operator-=(const FPlane& V);

	/**
	 * Scale this plane.
	 *
	 * @param Scale The scaling factor.
	 * @return Copy of plane after scaling.
	 */
	FPlane operator*=(float Scale);

	/**
	 * Multiply another plane with this.
	 *
	 * @param V The other plane.
	 * @return Copy of plane after multiplication.
	 */
	FPlane operator*=(const FPlane& V);

	/**
	 * Divide this plane.
	 *
	 * @param V What to divide by.
	 * @return Copy of plane after division.
	 */
	FPlane operator/=(float V);

	/**
	 * Serializer.
	 *
	 * @param Ar Serialization Archive.
	 * @param P Plane to serialize.
	 * @return Reference to Archive after serialization.
	 */
	friend FArchive& operator<<(FArchive& Ar, FPlane &P)
	{
		return Ar << (FVector&)P << P.W;
	}

	bool Serialize(FArchive& Ar)
	{
		if (Ar.UE4Ver() >= VER_UE4_ADDED_NATIVE_SERIALIZATION_FOR_IMMUTABLE_STRUCTURES)
		{
			Ar << *this;
			return true;
		}
		return false;
	}

	/**
	 * Serializes the vector compressed for e.g. network transmission.
	 * @param Ar Archive to serialize to/ from.
	 * @return false to allow the ordinary struct code to run (this never happens).
	 */
	bool NetSerialize(FArchive& Ar, class UPackageMap*, bool& bOutSuccess)
	{
		if(Ar.IsLoading())
		{
			int16 iX, iY, iZ, iW;
			Ar << iX << iY << iZ << iW;
			*this = FPlane(iX,iY,iZ,iW);
		}
		else
		{
			int16 iX(FMath::RoundToInt(X));
			int16 iY(FMath::RoundToInt(Y));
			int16 iZ(FMath::RoundToInt(Z));
			int16 iW(FMath::RoundToInt(W));
			Ar << iX << iY << iZ << iW;
		}
		bOutSuccess = true;
		return true;
	}
} GCC_ALIGN(16);

/* FMath inline functions
 *****************************************************************************/

inline FVector FMath::LinePlaneIntersection
	(
	const FVector &Point1,
	const FVector &Point2,
	const FPlane  &Plane
	)
{
	return
		Point1
		+	(Point2-Point1)
		*	((Plane.W - (Point1|Plane))/((Point2 - Point1)|Plane));
}

inline bool FMath::IntersectPlanes3( FVector& I, const FPlane& P1, const FPlane& P2, const FPlane& P3 )
{
	// Compute determinant, the triple product P1|(P2^P3)==(P1^P2)|P3.
	const float Det = (P1 ^ P2) | P3;
	if( Square(Det) < Square(0.001f) )
	{
		// Degenerate.
		I = FVector::ZeroVector;
		return 0;
	}
	else
	{
		// Compute the intersection point, guaranteed valid if determinant is nonzero.
		I = (P1.W*(P2^P3) + P2.W*(P3^P1) + P3.W*(P1^P2)) / Det;
	}
	return 1;
}

inline bool FMath::IntersectPlanes2( FVector& I, FVector& D, const FPlane& P1, const FPlane& P2 )
{
	// Compute line direction, perpendicular to both plane normals.
	D = P1 ^ P2;
	const float DD = D.SizeSquared();
	if( DD < Square(0.001f) )
	{
		// Parallel or nearly parallel planes.
		D = I = FVector::ZeroVector;
		return 0;
	}
	else
	{
		// Compute intersection.
		I = (P1.W*(P2^D) + P2.W*(D^P1)) / DD;
		D.Normalize();
		return 1;
	}
}

/* FVector inline functions
 *****************************************************************************/

inline FVector FVector::MirrorByPlane( const FPlane& Plane ) const
{
	return *this - Plane * (2.f * Plane.PlaneDot(*this) );
}

inline FVector FVector::PointPlaneProject(const FVector& Point, const FPlane& Plane)
{
	//Find the distance of X from the plane
	//Add the distance back along the normal from the point
	return Point - Plane.PlaneDot(Point) * Plane;
}

inline FVector FVector::PointPlaneProject(const FVector& Point, const FVector& A, const FVector& B, const FVector& C)
{
	//Compute the plane normal from ABC
	FPlane Plane(A, B, C);

	//Find the distance of X from the plane
	//Add the distance back along the normal from the point
	return Point - Plane.PlaneDot(Point) * Plane;
}

/* FPlane inline functions
 *****************************************************************************/

FORCEINLINE FPlane::FPlane()
{}


FORCEINLINE FPlane::FPlane(const FPlane& P)
	:	FVector(P)
	,	W(P.W)
{}


FORCEINLINE FPlane::FPlane(const FVector4& V)
	:	FVector(V)
	,	W(V.W)
{}


FORCEINLINE FPlane::FPlane(float InX, float InY, float InZ, float InW)
	:	FVector(InX,InY,InZ)
	,	W(InW)
{}


FORCEINLINE FPlane::FPlane(FVector InNormal, float InW)
	:	FVector(InNormal), W(InW)
{}


FORCEINLINE FPlane::FPlane(FVector InBase, const FVector &InNormal)
	:	FVector(InNormal)
	,	W(InBase | InNormal)
{}


FORCEINLINE FPlane::FPlane(FVector A, FVector B, FVector C)
	:	FVector(((B-A)^(C-A)).GetSafeNormal())
{
	W = A | (FVector)(*this);
}


FORCEINLINE FPlane::FPlane(EForceInit)
	: FVector(ForceInit), W(0.f)
{}


FORCEINLINE float FPlane::PlaneDot(const FVector &P) const
{
	return X * P.X + Y * P.Y + Z * P.Z - W;
}


FORCEINLINE FPlane FPlane::Flip() const
{
	return FPlane(-X, -Y, -Z, -W);
}

FORCEINLINE bool FPlane::operator==(const FPlane& V) const
{
	return (X == V.X) && (Y == V.Y) && (Z == V.Z) && (W == V.W);
}


FORCEINLINE bool FPlane::operator!=(const FPlane& V) const
{
	return (X != V.X) || (Y != V.Y) || (Z != V.Z) || (W != V.W);
}


FORCEINLINE bool FPlane::Equals(const FPlane& V, float Tolerance) const
{
	return (FMath::Abs(X - V.X) < Tolerance) && (FMath::Abs(Y - V.Y) < Tolerance) && (FMath::Abs(Z - V.Z) < Tolerance) && (FMath::Abs(W - V.W) < Tolerance);
}


FORCEINLINE float FPlane::operator|(const FPlane& V) const
{
	return X * V.X + Y * V.Y + Z * V.Z + W * V.W;
}


FORCEINLINE FPlane FPlane::operator+(const FPlane& V) const
{
	return FPlane(X + V.X, Y + V.Y, Z + V.Z, W + V.W);
}


FORCEINLINE FPlane FPlane::operator-(const FPlane& V) const
{
	return FPlane(X - V.X, Y - V.Y, Z - V.Z, W - V.W);
}


FORCEINLINE FPlane FPlane::operator/(float Scale) const
{
	const float RScale = 1.f / Scale;
	return FPlane(X * RScale, Y * RScale, Z * RScale, W * RScale);
}


FORCEINLINE FPlane FPlane::operator*(float Scale) const
{
	return FPlane(X * Scale, Y * Scale, Z * Scale, W * Scale);
}


FORCEINLINE FPlane FPlane::operator*(const FPlane& V)
{
	return FPlane (X * V.X, Y * V.Y, Z * V.Z, W * V.W);
}


FORCEINLINE FPlane FPlane::operator+=(const FPlane& V)
{
	X += V.X; Y += V.Y; Z += V.Z; W += V.W;
	return *this;
}


FORCEINLINE FPlane FPlane::operator-=(const FPlane& V)
{
	X -= V.X; Y -= V.Y; Z -= V.Z; W -= V.W;
	return *this;
}


FORCEINLINE FPlane FPlane::operator*=(float Scale)
{
	X *= Scale; Y *= Scale; Z *= Scale; W *= Scale;
	return *this;
}


FORCEINLINE FPlane FPlane::operator*=(const FPlane& V)
{
	X *= V.X; Y *= V.Y; Z *= V.Z; W *= V.W;
	return *this;
}


FORCEINLINE FPlane FPlane::operator/=(float V)
{
	const float RV = 1.f / V;
	X *= RV; Y *= RV; Z *= RV; W *= RV;
	return *this;
}


template <> struct TIsPODType<FPlane> { enum { Value = true }; };
