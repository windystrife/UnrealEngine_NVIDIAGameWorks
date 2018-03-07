// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Math/NumericLimits.h"
#include "Math/UnrealMathUtility.h"
#include "Templates/UnrealTemplate.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Logging/LogMacros.h"
#include "Containers/Map.h"
#include "Math/Vector.h"
#include "Math/VectorRegister.h"
#include "Math/Plane.h"
#include "Math/Rotator.h"
#include "Math/Matrix.h"
#include "Math/RotationMatrix.h"
#include "Math/Quat.h"
#include "Math/QuatRotationTranslationMatrix.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

DEFINE_LOG_CATEGORY_STATIC(LogUnrealMathTest, Log, All);

// Create some temporary storage variables
MS_ALIGN( 16 ) static float GScratch[16] GCC_ALIGN( 16 );
static float GSum;
static bool GPassing;

#define MATHTEST_INLINE FORCENOINLINE // if you want to do performance testing change to FORCEINLINE or FORCEINLINE_DEBUGGABLE

/**
 * Tests if two vectors (xyzw) are bitwise equal
 *
 * @param Vec0 First vector
 * @param Vec1 Second vector
 *
 * @return true if equal
 */
bool TestVectorsEqualBitwise( VectorRegister Vec0, VectorRegister Vec1)
{
	VectorStoreAligned( Vec0, GScratch + 0 );
	VectorStoreAligned( Vec1, GScratch + 4 );

	const bool Passed = (memcmp(GScratch + 0, GScratch + 4, sizeof(float) * 4) == 0);

	GPassing = GPassing && Passed;
	return Passed;
}


/**
 * Tests if two vectors (xyzw) are equal within an optional tolerance
 *
 * @param Vec0 First vector
 * @param Vec1 Second vector
 * @param Tolerance Error allowed for the comparison
 *
 * @return true if equal(ish)
 */
bool TestVectorsEqual( VectorRegister Vec0, VectorRegister Vec1, float Tolerance = 0.0f)
{
	VectorStoreAligned( Vec0, GScratch + 0 );
	VectorStoreAligned( Vec1, GScratch + 4 );
	GSum = 0.f;
	for ( int32 Component = 0; Component < 4; Component++ ) 
	{
		float Diff = GScratch[ Component + 0 ] - GScratch[ Component + 4 ];
		GSum += ( Diff >= 0.0f ) ? Diff : -Diff;
	}
	GPassing = GPassing && GSum <= Tolerance;
	return GSum <= Tolerance;
}


/**
 * Enforce tolerance per-component, not summed error
 */
bool TestVectorsEqual_ComponentWiseError(VectorRegister Vec0, VectorRegister Vec1, float Tolerance = 0.0f)
{
	VectorStoreAligned(Vec0, GScratch + 0);
	VectorStoreAligned(Vec1, GScratch + 4);
	bool bPassing = true;
	for (int32 Component = 0; Component < 4; Component++)
	{
		float Diff = GScratch[Component + 0] - GScratch[Component + 4];
		bPassing &= FMath::IsNearlyZero(Diff, Tolerance);
	}
	GPassing &= bPassing;
	return bPassing;
}


/**
 * Tests if two vectors (xyz) are equal within an optional tolerance
 *
 * @param Vec0 First vector
 * @param Vec1 Second vector
 * @param Tolerance Error allowed for the comparison
 *
 * @return true if equal(ish)
 */
bool TestVectorsEqual3( VectorRegister Vec0, VectorRegister Vec1, float Tolerance = 0.0f)
{
	VectorStoreAligned( Vec0, GScratch + 0 );
	VectorStoreAligned( Vec1, GScratch + 4 );
	GSum = 0.f;
	for ( int32 Component = 0; Component < 3; Component++ ) 
	{
		GSum += FMath::Abs<float>( GScratch[ Component + 0 ] - GScratch[ Component + 4 ] );
	}
	GPassing = GPassing && GSum <= Tolerance;
	return GSum <= Tolerance;
}

/**
 * Tests if two vectors (xyz) are equal within an optional tolerance
 *
 * @param Vec0 First vector
 * @param Vec1 Second vector
 * @param Tolerance Error allowed for the comparison
 *
 * @return true if equal(ish)
 */
bool TestFVector3Equal( const FVector& Vec0, const FVector& Vec1, float Tolerance = 0.0f)
{
	GScratch[0] = Vec0.X;
	GScratch[1] = Vec0.Y;
	GScratch[2] = Vec0.Z;
	GScratch[3] = 0.0f;
	GScratch[4] = Vec1.X;
	GScratch[5] = Vec1.Y;
	GScratch[6] = Vec1.Z;
	GScratch[7] = 0.0f;
	GSum = 0.f;

	for ( int32 Component = 0; Component < 3; Component++ ) 
	{
		GSum += FMath::Abs<float>( GScratch[ Component + 0 ] - GScratch[ Component + 4 ] );
	}
	GPassing = GPassing && GSum <= Tolerance;
	return GSum <= Tolerance;
}

bool TestQuatsEqual(const FQuat& Q0, const FQuat& Q1, float Tolerance)
{
	GScratch[0] = Q0.X;
	GScratch[1] = Q0.Y;
	GScratch[2] = Q0.Z;
	GScratch[3] = Q0.W;
	GScratch[4] = Q1.X;
	GScratch[5] = Q1.Y;
	GScratch[6] = Q1.Z;
	GScratch[7] = Q1.W;
	GSum = 0.f;

	const bool bEqual = Q0.Equals(Q1, Tolerance);
	GPassing = GPassing && bEqual;
	return bEqual;
}

/**
 * Tests if two matrices (4x4 xyzw) are equal within an optional tolerance
 *
 * @param Mat0 First Matrix
 * @param Mat1 Second Matrix
 * @param Tolerance Error per column allowed for the comparison
 *
 * @return true if equal(ish)
 */
bool TestMatricesEqual( FMatrix &Mat0, FMatrix &Mat1, float Tolerance = 0.0f)
{
	for (int32 Row = 0; Row < 4; ++Row ) 
	{
		GSum = 0.f;
		for ( int32 Column = 0; Column < 4; ++Column ) 
		{
			float Diff = Mat0.M[Row][Column] - Mat1.M[Row][Column];
			GSum += ( Diff >= 0.0f ) ? Diff : -Diff;
		}
		if (GSum > Tolerance)
		{
			GPassing = false; 
			return false;
		}
	}
	return true;
}


/**
 * Multiplies two 4x4 matrices.
 *
 * @param Result	Pointer to where the result should be stored
 * @param Matrix1	Pointer to the first matrix
 * @param Matrix2	Pointer to the second matrix
 */
void TestVectorMatrixMultiply( void* Result, const void* Matrix1, const void* Matrix2 )
{
	typedef float Float4x4[4][4];
	const Float4x4& A = *((const Float4x4*) Matrix1);
	const Float4x4& B = *((const Float4x4*) Matrix2);
	Float4x4 Temp;
	Temp[0][0] = A[0][0] * B[0][0] + A[0][1] * B[1][0] + A[0][2] * B[2][0] + A[0][3] * B[3][0];
	Temp[0][1] = A[0][0] * B[0][1] + A[0][1] * B[1][1] + A[0][2] * B[2][1] + A[0][3] * B[3][1];
	Temp[0][2] = A[0][0] * B[0][2] + A[0][1] * B[1][2] + A[0][2] * B[2][2] + A[0][3] * B[3][2];
	Temp[0][3] = A[0][0] * B[0][3] + A[0][1] * B[1][3] + A[0][2] * B[2][3] + A[0][3] * B[3][3];

	Temp[1][0] = A[1][0] * B[0][0] + A[1][1] * B[1][0] + A[1][2] * B[2][0] + A[1][3] * B[3][0];
	Temp[1][1] = A[1][0] * B[0][1] + A[1][1] * B[1][1] + A[1][2] * B[2][1] + A[1][3] * B[3][1];
	Temp[1][2] = A[1][0] * B[0][2] + A[1][1] * B[1][2] + A[1][2] * B[2][2] + A[1][3] * B[3][2];
	Temp[1][3] = A[1][0] * B[0][3] + A[1][1] * B[1][3] + A[1][2] * B[2][3] + A[1][3] * B[3][3];

	Temp[2][0] = A[2][0] * B[0][0] + A[2][1] * B[1][0] + A[2][2] * B[2][0] + A[2][3] * B[3][0];
	Temp[2][1] = A[2][0] * B[0][1] + A[2][1] * B[1][1] + A[2][2] * B[2][1] + A[2][3] * B[3][1];
	Temp[2][2] = A[2][0] * B[0][2] + A[2][1] * B[1][2] + A[2][2] * B[2][2] + A[2][3] * B[3][2];
	Temp[2][3] = A[2][0] * B[0][3] + A[2][1] * B[1][3] + A[2][2] * B[2][3] + A[2][3] * B[3][3];

	Temp[3][0] = A[3][0] * B[0][0] + A[3][1] * B[1][0] + A[3][2] * B[2][0] + A[3][3] * B[3][0];
	Temp[3][1] = A[3][0] * B[0][1] + A[3][1] * B[1][1] + A[3][2] * B[2][1] + A[3][3] * B[3][1];
	Temp[3][2] = A[3][0] * B[0][2] + A[3][1] * B[1][2] + A[3][2] * B[2][2] + A[3][3] * B[3][2];
	Temp[3][3] = A[3][0] * B[0][3] + A[3][1] * B[1][3] + A[3][2] * B[2][3] + A[3][3] * B[3][3];
	memcpy( Result, &Temp, 16*sizeof(float) );
}


/**
 * Calculate the inverse of an FMatrix.
 *
 * @param DstMatrix		FMatrix pointer to where the result should be stored
 * @param SrcMatrix		FMatrix pointer to the Matrix to be inversed
 */
 void TestVectorMatrixInverse( void* DstMatrix, const void* SrcMatrix )
{
	typedef float Float4x4[4][4];
	const Float4x4& M = *((const Float4x4*) SrcMatrix);
	Float4x4 Result;
	float Det[4];
	Float4x4 Tmp;

	Tmp[0][0]	= M[2][2] * M[3][3] - M[2][3] * M[3][2];
	Tmp[0][1]	= M[1][2] * M[3][3] - M[1][3] * M[3][2];
	Tmp[0][2]	= M[1][2] * M[2][3] - M[1][3] * M[2][2];

	Tmp[1][0]	= M[2][2] * M[3][3] - M[2][3] * M[3][2];
	Tmp[1][1]	= M[0][2] * M[3][3] - M[0][3] * M[3][2];
	Tmp[1][2]	= M[0][2] * M[2][3] - M[0][3] * M[2][2];

	Tmp[2][0]	= M[1][2] * M[3][3] - M[1][3] * M[3][2];
	Tmp[2][1]	= M[0][2] * M[3][3] - M[0][3] * M[3][2];
	Tmp[2][2]	= M[0][2] * M[1][3] - M[0][3] * M[1][2];

	Tmp[3][0]	= M[1][2] * M[2][3] - M[1][3] * M[2][2];
	Tmp[3][1]	= M[0][2] * M[2][3] - M[0][3] * M[2][2];
	Tmp[3][2]	= M[0][2] * M[1][3] - M[0][3] * M[1][2];

	Det[0]		= M[1][1]*Tmp[0][0] - M[2][1]*Tmp[0][1] + M[3][1]*Tmp[0][2];
	Det[1]		= M[0][1]*Tmp[1][0] - M[2][1]*Tmp[1][1] + M[3][1]*Tmp[1][2];
	Det[2]		= M[0][1]*Tmp[2][0] - M[1][1]*Tmp[2][1] + M[3][1]*Tmp[2][2];
	Det[3]		= M[0][1]*Tmp[3][0] - M[1][1]*Tmp[3][1] + M[2][1]*Tmp[3][2];

	float Determinant = M[0][0]*Det[0] - M[1][0]*Det[1] + M[2][0]*Det[2] - M[3][0]*Det[3];
	const float	RDet = 1.0f / Determinant;

	Result[0][0] =  RDet * Det[0];
	Result[0][1] = -RDet * Det[1];
	Result[0][2] =  RDet * Det[2];
	Result[0][3] = -RDet * Det[3];
	Result[1][0] = -RDet * (M[1][0]*Tmp[0][0] - M[2][0]*Tmp[0][1] + M[3][0]*Tmp[0][2]);
	Result[1][1] =  RDet * (M[0][0]*Tmp[1][0] - M[2][0]*Tmp[1][1] + M[3][0]*Tmp[1][2]);
	Result[1][2] = -RDet * (M[0][0]*Tmp[2][0] - M[1][0]*Tmp[2][1] + M[3][0]*Tmp[2][2]);
	Result[1][3] =  RDet * (M[0][0]*Tmp[3][0] - M[1][0]*Tmp[3][1] + M[2][0]*Tmp[3][2]);
	Result[2][0] =  RDet * (
					M[1][0] * (M[2][1] * M[3][3] - M[2][3] * M[3][1]) -
					M[2][0] * (M[1][1] * M[3][3] - M[1][3] * M[3][1]) +
					M[3][0] * (M[1][1] * M[2][3] - M[1][3] * M[2][1])
				);
	Result[2][1] = -RDet * (
					M[0][0] * (M[2][1] * M[3][3] - M[2][3] * M[3][1]) -
					M[2][0] * (M[0][1] * M[3][3] - M[0][3] * M[3][1]) +
					M[3][0] * (M[0][1] * M[2][3] - M[0][3] * M[2][1])
				);
	Result[2][2] =  RDet * (
					M[0][0] * (M[1][1] * M[3][3] - M[1][3] * M[3][1]) -
					M[1][0] * (M[0][1] * M[3][3] - M[0][3] * M[3][1]) +
					M[3][0] * (M[0][1] * M[1][3] - M[0][3] * M[1][1])
				);
	Result[2][3] = -RDet * (
					M[0][0] * (M[1][1] * M[2][3] - M[1][3] * M[2][1]) -
					M[1][0] * (M[0][1] * M[2][3] - M[0][3] * M[2][1]) +
					M[2][0] * (M[0][1] * M[1][3] - M[0][3] * M[1][1])
				);
	Result[3][0] = -RDet * (
					M[1][0] * (M[2][1] * M[3][2] - M[2][2] * M[3][1]) -
					M[2][0] * (M[1][1] * M[3][2] - M[1][2] * M[3][1]) +
					M[3][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1])
				);
	Result[3][1] =  RDet * (
					M[0][0] * (M[2][1] * M[3][2] - M[2][2] * M[3][1]) -
					M[2][0] * (M[0][1] * M[3][2] - M[0][2] * M[3][1]) +
					M[3][0] * (M[0][1] * M[2][2] - M[0][2] * M[2][1])
				);
	Result[3][2] = -RDet * (
					M[0][0] * (M[1][1] * M[3][2] - M[1][2] * M[3][1]) -
					M[1][0] * (M[0][1] * M[3][2] - M[0][2] * M[3][1]) +
					M[3][0] * (M[0][1] * M[1][2] - M[0][2] * M[1][1])
				);
	Result[3][3] =  RDet * (
				M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1]) -
				M[1][0] * (M[0][1] * M[2][2] - M[0][2] * M[2][1]) +
				M[2][0] * (M[0][1] * M[1][2] - M[0][2] * M[1][1])
			);

	memcpy( DstMatrix, &Result, 16*sizeof(float) );
}


/**
 * Calculate Homogeneous transform.
 *
 * @param VecP			VectorRegister 
 * @param MatrixM		FMatrix pointer to the Matrix to apply transform
 * @return VectorRegister = VecP*MatrixM
 */
VectorRegister TestVectorTransformVector(const VectorRegister&  VecP,  const void* MatrixM )
{
	typedef float Float4x4[4][4];
	union { VectorRegister v; float f[4]; } Tmp, Result;
	Tmp.v = VecP;
	const Float4x4& M = *((const Float4x4*)MatrixM);	

	Result.f[0] = Tmp.f[0] * M[0][0] + Tmp.f[1] * M[1][0] + Tmp.f[2] * M[2][0] + Tmp.f[3] * M[3][0];
	Result.f[1] = Tmp.f[0] * M[0][1] + Tmp.f[1] * M[1][1] + Tmp.f[2] * M[2][1] + Tmp.f[3] * M[3][1];
	Result.f[2] = Tmp.f[0] * M[0][2] + Tmp.f[1] * M[1][2] + Tmp.f[2] * M[2][2] + Tmp.f[3] * M[3][2];
	Result.f[3] = Tmp.f[0] * M[0][3] + Tmp.f[1] * M[1][3] + Tmp.f[2] * M[2][3] + Tmp.f[3] * M[3][3];

	return Result.v;
}

/**
* Get Rotation as a quaternion.
* @param Rotator FRotator 
* @return Rotation as a quaternion.
*/
MATHTEST_INLINE FQuat TestRotatorToQuaternion( const FRotator& Rotator)
{
	const float CR = FMath::Cos(FMath::DegreesToRadians(Rotator.Roll  * 0.5f));
	const float CP = FMath::Cos(FMath::DegreesToRadians(Rotator.Pitch * 0.5f));
	const float CY = FMath::Cos(FMath::DegreesToRadians(Rotator.Yaw   * 0.5f));
	const float SR = FMath::Sin(FMath::DegreesToRadians(Rotator.Roll  * 0.5f));
	const float SP = FMath::Sin(FMath::DegreesToRadians(Rotator.Pitch * 0.5f));
	const float SY = FMath::Sin(FMath::DegreesToRadians(Rotator.Yaw   * 0.5f));

	FQuat RotationQuat;
	RotationQuat.W = CR*CP*CY + SR*SP*SY;
	RotationQuat.X = CR*SP*SY - SR*CP*CY;
	RotationQuat.Y = -CR*SP*CY - SR*CP*SY;
	RotationQuat.Z = CR*CP*SY - SR*SP*CY;
	return RotationQuat;
}

MATHTEST_INLINE FVector TestQuaternionRotateVectorScalar(const FQuat& Quat, const FVector& Vector)
{
	// (q.W*q.W-qv.qv)v + 2(qv.v)qv + 2 q.W (qv x v)
	const FVector qv(Quat.X, Quat.Y, Quat.Z);
	FVector vOut = (2.f * Quat.W) * (qv ^ Vector);
	vOut += ((Quat.W * Quat.W) - (qv | qv)) * Vector;
	vOut += (2.f * (qv | Vector)) * qv;
	
	return vOut;
}

// Q * V * Q^-1
MATHTEST_INLINE FVector TestQuaternionMultiplyVector(const FQuat& Quat, const FVector& Vector)
{
	FQuat VQ(Vector.X, Vector.Y, Vector.Z, 0.f);
	FQuat VT, VR;
	FQuat I = Quat.Inverse();
	VectorQuaternionMultiply(&VT, &Quat, &VQ);
	VectorQuaternionMultiply(&VR, &VT, &I);

	return FVector(VR.X, VR.Y, VR.Z);
}

MATHTEST_INLINE FVector TestQuaternionRotateVectorRegister(const FQuat& Quat, const FVector &V)
{
	const VectorRegister Rotation = *((const VectorRegister*)(&Quat));
	const VectorRegister InputVectorW0 = VectorLoadFloat3_W0(&V);
	const VectorRegister RotatedVec = VectorQuaternionRotateVector(Rotation, InputVectorW0);

	FVector Result;
	VectorStoreFloat3(RotatedVec, &Result);
	return Result;
}


/**
* Multiplies two quaternions: The order matters.
*
* @param Result	Pointer to where the result should be stored
* @param Quat1	Pointer to the first quaternion (must not be the destination)
* @param Quat2	Pointer to the second quaternion (must not be the destination)
*/
void TestVectorQuaternionMultiply( void *Result, const void* Quat1, const void* Quat2)
{
	typedef float Float4[4];
	const Float4& A = *((const Float4*) Quat1);
	const Float4& B = *((const Float4*) Quat2);
	Float4 & R = *((Float4*) Result);

	// store intermediate results in temporaries
	const float TX = A[3]*B[0] + A[0]*B[3] + A[1]*B[2] - A[2]*B[1];
	const float TY = A[3]*B[1] - A[0]*B[2] + A[1]*B[3] + A[2]*B[0];
	const float TZ = A[3]*B[2] + A[0]*B[1] - A[1]*B[0] + A[2]*B[3];
	const float TW = A[3]*B[3] - A[0]*B[0] - A[1]*B[1] - A[2]*B[2];

	// copy intermediate result to *this
	R[0] = TX;
	R[1] = TY;
	R[2] = TZ;
	R[3] = TW;
}

/**
 * Converts a Quaternion to a Rotator.
 */
FORCENOINLINE FRotator TestQuaternionToRotator(const FQuat& Quat)
{
	const float X = Quat.X;
	const float Y = Quat.Y;
	const float Z = Quat.Z;
	const float W = Quat.W;

	const float SingularityTest = Z*X-W*Y;
	const float YawY = 2.f*(W*Z+X*Y);
	const float YawX = (1.f-2.f*(FMath::Square(Y) + FMath::Square(Z)));
	const float SINGULARITY_THRESHOLD = 0.4999995f;

	static const float RAD_TO_DEG = (180.f)/PI;
	FRotator RotatorFromQuat;

	// Note: using stock C functions for some trig functions since this is the "reference" implementation
	// and we don't want fast approximations to be used here.
	if (SingularityTest < -SINGULARITY_THRESHOLD)
	{
		RotatorFromQuat.Pitch = 270.f;
		RotatorFromQuat.Yaw = atan2f(YawY, YawX) * RAD_TO_DEG;
		RotatorFromQuat.Roll = -RotatorFromQuat.Yaw - (2.f * atan2f(X, W) * RAD_TO_DEG);
	}
	else if (SingularityTest > SINGULARITY_THRESHOLD)
	{
		RotatorFromQuat.Pitch = 90.f;
		RotatorFromQuat.Yaw = atan2f(YawY, YawX) * RAD_TO_DEG;
		RotatorFromQuat.Roll = RotatorFromQuat.Yaw - (2.f * atan2f(X, W) * RAD_TO_DEG);
	}
	else
	{
		RotatorFromQuat.Pitch = FMath::Asin(2.f*(SingularityTest)) * RAD_TO_DEG;
		RotatorFromQuat.Yaw = atan2f(YawY, YawX) * RAD_TO_DEG;
		RotatorFromQuat.Roll = atan2f(-2.f*(W*X+Y*Z), (1.f-2.f*(FMath::Square(X) + FMath::Square(Y)))) * RAD_TO_DEG;
	}

	RotatorFromQuat.Pitch = FRotator::NormalizeAxis(RotatorFromQuat.Pitch);
	RotatorFromQuat.Yaw = FRotator::NormalizeAxis(RotatorFromQuat.Yaw);
	RotatorFromQuat.Roll = FRotator::NormalizeAxis(RotatorFromQuat.Roll);

	return RotatorFromQuat;
}


FORCENOINLINE FQuat FindBetween_Old(const FVector& vec1, const FVector& vec2)
{
	const FVector cross = vec1 ^ vec2;
	const float crossMag = cross.Size();

	// See if vectors are parallel or anti-parallel
	if (crossMag < KINDA_SMALL_NUMBER)
	{
		// If these vectors are parallel - just return identity quaternion (ie no rotation).
		const float Dot = vec1 | vec2;
		if (Dot > -KINDA_SMALL_NUMBER)
		{
			return FQuat::Identity; // no rotation
		}
		// Exactly opposite..
		else
		{
			// ..rotation by 180 degrees around a vector orthogonal to vec1 & vec2
			FVector Vec = vec1.SizeSquared() > vec2.SizeSquared() ? vec1 : vec2;
			Vec.Normalize();

			FVector AxisA, AxisB;
			Vec.FindBestAxisVectors(AxisA, AxisB);

			return FQuat(AxisA.X, AxisA.Y, AxisA.Z, 0.f); // (axis*sin(pi/2), cos(pi/2)) = (axis, 0)
		}
	}

	// Not parallel, so use normal code
	float angle = FMath::Asin(crossMag);

	const float dot = vec1 | vec2;
	if (dot < 0.0f)
	{
		angle = PI - angle;
	}

	float sinHalfAng, cosHalfAng;
	FMath::SinCos(&sinHalfAng, &cosHalfAng, 0.5f * angle);
	const FVector axis = cross / crossMag;

	return FQuat(
		sinHalfAng * axis.X,
		sinHalfAng * axis.Y,
		sinHalfAng * axis.Z,
		cosHalfAng);
}


// ROTATOR TESTS

bool TestRotatorEqual0(const FRotator& A, const FRotator& B, const float Tolerance)
{
	// This is the version used for a few years (known working version).
	return (FMath::Abs(FRotator::NormalizeAxis(A.Pitch - B.Pitch)) <= Tolerance)
		&& (FMath::Abs(FRotator::NormalizeAxis(A.Yaw - B.Yaw)) <= Tolerance)
		&& (FMath::Abs(FRotator::NormalizeAxis(A.Roll - B.Roll)) <= Tolerance);
}

bool TestRotatorEqual1(const FRotator& A, const FRotator& B, const float Tolerance)
{
	// Test the vectorized method.
	const VectorRegister RegA = VectorLoadFloat3_W0(&A);
	const VectorRegister RegB = VectorLoadFloat3_W0(&B);
	const VectorRegister NormDelta = VectorNormalizeRotator(VectorSubtract(RegA, RegB));
	const VectorRegister AbsNormDelta = VectorAbs(NormDelta);
	return !VectorAnyGreaterThan(AbsNormDelta, VectorLoadFloat1(&Tolerance));
}

bool TestRotatorEqual2(const FRotator& A, const FRotator& B, const float Tolerance)
{
	// Test the FRotator method itself. It will likely be an equivalent implementation as 0 or 1 above.
	return A.Equals(B, Tolerance);
}

bool TestRotatorEqual3(const FRotator& A, const FRotator& B, const float Tolerance)
{
	// Logically equivalent to tests above. Also tests IsNearlyZero().
	return (A-B).IsNearlyZero(Tolerance);
}

// Report an error if bComparison is not equal to bExpected.
void LogRotatorTest(bool bExpected, const TCHAR* TestName, const FRotator& A, const FRotator& B, bool bComparison)
{
	const bool bHasPassed = (bComparison == bExpected);
	if (bHasPassed == false)
	{
		UE_LOG(LogUnrealMathTest, Log, TEXT("%s: %s"), bHasPassed ? TEXT("PASSED") : TEXT("FAILED"), TestName);
		UE_LOG(LogUnrealMathTest, Log, TEXT("(%s).Equals(%s) = %d"), *A.ToString(), *B.ToString(), bComparison);
		GPassing = false;
	}
}


void LogRotatorTest(const TCHAR* TestName, const FRotator& A, const FRotator& B, bool bComparison)
{
	if (bComparison == false)
	{
		UE_LOG(LogUnrealMathTest, Log, TEXT("%s: %s"), bComparison ? TEXT("PASSED") : TEXT("FAILED"), TestName);
		UE_LOG(LogUnrealMathTest, Log, TEXT("(%s).Equals(%s) = %d"), *A.ToString(), *B.ToString(), bComparison);
		GPassing = false;
	}
}

void LogQuaternionTest(const TCHAR* TestName, const FQuat& A, const FQuat& B, bool bComparison)
{
	if (bComparison == false)
	{
		UE_LOG(LogUnrealMathTest, Log, TEXT("%s: %s"), bComparison ? TEXT("PASSED") : TEXT("FAILED"), TestName);
		UE_LOG(LogUnrealMathTest, Log, TEXT("(%s).Equals(%s) = %d"), *A.ToString(), *B.ToString(), bComparison);
		GPassing = false;
	}
}


// Normalize tests

MATHTEST_INLINE VectorRegister TestVectorNormalize_Sqrt(const VectorRegister& V)
{
	const VectorRegister Len = VectorDot4(V, V);
	const float rlen = 1.0f / FMath::Sqrt(VectorGetComponent(Len, 0));
	return VectorMultiply(V, VectorLoadFloat1(&rlen));
}

MATHTEST_INLINE VectorRegister TestVectorNormalize_InvSqrt(const VectorRegister& V)
{
	const VectorRegister Len = VectorDot4(V, V);
	const float rlen = FMath::InvSqrt(VectorGetComponent(Len, 0));
	return VectorMultiply(V, VectorLoadFloat1(&rlen));
}

MATHTEST_INLINE VectorRegister TestVectorNormalize_InvSqrtEst(const VectorRegister& V)
{
	const VectorRegister Len = VectorDot4(V, V);
	const float rlen = FMath::InvSqrtEst(VectorGetComponent(Len, 0));
	return VectorMultiply(V, VectorLoadFloat1(&rlen));
}


// A Mod M
MATHTEST_INLINE VectorRegister TestReferenceMod(const VectorRegister& A, const VectorRegister& M)
{
	return MakeVectorRegister(
		(float)fmodf(VectorGetComponent(A, 0), VectorGetComponent(M, 0)),
		(float)fmodf(VectorGetComponent(A, 1), VectorGetComponent(M, 1)),
		(float)fmodf(VectorGetComponent(A, 2), VectorGetComponent(M, 2)),
		(float)fmodf(VectorGetComponent(A, 3), VectorGetComponent(M, 3)));
}

// SinCos
MATHTEST_INLINE void TestReferenceSinCos(VectorRegister& S, VectorRegister& C, const VectorRegister& VAngles)
{
	S = MakeVectorRegister(
			FMath::Sin(VectorGetComponent(VAngles, 0)),
			FMath::Sin(VectorGetComponent(VAngles, 1)),
			FMath::Sin(VectorGetComponent(VAngles, 2)),
			FMath::Sin(VectorGetComponent(VAngles, 3))
			);

	C = MakeVectorRegister(
			FMath::Cos(VectorGetComponent(VAngles, 0)),
			FMath::Cos(VectorGetComponent(VAngles, 1)),
			FMath::Cos(VectorGetComponent(VAngles, 2)),
			FMath::Cos(VectorGetComponent(VAngles, 3))
			);
}

MATHTEST_INLINE void TestFastSinCos(VectorRegister& S, VectorRegister& C, const VectorRegister& VAngles)
{
	float SFloat[4], CFloat[4];
	FMath::SinCos(&SFloat[0], &CFloat[0], VectorGetComponent(VAngles, 0));
	FMath::SinCos(&SFloat[1], &CFloat[1], VectorGetComponent(VAngles, 1));
	FMath::SinCos(&SFloat[2], &CFloat[2], VectorGetComponent(VAngles, 2));
	FMath::SinCos(&SFloat[3], &CFloat[3], VectorGetComponent(VAngles, 3));

	S = VectorLoad(SFloat);
	C = VectorLoad(CFloat);
}

MATHTEST_INLINE void TestVectorSinCos(VectorRegister& S, VectorRegister& C, const VectorRegister& VAngles)
{
	VectorSinCos(&S, &C, &VAngles);
}

/**
 * Helper debugf function to print out success or failure information for a test
 *
 * @param TestName Name of the current test
 * @param bHasPassed true if the test has passed
 */
void LogTest( const TCHAR *TestName, bool bHasPassed ) 
{
	if ( bHasPassed == false )
	{
		UE_LOG(LogUnrealMathTest, Log,  TEXT("%s: %s"), bHasPassed ? TEXT("PASSED") : TEXT("FAILED"), TestName );
		UE_LOG(LogUnrealMathTest, Log,  TEXT("Bad(%f): (%f %f %f %f) (%f %f %f %f)"), GSum, GScratch[0], GScratch[1], GScratch[2], GScratch[3], GScratch[4], GScratch[5], GScratch[6], GScratch[7] );
		GPassing = false;
	}
}


/** 
 * Set the contents of the scratch memory
 * 
 * @param X,Y,Z,W,U values to push into GScratch
 */
void SetScratch( float X, float Y, float Z, float W, float U = 0.0f )
{
	GScratch[0] = X;
	GScratch[1] = Y;
	GScratch[2] = Z;
	GScratch[3] = W;
	GScratch[4] = U;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVectorRegisterAbstractionTest, "System.Core.Math.Vector Register Abstraction Test", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

/**
 * Run a suite of vector operations to validate vector intrinsics are working on the platform
 */
bool FVectorRegisterAbstractionTest::RunTest(const FString& Parameters)
{
	float F1 = 1.f;
	uint32 D1 = *(uint32 *)&F1;
	VectorRegister V0, V1, V2, V3;

	GPassing = true;

	V0 = MakeVectorRegister( D1, D1, D1, D1 );
	V1 = MakeVectorRegister( F1, F1, F1, F1 );
	LogTest( TEXT("MakeVectorRegister"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 0.f, 0.f, 0.f, 0.f );
	V1 = VectorZero();
	LogTest( TEXT("VectorZero"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 1.f, 1.f, 1.f, 1.f );
	V1 = VectorOne();
	LogTest( TEXT("VectorOne"), TestVectorsEqual( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = MakeVectorRegister( 1.0f, 2.0f, -0.25f, -0.5f );
	V1 = VectorLoad( GScratch );
	LogTest( TEXT("VectorLoad"), TestVectorsEqual( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = MakeVectorRegister( 1.0f, 2.0f, -0.25f, -0.5f );
	V1 = VectorLoad( GScratch );
	LogTest( TEXT("VectorLoad"), TestVectorsEqual( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = MakeVectorRegister( 1.0f, 2.0f, -0.25f, -0.5f );
	V1 = VectorLoadAligned( GScratch );
	LogTest( TEXT("VectorLoadAligned"), TestVectorsEqual( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = VectorLoad( GScratch + 1 );
	V1 = VectorLoadFloat3( GScratch + 1 );
	LogTest( TEXT("VectorLoadFloat3"), TestVectorsEqual3( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = MakeVectorRegister( 1.0f, 2.0f, -0.25f, 0.0f );
	V1 = VectorLoadFloat3_W0( GScratch );
	LogTest( TEXT("VectorLoadFloat3_W0"), TestVectorsEqual( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = MakeVectorRegister( 1.0f, 2.0f, -0.25f, 1.0f );
	V1 = VectorLoadFloat3_W1( GScratch );
	LogTest( TEXT("VectorLoadFloat3_W1"), TestVectorsEqual( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = MakeVectorRegister( -0.5f, -0.5f, -0.5f, -0.5f );
	V1 = VectorLoadFloat1( GScratch + 3 );
	LogTest( TEXT("VectorLoadFloat1"), TestVectorsEqual( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = VectorSetFloat3( GScratch[1], GScratch[2], GScratch[3] );
	V1 = VectorLoadFloat3( GScratch + 1 );
	LogTest( TEXT("VectorSetFloat3"), TestVectorsEqual3( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = VectorSet( GScratch[1], GScratch[2], GScratch[3], GScratch[4] );
	V1 = VectorLoad( GScratch + 1 );
	LogTest( TEXT("VectorSet"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 1.0f, 2.0f, -0.25f, 1.0f );
	VectorStoreAligned( V0, GScratch + 8 );
	V1 = VectorLoad( GScratch + 8 );
	LogTest( TEXT("VectorStoreAligned"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 1.0f, 2.0f, -0.55f, 1.0f );
	VectorStore( V0, GScratch + 7 );
	V1 = VectorLoad( GScratch + 7 );
	LogTest( TEXT("VectorStore"), TestVectorsEqual( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = MakeVectorRegister( 5.0f, 3.0f, 1.0f, -1.0f );
	VectorStoreFloat3( V0, GScratch );
	V1 = VectorLoad( GScratch );
	V0 = MakeVectorRegister( 5.0f, 3.0f, 1.0f, -0.5f );
	LogTest( TEXT("VectorStoreFloat3"), TestVectorsEqual( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = MakeVectorRegister( 5.0f, 3.0f, 1.0f, -1.0f );
	VectorStoreFloat1( V0, GScratch + 1 );
	V1 = VectorLoad( GScratch );
	V0 = MakeVectorRegister( 1.0f, 5.0f, -0.25f, -0.5f );
	LogTest( TEXT("VectorStoreFloat1"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	V1 = VectorReplicate( V0, 1 );
	V0 = MakeVectorRegister( 2.0f, 2.0f, 2.0f, 2.0f );
	LogTest( TEXT("VectorReplicate"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 1.0f, -2.0f, 3.0f, -4.0f );
	V1 = VectorAbs( V0 );
	V0 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	LogTest( TEXT("VectorAbs"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 1.0f, -2.0f, 3.0f, -4.0f );
	V1 = VectorNegate( V0 );
	V0 = MakeVectorRegister( -1.0f, 2.0f, -3.0f, 4.0f );
	LogTest( TEXT("VectorNegate"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = VectorAdd( V0, V1 );
	V0 = MakeVectorRegister( 3.0f, 6.0f, 9.0f, 12.0f );
	LogTest( TEXT("VectorAdd"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	V1 = VectorSubtract( V0, V1 );
	V0 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	LogTest( TEXT("VectorSubtract"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	V1 = VectorMultiply( V0, V1 );
	V0 = MakeVectorRegister( 2.0f, 8.0f, 18.0f, 32.0f );
	LogTest( TEXT("VectorMultiply"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	V1 = VectorMultiplyAdd( V0, V1, VectorOne() );
	V0 = MakeVectorRegister( 3.0f, 9.0f, 19.0f, 33.0f );
	LogTest( TEXT("VectorMultiplyAdd"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	V1 = VectorDot3( V0, V1 );
	V0 = MakeVectorRegister( 28.0f, 28.0f, 28.0f, 28.0f );
	LogTest( TEXT("VectorDot3"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	V1 = VectorDot4( V0, V1 );
	V0 = MakeVectorRegister( 60.0f, 60.0f, 60.0f, 60.0f );
	LogTest( TEXT("VectorDot4"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 1.0f, 0.0f, 0.0f, 8.0f );
	V1 = MakeVectorRegister( 0.0f, 2.0f, 0.0f, 4.0f );
	V1 = VectorCross( V0, V1 );
	V0 = MakeVectorRegister( 0.f, 0.0f, 2.0f, 0.0f );
	LogTest( TEXT("VectorCross"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	V1 = VectorPow( V0, V1 );
	V0 = MakeVectorRegister( 16.0f, 64.0f, 36.0f, 8.0f );
	LogTest( TEXT("VectorPow"), TestVectorsEqual( V0, V1, 0.001f ) );

	V0 = MakeVectorRegister( 2.0f, -2.0f, 2.0f, -2.0f );
	V1 = VectorReciprocalLen( V0 );
	V0 = MakeVectorRegister( 0.25f, 0.25f, 0.25f, 0.25f );
	LogTest( TEXT("VectorReciprocalLen"), TestVectorsEqual( V0, V1, 0.001f ) );

	V0 = MakeVectorRegister( 2.0f, -2.0f, 2.0f, -2.0f );
	V1 = VectorNormalize( V0 );
	V0 = MakeVectorRegister( 0.5f, -0.5f, 0.5f, -0.5f );
	LogTest( TEXT("VectorNormalize"), TestVectorsEqual( V0, V1, 0.001f ) );

	V0 = MakeVectorRegister(2.0f, -2.0f, 2.0f, -2.0f);
	V1 = VectorNormalizeAccurate(V0);
	V0 = MakeVectorRegister(0.5f, -0.5f, 0.5f, -0.5f);
	LogTest(TEXT("VectorNormalizeAccurate"), TestVectorsEqual(V0, V1, 1e-8f));

	V0 = MakeVectorRegister(2.0f, -2.0f, 2.0f, -2.0f);
	V1 = TestVectorNormalize_Sqrt(V0);
	V0 = MakeVectorRegister(0.5f, -0.5f, 0.5f, -0.5f);
	LogTest(TEXT("TestVectorNormalize_Sqrt"), TestVectorsEqual(V0, V1, 1e-8f));

	V0 = MakeVectorRegister(2.0f, -2.0f, 2.0f, -2.0f);
	V1 = TestVectorNormalize_InvSqrt(V0);
	V0 = MakeVectorRegister(0.5f, -0.5f, 0.5f, -0.5f);
	LogTest(TEXT("TestVectorNormalize_InvSqrt"), TestVectorsEqual(V0, V1, 1e-8f));

	V0 = MakeVectorRegister(2.0f, -2.0f, 2.0f, -2.0f);
	V1 = TestVectorNormalize_InvSqrtEst(V0);
	V0 = MakeVectorRegister(0.5f, -0.5f, 0.5f, -0.5f);
	LogTest(TEXT("TestVectorNormalize_InvSqrtEst"), TestVectorsEqual(V0, V1, 1e-6f));

	V0 = MakeVectorRegister( 2.0f, -2.0f, 2.0f, -2.0f );
	V1 = VectorSet_W0( V0 );
	V0 = MakeVectorRegister( 2.0f, -2.0f, 2.0f, 0.0f );
	LogTest( TEXT("VectorSet_W0"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, -2.0f, 2.0f, -2.0f );
	V1 = VectorSet_W1( V0 );
	V0 = MakeVectorRegister( 2.0f, -2.0f, 2.0f, 1.0f );
	LogTest( TEXT("VectorSet_W1"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	V1 = VectorMin( V0, V1 );
	V0 = MakeVectorRegister( 2.0f, 3.0f, 2.0f, 1.0f );
	LogTest( TEXT("VectorMin"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	V1 = VectorMax( V0, V1 );
	V0 = MakeVectorRegister( 4.0f, 4.0f, 6.0f, 8.0f );
	LogTest( TEXT("VectorMax"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	V1 = VectorSwizzle( V0, 1, 0, 3, 2 );
	V0 = MakeVectorRegister( 3.0f, 4.0f, 1.0f, 2.0f );
	LogTest( TEXT("VectorSwizzle1032"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	V1 = VectorSwizzle( V0, 1, 2, 0, 1 );
	V0 = MakeVectorRegister( 3.0f, 2.0f, 4.0f, 3.0f );
	LogTest( TEXT("VectorSwizzle1201"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	V1 = VectorSwizzle( V0, 2, 0, 1, 3 );
	V0 = MakeVectorRegister( 2.0f, 4.0f, 3.0f, 1.0f );
	LogTest( TEXT("VectorSwizzle2013"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	V1 = VectorSwizzle( V0, 2, 3, 0, 1 );
	V0 = MakeVectorRegister( 2.0f, 1.0f, 4.0f, 3.0f );
	LogTest( TEXT("VectorSwizzle2301"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	V1 = VectorSwizzle( V0, 3, 2, 1, 0 );
	V0 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	LogTest( TEXT("VectorSwizzle3210"), TestVectorsEqual( V0, V1 ) );

	uint8 Bytes[4] = { 25, 75, 125, 200 };
	V0 = VectorLoadByte4( Bytes );
	V1 = MakeVectorRegister( 25.f, 75.f, 125.f, 200.f );
	LogTest( TEXT("VectorLoadByte4"), TestVectorsEqual( V0, V1 ) );

	V0 = VectorLoadByte4Reverse( Bytes );
	V1 = MakeVectorRegister( 25.f, 75.f, 125.f, 200.f );
	V1 = VectorSwizzle( V1, 3, 2, 1, 0 );
	LogTest( TEXT("VectorLoadByte4Reverse"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	VectorStoreByte4( V0, Bytes );
	V1 = VectorLoadByte4( Bytes );
	LogTest( TEXT("VectorStoreByte4"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	bool bIsVAGT_TRUE = VectorAnyGreaterThan( V0, V1 ) != 0;
	LogTest( TEXT("VectorAnyGreaterThan-true"), bIsVAGT_TRUE );

	V0 = MakeVectorRegister( 1.0f, 3.0f, 2.0f, 1.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	bool bIsVAGT_FALSE = VectorAnyGreaterThan( V0, V1 ) == 0;
	LogTest( TEXT("VectorAnyGreaterThan-false"), bIsVAGT_FALSE );

	V0 = MakeVectorRegister( 1.0f, 3.0f, 2.0f, 1.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	LogTest( TEXT("VectorAnyLesserThan-true"), VectorAnyLesserThan( V0, V1 ) != 0 );

	V0 = MakeVectorRegister( 3.0f, 5.0f, 7.0f, 9.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	LogTest( TEXT("VectorAnyLesserThan-false"), VectorAnyLesserThan( V0, V1 ) == 0 );

	V0 = MakeVectorRegister( 3.0f, 5.0f, 7.0f, 9.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	LogTest( TEXT("VectorAllGreaterThan-true"), VectorAllGreaterThan( V0, V1 ) != 0 );

	V0 = MakeVectorRegister( 3.0f, 1.0f, 7.0f, 9.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	LogTest( TEXT("VectorAllGreaterThan-false"), VectorAllGreaterThan( V0, V1 ) == 0 );

	V0 = MakeVectorRegister( 1.0f, 3.0f, 2.0f, 1.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	LogTest( TEXT("VectorAllLesserThan-true"), VectorAllLesserThan( V0, V1 ) != 0 );

	V0 = MakeVectorRegister( 3.0f, 3.0f, 2.0f, 1.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	LogTest( TEXT("VectorAllLesserThan-false"), VectorAllLesserThan( V0, V1 ) == 0 );

	V0 = MakeVectorRegister( 1.0f, 3.0f, 2.0f, 8.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 2.0f, 1.0f );
	V2 = VectorCompareGT( V0, V1 );
	V3 = MakeVectorRegister( (uint32)0, (uint32)0, (uint32)0, (uint32)-1 );
	LogTest( TEXT("VectorCompareGT"), TestVectorsEqualBitwise( V2, V3 ) );

	V0 = MakeVectorRegister( 1.0f, 3.0f, 2.0f, 8.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 2.0f, 1.0f );
	V2 = VectorCompareGE( V0, V1 );
	V3 = MakeVectorRegister( (uint32)0, (uint32)0, (uint32)-1, (uint32)-1 );
	LogTest( TEXT("VectorCompareGE"), TestVectorsEqualBitwise( V2, V3 ) );

	V0 = MakeVectorRegister( 1.0f, 3.0f, 2.0f, 8.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 2.0f, 1.0f );
	V2 = VectorCompareEQ( V0, V1 );
	V3 = MakeVectorRegister( (uint32)0, (uint32)0, (uint32)-1, (uint32)0 );
	LogTest( TEXT("VectorCompareEQ"), TestVectorsEqualBitwise( V2, V3 ) );

	V0 = MakeVectorRegister( 1.0f, 3.0f, 2.0f, 8.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 2.0f, 1.0f );
	V2 = VectorCompareNE( V0, V1 );
	V3 = MakeVectorRegister( (uint32)(0xFFFFFFFFU), (uint32)(0xFFFFFFFFU), (uint32)(0), (uint32)(0xFFFFFFFFU) );
	LogTest( TEXT("VectorCompareNE"), TestVectorsEqualBitwise( V2, V3 ) );
	
	V0 = MakeVectorRegister( 1.0f, 3.0f, 2.0f, 8.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 2.0f, 1.0f );
	V2 = MakeVectorRegister( (uint32)-1, (uint32)0, (uint32)0, (uint32)-1 );
	V2 = VectorSelect( V2, V0, V1 );
	V3 = MakeVectorRegister( 1.0f, 4.0f, 2.0f, 8.0f );
	LogTest( TEXT("VectorSelect"), TestVectorsEqual( V2, V3 ) );

	V0 = MakeVectorRegister( 1.0f, 3.0f, 0.0f, 0.0f );
	V1 = MakeVectorRegister( 0.0f, 0.0f, 2.0f, 1.0f );
	V2 = VectorBitwiseOr( V0, V1 );
	V3 = MakeVectorRegister( 1.0f, 3.0f, 2.0f, 1.0f );
	LogTest( TEXT("VectorBitwiseOr-Float1"), TestVectorsEqual( V2, V3 ) );

	V0 = MakeVectorRegister( 1.0f, 3.0f, 24.0f, 36.0f );
	V1 = MakeVectorRegister( (uint32)(0x80000000U), (uint32)(0x80000000U), (uint32)(0x80000000U), (uint32)(0x80000000U) );
	V2 = VectorBitwiseOr( V0, V1 );
	V3 = MakeVectorRegister( -1.0f, -3.0f, -24.0f, -36.0f );
	LogTest( TEXT("VectorBitwiseOr-Float2"), TestVectorsEqual( V2, V3 ) );

	V0 = MakeVectorRegister( -1.0f, -3.0f, -24.0f, 36.0f );
	V1 = MakeVectorRegister( (uint32)(0xFFFFFFFFU), (uint32)(0x7FFFFFFFU), (uint32)(0x7FFFFFFFU), (uint32)(0xFFFFFFFFU) );
	V2 = VectorBitwiseAnd( V0, V1 );
	V3 = MakeVectorRegister( -1.0f, 3.0f, 24.0f, 36.0f );
	LogTest( TEXT("VectorBitwiseAnd-Float"), TestVectorsEqual( V2, V3 ) );

	V0 = MakeVectorRegister( -1.0f, -3.0f, -24.0f, 36.0f );
	V1 = MakeVectorRegister( (uint32)(0x80000000U), (uint32)(0x00000000U), (uint32)(0x80000000U), (uint32)(0x80000000U) );
	V2 = VectorBitwiseXor( V0, V1 );
	V3 = MakeVectorRegister( 1.0f, -3.0f, 24.0f, -36.0f );
	LogTest( TEXT("VectorBitwiseXor-Float"), TestVectorsEqual( V2, V3 ) );

	V0 = MakeVectorRegister( -1.0f, -3.0f, -24.0f, 36.0f );
	V1 = MakeVectorRegister( 5.0f, 35.0f, 23.0f, 48.0f );
	V2 = VectorMergeVecXYZ_VecW( V0, V1 );
	V3 = MakeVectorRegister( -1.0f, -3.0f, -24.0f, 48.0f );
	LogTest( TEXT("VectorMergeXYZ_VecW-1"), TestVectorsEqual( V2, V3 ) );

	V0 = MakeVectorRegister( -1.0f, -3.0f, -24.0f, 36.0f );
	V1 = MakeVectorRegister( 5.0f, 35.0f, 23.0f, 48.0f );
	V2 = VectorMergeVecXYZ_VecW( V1, V0 );
	V3 = MakeVectorRegister( 5.0f, 35.0f, 23.0f, 36.0f );
	LogTest( TEXT("VectorMergeXYZ_VecW-2"), TestVectorsEqual( V2, V3 ) );

	V0 = MakeVectorRegister( 1.0f, 1.0e6f, 1.3e-8f, 35.0f );
	V1 = VectorReciprocal( V0 );
	V3 = VectorMultiply(V1, V0);
	LogTest( TEXT("VectorReciprocal"), TestVectorsEqual( VectorOne(), V3, 1e-3f ) );

	V0 = MakeVectorRegister( 1.0f, 1.0e6f, 1.3e-8f, 35.0f );
	V1 = VectorReciprocalAccurate( V0 );
	V3 = VectorMultiply(V1, V0);
	LogTest( TEXT("VectorReciprocalAccurate"), TestVectorsEqual( VectorOne(), V3, 1e-7f ) );

	V0 = MakeVectorRegister( 1.0f, 1.0e6f, 1.3e-8f, 35.0f );
	V1 = VectorReciprocalSqrt( V0 );
	V3 = VectorMultiply(VectorMultiply(V1, V1), V0);
	LogTest( TEXT("VectorReciprocalSqrt"), TestVectorsEqual( VectorOne(), V3, 2e-3f ) );

	V0 = MakeVectorRegister( 1.0f, 1.0e6f, 1.3e-8f, 35.0f );
	V1 = VectorReciprocalSqrtAccurate( V0 );
	V3 = VectorMultiply(VectorMultiply(V1, V1), V0);
	LogTest( TEXT("VectorReciprocalSqrtAccurate"), TestVectorsEqual( VectorOne(), V3, 1e-6f ) );

	// VectorMod
	V0 = MakeVectorRegister(0.0f, 3.2f, 2.8f,  1.5f);
	V1 = MakeVectorRegister(2.0f, 1.2f, 2.0f,  3.0f);
	V2 = TestReferenceMod(V0, V1);
	V3 = VectorMod(V0, V1);
	LogTest( TEXT("VectorMod positive"), TestVectorsEqual(V2, V3));

	V0 = MakeVectorRegister(-2.0f,  3.2f, -2.8f,  -1.5f);
	V1 = MakeVectorRegister(-1.5f, -1.2f,  2.0f,   3.0f);
	V2 = TestReferenceMod(V0, V1);
	V3 = VectorMod(V0, V1);
	LogTest( TEXT("VectorMod negative"), TestVectorsEqual(V2, V3));

	FMatrix	M0, M1, M2, M3;
	FVector Eye, LookAt, Up;	
	// Create Look at Matrix
	Eye    = FVector(1024.0f, -512.0f, -2048.0f);
	LookAt = FVector(0.0f,		  0.0f,     0.0f);
	Up     = FVector(0.0f,       1.0f,    0.0f);
	M0	= FLookAtMatrix(Eye, LookAt, Up);		

	// Create GL ortho projection matrix
	const float Width = 1920.0f;
	const float Height = 1080.0f;
	const float Left = 0.0f;
	const float Right = Left+Width;
	const float Top = 0.0f;
	const float Bottom = Top+Height;
	const float ZNear = -100.0f;
	const float ZFar = 100.0f;

	M1 = FMatrix(FPlane(2.0f/(Right-Left),	0,							0,					0 ),
		FPlane(0,							2.0f/(Top-Bottom),			0,					0 ),
		FPlane(0,							0,							1/(ZNear-ZFar),		0 ),
		FPlane((Left+Right)/(Left-Right),	(Top+Bottom)/(Bottom-Top),	ZNear/(ZNear-ZFar), 1 ) );

	VectorMatrixMultiply( &M2, &M0, &M1 );
	TestVectorMatrixMultiply( &M3, &M0, &M1 );
	LogTest( TEXT("VectorMatrixMultiply"), TestMatricesEqual( M2, M3 ) );

	VectorMatrixInverse( &M2, &M1 );
	TestVectorMatrixInverse( &M3, &M1 );
	LogTest( TEXT("VectorMatrixInverse"), TestMatricesEqual( M2, M3 ) );

// 	FTransform Transform;
// 	Transform.SetFromMatrix(M1);
// 	FTransform InvTransform = Transform.Inverse();
// 	FTransform InvTransform2 = FTransform(Transform.ToMatrixWithScale().Inverse());
// 	LogTest( TEXT("FTransform Inverse"), InvTransform.Equals(InvTransform2, 1e-3f ) );

	V0 = MakeVectorRegister( 100.0f, -100.0f, 200.0f, 1.0f );
	V1 = VectorTransformVector(V0, &M0);
	V2 = TestVectorTransformVector(V0, &M0);
	LogTest( TEXT("VectorTransformVector"), TestVectorsEqual( V1, V2 ) );

	V0 = MakeVectorRegister( 32768.0f,131072.0f, -8096.0f, 1.0f );
	V1 = VectorTransformVector(V0, &M1);
	V2 = TestVectorTransformVector(V0, &M1);
	LogTest( TEXT("VectorTransformVector"), TestVectorsEqual( V1, V2 ) );

	// NaN / Inf tests
	// Using a union as we need to do a bitwise cast of 0xFFFFFFFF into a float.
	typedef union
	{
		unsigned int IntNaN;
		float FloatNaN;
	} IntFloatUnion;
	IntFloatUnion NaNU;
	NaNU.IntNaN = 0xFFFFFFFF;
	const float NaN = NaNU.FloatNaN;

	LogTest(TEXT("VectorContainsNaNOrInfinite true"), VectorContainsNaNOrInfinite(MakeVectorRegister(NaN, NaN, NaN, NaN)));
	LogTest(TEXT("VectorContainsNaNOrInfinite true"), VectorContainsNaNOrInfinite(MakeVectorRegister(NaN, 0.f, 0.f, 0.f)));
	LogTest(TEXT("VectorContainsNaNOrInfinite true"), VectorContainsNaNOrInfinite(MakeVectorRegister(0.f, 0.f, 0.f, NaN)));
	LogTest(TEXT("VectorContainsNaNOrInfinite true"), VectorContainsNaNOrInfinite(GlobalVectorConstants::FloatInfinity));
	LogTest(TEXT("VectorContainsNaNOrInfinite true"), VectorContainsNaNOrInfinite(MakeVectorRegister((uint32)0xFF800000, (uint32)0xFF800000, (uint32)0xFF800000, (uint32)0xFF800000))); // negative infinity
	LogTest(TEXT("VectorContainsNaNOrInfinite true"), VectorContainsNaNOrInfinite(GlobalVectorConstants::AllMask));

	// Not Nan/Inf
	LogTest(TEXT("VectorContainsNaNOrInfinite false"), !VectorContainsNaNOrInfinite(GlobalVectorConstants::FloatZero));
	LogTest(TEXT("VectorContainsNaNOrInfinite false"), !VectorContainsNaNOrInfinite(GlobalVectorConstants::FloatOne));
	LogTest(TEXT("VectorContainsNaNOrInfinite false"), !VectorContainsNaNOrInfinite(GlobalVectorConstants::FloatMinusOneHalf));
	LogTest(TEXT("VectorContainsNaNOrInfinite false"), !VectorContainsNaNOrInfinite(GlobalVectorConstants::SmallNumber));
	LogTest(TEXT("VectorContainsNaNOrInfinite false"), !VectorContainsNaNOrInfinite(GlobalVectorConstants::BigNumber));


	FQuat Q0, Q1, Q2, Q3;

	// SinCos tests
	{
		const VectorRegister QuadrantDegreesArray[] = {
			MakeVectorRegister( 0.0f, 10.0f, 20.0f, 30.0f),
			MakeVectorRegister(45.0f, 60.0f, 70.0f, 80.0f),
		};

		const float SinCosTolerance = 1e-6f;
		const int32 Cycles = 3; // Go through a full circle this many times (negative and positive)
		for (int32 OffsetQuadrant = -4*Cycles; OffsetQuadrant <= 4*Cycles; ++OffsetQuadrant)
		{
			const float OffsetFloat = (float)OffsetQuadrant * 90.0f; // Add 90 degrees repeatedly to cover all quadrants and wrap a few times
			const VectorRegister VOffset = VectorLoadFloat1(&OffsetFloat);
			for (VectorRegister const& VDegrees : QuadrantDegreesArray)
			{
				const VectorRegister VAnglesDegrees = VectorAdd(VOffset, VDegrees);
				const VectorRegister VAngles = VectorMultiply(VAnglesDegrees, GlobalVectorConstants::DEG_TO_RAD);
				VectorRegister S[3], C[3];
				TestReferenceSinCos(S[0], C[0], VAngles);
				TestFastSinCos(S[1], C[1], VAngles);
				TestVectorSinCos(S[2], C[2], VAngles);
				LogTest(TEXT("SinCos (Sin): Ref vs Fast"), TestVectorsEqual_ComponentWiseError(S[0], S[1], SinCosTolerance));
				LogTest(TEXT("SinCos (Cos): Ref vs Fast"), TestVectorsEqual_ComponentWiseError(C[0], C[1], SinCosTolerance));
				LogTest(TEXT("SinCos (Sin): Ref vs Vec"), TestVectorsEqual_ComponentWiseError(S[0], S[2], SinCosTolerance));
				LogTest(TEXT("SinCos (Cos): Ref vs Vec"), TestVectorsEqual_ComponentWiseError(C[0], C[2], SinCosTolerance));
			}
		}
	}

	// Quat<->Rotator conversions and equality
	{
		// Identity conversion
		{
			const FRotator R0 = FRotator::ZeroRotator;
			const FRotator R1 = FRotator(FQuat::Identity);
			LogRotatorTest(true, TEXT("FRotator::ZeroRotator ~= FQuat::Identity : Rotator"), R0, R1, R0.Equals(R1, 0.f));
			LogRotatorTest(true, TEXT("FRotator::ZeroRotator == FQuat::Identity : Rotator"), R0, R1, R0 == R1);
			LogRotatorTest(true, TEXT("FRotator::ZeroRotator not != FQuat::Identity : Rotator"), R0, R1, !(R0 != R1));

			Q0 = FQuat::Identity;
			Q1 = FQuat(FRotator::ZeroRotator);
			LogQuaternionTest(TEXT("FRotator::ZeroRotator ~= FQuat::Identity : Quaternion"), Q0, Q1, Q0.Equals(Q1, 0.f));
			LogQuaternionTest(TEXT("FRotator::ZeroRotator == FQuat::Identity : Quaternion"), Q0, Q1, Q0 == Q1);
			LogQuaternionTest(TEXT("FRotator::ZeroRotator not != FQuat::Identity : Quaternion"), Q0, Q1, !(Q0 != Q1));
		}

		const float Nudge = KINDA_SMALL_NUMBER * 0.25f;
		const FRotator RotArray[] = {
			FRotator(0.f, 0.f, 0.f),
			FRotator(Nudge, -Nudge, Nudge),
			FRotator(+180.f, -180.f, +180.f),
			FRotator(-180.f, +180.f, -180.f),
			FRotator(+45.0f - Nudge, -120.f + Nudge, +270.f - Nudge),
			FRotator(-45.0f + Nudge, +120.f - Nudge, -270.f + Nudge),
			FRotator(+315.f - 360.f, -240.f - 360.f, -90.0f - 360.f),
			FRotator(-315.f + 360.f, +240.f + 360.f, +90.0f + 360.f),
		};

		// FRotator tests
		{
			// Equality test
			const float RotTolerance = KINDA_SMALL_NUMBER;
			for (auto const& A : RotArray)
			{
				for (auto const& B : RotArray)
				{
					const bool bExpected = TestRotatorEqual0(A, B, RotTolerance);
					LogRotatorTest(bExpected, TEXT("TestRotatorEqual1"), A, B, TestRotatorEqual1(A, B, RotTolerance));
					LogRotatorTest(bExpected, TEXT("TestRotatorEqual2"), A, B, TestRotatorEqual2(A, B, RotTolerance));
					LogRotatorTest(bExpected, TEXT("TestRotatorEqual3"), A, B, TestRotatorEqual3(A, B, RotTolerance));
				}
			}
		}

		// Quaternion conversion test
		const float QuatTolerance = 1e-6f;
		for (auto const& A : RotArray)
		{
			const FQuat QA = TestRotatorToQuaternion(A);
			const FQuat QB = A.Quaternion();
			LogQuaternionTest(TEXT("TestRotatorToQuaternion"), QA, QB, TestQuatsEqual(QA, QB, QuatTolerance));
		}
	}

	// Rotator->Quat->Rotator
	{
		const FRotator RotArray[] ={
			FRotator(30.0f, -45.0f, 90.0f),
			FRotator(45.0f, 60.0f, -120.0f),
			FRotator(0.f, 90.f, 0.f),
			FRotator(0.f, -90.f, 0.f),
			FRotator(0.f, 180.f, 0.f),
			FRotator(0.f, -180.f, 0.f),
			FRotator(90.f, 0.f, 0.f),
			FRotator(-90.f, 0.f, 0.f),
			FRotator(150.f, 0.f, 0.f)
		};

		for (FRotator const& Rotator0 : RotArray)
		{
			Q0 = TestRotatorToQuaternion(Rotator0);
			FRotator Rotator1 = Q0.Rotator();
			FRotator Rotator2 = TestQuaternionToRotator(Q0);
			LogRotatorTest(TEXT("Rotator->Quat->Rotator"), Rotator1, Rotator2, Rotator1.Equals(Rotator2, 1e-4f));
		}
	}

	// Quat / Rotator conversion to vectors, matrices
	{
		FRotator Rotator0;
		Rotator0 = FRotator(30.0f, -45.0f, 90.0f);
		Q0 = Rotator0.Quaternion();
		Q1 = TestRotatorToQuaternion(Rotator0);
		LogTest( TEXT("TestRotatorToQuaternion"), TestQuatsEqual(Q0, Q1, 1e-6f));

		FVector FV0, FV1;
		FV0 = Rotator0.Vector();
		FV1 = FRotationMatrix( Rotator0 ).GetScaledAxis( EAxis::X );
		LogTest( TEXT("Test0 Rotator::Vector()"), TestFVector3Equal(FV0, FV1, 1e-6f));
		
		FV0 = FRotationMatrix( Rotator0 ).GetScaledAxis( EAxis::X );
		FV1 = FQuatRotationMatrix( Q0 ).GetScaledAxis( EAxis::X );
		LogTest( TEXT("Test0 FQuatRotationMatrix"), TestFVector3Equal(FV0, FV1, 1e-5f));

		Rotator0 = FRotator(45.0f,  60.0f, 120.0f);
		Q0 = Rotator0.Quaternion();
		Q1 = TestRotatorToQuaternion(Rotator0);
		LogTest( TEXT("TestRotatorToQuaternion"), TestQuatsEqual(Q0, Q1, 1e-6f));

		FV0 = Rotator0.Vector();
		FV1 = FRotationMatrix( Rotator0 ).GetScaledAxis( EAxis::X );
		LogTest( TEXT("Test1 Rotator::Vector()"), TestFVector3Equal(FV0, FV1, 1e-6f));

		FV0 = FRotationMatrix( Rotator0 ).GetScaledAxis( EAxis::X );
		FV1 = FQuatRotationMatrix( Q0 ).GetScaledAxis( EAxis::X );
		LogTest(TEXT("Test1 FQuatRotationMatrix"), TestFVector3Equal(FV0, FV1, 1e-5f));

		FV0 = FRotationMatrix(FRotator::ZeroRotator).GetScaledAxis(EAxis::X);
		FV1 = FQuatRotationMatrix(FQuat::Identity).GetScaledAxis(EAxis::X);
		LogTest(TEXT("Test2 FQuatRotationMatrix"), TestFVector3Equal(FV0, FV1, 1e-6f));
	}

	// Quat Rotation tests
	{
		// Use these Quats...
		const FQuat TestQuats[] = {
			FQuat(FQuat::Identity),
			FQuat(FRotator(30.0f, -45.0f, 90.0f)),
			FQuat(FRotator(45.0f,  60.0f, 120.0f)),
			FQuat(FRotator(0.0f, 180.0f, 45.0f)),
			FQuat(FRotator(-120.0f, -90.0f, 0.0f)),
			FQuat(FRotator(-0.01f, 0.02f, -0.03f)),
		};

		// ... to rotate these Vectors...
		const FVector TestVectors[] = {
			FVector::ZeroVector,
			FVector::ForwardVector,
			FVector::RightVector,
			FVector::UpVector,
			FVector(45.0f, -60.0f, 120.0f),
			FVector(-45.0f, 60.0f, -120.0f),
			FVector(0.57735026918962576451f, 0.57735026918962576451f, 0.57735026918962576451f),
			-FVector::ForwardVector,
		};

		// ... and test within this tolerance.
		const float Tolerance = 1e-4f;

		// Test Macro. Tests FQuat::RotateVector(_Vec) against _Func(_Vec)
		#define TEST_QUAT_ROTATE(_QIndex, _VIndex, _Quat, _Vec, _Func, _Tolerance) \
		{ \
			const FString _TestName = FString::Printf(TEXT("Test Quat%d: Vec%d: %s"), _QIndex, _VIndex, TEXT(#_Func)); \
			LogTest( *_TestName, TestFVector3Equal(_Quat.RotateVector(_Vec), _Func(_Quat, _Vec), _Tolerance) ); \
		}

		// Test loop
		for (int32 QIndex = 0; QIndex < ARRAY_COUNT(TestQuats); ++QIndex)
		{
			const FQuat& Q = TestQuats[QIndex];
			for (int32 VIndex = 0; VIndex < ARRAY_COUNT(TestVectors); ++VIndex)
			{
				const FVector& V = TestVectors[VIndex];
				TEST_QUAT_ROTATE(QIndex, VIndex, Q, V, TestQuaternionRotateVectorScalar, Tolerance);
				TEST_QUAT_ROTATE(QIndex, VIndex, Q, V, TestQuaternionRotateVectorRegister, Tolerance);
				TEST_QUAT_ROTATE(QIndex, VIndex, Q, V, TestQuaternionMultiplyVector, Tolerance);
			}
		}


		// FindBetween
		{
			for (FVector const& A: TestVectors)
			{
				for (FVector const &B : TestVectors)
				{
					const FVector ANorm = A.GetSafeNormal();
					const FVector BNorm = B.GetSafeNormal();

					const FQuat Old = FindBetween_Old(ANorm, BNorm);
					const FQuat NewNormal = FQuat::FindBetweenNormals(ANorm, BNorm);
					const FQuat NewVector = FQuat::FindBetweenVectors(A, B);

					const FVector RotAOld = Old.RotateVector(ANorm);
					const FVector RotANewNormal = NewNormal.RotateVector(ANorm);
					const FVector RotANewVector = NewVector.RotateVector(ANorm);

					if (A.IsZero() || B.IsZero())
					{
						LogTest(TEXT("FindBetween: Old == New (normal)"), TestQuatsEqual(Old, NewNormal, 1e-6f));
						LogTest(TEXT("FindBetween: Old == New (vector)"), TestQuatsEqual(Old, NewVector, 1e-6f));
					}
					else
					{
						LogTest(TEXT("FindBetween: Old A->B"), TestFVector3Equal(RotAOld, BNorm, KINDA_SMALL_NUMBER));
						LogTest(TEXT("FindBetween: New A->B (normal)"), TestFVector3Equal(RotANewNormal, BNorm, KINDA_SMALL_NUMBER));
						LogTest(TEXT("FindBetween: New A->B (vector)"), TestFVector3Equal(RotANewVector, BNorm, KINDA_SMALL_NUMBER));
					}
				}
			}
		}


		// FVector::ToOrientationRotator(), FVector::ToOrientationQuat()
		{
			for (FVector const& V: TestVectors)
			{
				const FVector VNormal = V.GetSafeNormal();

				Q0 = FQuat::FindBetweenNormals(FVector::ForwardVector, VNormal);
				Q1 = V.ToOrientationQuat();
				const FRotator R0 = V.ToOrientationRotator();

				const FVector Rotated0 = Q0.RotateVector(FVector::ForwardVector);
				const FVector Rotated1 = Q1.RotateVector(FVector::ForwardVector);
				const FVector Rotated2 = R0.RotateVector(FVector::ForwardVector);

				LogTest(TEXT("V.ToOrientationQuat() rotate"), TestFVector3Equal(Rotated0, Rotated1, KINDA_SMALL_NUMBER));
				LogTest(TEXT("V.ToOrientationRotator() rotate"), TestFVector3Equal(Rotated0, Rotated2, KINDA_SMALL_NUMBER));
			}
		}
	}

	// Quat multiplication
	{
		Q0 = FQuat(FRotator(30.0f, -45.0f, 90.0f));
		Q1 = FQuat(FRotator(45.0f, 60.0f, 120.0f));
		VectorQuaternionMultiply(&Q2, &Q0, &Q1);
		TestVectorQuaternionMultiply(&Q3, &Q0, &Q1);
		LogTest(TEXT("VectorQuaternionMultiply"), TestQuatsEqual(Q2, Q3, 1e-6f));
		V0 = VectorLoadAligned(&Q0);
		V1 = VectorLoadAligned(&Q1);
		V2 = VectorQuaternionMultiply2(V0, V1);
		V3 = VectorLoadAligned(&Q3);
		LogTest(TEXT("VectorQuaternionMultiply2"), TestVectorsEqual(V2, V3, 1e-6f));

		Q0 = FQuat(FRotator(0.0f, 180.0f, 45.0f));
		Q1 = FQuat(FRotator(-120.0f, -90.0f, 0.0f));
		VectorQuaternionMultiply(&Q2, &Q0, &Q1);
		TestVectorQuaternionMultiply(&Q3, &Q0, &Q1);
		LogTest(TEXT("VectorMatrixInverse"), TestQuatsEqual(Q2, Q3, 1e-6f));
		V0 = VectorLoadAligned(&Q0);
		V1 = VectorLoadAligned(&Q1);
		V2 = VectorQuaternionMultiply2(V0, V1);
		V3 = VectorLoadAligned(&Q3);
		LogTest(TEXT("VectorQuaternionMultiply2"), TestVectorsEqual(V2, V3, 1e-6f));
	}

	// FMath::FMod
	{
		struct XYPair
		{
			float X, Y;
		};

		static XYPair XYArray[] =
		{		
			// Test normal ranges
			{ 0.0,	 1.0},
			{ 1.5,	 1.0},
			{ 2.8,	 0.3},
			{-2.8,	 0.3},
			{ 2.8,	-0.3},
			{-2.8,	-0.3},
			{-0.4,	 5.5},
			{ 0.4,	-5.5},
			{ 2.8,	 2.0 + KINDA_SMALL_NUMBER},
			{-2.8,	 2.0 - KINDA_SMALL_NUMBER},

			// Analytically should be zero but floating point precision can cause results close to Y (or erroneously negative) depending on the method used.
			{55.8,	 9.3},
			{1234.1234, 0.1234},

			// Commonly used for FRotators and angles
			{725.2,		360.0},
			{179.9,		 90.0},
			{ 5.3*PI,	2.*PI},
			{-5.3*PI,	2.*PI},

			// Test extreme ranges
			{ 1.0,			 KINDA_SMALL_NUMBER},
			{ 1.0,			-KINDA_SMALL_NUMBER},
			{-SMALL_NUMBER,  SMALL_NUMBER},
			{ SMALL_NUMBER, -SMALL_NUMBER},
			{ 1.0,			 MIN_flt},
			{ 1.0,			-MIN_flt},
			{ MAX_flt,		 MIN_flt},
			{ MAX_flt,		-MIN_flt},

			// We define this to be zero and not NaN.
			// Disabled since we don't want to trigger an ensure, but left here for testing that logic.
			//{ 1.0,	 0.0}, 
			//{ 1.0,	-0.0},
		};

		for (auto XY : XYArray)
		{
			const float X = XY.X;
			const float Y = XY.Y;
			const float Ours = FMath::Fmod(X, Y);
			const float Theirs = fmodf(X, Y);
			
			// A compiler bug causes stock fmodf() to rarely return NaN for valid input, we don't want to report this as a fatal error.
			if (Y != 0 && FMath::IsNaN(Theirs))
			{
				UE_LOG(LogUnrealMathTest, Warning, TEXT("fmodf(%f, %f) with valid input resulted in NaN!"), X, Y);
				continue;
			}

			const float Delta = FMath::Abs(Ours - Theirs);
			if (Delta > 1e-5f)
			{
				// If we differ significantly, that is likely due to rounding and the difference should be nearly equal to Y.
				const float FractionalDelta = FMath::Abs(Delta - FMath::Abs(Y));
				if (FractionalDelta > 1e-4f)
				{
					UE_LOG(LogUnrealMathTest, Log, TEXT("FMath::Fmod(%f, %f)=%f <-> fmodf(%f, %f)=%f: FAILED"), X, Y, Ours, X, Y, Theirs);
					GPassing = false;
				}
			}
		}
	}

	if (!GPassing)
	{
		UE_LOG(LogUnrealMathTest, Fatal,TEXT("VectorIntrinsics Failed."));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInterpolationFunctionTests, "System.Core.Math.Interpolation Function Test", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool FInterpolationFunctionTests::RunTest(const FString&)
{
	// The purpose of this test is to verify that various combinations of the easing functions are actually equivalent.
	// It currently only tests the InOut versions over different ranges, because the initial implementation was bad.
	// Further improvements (optimizations, new easing functions) to the easing functions should be accompanied by
	// expansions to this test suite.

	typedef float(*EasingFunc)(float Percent);
	auto RunInOutTest = [](const TArray< TPair<EasingFunc, FString> >& Functions, FAutomationTestBase* TestContext)
	{
		for (int32 I = 0; I < 100; ++I)
		{
			float Percent = (float)I / 100.f;
			TArray<float> Values;
			for (const auto& Entry : Functions)
			{
				Values.Push(Entry.Key(Percent));
			}

			bool bSucceeded = true;
			int32 K = 0;
			for (int32 J = 1; J < Functions.Num(); ++J)
			{
				if (!FMath::IsNearlyEqual(Values[K], Values[J], 0.0001f))
				{
					TestContext->AddError(FString::Printf( TEXT("Easing Function tests failed at index %d!"), I ) );

					for (int32 L = 0; L < Values.Num(); ++L)
					{
						TestContext->AddInfo(FString::Printf(TEXT("%s: %f"), *(Functions[L].Value), Values[L]));
					}
					// don't record further failures, it would likely create a tremendous amount of spam
					return;
				}
			}
		}
	};

#define INTERP_WITH_RANGE( RANGE_MIN, RANGE_MAX, FUNCTION, IDENTIFIER ) \
	auto FUNCTION##IDENTIFIER = [](float Percent ) \
	{ \
		const float Min = RANGE_MIN; \
		const float Max = RANGE_MAX; \
		const float Range = Max - Min; \
		return (FMath::FUNCTION(Min, Max, Percent) - Min) / Range; \
	};

	{
		// Test InterpExpoInOut:
		INTERP_WITH_RANGE(.9f, 1.2f, InterpExpoInOut, A)
		INTERP_WITH_RANGE(0.f, 1.f, InterpExpoInOut, B)
		INTERP_WITH_RANGE(-8.6f;, 2.3f, InterpExpoInOut, C)
		TArray< TPair< EasingFunc, FString > > FunctionsToTest;
		FunctionsToTest.Emplace(InterpExpoInOutA, TEXT("InterpExpoInOutA"));
		FunctionsToTest.Emplace(InterpExpoInOutB, TEXT("InterpExpoInOutB"));
		FunctionsToTest.Emplace(InterpExpoInOutC, TEXT("InterpExpoInOutC"));
		RunInOutTest(FunctionsToTest, this);
	}

	{
		// Test InterpCircularInOut:
		INTERP_WITH_RANGE(5.f, 9.32f, InterpCircularInOut, A)
		INTERP_WITH_RANGE(0.f, 1.f, InterpCircularInOut, B)
		INTERP_WITH_RANGE(-8.1f;, -.75f, InterpCircularInOut, C)
		TArray< TPair< EasingFunc, FString > > FunctionsToTest;
		FunctionsToTest.Emplace(InterpCircularInOutA, TEXT("InterpCircularInOutA"));
		FunctionsToTest.Emplace(InterpCircularInOutB, TEXT("InterpCircularInOutB"));
		FunctionsToTest.Emplace(InterpCircularInOutC, TEXT("InterpCircularInOutC"));
		RunInOutTest(FunctionsToTest, this);
	}

	{
		// Test InterpSinInOut:
		INTERP_WITH_RANGE(10.f, 11.2f, InterpSinInOut, A)
		INTERP_WITH_RANGE(0.f, 1.f, InterpSinInOut, B)
		INTERP_WITH_RANGE(-5.6f;, -4.3f, InterpSinInOut, C)
		TArray< TPair< EasingFunc, FString > > FunctionsToTest;
		FunctionsToTest.Emplace(InterpSinInOutA, TEXT("InterpSinInOutA"));
		FunctionsToTest.Emplace(InterpSinInOutB, TEXT("InterpSinInOutB"));
		FunctionsToTest.Emplace(InterpSinInOutC, TEXT("InterpSinInOutC"));
		RunInOutTest(FunctionsToTest, this);
	}

	return true;
}

#endif //WITH_DEV_AUTOMATION_TESTS
