// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Transform.cpp
=============================================================================*/

#include "Math/Transform.h"

#if !ENABLE_VECTORIZED_TRANSFORM

#include "DefaultValueHelper.h"

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

	FString Output = FString::Printf(TEXT("Rotation: Pitch %f Yaw %f Roll %f\r\n"), R.Pitch, R.Yaw, R.Roll);
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
	FVector ParsedScale = FVector::OneVector;
	if( !FDefaultValueHelper::ParseVector(ComponentStrings[2], ParsedScale) )
	{
		return false;
	}

	SetComponents(FQuat(ParsedRotation), ParsedTranslation, ParsedScale);

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

	FVector SafeRecipScale3D = GetSafeScaleReciprocal(Scale3D);
	Result.Scale3D = Other.Scale3D*SafeRecipScale3D;	

	Result.Rotation = Other.Rotation*Rotation.Inverse();

	Result.Translation = Other.Translation - Result.Scale3D * ( Result.Rotation * Translation );

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

	const FVector SafeRecipScale3D = GetSafeScaleReciprocal(ParentTransform.Scale3D, SMALL_NUMBER);
	const FQuat InverseRot = ParentTransform.Rotation.Inverse();

	Scale3D *= SafeRecipScale3D;	
	Translation = (InverseRot * (Translation - ParentTransform.Translation)) * SafeRecipScale3D;
	Rotation = InverseRot * Rotation;

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
	FVector SafeRecipScale3D = GetSafeScaleReciprocal(Relative->Scale3D, SMALL_NUMBER);
	FVector DesiredScale3D = Base->Scale3D*SafeRecipScale3D;
	ConstructTransformFromMatrixWithDesiredScale(AM, BM.Inverse(), DesiredScale3D, *OutTransform);
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

	if (AnyHasNegativeScale(Scale3D, Other.GetScale3D()))
	{
		// @note, if you have 0 scale with negative, you're going to lose rotation as it can't convert back to quat
		GetRelativeTransformUsingMatrixWithScale(&Result, this, &Other);
	}
	else
	{
		FVector SafeRecipScale3D = GetSafeScaleReciprocal(Other.Scale3D, SMALL_NUMBER);
		Result.Scale3D = Scale3D*SafeRecipScale3D;

		if (Other.Rotation.IsNormalized() == false)
		{
			return FTransform::Identity;
		}

		FQuat Inverse = Other.Rotation.Inverse();
		Result.Rotation = Inverse*Rotation;

		Result.Translation = (Inverse*(Translation - Other.Translation))*(SafeRecipScale3D);

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
		if (!Scale3D.Equals(TestResult.Scale3D, 0.01f))
		{
			UE_LOG(LogTransform, Log, TEXT("Matrix(S)\t%s"), *TestResult.Scale3D.ToString());
			UE_LOG(LogTransform, Log, TEXT("VQS(S)\t%s"), *Scale3D.ToString());
		}

		// see now which one isn't equal
		if (!Rotation.Equals(TestResult.Rotation))
		{
			UE_LOG(LogTransform, Log, TEXT("Matrix(R)\t%s"), *TestResult.Rotation.ToString());
			UE_LOG(LogTransform, Log, TEXT("VQS(R)\t%s"), *Rotation.ToString());
		}

		// see now which one isn't equal
		if (!Translation.Equals(TestResult.Translation, 0.01f))
		{
			UE_LOG(LogTransform, Log, TEXT("Matrix(T)\t%s"), *TestResult.Translation.ToString());
			UE_LOG(LogTransform, Log, TEXT("VQS(T)\t%s"), *Translation.ToString());
		}
		return false;
	}

	return true;
}

#endif // #if !ENABLE_VECTORIZED_TRANSFORM
