// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/*=============================================================================
	ConvexVolume.h: Convex volume definitions.
=============================================================================*/

/**
 * Encapsulates the inside and/or outside state of an intersection test.
 */
struct FOutcode
{
private:

	uint32	Inside : 1,
				Outside : 1;

public:

	// Constructor.

	FOutcode():
		Inside(0), Outside(0)
	{}
	FOutcode(bool InInside,bool InOutside):
		Inside(InInside), Outside(InOutside)
	{}

	// Accessors.

	FORCEINLINE void SetInside(bool NewInside) { Inside = NewInside; }
	FORCEINLINE void SetOutside(bool NewOutside) { Outside = NewOutside; }
	FORCEINLINE bool GetInside() const { return Inside; }
	FORCEINLINE bool GetOutside() const { return Outside; }
};

//
//	FConvexVolume
//

struct ENGINE_API FConvexVolume
{
public:

	typedef TArray<FPlane,TInlineAllocator<6> > FPlaneArray;
	typedef TArray<FPlane,TInlineAllocator<8> > FPermutedPlaneArray;

	FPlaneArray Planes;
	/** This is the set of planes pre-permuted to SSE/Altivec form */
	FPermutedPlaneArray PermutedPlanes;

	FConvexVolume()
	{
//		int32 N = 5;
	}

	/**
	 * Builds the set of planes used to clip against. Also, puts the planes
	 * into a form more readily used by SSE/Altivec so 4 planes can be
	 * clipped against at once.
	 */
	FConvexVolume(const TArray<FPlane, TInlineAllocator<6>>& InPlanes) :
		Planes( InPlanes )
	{
		Init();
	}

	/**
	 * Builds the permuted planes for SSE/Altivec fast clipping
	 */
	void Init(void);

	/**
	 * Clips a polygon to the volume.
	 *
	 * @param	Polygon - The polygon to be clipped.  If the true is returned, contains the
	 *			clipped polygon.
	 *
	 * @return	Returns false if the polygon is entirely outside the volume and true otherwise.
	 */
	bool ClipPolygon(class FPoly& Polygon) const;

	// Intersection tests.

	FOutcode GetBoxIntersectionOutcode(const FVector& Origin,const FVector& Extent) const;

	bool IntersectBox(const FVector& Origin,const FVector& Extent) const;
	bool IntersectBox(const FVector& Origin,const FVector& Extent, bool& bOutFullyContained) const;
	bool IntersectSphere(const FVector& Origin,const float& Radius) const;
	bool IntersectSphere(const FVector& Origin,const float& Radius, bool& bOutFullyContained) const;

	/**
	 * Intersection test with a translated axis-aligned box.
	 * @param Origin - Origin of the box.
	 * @param Translation - Translation to apply to the box.
	 * @param Extent - Extent of the box along each axis.
	 * @returns true if this convex volume intersects the given translated box.
	 */
	bool IntersectBox(const FVector& Origin,const FVector& Translation,const FVector& Extent) const;

	/**
	 * Serializor
	 *
	 * @param	Ar				Archive to serialize data to
	 * @param	ConvexVolume	Convex volumes to serialize to archive
	 *
	 * @return passed in archive
	 */
	friend ENGINE_API FArchive& operator<<(FArchive& Ar,FConvexVolume& ConvexVolume);
};

/**
 * Creates a convex volume bounding the view frustum for a view-projection matrix.
 *
 * @param [out]	The FConvexVolume which contains the view frustum bounds.
 * @param		ViewProjectionMatrix - The view-projection matrix which defines the view frustum.
 * @param		bUseNearPlane - True if the convex volume should be bounded by the view frustum's near clipping plane.
 */
extern ENGINE_API void GetViewFrustumBounds(FConvexVolume& OutResult, const FMatrix& ViewProjectionMatrix, bool bUseNearPlane);

/**
 * Creates a convex volume bounding the view frustum for a view-projection matrix, with an optional far plane override.
 *
 * @param [out]	The FConvexVolume which contains the view frustum bounds.
 * @param		ViewProjectionMatrix - The view-projection matrix which defines the view frustum.
 * @param		InFarPlane - Plane to use if bOverrideFarPlane is true.
 * @param		bOverrideFarPlane - Whether to override the far plane.
 * @param		bUseNearPlane - True if the convex volume should be bounded by the view frustum's near clipping plane.
 */
extern ENGINE_API void GetViewFrustumBounds(FConvexVolume& OutResult, const FMatrix& ViewProjectionMatrix, const FPlane& InFarPlane, bool bOverrideFarPlane, bool bUseNearPlane);
