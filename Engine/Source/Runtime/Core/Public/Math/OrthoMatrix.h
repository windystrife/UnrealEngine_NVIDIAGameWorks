// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Math/Plane.h"
#include "Math/Matrix.h"

class FOrthoMatrix
	: public FMatrix
{
public:

	/**
	 * Constructor
	 *
	 * @param Width view space width
	 * @param Height view space height
	 * @param ZScale scale in the Z axis
	 * @param ZOffset offset in the Z axis
	 */
	FOrthoMatrix(float Width,float Height,float ZScale,float ZOffset);
};


class FReversedZOrthoMatrix : public FMatrix
{
public:
	FReversedZOrthoMatrix(float Width,float Height,float ZScale,float ZOffset);
};


FORCEINLINE FOrthoMatrix::FOrthoMatrix(float Width,float Height,float ZScale,float ZOffset)
	: FMatrix(
		FPlane((Width)? (1.0f / Width) : 1.0f,	0.0f,								0.0f,				0.0f),
		FPlane(0.0f,							(Height)? (1.0f / Height) : 1.f,	0.0f,				0.0f),
		FPlane(0.0f,							0.0f,								ZScale,				0.0f),
		FPlane(0.0f,							0.0f,								ZOffset * ZScale,	1.0f)
	)
{ }


FORCEINLINE FReversedZOrthoMatrix::FReversedZOrthoMatrix(float Width,float Height,float ZScale,float ZOffset)
	: FMatrix(
		FPlane((Width)? (1.0f / Width) : 1.0f,	0.0f,								0.0f,					0.0f),
		FPlane(0.0f,							(Height)? (1.0f / Height) : 1.f,	0.0f,					0.0f),
		FPlane(0.0f,							0.0f,								-ZScale,				0.0f),
		FPlane(0.0f,							0.0f,								1.0 - ZOffset * ZScale,	1.0f)
	)
{ }
