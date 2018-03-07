// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Math/Vector.h"

/**
 * Structure for capsules.
 *
 * A capsule consists of two sphere connected by a cylinder.
 */
struct FCapsuleShape
{
	/** The capsule's center point. */
	FVector Center;

	/** The capsule's radius. */
	float Radius;

	/** The capsule's orientation in space. */
	FVector Orientation;

	/** The capsule's length. */
	float Length;

public:

	/** Default constructor. */
	FCapsuleShape() { }

	/**
	 * Create and inintialize a new instance.
	 *
	 * @param InCenter The capsule's center point.
	 * @param InRadius The capsule's radius.
	 * @param InOrientation The capsule's orientation in space.
	 * @param InLength The capsule's length.
	 */
	FCapsuleShape(FVector InCenter, float InRadius, FVector InOrientation, float InLength)
		: Center(InCenter)
		, Radius(InRadius)
		, Orientation(InOrientation)
		, Length(InLength)
	{ }
};
