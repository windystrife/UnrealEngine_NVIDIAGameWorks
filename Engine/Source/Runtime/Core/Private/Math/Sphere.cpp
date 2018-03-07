// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Sphere.cpp: Implements the FSphere class.
=============================================================================*/

#include "Math/Sphere.h"
#include "Math/Box.h"
#include "Math/Transform.h"


/* FSphere structors
 *****************************************************************************/

FSphere::FSphere(const FVector* Pts, int32 Count)
	: Center(0, 0, 0)
	, W(0)
{
	if (Count)
	{
		const FBox Box(Pts, Count);

		*this = FSphere((Box.Min + Box.Max) / 2, 0);

		for (int32 i = 0; i < Count; i++)
		{
			const float Dist = FVector::DistSquared(Pts[i], Center);

			if (Dist > W)
			{
				W = Dist;
			}
		}

		W = FMath::Sqrt(W) * 1.001f;
	}
}


/* FSphere interface
 *****************************************************************************/

FSphere FSphere::TransformBy(const FMatrix& M) const
{
	FSphere	Result;

	Result.Center = M.TransformPosition(this->Center);

	const FVector XAxis(M.M[0][0], M.M[0][1], M.M[0][2]);
	const FVector YAxis(M.M[1][0], M.M[1][1], M.M[1][2]);
	const FVector ZAxis(M.M[2][0], M.M[2][1], M.M[2][2]);

	Result.W = FMath::Sqrt(FMath::Max(XAxis | XAxis, FMath::Max(YAxis | YAxis, ZAxis | ZAxis))) * W;

	return Result;
}


FSphere FSphere::TransformBy(const FTransform& M) const
{
	FSphere	Result;

	Result.Center = M.TransformPosition(this->Center);
	Result.W = M.GetMaximumAxisScale() * W;

	return Result;
}

float FSphere::GetVolume() const
{
	return (4.f / 3.f) * PI * (W * W * W);
}

FSphere& FSphere::operator+=(const FSphere &Other)
{
	if (W == 0.f)
	{
		*this = Other;
	}
	else if (IsInside(Other))
	{
		*this = Other;
	}
	else if (Other.IsInside(*this))
	{
		// no change		
	}
	else
	{
		FSphere NewSphere;

		FVector DirToOther = Other.Center - Center;
		FVector UnitDirToOther = DirToOther;
		UnitDirToOther.Normalize();

		float NewRadius = (DirToOther.Size() + Other.W + W) * 0.5f;

		// find end point
		FVector End1 = Other.Center + UnitDirToOther*Other.W;
		FVector End2 = Center - UnitDirToOther*W;
		FVector NewCenter = (End1 + End2)*0.5f;

		NewSphere.Center = NewCenter; 
		NewSphere.W = NewRadius;

		// make sure both are inside afterwards
		checkSlow (Other.IsInside(NewSphere, 1.f));
		checkSlow (IsInside(NewSphere, 1.f));

		*this = NewSphere;
	}

	return *this;
}
