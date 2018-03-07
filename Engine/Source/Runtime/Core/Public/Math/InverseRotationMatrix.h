// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Plane.h"
#include "Math/Matrix.h"

/** Inverse Rotation matrix */
class FInverseRotationMatrix
	: public FMatrix
{
public:
	/**
	 * Constructor.
	 *
	 * @param Rot rotation
	 */
	FInverseRotationMatrix(const FRotator& Rot);
};


FORCEINLINE FInverseRotationMatrix::FInverseRotationMatrix(const FRotator& Rot)
	: FMatrix(
		FMatrix( // Yaw
		FPlane(+FMath::Cos(Rot.Yaw * PI / 180.f), -FMath::Sin(Rot.Yaw * PI / 180.f), 0.0f, 0.0f),
		FPlane(+FMath::Sin(Rot.Yaw * PI / 180.f), +FMath::Cos(Rot.Yaw * PI / 180.f), 0.0f, 0.0f),
		FPlane(0.0f, 0.0f, 1.0f, 0.0f),
		FPlane(0.0f, 0.0f, 0.0f, 1.0f)) *
		FMatrix( // Pitch
		FPlane(+FMath::Cos(Rot.Pitch * PI / 180.f), 0.0f, -FMath::Sin(Rot.Pitch * PI / 180.f), 0.0f),
		FPlane(0.0f, 1.0f, 0.0f, 0.0f),
		FPlane(+FMath::Sin(Rot.Pitch * PI / 180.f), 0.0f, +FMath::Cos(Rot.Pitch * PI / 180.f), 0.0f),
		FPlane(0.0f, 0.0f, 0.0f, 1.0f)) *
		FMatrix( // Roll
		FPlane(1.0f, 0.0f, 0.0f, 0.0f),
		FPlane(0.0f, +FMath::Cos(Rot.Roll * PI / 180.f), +FMath::Sin(Rot.Roll * PI / 180.f), 0.0f),
		FPlane(0.0f, -FMath::Sin(Rot.Roll * PI / 180.f), +FMath::Cos(Rot.Roll * PI / 180.f), 0.0f),
		FPlane(0.0f, 0.0f, 0.0f, 1.0f))
	)
{ }
