// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "Math/RotationTranslationMatrix.h"

/** Rotates about an Origin point. */
class FRotationAboutPointMatrix
	: public FRotationTranslationMatrix
{
public:

	/**
	 * Constructor.
	 *
	 * @param Rot rotation
	 * @param Origin about which to rotate.
	 */
	FRotationAboutPointMatrix(const FRotator& Rot, const FVector& Origin);

	/** Matrix factory. Return an FMatrix so we don't have type conversion issues in expressions. */
	static FMatrix Make(const FRotator& Rot, const FVector& Origin)
	{
		return FRotationAboutPointMatrix(Rot, Origin);
	}

	/** Matrix factory. Return an FMatrix so we don't have type conversion issues in expressions. */
	static CORE_API FMatrix Make(const FQuat& Rot, const FVector& Origin);
};


FORCEINLINE FRotationAboutPointMatrix::FRotationAboutPointMatrix(const FRotator& Rot, const FVector& Origin)
	: FRotationTranslationMatrix(Rot, Origin)
{
	// FRotationTranslationMatrix generates R * T.
	// We need -T * R * T, so prepend that translation:
	FVector XAxis(M[0][0], M[1][0], M[2][0]);
	FVector YAxis(M[0][1], M[1][1], M[2][1]);
	FVector ZAxis(M[0][2], M[1][2], M[2][2]);

	M[3][0]	-= XAxis | Origin;
	M[3][1]	-= YAxis | Origin;
	M[3][2]	-= ZAxis | Origin;
}
