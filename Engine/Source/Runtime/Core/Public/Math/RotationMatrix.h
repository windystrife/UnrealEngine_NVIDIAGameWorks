// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Math/Matrix.h"
#include "Math/RotationTranslationMatrix.h"

/** Rotation matrix no translation */
class FRotationMatrix
	: public FRotationTranslationMatrix
{
public:

	/**
	 * Constructor.
	 *
	 * @param Rot rotation
	 */
	FRotationMatrix(const FRotator& Rot)
		: FRotationTranslationMatrix(Rot, FVector::ZeroVector)
	{ }

	/** Matrix factory. Return an FMatrix so we don't have type conversion issues in expressions. */
	static FMatrix Make(FRotator const& Rot)
	{
		return FRotationMatrix(Rot);
	}

	/** Matrix factory. Return an FMatrix so we don't have type conversion issues in expressions. */
	static CORE_API FMatrix Make(FQuat const& Rot);

	/** Builds a rotation matrix given only a XAxis. Y and Z are unspecified but will be orthonormal. XAxis need not be normalized. */
	static CORE_API FMatrix MakeFromX(FVector const& XAxis);

	/** Builds a rotation matrix given only a YAxis. X and Z are unspecified but will be orthonormal. YAxis need not be normalized. */
	static CORE_API FMatrix MakeFromY(FVector const& YAxis);

	/** Builds a rotation matrix given only a ZAxis. X and Y are unspecified but will be orthonormal. ZAxis need not be normalized. */
	static CORE_API FMatrix MakeFromZ(FVector const& ZAxis);

	/** Builds a matrix with given X and Y axes. X will remain fixed, Y may be changed minimally to enforce orthogonality. Z will be computed. Inputs need not be normalized. */
	static CORE_API FMatrix MakeFromXY(FVector const& XAxis, FVector const& YAxis);

	/** Builds a matrix with given X and Z axes. X will remain fixed, Z may be changed minimally to enforce orthogonality. Y will be computed. Inputs need not be normalized. */
	static CORE_API FMatrix MakeFromXZ(FVector const& XAxis, FVector const& ZAxis);

	/** Builds a matrix with given Y and X axes. Y will remain fixed, X may be changed minimally to enforce orthogonality. Z will be computed. Inputs need not be normalized. */
	static CORE_API FMatrix MakeFromYX(FVector const& YAxis, FVector const& XAxis);

	/** Builds a matrix with given Y and Z axes. Y will remain fixed, Z may be changed minimally to enforce orthogonality. X will be computed. Inputs need not be normalized. */
	static CORE_API FMatrix MakeFromYZ(FVector const& YAxis, FVector const& ZAxis);

	/** Builds a matrix with given Z and X axes. Z will remain fixed, X may be changed minimally to enforce orthogonality. Y will be computed. Inputs need not be normalized. */
	static CORE_API FMatrix MakeFromZX(FVector const& ZAxis, FVector const& XAxis);

	/** Builds a matrix with given Z and Y axes. Z will remain fixed, Y may be changed minimally to enforce orthogonality. X will be computed. Inputs need not be normalized. */
	static CORE_API FMatrix MakeFromZY(FVector const& ZAxis, FVector const& YAxis);
};
