// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/UnrealString.h"
#include "Math/Vector.h"
// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
#include "GFSDK_VXGI.h"
#endif
// NVCHANGE_END: Add VXGI

/**
 * Implements an axis-aligned box.
 *
 * Boxes describe an axis-aligned extent in three dimensions. They are used for many different things in the
 * Engine and in games, such as bounding volumes, collision detection and visibility calculation.
 */
struct FBox
{
public:

	/** Holds the box's minimum point. */
	FVector Min;

	/** Holds the box's maximum point. */
	FVector Max;

	/** Holds a flag indicating whether this box is valid. */
	uint8 IsValid;

public:

	/** Default constructor (no initialization). */
	FBox() { }

	/** Creates and initializes a new box with zero extent and marks it as invalid. */
	DEPRECATED(4.16, "Use ForceInit constructor instead.")
	FBox( int32 )
	{
		Init();
	}

	/**
	 * Creates and initializes a new box with zero extent and marks it as invalid.
	 *
	 * @param EForceInit Force Init Enum.
	 */
	explicit FBox( EForceInit )
	{
		Init();
	}

	/**
	 * Creates and initializes a new box from the specified extents.
	 *
	 * @param InMin The box's minimum point.
	 * @param InMax The box's maximum point.
	 */
	FBox( const FVector& InMin, const FVector& InMax )
		: Min(InMin)
		, Max(InMax)
		, IsValid(1)
	{ }

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	FBox(const VXGI::Box3f& VxgiBox)
		: Min(FVector(VxgiBox.lower.x, VxgiBox.lower.y, VxgiBox.lower.z))
		, Max(FVector(VxgiBox.upper.x, VxgiBox.upper.y, VxgiBox.upper.z))
		, IsValid(1)
	{ }
#endif
	// NVCHANGE_END: Add VXGI

	/**
	 * Creates and initializes a new box from the given set of points.
	 *
	 * @param Points Array of Points to create for the bounding volume.
	 * @param Count The number of points.
	 */
	CORE_API FBox( const FVector* Points, int32 Count );

	/**
	 * Creates and initializes a new box from an array of points.
	 *
	 * @param Points Array of Points to create for the bounding volume.
	 */
	CORE_API FBox( const TArray<FVector>& Points );

public:

	/**
	 * Compares two boxes for equality.
	 *
	 * @return true if the boxes are equal, false otherwise.
	 */
	FORCEINLINE bool operator==( const FBox& Other ) const
	{
		return (Min == Other.Min) && (Max == Other.Max);
	}

	/**
	 * Adds to this bounding box to include a given point.
	 *
	 * @param Other the point to increase the bounding volume to.
	 * @return Reference to this bounding box after resizing to include the other point.
	 */
	FORCEINLINE FBox& operator+=( const FVector &Other );

	/**
	 * Gets the result of addition to this bounding volume.
	 *
	 * @param Other The other point to add to this.
	 * @return A new bounding volume.
	 */
	FORCEINLINE FBox operator+( const FVector& Other ) const
	{
		return FBox(*this) += Other;
	}

	/**
	 * Adds to this bounding box to include a new bounding volume.
	 *
	 * @param Other the bounding volume to increase the bounding volume to.
	 * @return Reference to this bounding volume after resizing to include the other bounding volume.
	 */
	FORCEINLINE FBox& operator+=( const FBox& Other );

	/**
	 * Gets the result of addition to this bounding volume.
	 *
	 * @param Other The other volume to add to this.
	 * @return A new bounding volume.
	 */
	FORCEINLINE FBox operator+( const FBox& Other ) const
	{
		return FBox(*this) += Other;
	}

	/**
	 * Gets reference to the min or max of this bounding volume.
	 *
	 * @param Index the index into points of the bounding volume.
	 * @return a reference to a point of the bounding volume.
	 */
    FORCEINLINE FVector& operator[]( int32 Index )
	{
		check((Index >= 0) && (Index < 2));

		if (Index == 0)
		{
			return Min;
		}

		return Max;
	}

public:

	/** 
	 * Calculates the distance of a point to this box.
	 *
	 * @param Point The point.
	 * @return The distance.
	 */
	FORCEINLINE float ComputeSquaredDistanceToPoint( const FVector& Point ) const
	{
		return ComputeSquaredDistanceFromBoxToPoint(Min, Max, Point);
	}

	/** 
	 * Increases the box size.
	 *
	 * @param W The size to increase the volume by.
	 * @return A new bounding box.
	 */
	FORCEINLINE FBox ExpandBy(float W) const
	{
		return FBox(Min - FVector(W, W, W), Max + FVector(W, W, W));
	}

	/**
	* Increases the box size.
	*
	* @param V The size to increase the volume by.
	* @return A new bounding box.
	*/
	FORCEINLINE FBox ExpandBy(const FVector& V) const
	{
		return FBox(Min - V, Max + V);
	}

	/**
	* Increases the box size.
	*
	* @param Neg The size to increase the volume by in the negative direction (positive values move the bounds outwards)
	* @param Pos The size to increase the volume by in the positive direction (positive values move the bounds outwards)
	* @return A new bounding box.
	*/
	FBox ExpandBy(const FVector& Neg, const FVector& Pos) const
	{
		return FBox(Min - Neg, Max + Pos);
	}

	/** 
	 * Shifts the bounding box position.
	 *
	 * @param Offset The vector to shift the box by.
	 * @return A new bounding box.
	 */
	FORCEINLINE FBox ShiftBy( const FVector& Offset ) const
	{
		return FBox(Min + Offset, Max + Offset);
	}

	/** 
	 * Moves the center of bounding box to new destination.
	 *
	 * @param The destination point to move center of box to.
	 * @return A new bounding box.
	 */
	FORCEINLINE FBox MoveTo( const FVector& Destination ) const
	{
		const FVector Offset = Destination - GetCenter();
		return FBox(Min + Offset, Max + Offset);
	}

	/**
	 * Gets the center point of this box.
	 *
	 * @return The center point.
	 * @see GetCenterAndExtents, GetExtent, GetSize, GetVolume
	 */
	FORCEINLINE FVector GetCenter() const
	{
		return FVector((Min + Max) * 0.5f);
	}

	/**
	 * Gets the center and extents of this box.
	 *
	 * @param center[out] Will contain the box center point.
	 * @param Extents[out] Will contain the extent around the center.
	 * @see GetCenter, GetExtent, GetSize, GetVolume
	 */
	FORCEINLINE void GetCenterAndExtents( FVector& center, FVector& Extents ) const
	{
		Extents = GetExtent();
		center = Min + Extents;
	}

	/**
	 * Calculates the closest point on or inside the box to a given point in space.
	 *
	 * @param Point The point in space.
	 * @return The closest point on or inside the box.
	 */
	FORCEINLINE FVector GetClosestPointTo( const FVector& Point ) const;

	/**
	 * Gets the extents of this box.
	 *
	 * @return The box extents.
	 * @see GetCenter, GetCenterAndExtents, GetSize, GetVolume
	 */
	FORCEINLINE FVector GetExtent() const
	{
		return 0.5f * (Max - Min);
	}

	/**
	 * Gets a reference to the specified point of the bounding box.
	 *
	 * @param PointIndex The index of the extrema point to return.
	 * @return A reference to the point.
	 */
	FORCEINLINE FVector& GetExtrema( int PointIndex )
	{
		return (&Min)[PointIndex];
	}

	/**
	 * Gets a read-only reference to the specified point of the bounding box.
	 *
	 * @param PointIndex The index of extrema point to return.
	 * @return A read-only reference to the point.
	 */
	FORCEINLINE const FVector& GetExtrema( int PointIndex ) const
	{
		return (&Min)[PointIndex];
	}

	/**
	 * Gets the size of this box.
	 *
	 * @return The box size.
	 * @see GetCenter, GetCenterAndExtents, GetExtent, GetVolume
	 */
	FORCEINLINE FVector GetSize() const
	{
		return (Max - Min);
	}

	/**
	 * Gets the volume of this box.
	 *
	 * @return The box volume.
	 * @see GetCenter, GetCenterAndExtents, GetExtent, GetSize
	 */
	FORCEINLINE float GetVolume() const
	{
		return ((Max.X - Min.X) * (Max.Y - Min.Y) * (Max.Z - Min.Z));
	}

	/**
	 * Set the initial values of the bounding box to Zero.
	 */
	FORCEINLINE void Init()
	{
		Min = Max = FVector::ZeroVector;
		IsValid = 0;
	}

	/**
	 * Checks whether the given bounding box intersects this bounding box.
	 *
	 * @param Other The bounding box to intersect with.
	 * @return true if the boxes intersect, false otherwise.
	 */
	FORCEINLINE bool Intersect( const FBox& other ) const;

	/**
	 * Checks whether the given bounding box intersects this bounding box in the XY plane.
	 *
	 * @param Other The bounding box to test intersection.
	 * @return true if the boxes intersect in the XY Plane, false otherwise.
	 */
	FORCEINLINE bool IntersectXY( const FBox& Other ) const;

	/**
	 * Returns the overlap FBox of two box
	 *
	 * @param Other The bounding box to test overlap
	 * @return the overlap box. It can be 0 if they don't overlap
	 */
	CORE_API FBox Overlap( const FBox& Other ) const;

	/**
	  * Gets a bounding volume transformed by an inverted FTransform object.
	  *
	  * @param M The transformation object to perform the inversely transform this box with.
	  * @return	The transformed box.
	  */
	CORE_API FBox InverseTransformBy( const FTransform& M ) const;

	/** 
	 * Checks whether the given location is inside this box.
	 * 
	 * @param In The location to test for inside the bounding volume.
	 * @return true if location is inside this volume.
	 * @see IsInsideXY
	 */
	FORCEINLINE bool IsInside( const FVector& In ) const
	{
		return ((In.X > Min.X) && (In.X < Max.X) && (In.Y > Min.Y) && (In.Y < Max.Y) && (In.Z > Min.Z) && (In.Z < Max.Z));
	}

	/** 
	 * Checks whether the given location is inside or on this box.
	 * 
	 * @param In The location to test for inside the bounding volume.
	 * @return true if location is inside this volume.
	 * @see IsInsideXY
	 */
	FORCEINLINE bool IsInsideOrOn( const FVector& In ) const
	{
		return ((In.X >= Min.X) && (In.X <= Max.X) && (In.Y >= Min.Y) && (In.Y <= Max.Y) && (In.Z >= Min.Z) && (In.Z <= Max.Z));
	}

	/** 
	 * Checks whether a given box is fully encapsulated by this box.
	 * 
	 * @param Other The box to test for encapsulation within the bounding volume.
	 * @return true if box is inside this volume.
	 */
	FORCEINLINE bool IsInside( const FBox& Other ) const
	{
		return (IsInside(Other.Min) && IsInside(Other.Max));
	}

	/** 
	 * Checks whether the given location is inside this box in the XY plane.
	 * 
	 * @param In The location to test for inside the bounding box.
	 * @return true if location is inside this box in the XY plane.
	 * @see IsInside
	 */
	FORCEINLINE bool IsInsideXY( const FVector& In ) const
	{
		return ((In.X > Min.X) && (In.X < Max.X) && (In.Y > Min.Y) && (In.Y < Max.Y));
	}

	/** 
	 * Checks whether the given box is fully encapsulated by this box in the XY plane.
	 * 
	 * @param Other The box to test for encapsulation within the bounding box.
	 * @return true if box is inside this box in the XY plane.
	 */
	FORCEINLINE bool IsInsideXY( const FBox& Other ) const
	{
		return (IsInsideXY(Other.Min) && IsInsideXY(Other.Max));
	}

	/**
	 * Gets a bounding volume transformed by a matrix.
	 *
	 * @param M The matrix to transform by.
	 * @return The transformed box.
	 * @see TransformProjectBy
	 */
	CORE_API FBox TransformBy( const FMatrix& M ) const;

	/**
	 * Gets a bounding volume transformed by a FTransform object.
	 *
	 * @param M The transformation object.
	 * @return The transformed box.
	 * @see TransformProjectBy
	 */
	CORE_API FBox TransformBy( const FTransform& M ) const;

	/** 
	 * Transforms and projects a world bounding box to screen space
	 *
	 * @param ProjM The projection matrix.
	 * @return The transformed box.
	 * @see TransformBy
	 */
	CORE_API FBox TransformProjectBy( const FMatrix& ProjM ) const;

	/**
	 * Get a textual representation of this box.
	 *
	 * @return A string describing the box.
	 */
	FString ToString() const;

public:

	/** 
	 * Utility function to build an AABB from Origin and Extent 
	 *
	 * @param Origin The location of the bounding box.
	 * @param Extent Half size of the bounding box.
	 * @return A new axis-aligned bounding box.
	 */
	static FBox BuildAABB( const FVector& Origin, const FVector& Extent )
	{
		FBox NewBox(Origin - Extent, Origin + Extent);

		return NewBox;
	}

public:

	/**
	 * Serializes the bounding box.
	 *
	 * @param Ar The archive to serialize into.
	 * @param Box The box to serialize.
	 * @return Reference to the Archive after serialization.
	 */
	friend FArchive& operator<<( FArchive& Ar, FBox& Box )
	{
		return Ar << Box.Min << Box.Max << Box.IsValid;
	}

	bool Serialize( FArchive& Ar )
	{
		Ar << *this;
		return true;
	}
};


/**
 * FBox specialization for TIsPODType trait.
 */
template<> struct TIsPODType<FBox> { enum { Value = true }; };

/* FBox inline functions
 *****************************************************************************/

FORCEINLINE FBox& FBox::operator+=( const FVector &Other )
{
	if (IsValid)
	{
		Min.X = FMath::Min(Min.X, Other.X);
		Min.Y = FMath::Min(Min.Y, Other.Y);
		Min.Z = FMath::Min(Min.Z, Other.Z);

		Max.X = FMath::Max(Max.X, Other.X);
		Max.Y = FMath::Max(Max.Y, Other.Y);
		Max.Z = FMath::Max(Max.Z, Other.Z);
	}
	else
	{
		Min = Max = Other;
		IsValid = 1;
	}

	return *this;
}


FORCEINLINE FBox& FBox::operator+=( const FBox& Other )
{
	if (IsValid && Other.IsValid)
	{
		Min.X = FMath::Min(Min.X, Other.Min.X);
		Min.Y = FMath::Min(Min.Y, Other.Min.Y);
		Min.Z = FMath::Min(Min.Z, Other.Min.Z);

		Max.X = FMath::Max(Max.X, Other.Max.X);
		Max.Y = FMath::Max(Max.Y, Other.Max.Y);
		Max.Z = FMath::Max(Max.Z, Other.Max.Z);
	}
	else if (Other.IsValid)
	{
		*this = Other;
	}

	return *this;
}


FORCEINLINE FVector FBox::GetClosestPointTo( const FVector& Point ) const
{
	// start by considering the point inside the box
	FVector ClosestPoint = Point;

	// now clamp to inside box if it's outside
	if (Point.X < Min.X)
	{
		ClosestPoint.X = Min.X;
	}
	else if (Point.X > Max.X)
	{
		ClosestPoint.X = Max.X;
	}

	// now clamp to inside box if it's outside
	if (Point.Y < Min.Y)
	{
		ClosestPoint.Y = Min.Y;
	}
	else if (Point.Y > Max.Y)
	{
		ClosestPoint.Y = Max.Y;
	}

	// Now clamp to inside box if it's outside.
	if (Point.Z < Min.Z)
	{
		ClosestPoint.Z = Min.Z;
	}
	else if (Point.Z > Max.Z)
	{
		ClosestPoint.Z = Max.Z;
	}

	return ClosestPoint;
}


FORCEINLINE bool FBox::Intersect( const FBox& Other ) const
{
	if ((Min.X > Other.Max.X) || (Other.Min.X > Max.X))
	{
		return false;
	}

	if ((Min.Y > Other.Max.Y) || (Other.Min.Y > Max.Y))
	{
		return false;
	}

	if ((Min.Z > Other.Max.Z) || (Other.Min.Z > Max.Z))
	{
		return false;
	}

	return true;
}


FORCEINLINE bool FBox::IntersectXY( const FBox& Other ) const
{
	if ((Min.X > Other.Max.X) || (Other.Min.X > Max.X))
	{
		return false;
	}

	if ((Min.Y > Other.Max.Y) || (Other.Min.Y > Max.Y))
	{
		return false;
	}

	return true;
}


FORCEINLINE FString FBox::ToString() const
{
	return FString::Printf(TEXT("IsValid=%s, Min=(%s), Max=(%s)"), IsValid ? TEXT("true") : TEXT("false"), *Min.ToString(), *Max.ToString());
}

/* FMath inline functions
 *****************************************************************************/

inline bool FMath::PointBoxIntersection
	(
	const FVector&	Point,
	const FBox&		Box
	)
{
	if(Point.X >= Box.Min.X && Point.X <= Box.Max.X &&
		Point.Y >= Box.Min.Y && Point.Y <= Box.Max.Y &&
		Point.Z >= Box.Min.Z && Point.Z <= Box.Max.Z)
		return 1;
	else
		return 0;
}

inline bool FMath::LineBoxIntersection
	( 
	const FBox& Box, 
	const FVector& Start, 
	const FVector& End, 
	const FVector& Direction
	)
{
	return LineBoxIntersection(Box, Start, End, Direction, Direction.Reciprocal());
}

inline bool FMath::LineBoxIntersection
	(
	const FBox&		Box,
	const FVector&	Start,
	const FVector&	End,
	const FVector&	Direction,
	const FVector&	OneOverDirection
	)
{
	FVector	Time;
	bool	bStartIsOutside = false;

	if(Start.X < Box.Min.X)
	{
		bStartIsOutside = true;
		if(End.X >= Box.Min.X)
		{
			Time.X = (Box.Min.X - Start.X) * OneOverDirection.X;
		}
		else
		{
			return false;
		}
	}
	else if(Start.X > Box.Max.X)
	{
		bStartIsOutside = true;
		if(End.X <= Box.Max.X)
		{
			Time.X = (Box.Max.X - Start.X) * OneOverDirection.X;
		}
		else
		{
			return false;
		}
	}
	else
	{
		Time.X = 0.0f;
	}

	if(Start.Y < Box.Min.Y)
	{
		bStartIsOutside = true;
		if(End.Y >= Box.Min.Y)
		{
			Time.Y = (Box.Min.Y - Start.Y) * OneOverDirection.Y;
		}
		else
		{
			return false;
		}
	}
	else if(Start.Y > Box.Max.Y)
	{
		bStartIsOutside = true;
		if(End.Y <= Box.Max.Y)
		{
			Time.Y = (Box.Max.Y - Start.Y) * OneOverDirection.Y;
		}
		else
		{
			return false;
		}
	}
	else
	{
		Time.Y = 0.0f;
	}

	if(Start.Z < Box.Min.Z)
	{
		bStartIsOutside = true;
		if(End.Z >= Box.Min.Z)
		{
			Time.Z = (Box.Min.Z - Start.Z) * OneOverDirection.Z;
		}
		else
		{
			return false;
		}
	}
	else if(Start.Z > Box.Max.Z)
	{
		bStartIsOutside = true;
		if(End.Z <= Box.Max.Z)
		{
			Time.Z = (Box.Max.Z - Start.Z) * OneOverDirection.Z;
		}
		else
		{
			return false;
		}
	}
	else
	{
		Time.Z = 0.0f;
	}

	if(bStartIsOutside)
	{
		const float	MaxTime = Max3(Time.X,Time.Y,Time.Z);

		if(MaxTime >= 0.0f && MaxTime <= 1.0f)
		{
			const FVector Hit = Start + Direction * MaxTime;
			const float BOX_SIDE_THRESHOLD = 0.1f;
			if(	Hit.X > Box.Min.X - BOX_SIDE_THRESHOLD && Hit.X < Box.Max.X + BOX_SIDE_THRESHOLD &&
				Hit.Y > Box.Min.Y - BOX_SIDE_THRESHOLD && Hit.Y < Box.Max.Y + BOX_SIDE_THRESHOLD &&
				Hit.Z > Box.Min.Z - BOX_SIDE_THRESHOLD && Hit.Z < Box.Max.Z + BOX_SIDE_THRESHOLD)
			{
				return true;
			}
		}

		return false;
	}
	else
	{
		return true;
	}
}

/**
 * Performs a sphere vs box intersection test using Arvo's algorithm:
 *
 *	for each i in (x, y, z)
 *		if (SphereCenter(i) < BoxMin(i)) d2 += (SphereCenter(i) - BoxMin(i)) ^ 2
 *		else if (SphereCenter(i) > BoxMax(i)) d2 += (SphereCenter(i) - BoxMax(i)) ^ 2
 *
 * @param Sphere the center of the sphere being tested against the AABB
 * @param RadiusSquared the size of the sphere being tested
 * @param AABB the box being tested against
 *
 * @return Whether the sphere/box intersect or not.
 */
FORCEINLINE bool FMath::SphereAABBIntersection(const FVector& SphereCenter,const float RadiusSquared,const FBox& AABB)
{
	// Accumulates the distance as we iterate axis
	float DistSquared = 0.f;
	// Check each axis for min/max and add the distance accordingly
	// NOTE: Loop manually unrolled for > 2x speed up
	if (SphereCenter.X < AABB.Min.X)
	{
		DistSquared += FMath::Square(SphereCenter.X - AABB.Min.X);
	}
	else if (SphereCenter.X > AABB.Max.X)
	{
		DistSquared += FMath::Square(SphereCenter.X - AABB.Max.X);
	}
	if (SphereCenter.Y < AABB.Min.Y)
	{
		DistSquared += FMath::Square(SphereCenter.Y - AABB.Min.Y);
	}
	else if (SphereCenter.Y > AABB.Max.Y)
	{
		DistSquared += FMath::Square(SphereCenter.Y - AABB.Max.Y);
	}
	if (SphereCenter.Z < AABB.Min.Z)
	{
		DistSquared += FMath::Square(SphereCenter.Z - AABB.Min.Z);
	}
	else if (SphereCenter.Z > AABB.Max.Z)
	{
		DistSquared += FMath::Square(SphereCenter.Z - AABB.Max.Z);
	}
	// If the distance is less than or equal to the radius, they intersect
	return DistSquared <= RadiusSquared;
}

