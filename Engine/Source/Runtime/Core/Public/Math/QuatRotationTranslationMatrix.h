// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "Math/Quat.h"

/** Rotation and translation matrix using quaternion rotation */
class FQuatRotationTranslationMatrix
	: public FMatrix
{
public:

	/** Constructor
	*
	* @param Q rotation
	* @param Origin translation to apply
	*/
	FQuatRotationTranslationMatrix(const FQuat& Q, const FVector& Origin);

	/** Matrix factory. Return an FMatrix so we don't have type conversion issues in expressions. */
	static FMatrix Make(const FQuat& Q, const FVector& Origin)
	{
		return FQuatRotationTranslationMatrix(Q, Origin);
	}
};


/** Rotation matrix using quaternion rotation */
class FQuatRotationMatrix
	: public FQuatRotationTranslationMatrix
{
public:

	/** Constructor
	*
	* @param Q rotation
	*/
	FQuatRotationMatrix(const FQuat& Q)
		: FQuatRotationTranslationMatrix(Q, FVector::ZeroVector)
	{
	}

	/** Matrix factory. Return an FMatrix so we don't have type conversion issues in expressions. */
	static FMatrix Make(const FQuat& Q)
	{
		return FQuatRotationMatrix(Q);
	}
};


FORCEINLINE FQuatRotationTranslationMatrix::FQuatRotationTranslationMatrix(const FQuat& Q, const FVector& Origin)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && WITH_EDITORONLY_DATA
	// Make sure Quaternion is normalized
	check( Q.IsNormalized() );
#endif
	const float x2 = Q.X + Q.X;  const float y2 = Q.Y + Q.Y;  const float z2 = Q.Z + Q.Z;
	const float xx = Q.X * x2;   const float xy = Q.X * y2;   const float xz = Q.X * z2;
	const float yy = Q.Y * y2;   const float yz = Q.Y * z2;   const float zz = Q.Z * z2;
	const float wx = Q.W * x2;   const float wy = Q.W * y2;   const float wz = Q.W * z2;

	M[0][0] = 1.0f - (yy + zz);	M[1][0] = xy - wz;				M[2][0] = xz + wy;			M[3][0] = Origin.X;
	M[0][1] = xy + wz;			M[1][1] = 1.0f - (xx + zz);		M[2][1] = yz - wx;			M[3][1] = Origin.Y;
	M[0][2] = xz - wy;			M[1][2] = yz + wx;				M[2][2] = 1.0f - (xx + yy);	M[3][2] = Origin.Z;
	M[0][3] = 0.0f;				M[1][3] = 0.0f;					M[2][3] = 0.0f;				M[3][3] = 1.0f;
}
