// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Transform.cpp
=============================================================================*/

#include "TransformVectorized.h"
#include "Misc/AssertionMacros.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Logging/LogMacros.h"
#include "Math/Vector.h"
#include "Math/VectorRegister.h"
#include "Math/Rotator.h"
#include "Math/Matrix.h"
#include "Math/Quat.h"
#include "Math/Transform.h"

#if ENABLE_VECTORIZED_TRANSFORM

#include "Misc/DefaultValueHelper.h"

DEFINE_LOG_CATEGORY_STATIC(LogTransform, Log, All);

// FTransform identity
// @Note: Do not reference FVector::ZeroVector or FVector::OneVector
// because they're not initialized yet, it will come as 0 vector
const FTransform FTransform::Identity(FQuat(0.f,0.f,0.f,1.f), FVector(0.f), FVector(1.f));

// Replacement of Inverse of FMatrix

/**
* Does a debugf of the contents of this BoneAtom.
*/
void FTransform::DebugPrint() const
{
	UE_LOG(LogTransform, Log, TEXT("%s"), *ToHumanReadableString());
}

FString FTransform::ToHumanReadableString() const
{
	FRotator R(GetRotation());
	FVector T(GetTranslation());
	FVector S(GetScale3D());

	FString Output= FString::Printf(TEXT("Rotation: Pitch %f Yaw %f Roll %f\r\n"), R.Pitch, R.Yaw, R.Roll);
	Output += FString::Printf(TEXT("Translation: %f %f %f\r\n"), T.X, T.Y, T.Z);
	Output += FString::Printf(TEXT("Scale3D: %f %f %f\r\n"), S.X, S.Y, S.Z);

	return Output;
}


FString FTransform::ToString() const
{
	const FRotator R(Rotator());
	const FVector T(GetTranslation());
	const FVector S(GetScale3D());

	return FString::Printf(TEXT("%f,%f,%f|%f,%f,%f|%f,%f,%f"), T.X, T.Y, T.Z, R.Pitch, R.Yaw, R.Roll, S.X, S.Y, S.Z);
}

bool FTransform::InitFromString( const FString& Source )
{
	TArray<FString> ComponentStrings;
	Source.ParseIntoArray(ComponentStrings, TEXT("|"), true);
	const int32 NumComponents = ComponentStrings.Num();
	if(3 != NumComponents)
	{
		return false;
	}

	// Translation
	FVector ParsedTranslation = FVector::ZeroVector;
	if( !FDefaultValueHelper::ParseVector(ComponentStrings[0], ParsedTranslation) )
	{
		return false;
	}

	// Rotation
	FRotator ParsedRotation = FRotator::ZeroRotator;
	if( !FDefaultValueHelper::ParseRotator(ComponentStrings[1], ParsedRotation) )
	{
		return false;
	}

	// Scale
	FVector Scale = FVector::OneVector;
	if( !FDefaultValueHelper::ParseVector(ComponentStrings[2], Scale) )
	{
		return false;
	}

	SetComponents(FQuat(ParsedRotation), ParsedTranslation, Scale);

	return true;
}

#define DEBUG_INVERSE_TRANSFORM 0
FTransform FTransform::GetRelativeTransformReverse(const FTransform& Other) const
{
	// A (-1) * B = VQS(B)(VQS (A)(-1))
	// 
	// Scale = S(B)/S(A)
	// Rotation = Q(B) * Q(A)(-1)
	// Translation = T(B)-S(B)/S(A) *[Q(B)*Q(A)(-1)*T(A)*Q(A)*Q(B)(-1)]
	// where A = this, and B = Other
	FTransform Result;

	// Scale = S(B)/S(A)	
	VectorRegister VSafeScale3D	= VectorSet_W0(GetSafeScaleReciprocal(Scale3D));
	VectorRegister VScale3D = VectorMultiply(Other.Scale3D, VSafeScale3D);
	
	// Rotation = Q(B) * Q(A)(-1)	
	VectorRegister VInverseRot = VectorQuaternionInverse(Rotation);
	VectorRegister VRotation = VectorQuaternionMultiply2(Other.Rotation, VInverseRot );
	
	// RotatedTranslation
	VectorRegister VR = VectorQuaternionRotateVector(VRotation, Translation);

	// Translation = T(B)-S(B)/S(A) *[Q(B)*Q(A)(-1)*T(A)*Q(A)*Q(B)(-1)]	
	VectorRegister VTranslation = VectorSet_W0(VectorSubtract(Other.Translation, VectorMultiply(VScale3D, VR)));

	Result.Scale3D = VScale3D;	
	Result.Translation = VTranslation;
	Result.Rotation = VRotation;
		
	Result.DiagnosticCheckNaN_All(); 

#if DEBUG_INVERSE_TRANSFORM
	FMatrix AM = ToMatrixWithScale();
	FMatrix BM = Other.ToMatrixWithScale();

	Result.DebugEqualMatrix(AM.InverseFast() *  BM);
#endif

	return Result;
}

/**
 * Set current transform and the relative to ParentTransform.
 * Equates to This = This->GetRelativeTransform(Parent), but saves the intermediate FTransform storage and copy.
 */
void FTransform::SetToRelativeTransform(const FTransform& ParentTransform)
{
	// A * B(-1) = VQS(B)(-1) (VQS (A))
	// 
	// Scale = S(A)/S(B)
	// Rotation = Q(B)(-1) * Q(A)
	// Translation = 1/S(B) *[Q(B)(-1)*(T(A)-T(B))*Q(B)]
	// where A = this, B = Other
#if DEBUG_INVERSE_TRANSFORM
	FMatrix AM = ToMatrixWithScale();
	FMatrix BM = ParentTransform.ToMatrixWithScale();
#endif
	
	checkSlow(ParentTransform.IsRotationNormalized());

	// Scale = S(A)/S(B)	
	VectorRegister VSafeScale3D	= VectorSet_W0(GetSafeScaleReciprocal(ParentTransform.Scale3D, ScalarRegister(SMALL_NUMBER)));
	Scale3D = VectorMultiply(Scale3D, VSafeScale3D);
	
	//VQTranslation = (  ( T(A).X - T(B).X ),  ( T(A).Y - T(B).Y ), ( T(A).Z - T(B).Z), 0.f );
	VectorRegister VQTranslation = VectorSet_W0(VectorSubtract(Translation, ParentTransform.Translation));

	// Inverse RotatedTranslation
	VectorRegister VInverseParentRot = VectorQuaternionInverse(ParentTransform.Rotation);
	VectorRegister VR = VectorQuaternionRotateVector(VInverseParentRot, VQTranslation);

	// Translation = 1/S(B)
	Translation = VectorMultiply(VR, VSafeScale3D);

	// Rotation = Q(B)(-1) * Q(A)	
	Rotation = VectorQuaternionMultiply2(VInverseParentRot, Rotation );

	DiagnosticCheckNaN_All(); 

#if DEBUG_INVERSE_TRANSFORM
	DebugEqualMatrix(AM *  BM.InverseFast());
#endif
}

void FTransform::GetRelativeTransformUsingMatrixWithScale(FTransform* OutTransform, const FTransform* Base, const FTransform* Relative)
{
	// the goal of using M is to get the correct orientation
	// but for translation, we still need scale
	FMatrix AM = Base->ToMatrixWithScale();
	FMatrix BM = Relative->ToMatrixWithScale();
	// get combined scale
	// Scale = S(A)/S(B)
	static ScalarRegister STolerance(SMALL_NUMBER);
	VectorRegister VSafeScale3D = VectorSet_W0(GetSafeScaleReciprocal(Relative->Scale3D, STolerance));
	VectorRegister VScale3D = VectorMultiply(Base->Scale3D, VSafeScale3D);
	ConstructTransformFromMatrixWithDesiredScale(AM, BM.Inverse(), VScale3D, *OutTransform);
}

FTransform FTransform::GetRelativeTransform(const FTransform& Other) const
{
	// A * B(-1) = VQS(B)(-1) (VQS (A))
	// 
	// Scale = S(A)/S(B)
	// Rotation = Q(B)(-1) * Q(A)
	// Translation = 1/S(B) *[Q(B)(-1)*(T(A)-T(B))*Q(B)]
	// where A = this, B = Other
	FTransform Result;
		
	if (Other.IsRotationNormalized() == false)
	{
		return FTransform::Identity;
	}

	if (Private_AnyHasNegativeScale(this->Scale3D, Other.Scale3D))
	{
		// @note, if you have 0 scale with negative, you're going to lose rotation as it can't convert back to quat
		GetRelativeTransformUsingMatrixWithScale(&Result, this, &Other);
	}
	else
	{
		// Scale = S(A)/S(B)
		static ScalarRegister STolerance(SMALL_NUMBER);
		VectorRegister VSafeScale3D = VectorSet_W0(GetSafeScaleReciprocal(Other.Scale3D, STolerance));

		VectorRegister VScale3D = VectorMultiply(Scale3D, VSafeScale3D);

		//VQTranslation = (  ( T(A).X - T(B).X ),  ( T(A).Y - T(B).Y ), ( T(A).Z - T(B).Z), 0.f );
		VectorRegister VQTranslation = VectorSet_W0(VectorSubtract(Translation, Other.Translation));

		// Inverse RotatedTranslation
		VectorRegister VInverseRot = VectorQuaternionInverse(Other.Rotation);
		VectorRegister VR = VectorQuaternionRotateVector(VInverseRot, VQTranslation);

		//Translation = 1/S(B)
		VectorRegister VTranslation = VectorMultiply(VR, VSafeScale3D);

		// Rotation = Q(B)(-1) * Q(A)	
		VectorRegister VRotation = VectorQuaternionMultiply2(VInverseRot, Rotation);

		Result.Scale3D = VScale3D;
		Result.Translation = VTranslation;
		Result.Rotation = VRotation;

		Result.DiagnosticCheckNaN_All();
#if DEBUG_INVERSE_TRANSFORM
		FMatrix AM = ToMatrixWithScale();
		FMatrix BM = Other.ToMatrixWithScale();

		Result.DebugEqualMatrix(AM *  BM.InverseFast());
#endif
	}

	return Result;
}

bool FTransform::DebugEqualMatrix(const FMatrix& Matrix) const
{
	FTransform TestResult(Matrix);
	if (!Equals(TestResult))
	{
		// see now which one isn't equal		
		if (!Scale3DEquals(TestResult, 0.01f))
		{
			UE_LOG(LogTransform, Log, TEXT("Matrix(S)\t%s"), *TestResult.GetScale3D().ToString());
			UE_LOG(LogTransform, Log, TEXT("VQS(S)\t%s"), *GetScale3D().ToString());
		}

		// see now which one isn't equal
		if (!RotationEquals(TestResult))
		{
			UE_LOG(LogTransform, Log, TEXT("Matrix(R)\t%s"), *TestResult.GetRotation().ToString());
			UE_LOG(LogTransform, Log, TEXT("VQS(R)\t%s"), *GetRotation().ToString());
		}

		// see now which one isn't equal
		if (!TranslationEquals(TestResult, 0.01f))
		{
			UE_LOG(LogTransform, Log, TEXT("Matrix(T)\t%s"), *TestResult.GetTranslation().ToString());
			UE_LOG(LogTransform, Log, TEXT("VQS(T)\t%s"), *GetTranslation().ToString());
		}
		return false;
	}

	return true;
}

#endif // ENABLE_VECTORIZED_TRANSFORM
