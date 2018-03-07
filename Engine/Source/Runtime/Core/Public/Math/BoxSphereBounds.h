// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/UnrealString.h"
#include "Logging/LogMacros.h"
#include "Math/Vector.h"
#include "Math/Sphere.h"
#include "Math/Box.h"

/**
 * Structure for a combined axis aligned bounding box and bounding sphere with the same origin. (28 bytes).
 */
struct FBoxSphereBounds
{
	/** Holds the origin of the bounding box and sphere. */
	FVector	Origin;

	/** Holds the extent of the bounding box. */
	FVector BoxExtent;

	/** Holds the radius of the bounding sphere. */
	float SphereRadius;

public:

	/** Default constructor. */
	FBoxSphereBounds() { }

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param EForceInit Force Init Enum.
	 */
	explicit FORCEINLINE FBoxSphereBounds( EForceInit ) 
		: Origin(ForceInit)
		, BoxExtent(ForceInit)
		, SphereRadius(0.f)
	{
		DiagnosticCheckNaN();
	}

	/**
	 * Creates and initializes a new instance from the specified parameters.
	 *
	 * @param InOrigin origin of the bounding box and sphere.
	 * @param InBoxExtent half size of box.
	 * @param InSphereRadius radius of the sphere.
	 */
	FBoxSphereBounds( const FVector& InOrigin, const FVector& InBoxExtent, float InSphereRadius )
		: Origin(InOrigin)
		, BoxExtent(InBoxExtent)
		, SphereRadius(InSphereRadius)
	{
		DiagnosticCheckNaN();
	}

	/**
	 * Creates and initializes a new instance from the given Box and Sphere.
	 *
	 * @param Box The bounding box.
	 * @param Sphere The bounding sphere.
	 */
	FBoxSphereBounds( const FBox& Box, const FSphere& Sphere )
	{
		Box.GetCenterAndExtents(Origin,BoxExtent);
		SphereRadius = FMath::Min(BoxExtent.Size(), (Sphere.Center - Origin).Size() + Sphere.W);

		DiagnosticCheckNaN();
	}
	
	/**
	 * Creates and initializes a new instance the given Box.
	 *
	 * The sphere radius is taken from the extent of the box.
	 *
	 * @param Box The bounding box.
	 */
	FBoxSphereBounds( const FBox& Box )
	{
		Box.GetCenterAndExtents(Origin,BoxExtent);
		SphereRadius = BoxExtent.Size();

		DiagnosticCheckNaN();
	}

	/**
	 * Creates and initializes a new instance for the given sphere.
	 */
	FBoxSphereBounds( const FSphere& Sphere )
	{
		Origin = Sphere.Center;
		BoxExtent = FVector(Sphere.W);
		SphereRadius = Sphere.W;

		DiagnosticCheckNaN();
	}

	/**
	 * Creates and initializes a new instance from the given set of points.
	 *
	 * The sphere radius is taken from the extent of the box.
	 *
	 * @param Points The points to be considered for the bounding box.
	 * @param NumPoints Number of points in the Points array.
	 */
	FBoxSphereBounds( const FVector* Points, uint32 NumPoints );


public:

	/**
	 * Constructs a bounding volume containing both this and B.
	 *
	 * @param Other The other bounding volume.
	 * @return The combined bounding volume.
	 */
	FORCEINLINE FBoxSphereBounds operator+( const FBoxSphereBounds& Other ) const;

	/**
	 * Compare bounding volume this and Other.
	 *
	 * @param Other The other bounding volume.
	 * @return true of they match.
	 */
	FORCEINLINE bool operator==(const FBoxSphereBounds& Other) const;
	
	/**
	 * Compare bounding volume this and Other.
	 *
	 * @param Other The other bounding volume.
	 * @return true of they do not match.
	 */	
	FORCEINLINE bool operator!=(const FBoxSphereBounds& Other) const;

public:

	/**
	 * Calculates the squared distance from a point to a bounding box
	 *
	 * @param Point The point.
	 * @return The distance.
	 */
	FORCEINLINE float ComputeSquaredDistanceFromBoxToPoint( const FVector& Point ) const
	{
		FVector Mins = Origin - BoxExtent;
		FVector Maxs = Origin + BoxExtent;

		return ::ComputeSquaredDistanceFromBoxToPoint(Mins, Maxs, Point);
	}

	/**
	 * Test whether the spheres from two BoxSphereBounds intersect/overlap.
	 * 
	 * @param  A First BoxSphereBounds to test.
	 * @param  B Second BoxSphereBounds to test.
	 * @param  Tolerance Error tolerance added to test distance.
	 * @return true if spheres intersect, false otherwise.
	 */
	FORCEINLINE static bool SpheresIntersect(const FBoxSphereBounds& A, const FBoxSphereBounds& B, float Tolerance = KINDA_SMALL_NUMBER)
	{
		return (A.Origin - B.Origin).SizeSquared() <= FMath::Square(FMath::Max(0.f, A.SphereRadius + B.SphereRadius + Tolerance));
	}

	/**
	 * Test whether the boxes from two BoxSphereBounds intersect/overlap.
	 * 
	 * @param  A First BoxSphereBounds to test.
	 * @param  B Second BoxSphereBounds to test.
	 * @return true if boxes intersect, false otherwise.
	 */
	FORCEINLINE static bool BoxesIntersect(const FBoxSphereBounds& A, const FBoxSphereBounds& B)
	{
		return A.GetBox().Intersect(B.GetBox());
	}

	/**
	 * Gets the bounding box.
	 *
	 * @return The bounding box.
	 */
	FORCEINLINE FBox GetBox() const
	{
		return FBox(Origin - BoxExtent,Origin + BoxExtent);
	}

	/**
	 * Gets the extrema for the bounding box.
	 *
	 * @param Extrema 1 for positive extrema from the origin, else negative
	 * @return The boxes extrema
	 */
	FVector GetBoxExtrema( uint32 Extrema ) const
	{
		if (Extrema)
		{
			return Origin + BoxExtent;
		}

		return Origin - BoxExtent;
	}

	/**
	 * Gets the bounding sphere.
	 *
	 * @return The bounding sphere.
	 */
	FORCEINLINE FSphere GetSphere() const
	{
		return FSphere(Origin,SphereRadius);
	}

	/**
	 * Increase the size of the box and sphere by a given size.
	 *
	 * @param ExpandAmount The size to increase by.
	 * @return A new box with the expanded size.
	 */
	FORCEINLINE FBoxSphereBounds ExpandBy( float ExpandAmount ) const
	{
		return FBoxSphereBounds(Origin, BoxExtent + ExpandAmount, SphereRadius + ExpandAmount);
	}

	/**
	 * Gets a bounding volume transformed by a matrix.
	 *
	 * @param M The matrix.
	 * @return The transformed volume.
	 */
	CORE_API FBoxSphereBounds TransformBy( const FMatrix& M ) const;

	/**
	 * Gets a bounding volume transformed by a FTransform object.
	 *
	 * @param M The FTransform object.
	 * @return The transformed volume.
	 */
	CORE_API FBoxSphereBounds TransformBy( const FTransform& M ) const;

	/**
	 * Get a textual representation of this bounding box.
	 *
	 * @return Text describing the bounding box.
	 */
	FString ToString() const;

	/**
	 * Constructs a bounding volume containing both A and B.
	 *
	 * This is a legacy version of the function used to compute primitive bounds, to avoid the need to rebuild lighting after the change.
	 */
	friend FBoxSphereBounds Union( const FBoxSphereBounds& A,const FBoxSphereBounds& B )
	{
		FBox BoundingBox(ForceInit);

		BoundingBox += (A.Origin - A.BoxExtent);
		BoundingBox += (A.Origin + A.BoxExtent);
		BoundingBox += (B.Origin - B.BoxExtent);
		BoundingBox += (B.Origin + B.BoxExtent);

		// Build a bounding sphere from the bounding box's origin and the radii of A and B.
		FBoxSphereBounds Result(BoundingBox);

		Result.SphereRadius = FMath::Min(Result.SphereRadius,FMath::Max((A.Origin - Result.Origin).Size() + A.SphereRadius,(B.Origin - Result.Origin).Size() + B.SphereRadius));
		Result.DiagnosticCheckNaN();

		return Result;
	}

#if ENABLE_NAN_DIAGNOSTIC
	FORCEINLINE void DiagnosticCheckNaN() const
	{
		if (Origin.ContainsNaN())
		{
			logOrEnsureNanError(TEXT("Origin contains NaN: %s"), *Origin.ToString());
			const_cast<FBoxSphereBounds*>(this)->Origin = FVector::ZeroVector;
		}
		if (BoxExtent.ContainsNaN())
		{
			logOrEnsureNanError(TEXT("BoxExtent contains NaN: %s"), *BoxExtent.ToString());
			const_cast<FBoxSphereBounds*>(this)->BoxExtent = FVector::ZeroVector;
		}
		if (FMath::IsNaN(SphereRadius) || !FMath::IsFinite(SphereRadius))
		{
			logOrEnsureNanError(TEXT("SphereRadius contains NaN: %f"), SphereRadius);
			const_cast<FBoxSphereBounds*>(this)->SphereRadius = 0.f;
		}
	}
#else
	FORCEINLINE void DiagnosticCheckNaN() const {}
#endif

	inline bool ContainsNaN() const
	{
		return Origin.ContainsNaN() || BoxExtent.ContainsNaN() || !FMath::IsFinite(SphereRadius);
	}

public:

	/**
	 * Serializes the given bounding volume from or into the specified archive.
	 *
	 * @param Ar The archive to serialize from or into.
	 * @param Bounds The bounding volume to serialize.
	 * @return The archive..
	 */
	friend FArchive& operator<<( FArchive& Ar, FBoxSphereBounds& Bounds )
	{
		return Ar << Bounds.Origin << Bounds.BoxExtent << Bounds.SphereRadius;
	}
};


/* FBoxSphereBounds inline functions
 *****************************************************************************/

FORCEINLINE FBoxSphereBounds::FBoxSphereBounds( const FVector* Points, uint32 NumPoints )
{
	FBox BoundingBox(ForceInit);

	// find an axis aligned bounding box for the points.
	for (uint32 PointIndex = 0; PointIndex < NumPoints; PointIndex++)
	{
		BoundingBox += Points[PointIndex];
	}

	BoundingBox.GetCenterAndExtents(Origin, BoxExtent);

	// using the center of the bounding box as the origin of the sphere, find the radius of the bounding sphere.
	SphereRadius = 0.0f;

	for (uint32 PointIndex = 0; PointIndex < NumPoints; PointIndex++)
	{
		SphereRadius = FMath::Max(SphereRadius,(Points[PointIndex] - Origin).Size());
	}

	DiagnosticCheckNaN();
}


FORCEINLINE FBoxSphereBounds FBoxSphereBounds::operator+( const FBoxSphereBounds& Other ) const
{
	FBox BoundingBox(ForceInit);

	BoundingBox += (this->Origin - this->BoxExtent);
	BoundingBox += (this->Origin + this->BoxExtent);
	BoundingBox += (Other.Origin - Other.BoxExtent);
	BoundingBox += (Other.Origin + Other.BoxExtent);

	// build a bounding sphere from the bounding box's origin and the radii of A and B.
	FBoxSphereBounds Result(BoundingBox);

	Result.SphereRadius = FMath::Min(Result.SphereRadius, FMath::Max((Origin - Result.Origin).Size() + SphereRadius, (Other.Origin - Result.Origin).Size() + Other.SphereRadius));
	Result.DiagnosticCheckNaN();

	return Result;
}

FORCEINLINE bool FBoxSphereBounds::operator==(const FBoxSphereBounds& Other) const
{
	return Origin == Other.Origin && BoxExtent == Other.BoxExtent &&  SphereRadius == Other.SphereRadius;
}

FORCEINLINE bool FBoxSphereBounds::operator!=(const FBoxSphereBounds& Other) const
{
	return !(*this == Other);
}

FORCEINLINE FString FBoxSphereBounds::ToString() const
{
	return FString::Printf(TEXT("Origin=%s, BoxExtent=(%s), SphereRadius=(%f)"), *Origin.ToString(), *BoxExtent.ToString(), SphereRadius);
}

template <> struct TIsPODType<FBoxSphereBounds> { enum { Value = true }; };
