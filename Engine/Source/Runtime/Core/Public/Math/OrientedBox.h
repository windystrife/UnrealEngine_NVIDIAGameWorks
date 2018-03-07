// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Math/Vector.h"
#include "Math/Interval.h"

/**
 * Structure for arbitrarily oriented boxes (not necessarily axis-aligned).
 */
struct FOrientedBox
{
	/** Holds the center of the box. */
	FVector Center;

	/** Holds the x-axis vector of the box. Must be a unit vector. */
	FVector AxisX;
	
	/** Holds the y-axis vector of the box. Must be a unit vector. */
	FVector AxisY;
	
	/** Holds the z-axis vector of the box. Must be a unit vector. */
	FVector AxisZ;

	/** Holds the extent of the box along its x-axis. */
	float ExtentX;
	
	/** Holds the extent of the box along its y-axis. */
	float ExtentY;

	/** Holds the extent of the box along its z-axis. */
	float ExtentZ;

public:

	/**
	 * Default constructor.
	 *
	 * Constructs a unit-sized, origin-centered box with axes aligned to the coordinate system.
	 */
	FOrientedBox()
		: Center(0.0f)
		, AxisX(1.0f, 0.0f, 0.0f)
		, AxisY(0.0f, 1.0f, 0.0f)
		, AxisZ(0.0f, 0.0f, 1.0f)
		, ExtentX(1.0f)
		, ExtentY(1.0f)
		, ExtentZ(1.0f)
	{ }

public:

	/**
	 * Fills in the Verts array with the eight vertices of the box.
	 *
	 * @param Verts The array to fill in with the vertices.
	 */
	FORCEINLINE void CalcVertices(FVector* Verts) const;

	/**
	 * Finds the projection interval of the box when projected onto Axis.
	 *
	 * @param Axis The unit vector defining the axis to project the box onto.
	 */
	FORCEINLINE FFloatInterval Project(const FVector& Axis) const;
};


/* FOrientedBox inline functions
 *****************************************************************************/

FORCEINLINE void FOrientedBox::CalcVertices( FVector* Verts ) const
{
	static const float Signs[] = { -1.0f, 1.0f };

	for (int32 i = 0; i < 2; i++)
	{
		for (int32 j = 0; j < 2; j++)
		{
			for (int32 k = 0; k < 2; k++)
			{
				*Verts++ = Center + Signs[i] * AxisX * ExtentX + Signs[j] * AxisY * ExtentY + Signs[k] * AxisZ * ExtentZ;
			}
		}
	}
}


FORCEINLINE FFloatInterval FOrientedBox::Project( const FVector& Axis ) const
{
	static const float Signs[] = {-1.0f, 1.0f};

	// calculate the projections of the box center and the extent-scaled axes.
	float ProjectedCenter = Axis | Center;
	float ProjectedAxisX = Axis | (ExtentX * AxisX);
	float ProjectedAxisY = Axis | (ExtentY * AxisY);
	float ProjectedAxisZ = Axis | (ExtentZ * AxisZ);

	FFloatInterval ProjectionInterval;

	for (int32 i = 0; i < 2; i++)
	{
		for (int32 j = 0; j < 2; j++)
		{
			for (int32 k = 0; k < 2; k++)
			{
				// project the box vertex onto the axis.
				float ProjectedVertex = ProjectedCenter + Signs[i] * ProjectedAxisX + Signs[j] * ProjectedAxisY + Signs[k] * ProjectedAxisZ;

				// if necessary, expand the projection interval to include the box vertex projection.
				ProjectionInterval.Include(ProjectedVertex);
			}
		}
	}

	return ProjectionInterval;
}

template <> struct TIsPODType<FOrientedBox> { enum { Value = true }; };
