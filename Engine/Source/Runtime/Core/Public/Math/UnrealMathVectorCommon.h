// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

struct FGenericPlatformMath;
struct FMath;

FORCEINLINE bool VectorIsAligned(const void *Ptr)
{
	return !(PTRINT(Ptr) & (SIMD_ALIGNMENT - 1));
}

// Returns a normalized 4 vector = Vector / |Vector|.
// There is no handling of zero length vectors, use VectorNormalizeSafe if this is a possible input.
FORCEINLINE VectorRegister VectorNormalizeAccurate( const VectorRegister& Vector )
{
	const VectorRegister SquareSum = VectorDot4(Vector, Vector);
	const VectorRegister InvLength = VectorReciprocalSqrtAccurate(SquareSum);
	const VectorRegister NormalizedVector = VectorMultiply(InvLength, Vector);
	return NormalizedVector;
}

// Returns ((Vector dot Vector) >= 1e-8) ? (Vector / |Vector|) : DefaultValue
// Uses accurate 1/sqrt, not the estimate
FORCEINLINE VectorRegister VectorNormalizeSafe( const VectorRegister& Vector, const VectorRegister& DefaultValue )
{
	const VectorRegister SquareSum = VectorDot4(Vector, Vector);
	const VectorRegister NonZeroMask = VectorCompareGE(SquareSum, GlobalVectorConstants::SmallLengthThreshold);
	const VectorRegister InvLength = VectorReciprocalSqrtAccurate(SquareSum);
	const VectorRegister NormalizedVector = VectorMultiply(InvLength, Vector);
	return VectorSelect(NonZeroMask, NormalizedVector, DefaultValue);
}

/**
 * Returns non-zero if any element in Vec1 is lesser than the corresponding element in Vec2, otherwise 0.
 *
 * @param Vec1			1st source vector
 * @param Vec2			2nd source vector
 * @return				Non-zero integer if (Vec1.x < Vec2.x) || (Vec1.y < Vec2.y) || (Vec1.z < Vec2.z) || (Vec1.w < Vec2.w)
 */
FORCEINLINE uint32 VectorAnyLesserThan( VectorRegister Vec1, VectorRegister Vec2 )
{
	return VectorAnyGreaterThan( Vec2, Vec1 );
}

/**
 * Returns non-zero if all elements in Vec1 are greater than the corresponding elements in Vec2, otherwise 0.
 *
 * @param Vec1			1st source vector
 * @param Vec2			2nd source vector
 * @return				Non-zero integer if (Vec1.x > Vec2.x) && (Vec1.y > Vec2.y) && (Vec1.z > Vec2.z) && (Vec1.w > Vec2.w)
 */
FORCEINLINE uint32 VectorAllGreaterThan( VectorRegister Vec1, VectorRegister Vec2 )
{
	return !VectorAnyGreaterThan( Vec2, Vec1 );
}

/**
 * Returns non-zero if all elements in Vec1 are lesser than the corresponding elements in Vec2, otherwise 0.
 *
 * @param Vec1			1st source vector
 * @param Vec2			2nd source vector
 * @return				Non-zero integer if (Vec1.x < Vec2.x) && (Vec1.y < Vec2.y) && (Vec1.z < Vec2.z) && (Vec1.w < Vec2.w)
 */
FORCEINLINE uint32 VectorAllLesserThan( VectorRegister Vec1, VectorRegister Vec2 )
{
	return !VectorAnyGreaterThan( Vec1, Vec2 );
}

/*----------------------------------------------------------------------------
	VectorRegister specialization of templates.
----------------------------------------------------------------------------*/

/** Returns the smaller of the two values (operates on each component individually) */
template<> FORCEINLINE VectorRegister FGenericPlatformMath::Min(const VectorRegister A, const VectorRegister B)
{
	return VectorMin(A, B);
}

/** Returns the larger of the two values (operates on each component individually) */
template<> FORCEINLINE VectorRegister FGenericPlatformMath::Max(const VectorRegister A, const VectorRegister B)
{
	return VectorMax(A, B);
}

// Specialization of Lerp template that works with vector registers
template<> FORCEINLINE VectorRegister FMath::Lerp(const VectorRegister& A, const VectorRegister& B, const VectorRegister& Alpha)
{
	const VectorRegister Delta = VectorSubtract(B, A);
	return VectorMultiplyAdd(Alpha, Delta, A);
}

// A and B are quaternions.  The result is A + (|A.B| >= 0 ? 1 : -1) * B
FORCEINLINE VectorRegister VectorAccumulateQuaternionShortestPath(const VectorRegister& A, const VectorRegister& B)
{
	// Blend rotation
	//     To ensure the 'shortest route', we make sure the dot product between the both rotations is positive.
	//     const float Bias = (|A.B| >= 0 ? 1 : -1)
	//     return A + B * Bias;
	const VectorRegister Zero = VectorZero();
	const VectorRegister RotationDot = VectorDot4(A, B);
	const VectorRegister QuatRotationDirMask = VectorCompareGE(RotationDot, Zero);
	const VectorRegister NegativeB = VectorSubtract(Zero, B);
	const VectorRegister BiasTimesB = VectorSelect(QuatRotationDirMask, B, NegativeB);
	return VectorAdd(A, BiasTimesB);
}

// Normalize quaternion ( result = (Q.Q >= 1e-8) ? (Q / |Q|) : (0,0,0,1) )
FORCEINLINE VectorRegister VectorNormalizeQuaternion(const VectorRegister& UnnormalizedQuat)
{
	return VectorNormalizeSafe(UnnormalizedQuat, GlobalVectorConstants::Float0001);
}

// Normalize Rotator
FORCEINLINE VectorRegister VectorNormalizeRotator(const VectorRegister& UnnormalizedRotator)
{
	// shift in the range [-360,360]
	VectorRegister V0	= VectorMod( UnnormalizedRotator, GlobalVectorConstants::Float360);
	VectorRegister V1	= VectorAdd( V0, GlobalVectorConstants::Float360 );
	VectorRegister V2	= VectorSelect(VectorCompareGE(V0, VectorZero()), V0, V1);

	// shift to [-180,180]
	VectorRegister V3	= VectorSubtract( V2, GlobalVectorConstants::Float360 );
	VectorRegister V4	= VectorSelect(VectorCompareGT(V2, GlobalVectorConstants::Float180), V3, V2);

	return  V4;
}

/** 
 * Fast Linear Quaternion Interpolation for quaternions stored in VectorRegisters.
 * Result is NOT normalized.
 */
FORCEINLINE VectorRegister VectorLerpQuat(const VectorRegister& A, const VectorRegister& B, const VectorRegister& Alpha)
{
	// Blend rotation
	//     To ensure the 'shortest route', we make sure the dot product between the both rotations is positive.
	//     const float Bias = (|A.B| >= 0 ? 1 : -1)
	//     Rotation = (B * Alpha) + (A * (Bias * (1.f - Alpha)));
	const VectorRegister Zero = VectorZero();

	const VectorRegister OneMinusAlpha = VectorSubtract(VectorOne(), Alpha);

	const VectorRegister RotationDot = VectorDot4(A, B);
	const VectorRegister QuatRotationDirMask = VectorCompareGE(RotationDot, Zero);
	const VectorRegister NegativeA = VectorSubtract(Zero, A);
	const VectorRegister BiasTimesA = VectorSelect(QuatRotationDirMask, A, NegativeA);
	const VectorRegister BTimesWeight = VectorMultiply(B, Alpha);
	const VectorRegister UnnormalizedResult = VectorMultiplyAdd(BiasTimesA, OneMinusAlpha, BTimesWeight);

	return UnnormalizedResult;
}

/** 
 * Bi-Linear Quaternion interpolation for quaternions stored in VectorRegisters.
 * Result is NOT normalized.
 */
FORCEINLINE VectorRegister VectorBiLerpQuat(const VectorRegister& P00, const VectorRegister& P10, const VectorRegister& P01, const VectorRegister& P11, const VectorRegister& FracX, const VectorRegister& FracY)
{
	return VectorLerpQuat(
		VectorLerpQuat(P00, P10, FracX),
		VectorLerpQuat(P01, P11, FracX),
		FracY);
}

/** 
 * Inverse quaternion ( -X, -Y, -Z, W) 
 */
FORCEINLINE VectorRegister VectorQuaternionInverse(const VectorRegister& NormalizedQuat)
{
	return VectorMultiply(GlobalVectorConstants::QINV_SIGN_MASK, NormalizedQuat);
}


/**
 * Rotate a vector using a unit Quaternion.
 *
 * @param Quat Unit Quaternion to use for rotation.
 * @param VectorW0 Vector to rotate. W component must be zero.
 * @return Vector after rotation by Quat.
 */
FORCEINLINE VectorRegister VectorQuaternionRotateVector(const VectorRegister& Quat, const VectorRegister& VectorW0)
{
	// Q * V * Q.Inverse
	//const VectorRegister InverseRotation = VectorQuaternionInverse(Quat);
	//const VectorRegister Temp = VectorQuaternionMultiply2(Quat, VectorW0);
	//const VectorRegister Rotated = VectorQuaternionMultiply2(Temp, InverseRotation);

	// Equivalence of above can be shown to be:
	// http://people.csail.mit.edu/bkph/articles/Quaternions.pdf
	// V' = V + 2w(Q x V) + (2Q x (Q x V))
	// refactor:
	// V' = V + w(2(Q x V)) + (Q x (2(Q x V)))
	// T = 2(Q x V);
	// V' = V + w*(T) + (Q x T)

	const VectorRegister QW = VectorReplicate(Quat, 3);
	VectorRegister T = VectorCross(Quat, VectorW0);
	T = VectorAdd(T, T);
	const VectorRegister VTemp0 = VectorMultiplyAdd(QW, T, VectorW0);
	const VectorRegister VTemp1 = VectorCross(Quat, T);
	const VectorRegister Rotated = VectorAdd(VTemp0, VTemp1);
	return Rotated;
}

/**
 * Rotate a vector using the inverse of a unit Quaternion (rotation in the opposite direction).
 *
 * @param Quat Unit Quaternion to use for rotation.
 * @param VectorW0 Vector to rotate. W component must be zero.
 * @return Vector after rotation by the inverse of Quat.
 */
FORCEINLINE VectorRegister VectorQuaternionInverseRotateVector(const VectorRegister& Quat, const VectorRegister& VectorW0)
{
	// Q.Inverse * V * Q
	//const VectorRegister InverseRotation = VectorQuaternionInverse(Quat);
	//const VectorRegister Temp = VectorQuaternionMultiply2(InverseRotation, VectorW0);
	//const VectorRegister Rotated = VectorQuaternionMultiply2(Temp, Quat);

	const VectorRegister QInv = VectorQuaternionInverse(Quat);
	return VectorQuaternionRotateVector(QInv, VectorW0);
}


/**
* Rotate a vector using a pointer to a unit Quaternion.
*
* @param Result		Pointer to where the result should be stored
* @param Quat		Pointer to the unit quaternion (must not be the destination)
* @param VectorW0	Pointer to the vector (must not be the destination). W component must be zero.
*/
FORCEINLINE void VectorQuaternionRotateVectorPtr( void* RESTRICT Result, const void* RESTRICT Quat, const void* RESTRICT VectorW0)
{
	*((VectorRegister *)Result) = VectorQuaternionRotateVector(*((const VectorRegister *)Quat), *((const VectorRegister *)VectorW0));
}

/**
* Rotate a vector using the inverse of a unit Quaternion (rotation in the opposite direction).
*
* @param Result		Pointer to where the result should be stored
* @param Quat		Pointer to the unit quaternion (must not be the destination)
* @param VectorW0	Pointer to the vector (must not be the destination). W component must be zero.
*/
FORCEINLINE void VectorQuaternionInverseRotateVectorPtr(void* RESTRICT Result, const void* RESTRICT Quat, const void* RESTRICT VectorW0)
{
	*((VectorRegister *)Result) = VectorQuaternionInverseRotateVector(*((const VectorRegister *)Quat), *((const VectorRegister *)VectorW0));
}
