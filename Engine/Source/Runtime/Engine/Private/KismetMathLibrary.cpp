// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Kismet/KismetMathLibrary.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"

#include "Blueprint/BlueprintSupport.h"
#include "Math/DualQuat.h"

#include "Misc/RuntimeErrors.h"

#define LOCTEXT_NAMESPACE "UKismetMathLibrary"

/** Interpolate a linear alpha value using an ease mode and function. */
float EaseAlpha(float InAlpha, uint8 EasingFunc, float BlendExp, int32 Steps)
{
	switch (EasingFunc)
	{
	case EEasingFunc::Step:					return FMath::InterpStep<float>(0.f, 1.f, InAlpha, Steps);
	case EEasingFunc::SinusoidalIn:			return FMath::InterpSinIn<float>(0.f, 1.f, InAlpha);
	case EEasingFunc::SinusoidalOut:		return FMath::InterpSinOut<float>(0.f, 1.f, InAlpha);
	case EEasingFunc::SinusoidalInOut:		return FMath::InterpSinInOut<float>(0.f, 1.f, InAlpha);
	case EEasingFunc::EaseIn:				return FMath::InterpEaseIn<float>(0.f, 1.f, InAlpha, BlendExp);
	case EEasingFunc::EaseOut:				return FMath::InterpEaseOut<float>(0.f, 1.f, InAlpha, BlendExp);
	case EEasingFunc::EaseInOut:			return FMath::InterpEaseInOut<float>(0.f, 1.f, InAlpha, BlendExp);
	case EEasingFunc::ExpoIn:				return FMath::InterpExpoIn<float>(0.f, 1.f, InAlpha);
	case EEasingFunc::ExpoOut:				return FMath::InterpExpoOut<float>(0.f, 1.f, InAlpha);
	case EEasingFunc::ExpoInOut:			return FMath::InterpExpoInOut<float>(0.f, 1.f, InAlpha);
	case EEasingFunc::CircularIn:			return FMath::InterpCircularIn<float>(0.f, 1.f, InAlpha);
	case EEasingFunc::CircularOut:			return FMath::InterpCircularOut<float>(0.f, 1.f, InAlpha);
	case EEasingFunc::CircularInOut:		return FMath::InterpCircularInOut<float>(0.f, 1.f, InAlpha);
	}
	return InAlpha;
}

const FName DivideByZeroWarning = FName("DivideByZeroWarning");
const FName NegativeSqrtWarning = FName("NegativeSqrtWarning");
const FName ZeroLengthProjectionWarning = FName("ZeroLengthProjectionWarning");
const FName InvalidDateWarning = FName("InvalidDateWarning");

UKismetMathLibrary::UKismetMathLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FBlueprintSupport::RegisterBlueprintWarning(
		FBlueprintWarningDeclaration (
			DivideByZeroWarning,
			LOCTEXT("DivideByZeroWarning", "Divide by zero")
		)
	);
	FBlueprintSupport::RegisterBlueprintWarning(
		FBlueprintWarningDeclaration (
			NegativeSqrtWarning,
			LOCTEXT("NegativeSqrtWarning", "Square root of negative number")
		)
	);
	FBlueprintSupport::RegisterBlueprintWarning(
		FBlueprintWarningDeclaration (
			ZeroLengthProjectionWarning,
			LOCTEXT("ZeroLengthProjectionWarning", "Projection onto vector of zero length")
		)
	);
	FBlueprintSupport::RegisterBlueprintWarning(
		FBlueprintWarningDeclaration (
			InvalidDateWarning,
			LOCTEXT("InvalidDateWarning", "Invalid date warning")
		)
	);
}

void UKismetMathLibrary::ReportError_Divide_ByteByte()
{
	FFrame::KismetExecutionMessage(TEXT("Divide by zero: Divide_ByteByte"), ELogVerbosity::Warning, DivideByZeroWarning);
}

void UKismetMathLibrary::ReportError_Percent_ByteByte()
{
	FFrame::KismetExecutionMessage(TEXT("Modulo by zero"), ELogVerbosity::Warning, DivideByZeroWarning);
}

void UKismetMathLibrary::ReportError_Divide_IntInt()
{
	FFrame::KismetExecutionMessage(TEXT("Divide by zero: Divide_IntInt"), ELogVerbosity::Warning, DivideByZeroWarning);
}

void UKismetMathLibrary::ReportError_Percent_IntInt()
{
	FFrame::KismetExecutionMessage(TEXT("Modulo by zero"), ELogVerbosity::Warning, DivideByZeroWarning);
}

void UKismetMathLibrary::ReportError_Sqrt()
{
	FFrame::KismetExecutionMessage(TEXT("Attempt to take Sqrt() of negative number - returning 0."), ELogVerbosity::Warning, NegativeSqrtWarning);
}

void UKismetMathLibrary::ReportError_Divide_VectorFloat()
{
	FFrame::KismetExecutionMessage(TEXT("Divide by zero: Divide_VectorFloat"), ELogVerbosity::Warning, DivideByZeroWarning);
}

void UKismetMathLibrary::ReportError_Divide_VectorInt()
{
	FFrame::KismetExecutionMessage(TEXT("Divide by zero: Divide_VectorInt"), ELogVerbosity::Warning, DivideByZeroWarning);
}

void UKismetMathLibrary::ReportError_Divide_VectorVector()
{
	FFrame::KismetExecutionMessage(TEXT("Divide by zero: Divide_VectorVector"), ELogVerbosity::Warning, DivideByZeroWarning);
}

void UKismetMathLibrary::ReportError_Divide_Vector2DVector2D()
{
	FFrame::KismetExecutionMessage(TEXT("Divide by zero: Divide_Vector2DVector2D"), ELogVerbosity::Warning, DivideByZeroWarning);
}

void UKismetMathLibrary::ReportError_ProjectVectorOnToVector()
{
	FFrame::KismetExecutionMessage(TEXT("Divide by zero: ProjectVectorOnToVector with zero Target vector"), ELogVerbosity::Warning, ZeroLengthProjectionWarning);
}

void UKismetMathLibrary::ReportError_Divide_Vector2DFloat()
{
	FFrame::KismetExecutionMessage(TEXT("Divide by zero: Divide_Vector2DFloat"), ELogVerbosity::Warning, DivideByZeroWarning);
}

void UKismetMathLibrary::ReportError_DaysInMonth()
{
	FFrame::KismetExecutionMessage(TEXT("Invalid month (must be between 1 and 12): DaysInMonth"), ELogVerbosity::Warning, InvalidDateWarning);
}


// Include code in this source file if it's not being inlined in the header.
#if !KISMET_MATH_INLINE_ENABLED
#include "Kismet/KismetMathLibrary.inl"
#endif

bool UKismetMathLibrary::RandomBoolWithWeight(float Weight)
{
	//If the Weight equals to 0.0f then always return false
	if (Weight <= 0.0f)
	{
		return false;
	}
	else
	{
		//If the Weight is higher or equal to the random number then return true
		return Weight >= FMath::FRandRange(0.0f, 1.0f);
	}

}

bool UKismetMathLibrary::RandomBoolWithWeightFromStream(float Weight, const FRandomStream& RandomStream)
{
	//If the Weight equals to 0.0f then always return false
	if (Weight <= 0.0f)
	{
		return false;
	}
	else
	{
		//Create the random float from the specified stream
		float Number = UKismetMathLibrary::RandomFloatFromStream(RandomStream);

		//If the Weight is higher or equal to number generated from stream then return true
		return Weight >= Number;
	}

}

/* This function is custom thunked, the real function is GenericDivide_FloatFloat */
float UKismetMathLibrary::Divide_FloatFloat(float A, float B)
{
	check(0);
	return 0;
}

/* This function is custom thunked, the real function is GenericPercent_FloatFloat */
float UKismetMathLibrary::Percent_FloatFloat(float A, float B)
{
	check(0);
	return 0;
}

float UKismetMathLibrary::GenericPercent_FloatFloat(float A, float B)
{
	return (B != 0.f) ? FMath::Fmod(A, B) : 0.f;
}

bool UKismetMathLibrary::InRange_FloatFloat(float Value, float Min, float Max, bool InclusiveMin, bool InclusiveMax)
{
	return ((InclusiveMin ? (Value >= Min) : (Value > Min)) && (InclusiveMax ? (Value <= Max) : (Value < Max)));
}

float UKismetMathLibrary::Hypotenuse(float Width, float Height)
{
	// This implementation avoids overflow/underflow caused by squaring width and height:
	Width = FMath::Abs(Width);
	Height = FMath::Abs(Height);

	float Min = FGenericPlatformMath::Min(Width, Height);
	float Max = FGenericPlatformMath::Max(Width, Height);
	float Ratio = Min / Max;
	return Max * FMath::Sqrt(1.f + Ratio * Ratio);
}

float UKismetMathLibrary::Log(float A, float Base)
{
	float Result = 0.f;
	if(Base <= 0.f)
	{
		FFrame::KismetExecutionMessage(TEXT("Divide by zero: Log"), ELogVerbosity::Warning, DivideByZeroWarning);
	}
	else
	{
		Result = FMath::Loge(A) / FMath::Loge(Base);
	}
	return Result;
}

int32 UKismetMathLibrary::FMod(float Dividend, float Divisor, float& Remainder)
{
	int32 Result;
	if (Divisor != 0.f)
	{
		const float Quotient = Dividend / Divisor;
		Result = (Quotient < 0.f ? -1 : 1) * FMath::FloorToInt(FMath::Abs(Quotient));
		Remainder = FMath::Fmod(Dividend, Divisor);
	}
	else
	{
		FFrame::KismetExecutionMessage(TEXT("Attempted modulo 0 - returning 0."), ELogVerbosity::Warning, DivideByZeroWarning);

		Result = 0;
		Remainder = 0.f;
	}

	return Result;
}

float UKismetMathLibrary::NormalizeToRange(float Value, float RangeMin, float RangeMax)
{
	if (RangeMin == RangeMax)
	{
		return RangeMin;
	}

	if (RangeMin > RangeMax)
	{
		Swap(RangeMin, RangeMax);
	}
	return (Value - RangeMin) / (RangeMax - RangeMin);
}

float UKismetMathLibrary::MapRangeUnclamped(float Value, float InRangeA, float InRangeB, float OutRangeA, float OutRangeB)
{
	return FMath::GetMappedRangeValueUnclamped(FVector2D(InRangeA, InRangeB), FVector2D(OutRangeA, OutRangeB), Value);
}

float UKismetMathLibrary::MapRangeClamped(float Value, float InRangeA, float InRangeB, float OutRangeA, float OutRangeB)
{
	return FMath::GetMappedRangeValueClamped(FVector2D(InRangeA, InRangeB), FVector2D(OutRangeA, OutRangeB), Value);
}

float UKismetMathLibrary::FInterpEaseInOut(float A, float B, float Alpha, float Exponent)
{
	return FMath::InterpEaseInOut<float>(A, B, Alpha, Exponent);
}

float UKismetMathLibrary::MakePulsatingValue(float InCurrentTime, float InPulsesPerSecond, float InPhase)
{
	return FMath::MakePulsatingValue((double)InCurrentTime, InPulsesPerSecond, InPhase);
}

void UKismetMathLibrary::MaxOfIntArray(const TArray<int32>& IntArray, int32& IndexOfMaxValue, int32& MaxValue)
{
	MaxValue = FMath::Max(IntArray, &IndexOfMaxValue);
}

void UKismetMathLibrary::MinOfIntArray(const TArray<int32>& IntArray, int32& IndexOfMinValue, int32& MinValue)
{
	MinValue = FMath::Min<int32>(IntArray, &IndexOfMinValue);
}

void UKismetMathLibrary::MaxOfFloatArray(const TArray<float>& FloatArray, int32& IndexOfMaxValue, float& MaxValue)
{
	MaxValue = FMath::Max(FloatArray, &IndexOfMaxValue);
}

void UKismetMathLibrary::MinOfFloatArray(const TArray<float>& FloatArray, int32& IndexOfMinValue, float& MinValue)
{
	MinValue = FMath::Min(FloatArray, &IndexOfMinValue);
}

void UKismetMathLibrary::MaxOfByteArray(const TArray<uint8>& ByteArray, int32& IndexOfMaxValue, uint8& MaxValue)
{
	MaxValue = FMath::Max(ByteArray, &IndexOfMaxValue);
}

void UKismetMathLibrary::MinOfByteArray(const TArray<uint8>& ByteArray, int32& IndexOfMinValue, uint8& MinValue)
{
	MinValue = FMath::Min(ByteArray, &IndexOfMinValue);
}

float UKismetMathLibrary::InverseLerp(float A, float B, float Value)
{
	if (FMath::IsNearlyEqual(A, B))
	{
		if (Value < A)
		{
			return 0;
		}
		else
		{
			return 1;
		}
	}
	else
	{
		return ((Value - A) / (B - A));
	}
}

float UKismetMathLibrary::Ease(float A, float B, float Alpha, TEnumAsByte<EEasingFunc::Type> EasingFunc, float BlendExp, int32 Steps)
{
	return Lerp(A, B, EaseAlpha(Alpha, EasingFunc, BlendExp, Steps));
}

FVector  UKismetMathLibrary::RotateAngleAxis(FVector InVect, float AngleDeg, FVector Axis)
{
	return InVect.RotateAngleAxis(AngleDeg, Axis.GetSafeNormal());
}

FVector UKismetMathLibrary::VEase(FVector A, FVector B, float Alpha, TEnumAsByte<EEasingFunc::Type> EasingFunc, float BlendExp, int32 Steps)
{
	return VLerp(A, B, EaseAlpha(Alpha, EasingFunc, BlendExp, Steps));
}

float ComputeDamping(float Mass, float Stiffness, float CriticalDampingFactor)
{
	return 2 * FMath::Sqrt(Mass * Stiffness) * CriticalDampingFactor;
}

template <typename T>
T GenericSpringInterp(T Current, T Target, T& PrevError, T& Velocity, float Stiffness, float CriticalDamping, float DeltaTime, float Mass)
{
	if (DeltaTime > SMALL_NUMBER)
	{
		if (!FMath::IsNearlyZero(Mass))
		{
			const float Damping = ComputeDamping(Mass, Stiffness, CriticalDamping);
			const T Error = Target - Current;
			const T ErrorDeriv = (Error - PrevError);	//ignore divide by delta time since we multiply later anyway
			Velocity += (Error * Stiffness * DeltaTime + ErrorDeriv * Damping) / Mass;
			PrevError = Error;

			const T NewValue = Current + Velocity * DeltaTime;
			return NewValue;
		}
		else
		{
			return Target;
		}
	}

	return Current;
}

float UKismetMathLibrary::FloatSpringInterp(float Current, float Target, FFloatSpringState& SpringState, float Stiffness, float CriticalDamping, float DeltaTime, float Mass)
{
	return GenericSpringInterp(Current, Target, SpringState.PrevError, SpringState.Velocity, Stiffness, CriticalDamping, DeltaTime, Mass);
}

FVector UKismetMathLibrary::VectorSpringInterp(FVector Current, FVector Target, FVectorSpringState& SpringState, float Stiffness, float CriticalDamping, float DeltaTime, float Mass)
{
	return GenericSpringInterp(Current, Target, SpringState.PrevError, SpringState.Velocity, Stiffness, CriticalDamping, DeltaTime, Mass);
}

void UKismetMathLibrary::ResetFloatSpringState(FFloatSpringState& SpringState)
{
	SpringState.Reset();
}

void UKismetMathLibrary::ResetVectorSpringState(FVectorSpringState& SpringState)
{
	SpringState.Reset();
}

FVector UKismetMathLibrary::RandomUnitVector()
{
	return FMath::VRand();
}

FVector UKismetMathLibrary::RandomUnitVectorInEllipticalConeInRadians(FVector ConeDir, float MaxYawInRadians, float MaxPitchInRadians)
{
	return FMath::VRandCone(ConeDir, MaxYawInRadians, MaxPitchInRadians);
}

FRotator UKismetMathLibrary::RandomRotator(bool bRoll)
{
	FRotator RRot;
	RRot.Yaw = FMath::FRand() * 360.f;
	RRot.Pitch = FMath::FRand() * 360.f;

	if (bRoll)
	{
		RRot.Roll = FMath::FRand() * 360.f;
	}
	else
	{
		RRot.Roll = 0;
	}
	return RRot;
}

FVector UKismetMathLibrary::GetReflectionVector(FVector Direction, FVector SurfaceNormal)
{
	return FMath::GetReflectionVector(Direction, SurfaceNormal);
}

FVector UKismetMathLibrary::FindClosestPointOnLine(FVector Point, FVector LineOrigin, FVector LineDirection)
{
	const FVector SafeDir = LineDirection.GetSafeNormal();
	const FVector ClosestPoint = LineOrigin + (SafeDir * ((Point-LineOrigin) | SafeDir));
	return ClosestPoint;
}

FVector UKismetMathLibrary::ClampVectorSize(FVector A, float Min, float Max)
{
	return A.GetClampedToSize(Min, Max);
}

FVector UKismetMathLibrary::GetVectorArrayAverage(const TArray<FVector>& Vectors)
{
	FVector Sum(0.f);
	FVector Average(0.f);

	if (Vectors.Num() > 0)
	{
		for (int32 VecIdx=0; VecIdx<Vectors.Num(); VecIdx++)
		{
			Sum += Vectors[VecIdx];
		}

		Average = Sum / ((float)Vectors.Num());
	}

	return Average;
}

FRotator UKismetMathLibrary::TransformRotation(const FTransform& T, FRotator Rotation)
{
	return T.TransformRotation(Rotation.Quaternion()).Rotator();
}

FRotator UKismetMathLibrary::InverseTransformRotation(const FTransform& T, FRotator Rotation)
{
	return T.InverseTransformRotation(Rotation.Quaternion()).Rotator();
}

FRotator UKismetMathLibrary::ComposeRotators(FRotator A, FRotator B)
{
	FQuat AQuat = FQuat(A);
	FQuat BQuat = FQuat(B);

	return FRotator(BQuat*AQuat);
}

void UKismetMathLibrary::GetAxes(FRotator A, FVector& X, FVector& Y, FVector& Z)
{
	FRotationMatrix R(A);
	R.GetScaledAxes(X, Y, Z);
}

FRotator UKismetMathLibrary::RLerp(FRotator A, FRotator B, float Alpha, bool bShortestPath)
{
	FRotator DeltaAngle = B - A;

	// if shortest path, we use Quaternion to interpolate instead of using FRotator
	if (bShortestPath)
	{
		FQuat AQuat(A);
		FQuat BQuat(B);

		FQuat Result = FQuat::Slerp(AQuat, BQuat, Alpha);
		Result.Normalize();

		return Result.Rotator();
	}

	return A + Alpha*DeltaAngle;
}

FRotator UKismetMathLibrary::REase(FRotator A, FRotator B, float Alpha, bool bShortestPath, TEnumAsByte<EEasingFunc::Type> EasingFunc, float BlendExp, int32 Steps)
{
	return RLerp(A, B, EaseAlpha(Alpha, EasingFunc, BlendExp, Steps), bShortestPath);
}

FRotator UKismetMathLibrary::NormalizedDeltaRotator(FRotator A, FRotator B)
{
	FRotator Delta = A - B;
	Delta.Normalize();
	return Delta;
}

FRotator UKismetMathLibrary::RotatorFromAxisAndAngle(FVector Axis, float Angle)
{
	FVector SafeAxis = Axis.GetSafeNormal(); // Make sure axis is unit length
	return FQuat(SafeAxis, FMath::DegreesToRadians(Angle)).Rotator();
}

float UKismetMathLibrary::ClampAxis(float Angle)
{
	return FRotator::ClampAxis(Angle);
}

float UKismetMathLibrary::NormalizeAxis(float Angle)
{
	return FRotator::NormalizeAxis(Angle);
}

FTransform UKismetMathLibrary::TLerp(const FTransform& A, const FTransform& B, float Alpha, TEnumAsByte<ELerpInterpolationMode::Type> LerpInterpolationMode)
{
	FTransform Result;

	FTransform NA = A;
	FTransform NB = B;
	NA.NormalizeRotation();
	NB.NormalizeRotation();

	// Quaternion interpolation
	if (LerpInterpolationMode == ELerpInterpolationMode::QuatInterp)
	{
		Result.Blend(NA, NB, Alpha);
		return Result;
	}
	// Euler Angle interpolation
	else if (LerpInterpolationMode == ELerpInterpolationMode::EulerInterp)
	{
		Result.SetTranslation(FMath::Lerp(NA.GetTranslation(), NB.GetTranslation(), Alpha));
		Result.SetScale3D(FMath::Lerp(NA.GetScale3D(), NB.GetScale3D(), Alpha));
		Result.SetRotation(FQuat(RLerp(NA.Rotator(), NB.Rotator(), Alpha, false)));
		return Result;
	}
	// Dual quaternion interpolation
	else
	{
		if ((NB.GetRotation() | NA.GetRotation()) < 0.0f)
		{
			NB.SetRotation(NB.GetRotation()*-1.0f);
		}
		return (FDualQuat(NA)*(1 - Alpha) + FDualQuat(NB)*Alpha).Normalized().AsFTransform(FMath::Lerp(NA.GetScale3D(), NB.GetScale3D(), Alpha));
	}
}

FTransform UKismetMathLibrary::TEase(const FTransform& A, const FTransform& B, float Alpha, TEnumAsByte<EEasingFunc::Type> EasingFunc, float BlendExp, int32 Steps)
{
	return TLerp(A, B, EaseAlpha(Alpha, EasingFunc, BlendExp, Steps));
}

FTransform UKismetMathLibrary::TInterpTo(const FTransform& Current, const FTransform& Target, float DeltaTime, float InterpSpeed)
{
	if (InterpSpeed <= 0.f)
	{
		return Target;
	}

	const float Alpha = FClamp(DeltaTime * InterpSpeed, 0.f, 1.f);

	return TLerp(Current, Target, Alpha);
}

bool UKismetMathLibrary::NearlyEqual_TransformTransform(const FTransform& A, const FTransform& B, float LocationTolerance, float RotationTolerance, float Scale3DTolerance)
{
	return
		FTransform::AreRotationsEqual(A, B, RotationTolerance) &&
		FTransform::AreTranslationsEqual(A, B, LocationTolerance) &&
		FTransform::AreScale3DsEqual(A, B, Scale3DTolerance);
}

bool UKismetMathLibrary::ClassIsChildOf(TSubclassOf<class UObject> TestClass, TSubclassOf<class UObject> ParentClass)
{
	return ((*ParentClass != NULL) && (*TestClass != NULL)) ? (*TestClass)->IsChildOf(*ParentClass) : false;
}

/* Plane functions
 *****************************************************************************/
FPlane UKismetMathLibrary::MakePlaneFromPointAndNormal(FVector Point, FVector Normal)
{
	return FPlane(Point, Normal.GetSafeNormal());
}

/* DateTime functions
 *****************************************************************************/
FDateTime UKismetMathLibrary::MakeDateTime(int32 Year, int32 Month, int32 Day, int32 Hour, int32 Minute, int32 Second, int32 Millisecond)
{
	if (!FDateTime::Validate(Year, Month, Day, Hour, Minute, Second, Millisecond))
	{
		FFrame::KismetExecutionMessage(*FString::Printf(TEXT("DateTime in bad format (year %d, month %d, day %d, hour %d, minute %d, second %d, millisecond %d). E.g. year, month and day can't be zero."), Year, Month, Day, Hour, Minute, Second, Millisecond), ELogVerbosity::Warning, InvalidDateWarning);

		return FDateTime(1, 1, 1, 0, 0, 0, 0);
	}

	return FDateTime(Year, Month, Day, Hour, Minute, Second, Millisecond);
}

void UKismetMathLibrary::BreakDateTime(FDateTime InDateTime, int32& Year, int32& Month, int32& Day, int32& Hour, int32& Minute, int32& Second, int32& Millisecond)
{
	Year = GetYear(InDateTime);
	Month = GetMonth(InDateTime);
	Day = GetDay(InDateTime);
	Hour = GetHour(InDateTime);
	Minute = GetMinute(InDateTime);
	Second = GetSecond(InDateTime);
	Millisecond = GetMillisecond(InDateTime);
}


/* Timespan functions
*****************************************************************************/

FTimespan UKismetMathLibrary::MakeTimespan(int32 Days, int32 Hours, int32 Minutes, int32 Seconds, int32 Milliseconds)
{
	return FTimespan(Days, Hours, Minutes, Seconds, Milliseconds * 1000 * 1000);
}


FTimespan UKismetMathLibrary::MakeTimespan2(int32 Days, int32 Hours, int32 Minutes, int32 Seconds, int32 FractionNano)
{
	return FTimespan(Days, Hours, Minutes, Seconds, FractionNano);
}


void UKismetMathLibrary::BreakTimespan(FTimespan InTimespan, int32& Days, int32& Hours, int32& Minutes, int32& Seconds, int32& Milliseconds)
{
	Days = InTimespan.GetDays();
	Hours = InTimespan.GetHours();
	Minutes = InTimespan.GetMinutes();
	Seconds = InTimespan.GetSeconds();
	Milliseconds = InTimespan.GetFractionMilli();
}


void UKismetMathLibrary::BreakTimespan2(FTimespan InTimespan, int32& Days, int32& Hours, int32& Minutes, int32& Seconds, int32& FractionNano)
{
	Days = InTimespan.GetDays();
	Hours = InTimespan.GetHours();
	Minutes = InTimespan.GetMinutes();
	Seconds = InTimespan.GetSeconds();
	FractionNano = InTimespan.GetFractionNano();
}


FTimespan UKismetMathLibrary::FromDays(float Days)
{
	if (Days < FTimespan::MinValue().GetTotalDays())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("DaysValue"), Days);
		LogRuntimeError(FText::Format(LOCTEXT("ClampDaysToMinTimespan", "Days value {DaysValue} is less than minimum days TimeSpan can represent. Clamping to MinValue."), Args));
		return FTimespan::MinValue();
	}
	else if (Days > FTimespan::MaxValue().GetTotalDays())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("DaysValue"), Days);
		LogRuntimeError(FText::Format(LOCTEXT("ClampDaysToMaxTimespan", "Days value {DaysValue} is greater than maximum days TimeSpan can represent. Clamping to MaxValue."), Args));
		return FTimespan::MaxValue();
	}

	return FTimespan::FromDays(Days);
}


FTimespan UKismetMathLibrary::FromHours(float Hours)
{
	if (Hours < FTimespan::MinValue().GetTotalHours())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("HoursValue"), Hours);
		LogRuntimeError(FText::Format(LOCTEXT("ClampHoursToMinTimespan", "Hours value {HoursValue} is less than minimum hours TimeSpan can represent. Clamping to MinValue."), Args));
		return FTimespan::MinValue();
	}
	else if (Hours > FTimespan::MaxValue().GetTotalHours())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("HoursValue"), Hours);
		LogRuntimeError(FText::Format(LOCTEXT("ClampHoursToMaxTimespan", "Hours value {HoursValue} is greater than maximum hours TimeSpan can represent. Clamping to MaxValue."), Args));
		return FTimespan::MaxValue();
	}

	return FTimespan::FromHours(Hours);
}


FTimespan UKismetMathLibrary::FromMinutes(float Minutes)
{
	if (Minutes < FTimespan::MinValue().GetTotalMinutes())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("MinutesValue"), Minutes);
		LogRuntimeError(FText::Format(LOCTEXT("ClampMinutesToMinTimespan", "Minutes value {MinutesValue} is less than minimum minutes TimeSpan can represent. Clamping to MinValue."), Args));
		return FTimespan::MinValue();
	}
	else if (Minutes > FTimespan::MaxValue().GetTotalMinutes())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("MinutesValue"), Minutes);
		LogRuntimeError(FText::Format(LOCTEXT("ClampMinutesToMaxTimespan", "Minutes value {MinutesValue} is greater than maximum minutes TimeSpan can represent. Clamping to MaxValue."), Args));
		return FTimespan::MaxValue();
	}

	return FTimespan::FromMinutes(Minutes);
}


FTimespan UKismetMathLibrary::FromSeconds(float Seconds)
{
	if (Seconds < FTimespan::MinValue().GetTotalSeconds())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("SecondsValue"), Seconds);
		LogRuntimeError(FText::Format(LOCTEXT("ClampSecondsToMinTimespan", "Seconds value {SecondsValue} is less than minimum seconds TimeSpan can represent. Clamping to MinValue."), Args));
		return FTimespan::MinValue();
	}
	else if (Seconds > FTimespan::MaxValue().GetTotalSeconds())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("SecondsValue"), Seconds);
		LogRuntimeError(FText::Format(LOCTEXT("ClampSecondsToMaxTimespan", "Seconds value {SecondsValue} is greater than maximum seconds TimeSpan can represent. Clamping to MaxValue."), Args));
		return FTimespan::MaxValue();
	}

	return FTimespan::FromSeconds(Seconds);
}


FTimespan UKismetMathLibrary::FromMilliseconds(float Milliseconds)
{
	if (Milliseconds < FTimespan::MinValue().GetTotalMilliseconds())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("MillisecondsValue"), Milliseconds);
		LogRuntimeError(FText::Format(LOCTEXT("ClampMillisecondsToMinTimespan", "Milliseconds value {MillisecondsValue} is less than minimum milliseconds TimeSpan can represent. Clamping to MinValue."), Args));
		return FTimespan::MinValue();
	}
	else if (Milliseconds > FTimespan::MaxValue().GetTotalMilliseconds())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("MillisecondsValue"), Milliseconds);
		LogRuntimeError(FText::Format(LOCTEXT("ClampMillisecondsToMaxTimespan", "Milliseconds value {MillisecondsValue} is greater than maximum milliseconds TimeSpan can represent. Clamping to MaxValue."), Args));
		return FTimespan::MaxValue();
	}

	return FTimespan::FromMilliseconds(Milliseconds);
}


/* Rotator functions
*****************************************************************************/

FVector UKismetMathLibrary::GetForwardVector(FRotator InRot)
{
	return InRot.Vector();
}

FVector UKismetMathLibrary::GetRightVector(FRotator InRot)
{
	return FRotationMatrix(InRot).GetScaledAxis(EAxis::Y);
}

FVector UKismetMathLibrary::GetUpVector(FRotator InRot)
{
	return FRotationMatrix(InRot).GetScaledAxis(EAxis::Z);
}

FVector UKismetMathLibrary::CreateVectorFromYawPitch(float Yaw, float Pitch, float Length /*= 1.0f */)
{
	// FRotator::Vector() behaviour 
	float CP, SP, CY, SY;
	FMath::SinCos(&SP, &CP, FMath::DegreesToRadians(Pitch));
	FMath::SinCos(&SY, &CY, FMath::DegreesToRadians(Yaw));
	FVector V = FVector(CP*CY, CP*SY, SP) * Length;

	return V;
}

void UKismetMathLibrary::GetYawPitchFromVector(FVector InVec, float& Yaw, float& Pitch)
{
	FVector NormalizedVector = InVec.GetSafeNormal();
	// Find yaw.
	Yaw = FMath::RadiansToDegrees(FMath::Atan2(NormalizedVector.Y, NormalizedVector.X));

	// Find pitch.
	Pitch = FMath::RadiansToDegrees(FMath::Atan2(NormalizedVector.Z, FMath::Sqrt(NormalizedVector.X*NormalizedVector.X + NormalizedVector.Y*NormalizedVector.Y)));
}

void UKismetMathLibrary::GetAzimuthAndElevation(FVector InDirection, const FTransform& ReferenceFrame, float& Azimuth, float& Elevation)
{
	FVector2D Result = FMath::GetAzimuthAndElevation
	(
		InDirection.GetSafeNormal(),
		ReferenceFrame.GetUnitAxis(EAxis::X),
		ReferenceFrame.GetUnitAxis(EAxis::Y),
		ReferenceFrame.GetUnitAxis(EAxis::Z)
	);

	Azimuth = FMath::RadiansToDegrees(Result.X);
	Elevation = FMath::RadiansToDegrees(Result.Y);
}

void UKismetMathLibrary::BreakRotIntoAxes(const FRotator& InRot, FVector& X, FVector& Y, FVector& Z)
{
	FRotationMatrix(InRot).GetScaledAxes(X, Y, Z);
}

FRotator UKismetMathLibrary::MakeRotationFromAxes(FVector Forward, FVector Right, FVector Up)
{
	Forward.Normalize();
	Right.Normalize();
	Up.Normalize();

	FMatrix RotMatrix(Forward, Right, Up, FVector::ZeroVector);

	return RotMatrix.Rotator();
}

int32 UKismetMathLibrary::RandomIntegerFromStream(int32 Max, const FRandomStream& Stream)
{
	return Stream.RandHelper(Max);
}

int32 UKismetMathLibrary::RandomIntegerInRangeFromStream(int32 Min, int32 Max, const FRandomStream& Stream)
{
	return Stream.RandRange(Min, Max);
}

bool UKismetMathLibrary::InRange_IntInt(int32 Value, int32 Min, int32 Max, bool InclusiveMin, bool InclusiveMax)
{
	return ((InclusiveMin ? (Value >= Min) : (Value > Min)) && (InclusiveMax ? (Value <= Max) : (Value < Max)));
}

bool UKismetMathLibrary::RandomBoolFromStream(const FRandomStream& Stream)
{
	return (Stream.RandRange(0, 1) == 1) ? true : false;
}

float UKismetMathLibrary::RandomFloatFromStream(const FRandomStream& Stream)
{
	return Stream.FRand();
}

float UKismetMathLibrary::RandomFloatInRangeFromStream(float Min, float Max, const FRandomStream& Stream)
{
	return Min + (Max - Min) * RandomFloatFromStream(Stream);
}

FVector UKismetMathLibrary::RandomUnitVectorFromStream(const FRandomStream& Stream)
{
	return Stream.VRand();
}

FRotator UKismetMathLibrary::RandomRotatorFromStream(bool bRoll, const FRandomStream& Stream)
{
	FRotator RRot;
	RRot.Yaw = RandomFloatFromStream(Stream) * 360.f;
	RRot.Pitch = RandomFloatFromStream(Stream) * 360.f;

	if (bRoll)
	{
		RRot.Roll = RandomFloatFromStream(Stream) * 360.f;
	}
	else
	{
		RRot.Roll = 0;
	}
	return RRot;
}

void UKismetMathLibrary::ResetRandomStream(const FRandomStream& Stream)
{
	Stream.Reset();
}

void UKismetMathLibrary::SeedRandomStream(FRandomStream& Stream)
{
	Stream.GenerateNewSeed();
}

void UKismetMathLibrary::SetRandomStreamSeed(FRandomStream& Stream, int32 NewSeed)
{
	Stream.Initialize(NewSeed);
}

void UKismetMathLibrary::MinimumAreaRectangle(class UObject* WorldContextObject, const TArray<FVector>& InVerts, const FVector& SampleSurfaceNormal, FVector& OutRectCenter, FRotator& OutRectRotation, float& OutSideLengthX, float& OutSideLengthY, bool bDebugDraw)
{
	float MinArea = -1.f;
	float CurrentArea = -1.f;
	FVector SupportVectorA, SupportVectorB;
	FVector RectSideA, RectSideB;
	float MinDotResultA, MinDotResultB, MaxDotResultA, MaxDotResultB;
	FVector TestEdge;
	float TestEdgeDot = 0.f;
	FVector PolyNormal(0.f, 0.f, 1.f);
	TArray<int32> PolyVertIndices;

	// Bail if we receive an empty InVerts array
	if (InVerts.Num() == 0)
	{
		return;
	}

	// Compute the approximate normal of the poly, using the direction of SampleSurfaceNormal for guidance
	PolyNormal = (InVerts[InVerts.Num() / 3] - InVerts[0]) ^ (InVerts[InVerts.Num() * 2 / 3] - InVerts[InVerts.Num() / 3]);
	if ((PolyNormal | SampleSurfaceNormal) < 0.f)
	{
		PolyNormal = -PolyNormal;
	}

	// Transform the sample points to 2D
	FMatrix SurfaceNormalMatrix = FRotationMatrix::MakeFromZX(PolyNormal, FVector(1.f, 0.f, 0.f));
	TArray<FVector> TransformedVerts;
	OutRectCenter = FVector(0.f);
	for (int32 Idx = 0; Idx < InVerts.Num(); ++Idx)
	{
		OutRectCenter += InVerts[Idx];
		TransformedVerts.Add(SurfaceNormalMatrix.InverseTransformVector(InVerts[Idx]));
	}
	OutRectCenter /= InVerts.Num();

	// Compute the convex hull of the sample points
	ConvexHull2D::ComputeConvexHull(TransformedVerts, PolyVertIndices);

	// Minimum area rectangle as computed by http://www.geometrictools.com/Documentation/MinimumAreaRectangle.pdf
	for (int32 Idx = 1; Idx < PolyVertIndices.Num() - 1; ++Idx)
	{
		SupportVectorA = (TransformedVerts[PolyVertIndices[Idx]] - TransformedVerts[PolyVertIndices[Idx-1]]).GetSafeNormal();
		SupportVectorA.Z = 0.f;
		SupportVectorB.X = -SupportVectorA.Y;
		SupportVectorB.Y = SupportVectorA.X;
		SupportVectorB.Z = 0.f;
		MinDotResultA = MinDotResultB = MaxDotResultA = MaxDotResultB = 0.f;

		for (int TestVertIdx = 1; TestVertIdx < PolyVertIndices.Num(); ++TestVertIdx)
		{
			TestEdge = TransformedVerts[PolyVertIndices[TestVertIdx]] - TransformedVerts[PolyVertIndices[0]];
			TestEdgeDot = SupportVectorA | TestEdge;
			if (TestEdgeDot < MinDotResultA)
			{
				MinDotResultA = TestEdgeDot;
			}
			else if (TestEdgeDot > MaxDotResultA)
			{
				MaxDotResultA = TestEdgeDot;
			}

			TestEdgeDot = SupportVectorB | TestEdge;
			if (TestEdgeDot < MinDotResultB)
			{
				MinDotResultB = TestEdgeDot;
			}
			else if (TestEdgeDot > MaxDotResultB)
			{
				MaxDotResultB = TestEdgeDot;
			}
		}

		CurrentArea = (MaxDotResultA - MinDotResultA) * (MaxDotResultB - MinDotResultB);
		if (MinArea < 0.f || CurrentArea < MinArea)
		{
			MinArea = CurrentArea;
			RectSideA = SupportVectorA * (MaxDotResultA - MinDotResultA);
			RectSideB = SupportVectorB * (MaxDotResultB - MinDotResultB);
		}
	}

	RectSideA = SurfaceNormalMatrix.TransformVector(RectSideA);
	RectSideB = SurfaceNormalMatrix.TransformVector(RectSideB);
	OutRectRotation = FRotationMatrix::MakeFromZX(PolyNormal, RectSideA).Rotator();
	OutSideLengthX = RectSideA.Size();
	OutSideLengthY = RectSideB.Size();

#if ENABLE_DRAW_DEBUG
	if (bDebugDraw)
	{
		if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
		{
			DrawDebugSphere(World, OutRectCenter, 10.f, 12, FColor::Yellow, true);
			DrawDebugCoordinateSystem(World, OutRectCenter, SurfaceNormalMatrix.Rotator(), 100.f, true);
			DrawDebugLine(World, OutRectCenter - RectSideA * 0.5f + FVector(0, 0, 10.f), OutRectCenter + RectSideA * 0.5f + FVector(0, 0, 10.f), FColor::Green, true, -1, 0, 5.f);
			DrawDebugLine(World, OutRectCenter - RectSideB * 0.5f + FVector(0, 0, 10.f), OutRectCenter + RectSideB * 0.5f + FVector(0, 0, 10.f), FColor::Blue, true, -1, 0, 5.f);
		}
	}
#endif
}

bool UKismetMathLibrary::IsPointInBox(FVector Point, FVector BoxOrigin, FVector BoxExtent)
{
	const FBox Box(BoxOrigin - BoxExtent, BoxOrigin + BoxExtent);
	return Box.IsInsideOrOn(Point);
}

bool UKismetMathLibrary::IsPointInBoxWithTransform(FVector Point, const FTransform& BoxWorldTransform, FVector BoxExtent)
{
	// Put point in component space
	const FVector PointInComponentSpace = BoxWorldTransform.InverseTransformPosition(Point);
	// Now it's just a normal point-in-box test, with a box at the origin.
	const FBox Box(-BoxExtent, BoxExtent);
	return Box.IsInsideOrOn(PointInComponentSpace);
}

bool UKismetMathLibrary::LinePlaneIntersection(const FVector& LineStart, const FVector& LineEnd, const FPlane& APlane, float& T, FVector& Intersection)
{
	FVector RayDir = LineEnd - LineStart;

	// Check ray is not parallel to plane
	if ((RayDir | APlane) == 0.0f)
	{
		T = -1.0f;
		Intersection = FVector::ZeroVector;
		return false;
	}

	T = ((APlane.W - (LineStart | APlane)) / (RayDir | APlane));

	// Check intersection is not outside line segment
	if (T < 0.0f || T > 1.0f)
	{
		Intersection = FVector::ZeroVector;
		return false;
	}

	// Calculate intersection point
	Intersection = LineStart + RayDir * T;

	return true;
}

bool UKismetMathLibrary::LinePlaneIntersection_OriginNormal(const FVector& LineStart, const FVector& LineEnd, FVector PlaneOrigin, FVector PlaneNormal, float& T, FVector& Intersection)
{
	FVector RayDir = LineEnd - LineStart;

	// Check ray is not parallel to plane
	if ((RayDir | PlaneNormal) == 0.0f)
	{
		T = -1.0f;
		Intersection = FVector::ZeroVector;
		return false;
	}

	T = (((PlaneOrigin - LineStart) | PlaneNormal) / (RayDir | PlaneNormal));

	// Check intersection is not outside line segment
	if (T < 0.0f || T > 1.0f)
	{
		Intersection = FVector::ZeroVector;
		return false;
	}

	// Calculate intersection point
	Intersection = LineStart + RayDir * T;

	return true;
}

void UKismetMathLibrary::BreakRandomStream(const FRandomStream& InRandomStream, int32& InitialSeed)
{
	InitialSeed = InRandomStream.GetInitialSeed();
}

FRandomStream UKismetMathLibrary::MakeRandomStream(int32 InitialSeed)
{
	return FRandomStream(InitialSeed);
}

FVector UKismetMathLibrary::RandomUnitVectorInConeInRadiansFromStream(const FVector& ConeDir, float ConeHalfAngleInRadians, const FRandomStream& Stream)
{
	return Stream.VRandCone(ConeDir, ConeHalfAngleInRadians);
}

FVector UKismetMathLibrary::RandomUnitVectorInEllipticalConeInRadiansFromStream(const FVector& ConeDir, float MaxYawInRadians, float MaxPitchInRadians, const FRandomStream& Stream)
{
	return Stream.VRandCone(ConeDir, MaxYawInRadians, MaxPitchInRadians);
}

#undef LOCTEXT_NAMESPACE
