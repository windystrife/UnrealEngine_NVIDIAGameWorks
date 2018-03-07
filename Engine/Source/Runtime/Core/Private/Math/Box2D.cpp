// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Box2D.cpp: Implements the FBox2D class.
=============================================================================*/

/* FBox2D structors
 *****************************************************************************/

#include "Math/Box2D.h"

FBox2D::FBox2D(const FVector2D* Points, const int32 Count)
	: Min(0.f, 0.f)
	, Max(0.f, 0.f)
	, bIsValid(false)
{
	for (int32 PointItr = 0; PointItr < Count; PointItr++)
	{
		*this += Points[PointItr];
	}
}


FBox2D::FBox2D(const TArray<FVector2D>& Points)
	: Min(0.f, 0.f)
	, Max(0.f, 0.f)
	, bIsValid(false)
{
	for(const FVector2D& EachPoint : Points)
	{
		*this += EachPoint;
	}
}
