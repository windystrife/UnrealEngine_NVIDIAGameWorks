// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

// Our own easily changed inline options
#if KISMET_MATH_INLINE_ENABLED
	#define KISMET_MATH_FORCEINLINE		FORCEINLINE_DEBUGGABLE
	#define KISMET_MATH_INLINE			inline
#else
	#define KISMET_MATH_FORCEINLINE		// nothing
	#define KISMET_MATH_INLINE			// nothing
#endif


KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::RandomBool()
{
	return FMath::RandBool();
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::Not_PreBool(bool A)
{
	return !A;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::EqualEqual_BoolBool(bool A, bool B)
{
	return ((!A) == (!B));
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::NotEqual_BoolBool(bool A, bool B)
{
	return ((!A) != (!B));
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::BooleanAND(bool A, bool B)
{
	return A && B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::BooleanNAND(bool A, bool B)
{
	return !(A && B);
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::BooleanOR(bool A, bool B)
{
	return A || B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::BooleanXOR(bool A, bool B)
{
	return A ^ B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::BooleanNOR(bool A, bool B)
{
	return !(A || B);
}

KISMET_MATH_FORCEINLINE
uint8 UKismetMathLibrary::Multiply_ByteByte(uint8 A, uint8 B)
{
	return A * B;
}

KISMET_MATH_FORCEINLINE
uint8 UKismetMathLibrary::Add_ByteByte(uint8 A, uint8 B)
{
	return A + B;
}

KISMET_MATH_FORCEINLINE
uint8 UKismetMathLibrary::Subtract_ByteByte(uint8 A, uint8 B)
{
	return A - B;
}

KISMET_MATH_FORCEINLINE
uint8 UKismetMathLibrary::Divide_ByteByte(uint8 A, uint8 B)
{
	if (B == 0)
	{
		ReportError_Divide_ByteByte();
		return 0;
	}

	return (A / B);
}

KISMET_MATH_FORCEINLINE
uint8 UKismetMathLibrary::Percent_ByteByte(uint8 A, uint8 B)
{
	if (B == 0)
	{
		ReportError_Percent_ByteByte();
		return 0;
	}

	return (A % B);
}

KISMET_MATH_FORCEINLINE
uint8 UKismetMathLibrary::BMin(uint8 A, uint8 B)
{
	return FMath::Min<uint8>(A, B);
}

KISMET_MATH_FORCEINLINE
uint8 UKismetMathLibrary::BMax(uint8 A, uint8 B)
{
	return FMath::Max<uint8>(A, B);
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::Less_ByteByte(uint8 A, uint8 B)
{
	return A < B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::Greater_ByteByte(uint8 A, uint8 B)
{
	return A > B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::LessEqual_ByteByte(uint8 A, uint8 B)
{
	return A <= B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::GreaterEqual_ByteByte(uint8 A, uint8 B)
{
	return A >= B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::EqualEqual_ByteByte(uint8 A, uint8 B)
{
	return A == B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::NotEqual_ByteByte(uint8 A, uint8 B)
{
	return A != B;
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::Multiply_IntInt(int32 A, int32 B)
{
	return A * B;
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::Add_IntInt(int32 A, int32 B)
{
	return A + B;
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::Subtract_IntInt(int32 A, int32 B)
{
	return A - B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::Less_IntInt(int32 A, int32 B)
{
	return A < B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::Greater_IntInt(int32 A, int32 B)
{
	return A > B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::LessEqual_IntInt(int32 A, int32 B)
{
	return A <= B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::GreaterEqual_IntInt(int32 A, int32 B)
{
	return A >= B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::EqualEqual_IntInt(int32 A, int32 B)
{
	return A == B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::NotEqual_IntInt(int32 A, int32 B)
{
	return A != B;
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::And_IntInt(int32 A, int32 B)
{
	return A & B;
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::Xor_IntInt(int32 A, int32 B)
{
	return A ^ B;
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::Or_IntInt(int32 A, int32 B)
{
	return A | B;
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::Not_Int(int32 A)
{
	return ~A;
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::SignOfInteger(int32 A)
{
	return FMath::Sign<int32>(A);
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::RandomInteger(int32 A)
{
	return FMath::RandHelper(A);
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::RandomIntegerInRange(int32 Min, int32 Max)
{
	return FMath::RandRange(Min, Max);
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::Min(int32 A, int32 B)
{
	return FMath::Min(A, B);
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::Max(int32 A, int32 B)
{
	return FMath::Max(A, B);
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::Clamp(int32 V, int32 A, int32 B)
{
	return FMath::Clamp(V, A, B);
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::Abs_Int(int32 A)
{
	return FMath::Abs(A);
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::MultiplyMultiply_FloatFloat(float Base, float Exp)
{
	return FMath::Pow(Base, Exp);
}	

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::Multiply_FloatFloat(float A, float B)
{
	return A * B;
}	

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::Multiply_IntFloat(int32 A, float B)
{
	return A * B;
}	

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::Divide_IntInt(int32 A, int32 B)
{
	if (B == 0)
	{
		ReportError_Divide_IntInt();
		return 0;
	}

	return (A / B);
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::Percent_IntInt(int32 A, int32 B)
{
	if (B == 0)
	{
		ReportError_Percent_IntInt();
		return 0;
	}

	return (A % B);
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::GenericDivide_FloatFloat(float A, float B)
{
	return A / B;
}	

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::Fraction(float A)
{
	return FMath::Fractional(A);
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::Add_FloatFloat(float A, float B)
{
	return A + B;
}	

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::Subtract_FloatFloat(float A, float B)
{
	return A - B;
}	

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::Less_FloatFloat(float A, float B)
{
	return A < B;
}	

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::Greater_FloatFloat(float A, float B)
{
	return A > B;
}	

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::LessEqual_FloatFloat(float A, float B)
{
	return A <= B;
}	

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::GreaterEqual_FloatFloat(float A, float B)
{
	return A >= B;
}	

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::EqualEqual_FloatFloat(float A, float B)
{
	return A == B;
}	

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::NearlyEqual_FloatFloat(float A, float B, float ErrorTolerance)
{
	return FMath::IsNearlyEqual(A, B, ErrorTolerance);
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::NotEqual_FloatFloat(float A, float B)
{
	return A != B;
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::GridSnap_Float(float Location, float GridSize)
{
	return FMath::GridSnap(Location, GridSize);
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::GetPI()
{
	return PI;
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::GetTAU()
{
	return 2.f * PI;
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::DegreesToRadians(float A)
{
	return FMath::DegreesToRadians(A);
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::RadiansToDegrees(float A)
{
	return FMath::RadiansToDegrees(A);
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::Abs(float A)
{
	return FMath::Abs(A);
}	

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::Sin(float A)
{
	return FMath::Sin(A);
}	

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::Asin(float A)
{
	return FMath::Asin(A);
}	

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::Cos(float A)
{
	return FMath::Cos(A);
}	

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::Acos(float A)
{
	return FMath::Acos(A);
}	

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::Tan(float A)
{
	return FMath::Tan(A);
}	

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::Atan(float A)
{
	return FMath::Atan(A);
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::Atan2(float A, float B)
{
	return FMath::Atan2(A, B);
}	

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::DegSin(float A)
{
	return FMath::Sin(PI/(180.f) * A);
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::DegAsin(float A)
{
	return (180.f)/PI * FMath::Asin(A);
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::DegCos(float A)
{
	return FMath::Cos(PI/(180.f) * A);
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::DegAcos(float A)
{
	return (180.f)/PI * FMath::Acos(A);
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::DegTan(float A)
{
	return FMath::Tan(PI/(180.f) * A);
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::DegAtan(float A)
{
	return (180.f)/PI * FMath::Atan(A);
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::DegAtan2(float A, float B)
{
	return (180.f)/PI * FMath::Atan2(A, B);
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::ClampAngle(float AngleDegrees, float MinAngleDegrees, float MaxAngleDegrees)
{
	return FMath::ClampAngle(AngleDegrees, MinAngleDegrees, MaxAngleDegrees);
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::Exp(float A)
{
	return FMath::Exp(A);
}	

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::Loge(float A)
{
	return FMath::Loge(A);
}	

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::Sqrt(float A)
{
	if (A >= 0.f)
	{
		return FMath::Sqrt(A);
	}
	else
	{
		ReportError_Sqrt();
		return 0.f;
	}
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::Square(float A)
{
	return FMath::Square(A);
}	

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::Round(float A)
{
	return FMath::RoundToInt(A);
}	

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::FFloor(float A)
{
	return FMath::FloorToInt(A);
}	

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::FTrunc(float A)
{
	return FMath::TruncToInt(A);
}	

KISMET_MATH_FORCEINLINE
FIntVector UKismetMathLibrary::FTruncVector(const FVector& InVector)
{
	return FIntVector(FMath::TruncToInt(InVector.X),
		FMath::TruncToInt(InVector.Y),
		FMath::TruncToInt(InVector.Z));
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::FCeil(float A)
{
	return FMath::CeilToInt(A);
}	

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::SignOfFloat(float A)
{
	return FMath::Sign<float>(A);
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::MultiplyByPi(float Value)
{
	return Value * PI;
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::FixedTurn(float InCurrent, float InDesired, float InDeltaRate)
{
	return FMath::FixedTurn(InCurrent, InDesired, InDeltaRate);
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::RandomFloat()
{
	return FMath::FRand();
}	

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::RandomFloatInRange(float Min, float Max)
{
	return FMath::FRandRange(Min, Max);
}	

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::FMin(float A, float B)
{
	return FMath::Min(A, B);
}	

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::FMax(float A, float B)
{
	return FMath::Max(A, B);
}	

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::FClamp(float V, float A, float B)
{
	return FMath::Clamp(V, A, B);
}	

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::Lerp(float A, float B, float V)
{
	return A + V*(B-A);
}	

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::FInterpTo(float Current, float Target, float DeltaTime, float InterpSpeed)
{
	return FMath::FInterpTo(Current, Target, DeltaTime, InterpSpeed);
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::FInterpTo_Constant(float Current, float Target, float DeltaTime, float InterpSpeed)
{
	return FMath::FInterpConstantTo(Current, Target, DeltaTime, InterpSpeed);
}

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::Multiply_VectorFloat(FVector A, float B)
{
	return A * B;
}

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::Multiply_VectorInt(FVector A, int32 B)
{
	return A * (float)B;
}

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::Multiply_VectorVector(FVector A, FVector B)
{
	return A * B;
}	

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::Divide_VectorFloat(FVector A, float B)
{
	if (B == 0.f)
	{
		ReportError_Divide_VectorFloat();
		return FVector::ZeroVector;
	}

	return A / B;
}

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::Divide_VectorInt(FVector A, int32 B)
{
	if (B == 0)
	{
		ReportError_Divide_VectorInt();
		return FVector::ZeroVector;
	}

	return A / (float)B;
}

KISMET_MATH_INLINE
FVector UKismetMathLibrary::Divide_VectorVector(FVector A, FVector B)
{
	if (B.X == 0.f || B.Y == 0.f || B.Z == 0.f)
	{
		ReportError_Divide_VectorVector();
		return FVector::ZeroVector;
	}

	return A / B;
}

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::Add_VectorVector(FVector A, FVector B)
{
	return A + B;
}	

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::Add_VectorFloat(FVector A, float B)
{
	return A + B;
}	

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::Add_VectorInt(FVector A, int32 B)
{
	return A + (float)B;
}

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::Subtract_VectorVector(FVector A, FVector B)
{
	return A - B;
}	

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::Subtract_VectorFloat(FVector A, float B)
{
	return A - B;
}

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::Subtract_VectorInt(FVector A, int32 B)
{
	return A - (float)B;
}

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::LessLess_VectorRotator(FVector A, FRotator B)
{
	return B.UnrotateVector(A);
}	

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::GreaterGreater_VectorRotator(FVector A, FRotator B)
{
	return B.RotateVector(A);
}	

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::EqualEqual_VectorVector(FVector A, FVector B, float ErrorTolerance)
{
	return A.Equals(B, ErrorTolerance);
}	

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::NotEqual_VectorVector(FVector A, FVector B, float ErrorTolerance)
{
	return !A.Equals(B, ErrorTolerance);
}	

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::Dot_VectorVector(FVector A, FVector B)
{
	return FVector::DotProduct(A, B);
}	

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::Cross_VectorVector(FVector A, FVector B)
{
	return FVector::CrossProduct(A, B);
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::DotProduct2D(FVector2D A, FVector2D B)
{
	return FVector2D::DotProduct(A, B);
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::CrossProduct2D(FVector2D A, FVector2D B)
{
	return FVector2D::CrossProduct(A, B);
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::VSize(FVector A)
{
	return A.Size();
}	

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::VSize2D(FVector2D A)
{
	return A.Size();
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::VSizeSquared(FVector A)
{
	return A.SizeSquared();
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::VSize2DSquared(FVector2D A)
{
	return A.SizeSquared();
}

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::Normal(FVector A)
{
	return A.GetSafeNormal();
}

KISMET_MATH_FORCEINLINE
FVector2D UKismetMathLibrary::Normal2D(FVector2D A)
{
	return A.GetSafeNormal();
}

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::VLerp(FVector A, FVector B, float V)
{
	return A + V*(B-A);
}	

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::VInterpTo(FVector Current, FVector Target, float DeltaTime, float InterpSpeed)
{
	return FMath::VInterpTo( Current, Target, DeltaTime, InterpSpeed );
}

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::VInterpTo_Constant(FVector Current, FVector Target, float DeltaTime, float InterpSpeed)
{
	return FMath::VInterpConstantTo(Current, Target, DeltaTime, InterpSpeed);
}

KISMET_MATH_FORCEINLINE
FVector2D UKismetMathLibrary::Vector2DInterpTo(FVector2D Current, FVector2D Target, float DeltaTime, float InterpSpeed)
{
	return FMath::Vector2DInterpTo( Current, Target, DeltaTime, InterpSpeed );
}

KISMET_MATH_FORCEINLINE
FVector2D UKismetMathLibrary::Vector2DInterpTo_Constant(FVector2D Current, FVector2D Target, float DeltaTime, float InterpSpeed)
{
	return FMath::Vector2DInterpConstantTo( Current, Target, DeltaTime, InterpSpeed );
}

KISMET_MATH_INLINE
FVector UKismetMathLibrary::RandomPointInBoundingBox(const FVector& Origin, const FVector& BoxExtent)
{
	const FVector BoxMin = Origin - BoxExtent;
	const FVector BoxMax = Origin + BoxExtent;
	return FMath::RandPointInBox(FBox(BoxMin, BoxMax));
}

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::RandomUnitVectorInConeInRadians(FVector ConeDir, float ConeHalfAngleInRadians)
{
	return FMath::VRandCone(ConeDir, ConeHalfAngleInRadians);
}

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::MirrorVectorByNormal(FVector A, FVector B)
{
	return FMath::GetReflectionVector(A, B);
}

KISMET_MATH_INLINE
FVector UKismetMathLibrary::ProjectVectorOnToVector(FVector V, FVector Target)
{
	if (Target.SizeSquared() > SMALL_NUMBER)
	{
		return V.ProjectOnTo(Target);
	}
	else
	{
		ReportError_ProjectVectorOnToVector();
		return FVector::ZeroVector;
	}
}

KISMET_MATH_FORCEINLINE
void UKismetMathLibrary::FindNearestPointsOnLineSegments(FVector Segment1Start, FVector Segment1End, FVector Segment2Start, FVector Segment2End, FVector& Segment1Point, FVector& Segment2Point)
{
	FMath::SegmentDistToSegmentSafe(Segment1Start, Segment1End, Segment2Start, Segment2End, Segment1Point, Segment2Point);
}

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::FindClosestPointOnSegment(FVector Point, FVector SegmentStart, FVector SegmentEnd)
{
	return FMath::ClosestPointOnSegment(Point, SegmentStart, SegmentEnd);
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::GetPointDistanceToSegment(FVector Point, FVector SegmentStart, FVector SegmentEnd)
{
	return FMath::PointDistToSegment(Point, SegmentStart, SegmentEnd);
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::GetPointDistanceToLine(FVector Point, FVector LineOrigin, FVector LineDirection)
{
	return FMath::PointDistToLine(Point, LineDirection, LineOrigin);
}

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::ProjectPointOnToPlane(FVector Point, FVector PlaneBase, FVector PlaneNormal)
{
	return FVector::PointPlaneProject(Point, PlaneBase, PlaneNormal);
}

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::ProjectVectorOnToPlane(FVector V, FVector PlaneNormal)
{
	return FVector::VectorPlaneProject(V, PlaneNormal);
}

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::NegateVector(FVector A)
{
	return -A;
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::GetMinElement(FVector A)
{
	return A.GetMin();
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::GetMaxElement(FVector A)
{
	return A.GetMax();
}

KISMET_MATH_INLINE
FVector UKismetMathLibrary::GetDirectionUnitVector(FVector From, FVector To)
{
	return (To - From).GetSafeNormal();
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::EqualEqual_RotatorRotator(FRotator A, FRotator B, float ErrorTolerance)
{
	return A.Equals(B, ErrorTolerance);
}	

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::NotEqual_RotatorRotator(FRotator A, FRotator B, float ErrorTolerance)
{
	return !A.Equals(B, ErrorTolerance);
}

KISMET_MATH_FORCEINLINE
FRotator UKismetMathLibrary::Multiply_RotatorFloat(FRotator A, float B)
{
	return A * B;
}

KISMET_MATH_FORCEINLINE
FRotator UKismetMathLibrary::Multiply_RotatorInt(FRotator A, int32 B)
{
	return A * (float)B;
}	

KISMET_MATH_FORCEINLINE
FRotator UKismetMathLibrary::NegateRotator( FRotator A )
{
	return A.GetInverse();
}

KISMET_MATH_FORCEINLINE
FRotator UKismetMathLibrary::RInterpTo(FRotator Current, FRotator Target, float DeltaTime, float InterpSpeed)
{
	return FMath::RInterpTo( Current, Target, DeltaTime, InterpSpeed);
}

KISMET_MATH_FORCEINLINE
FRotator UKismetMathLibrary::RInterpTo_Constant(FRotator Current, FRotator Target, float DeltaTime, float InterpSpeed)
{
	return FMath::RInterpConstantTo(Current, Target, DeltaTime, InterpSpeed);
}

KISMET_MATH_FORCEINLINE
FLinearColor UKismetMathLibrary::CInterpTo(FLinearColor Current, FLinearColor Target, float DeltaTime, float InterpSpeed)
{
	return FMath::CInterpTo(Current, Target, DeltaTime, InterpSpeed);
}

KISMET_MATH_INLINE
FLinearColor UKismetMathLibrary::LinearColorLerp(FLinearColor A, FLinearColor B, float Alpha)
{
	return A + Alpha * (B - A);
}

KISMET_MATH_FORCEINLINE
FLinearColor UKismetMathLibrary::LinearColorLerpUsingHSV(FLinearColor A, FLinearColor B, float Alpha)
{
	return FLinearColor::LerpUsingHSV( A, B, Alpha );
}

KISMET_MATH_FORCEINLINE
FLinearColor UKismetMathLibrary::Multiply_LinearColorLinearColor(FLinearColor A, FLinearColor B)
{
	return A * B;
}

KISMET_MATH_FORCEINLINE
FLinearColor UKismetMathLibrary::Multiply_LinearColorFloat(FLinearColor A, float B)
{
	return A * B;
}

KISMET_MATH_INLINE
FVector UKismetMathLibrary::TransformLocation(const FTransform& T, FVector Location)
{
	return T.TransformPosition(Location);
}

KISMET_MATH_INLINE
FVector UKismetMathLibrary::TransformDirection(const FTransform& T, FVector Direction)
{
	return T.TransformVectorNoScale(Direction);
}

KISMET_MATH_INLINE
FVector UKismetMathLibrary::InverseTransformLocation(const FTransform& T, FVector Location)
{
	return T.InverseTransformPosition(Location);
}

KISMET_MATH_INLINE
FVector UKismetMathLibrary::InverseTransformDirection(const FTransform& T, FVector Direction)
{
	return T.InverseTransformVectorNoScale(Direction);
}

KISMET_MATH_INLINE
FTransform UKismetMathLibrary::ComposeTransforms(const FTransform& A, const FTransform& B)
{
	return A * B;
}

KISMET_MATH_INLINE
FTransform UKismetMathLibrary::ConvertTransformToRelative(const FTransform& Transform, const FTransform& ParentTransform)
{
	return ParentTransform.GetRelativeTransform(Transform);
}

KISMET_MATH_INLINE
FTransform UKismetMathLibrary::InvertTransform(const FTransform& T)
{
	return T.Inverse();
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::EqualEqual_TransformTransform(const FTransform& A, const FTransform& B)
{
	return NearlyEqual_TransformTransform(A, B);
}

KISMET_MATH_FORCEINLINE
FVector2D UKismetMathLibrary::Add_Vector2DVector2D(FVector2D A, FVector2D B)
{
	return A + B;
}

KISMET_MATH_FORCEINLINE
FVector2D UKismetMathLibrary::Subtract_Vector2DVector2D(FVector2D A, FVector2D B)
{
	return A - B;
}

KISMET_MATH_FORCEINLINE
FVector2D UKismetMathLibrary::Multiply_Vector2DFloat(FVector2D A, float B)
{
	return A * B;
}

KISMET_MATH_FORCEINLINE
FVector2D UKismetMathLibrary::Multiply_Vector2DVector2D(FVector2D A, FVector2D B)
{
	return A * B;
}

KISMET_MATH_FORCEINLINE
FVector2D UKismetMathLibrary::Divide_Vector2DFloat(FVector2D A, float B)
{
	if (B == 0.f)
	{
		ReportError_Divide_Vector2DFloat();
		return FVector2D::ZeroVector;
	}

	return A / B;
}

KISMET_MATH_FORCEINLINE
FVector2D UKismetMathLibrary::Divide_Vector2DVector2D(FVector2D A, FVector2D B)
{
	if (B.X == 0.f || B.Y == 0.f)
	{
		ReportError_Divide_Vector2DVector2D();
		return FVector2D::ZeroVector;
	}

	return A / B;
}


KISMET_MATH_FORCEINLINE
FVector2D UKismetMathLibrary::Add_Vector2DFloat(FVector2D A, float B)
{
	return A+B;
}

KISMET_MATH_FORCEINLINE
FVector2D UKismetMathLibrary::Subtract_Vector2DFloat(FVector2D A, float B)
{
	return A-B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::EqualEqual_Vector2DVector2D(FVector2D A, FVector2D B, float ErrorTolerance)
{ 
	return A.Equals(B,ErrorTolerance);
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::NotEqual_Vector2DVector2D(FVector2D A, FVector2D B, float ErrorTolerance)
{
	return !A.Equals(B,ErrorTolerance);
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::EqualEqual_NameName(FName A, FName B)
{
	return A == B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::NotEqual_NameName(FName A, FName B)
{
	return A != B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::EqualEqual_ObjectObject(class UObject* A, class UObject* B)
{
	return A == B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::NotEqual_ObjectObject(class UObject* A, class UObject* B)
{
	return A != B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::EqualEqual_ClassClass(class UClass* A, class UClass* B)
{
	return A == B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::NotEqual_ClassClass(class UClass* A, class UClass* B)
{
	return A != B;
}

/* DateTime functions
*****************************************************************************/

KISMET_MATH_FORCEINLINE
FDateTime UKismetMathLibrary::Add_DateTimeTimespan( FDateTime A, FTimespan B )
{
	return A + B;
}

KISMET_MATH_FORCEINLINE
FDateTime UKismetMathLibrary::Subtract_DateTimeTimespan( FDateTime A, FTimespan B )
{
	return A - B;
}

KISMET_MATH_FORCEINLINE
FTimespan UKismetMathLibrary::Subtract_DateTimeDateTime(FDateTime A, FDateTime B)
{
	return A - B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::EqualEqual_DateTimeDateTime( FDateTime A, FDateTime B )
{
	return A == B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::NotEqual_DateTimeDateTime( FDateTime A, FDateTime B )
{
	return A != B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::Greater_DateTimeDateTime( FDateTime A, FDateTime B )
{
	return A > B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::GreaterEqual_DateTimeDateTime( FDateTime A, FDateTime B )
{
	return A >= B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::Less_DateTimeDateTime( FDateTime A, FDateTime B )
{
	return A < B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::LessEqual_DateTimeDateTime( FDateTime A, FDateTime B )
{
	return A <= B;
}

KISMET_MATH_FORCEINLINE
FDateTime UKismetMathLibrary::GetDate( FDateTime A )
{
	return A.GetDate();
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::GetDay( FDateTime A )
{
	return A.GetDay();
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::GetDayOfYear( FDateTime A )
{
	return A.GetDayOfYear();
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::GetHour( FDateTime A )
{
	return A.GetHour();
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::GetHour12( FDateTime A )
{
	return A.GetHour12();
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::GetMillisecond( FDateTime A )
{
	return A.GetMillisecond();
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::GetMinute( FDateTime A )
{
	return A.GetMinute();
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::GetMonth( FDateTime A )
{
	return A.GetMonth();
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::GetSecond( FDateTime A )
{
	return A.GetSecond();
}

KISMET_MATH_FORCEINLINE
FTimespan UKismetMathLibrary::GetTimeOfDay( FDateTime A )
{
	return A.GetTimeOfDay();
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::GetYear( FDateTime A )
{
	return A.GetYear();
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::IsAfternoon( FDateTime A )
{
	return A.IsAfternoon();
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::IsMorning( FDateTime A )
{
	return A.IsMorning();
}

KISMET_MATH_INLINE
int32 UKismetMathLibrary::DaysInMonth(int32 Year, int32 Month)
{
	if ((Month < 1) || (Month > 12))
	{
		ReportError_DaysInMonth();
		return 0;
	}

	return FDateTime::DaysInMonth(Year, Month);
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::DaysInYear( int32 Year )
{
	return FDateTime::DaysInYear(Year);
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::IsLeapYear( int32 Year )
{
	return FDateTime::IsLeapYear(Year);
}

KISMET_MATH_FORCEINLINE
FDateTime UKismetMathLibrary::DateTimeMaxValue( )
{
	return FDateTime::MaxValue();
}

KISMET_MATH_FORCEINLINE
FDateTime UKismetMathLibrary::DateTimeMinValue( )
{
	return FDateTime::MinValue();
}

KISMET_MATH_FORCEINLINE
FDateTime UKismetMathLibrary::Now( )
{
	return FDateTime::Now();
}

KISMET_MATH_FORCEINLINE
FDateTime UKismetMathLibrary::Today( )
{
	return FDateTime::Today();
}

KISMET_MATH_FORCEINLINE
FDateTime UKismetMathLibrary::UtcNow( )
{
	return FDateTime::UtcNow();
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::DateTimeFromIsoString(FString IsoString, FDateTime& Result)
{
	return FDateTime::ParseIso8601(*IsoString, Result);
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::DateTimeFromString(FString DateTimeString, FDateTime& Result)
{
	return FDateTime::Parse(DateTimeString, Result);
}

/* Timespan functions
*****************************************************************************/

KISMET_MATH_FORCEINLINE
FTimespan UKismetMathLibrary::Add_TimespanTimespan( FTimespan A, FTimespan B )
{
	return A + B;
}

KISMET_MATH_FORCEINLINE
FTimespan UKismetMathLibrary::Subtract_TimespanTimespan( FTimespan A, FTimespan B )
{
	return A - B;
}

KISMET_MATH_FORCEINLINE
FTimespan UKismetMathLibrary::Multiply_TimespanFloat( FTimespan A, float Scalar )
{
	return A * Scalar;
}

KISMET_MATH_FORCEINLINE
FTimespan UKismetMathLibrary::Divide_TimespanFloat(FTimespan A, float Scalar)
{
	return A / Scalar;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::EqualEqual_TimespanTimespan( FTimespan A, FTimespan B )
{
	return A == B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::NotEqual_TimespanTimespan( FTimespan A, FTimespan B )
{
	return A != B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::Greater_TimespanTimespan( FTimespan A, FTimespan B )
{
	return A > B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::GreaterEqual_TimespanTimespan( FTimespan A, FTimespan B )
{
	return A >= B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::Less_TimespanTimespan( FTimespan A, FTimespan B )
{
	return A < B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::LessEqual_TimespanTimespan( FTimespan A, FTimespan B )
{
	return A <= B;
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::GetDays( FTimespan A )
{
	return A.GetDays();
}

KISMET_MATH_FORCEINLINE
FTimespan UKismetMathLibrary::GetDuration( FTimespan A )
{
	return A.GetDuration();
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::GetHours( FTimespan A )
{
	return A.GetHours();
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::GetMilliseconds( FTimespan A )
{
	return A.GetFractionMilli();
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::GetMinutes( FTimespan A )
{
	return A.GetMinutes();
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::GetSeconds( FTimespan A )
{
	return A.GetSeconds();
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::GetTotalDays( FTimespan A )
{
	return A.GetTotalDays();
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::GetTotalHours( FTimespan A )
{
	return A.GetTotalHours();
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::GetTotalMilliseconds( FTimespan A )
{
	return A.GetTotalMilliseconds();
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::GetTotalMinutes( FTimespan A )
{
	return A.GetTotalMinutes();
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::GetTotalSeconds( FTimespan A )
{
	return A.GetTotalSeconds();
}

KISMET_MATH_FORCEINLINE
FTimespan UKismetMathLibrary::TimespanMaxValue( )
{
	return FTimespan::MaxValue();
}

KISMET_MATH_FORCEINLINE
FTimespan UKismetMathLibrary::TimespanMinValue( )
{
	return FTimespan::MinValue();
}

KISMET_MATH_INLINE
float UKismetMathLibrary::TimespanRatio( FTimespan A, FTimespan B )
{
	return FTimespan::Ratio(A, B);
}

KISMET_MATH_FORCEINLINE
FTimespan UKismetMathLibrary::TimespanZeroValue( )
{
	return FTimespan::Zero();
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::TimespanFromString(FString TimespanString, FTimespan& Result)
{
	return FTimespan::Parse(TimespanString, Result);
}


/* K2 Utilities
*****************************************************************************/

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::Conv_ByteToFloat(uint8 InByte)
{
	return (float)InByte;
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::Conv_IntToFloat(int32 InInt)
{
	return (float)InInt;
}

KISMET_MATH_FORCEINLINE
uint8 UKismetMathLibrary::Conv_IntToByte(int32 InInt)
{
	return (uint8)InInt;
}

KISMET_MATH_FORCEINLINE
FIntVector UKismetMathLibrary::Conv_IntToIntVector(int32 InInt)
{
	return FIntVector(InInt, InInt, InInt);
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::Conv_IntToBool(int32 InInt)
{
	return InInt == 0 ? false : true;
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::Conv_BoolToInt(bool InBool)
{
	return InBool ? 1 : 0;
}

KISMET_MATH_FORCEINLINE
uint8 UKismetMathLibrary::Conv_BoolToByte(bool InBool)
{
	return InBool ? 1 : 0;
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::Conv_BoolToFloat(bool InBool)
{
	return InBool ? 1.0f : 0.0f;
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::Conv_ByteToInt(uint8 InByte)
{
	return (int32)InByte;
}

KISMET_MATH_FORCEINLINE
FRotator UKismetMathLibrary::Conv_VectorToRotator(FVector InVec)
{
	return InVec.Rotation();
}

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::Conv_RotatorToVector(FRotator InRot)
{
	return InRot.Vector();
}

KISMET_MATH_FORCEINLINE
FLinearColor UKismetMathLibrary::Conv_VectorToLinearColor(FVector InVec)
{
	return FLinearColor(InVec);	
}

KISMET_MATH_FORCEINLINE
FVector2D UKismetMathLibrary::Conv_VectorToVector2D(FVector InVec)
{
	return FVector2D(InVec);
}

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::Conv_Vector2DToVector(FVector2D InVec2D, float Z)
{
	return FVector(InVec2D, Z);
}

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::Conv_IntVectorToVector(const FIntVector& InIntVector)
{
	return FVector(InIntVector);
}

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::Conv_LinearColorToVector(FLinearColor InLinearColor)
{
	return FVector(InLinearColor);
}

KISMET_MATH_FORCEINLINE
FLinearColor UKismetMathLibrary::Conv_ColorToLinearColor(FColor InColor)
{
	return FLinearColor(InColor);
}

KISMET_MATH_FORCEINLINE
FColor UKismetMathLibrary::Conv_LinearColorToColor(FLinearColor InLinearColor)
{
	return InLinearColor.ToFColor(true);
}

KISMET_MATH_FORCEINLINE
FTransform UKismetMathLibrary::Conv_VectorToTransform(FVector InTranslation)
{
	return FTransform(InTranslation);
}

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::Conv_FloatToVector(float InFloat)
{
	return FVector(InFloat);
}

KISMET_MATH_FORCEINLINE
FLinearColor UKismetMathLibrary::Conv_FloatToLinearColor(float InFloat)
{
	return FLinearColor(InFloat, InFloat, InFloat);
}

KISMET_MATH_FORCEINLINE
FBox UKismetMathLibrary::MakeBox(FVector Min, FVector Max)
{
	return FBox(Min, Max);
}

KISMET_MATH_FORCEINLINE
FBox2D UKismetMathLibrary::MakeBox2D(FVector2D Min, FVector2D Max)
{
	return FBox2D(Min, Max);
}

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::MakeVector(float X, float Y, float Z)
{
	return FVector(X,Y,Z);
}

KISMET_MATH_FORCEINLINE
void UKismetMathLibrary::BreakVector(FVector InVec, float& X, float& Y, float& Z)
{
	X = InVec.X;
	Y = InVec.Y;
	Z = InVec.Z;
}

KISMET_MATH_FORCEINLINE
FVector2D UKismetMathLibrary::MakeVector2D(float X, float Y)
{
	return FVector2D(X, Y);
}

KISMET_MATH_FORCEINLINE
void UKismetMathLibrary::BreakVector2D(FVector2D InVec, float& X, float& Y)
{
	X = InVec.X;
	Y = InVec.Y;
}

KISMET_MATH_FORCEINLINE
FRotator UKismetMathLibrary::MakeRotator(float Roll, float Pitch, float Yaw)
{
	return FRotator(Pitch,Yaw,Roll);
}

KISMET_MATH_FORCEINLINE
FRotator UKismetMathLibrary::FindLookAtRotation(const FVector& Start, const FVector& Target)
{
	return MakeRotFromX(Target - Start);
}

KISMET_MATH_FORCEINLINE
FRotator UKismetMathLibrary::MakeRotFromX(const FVector& X)
{
	return FRotationMatrix::MakeFromX(X).Rotator();
}

KISMET_MATH_FORCEINLINE
FRotator UKismetMathLibrary::MakeRotFromY(const FVector& Y)
{
	return FRotationMatrix::MakeFromY(Y).Rotator();
}

KISMET_MATH_FORCEINLINE
FRotator UKismetMathLibrary::MakeRotFromZ(const FVector& Z)
{
	return FRotationMatrix::MakeFromZ(Z).Rotator();
}

KISMET_MATH_FORCEINLINE
FRotator UKismetMathLibrary::MakeRotFromXY(const FVector& X, const FVector& Y)
{
	return FRotationMatrix::MakeFromXY(X, Y).Rotator();
}

KISMET_MATH_FORCEINLINE
FRotator UKismetMathLibrary::MakeRotFromXZ(const FVector& X, const FVector& Z)
{
	return FRotationMatrix::MakeFromXZ(X, Z).Rotator();
}

KISMET_MATH_FORCEINLINE
FRotator UKismetMathLibrary::MakeRotFromYX(const FVector& Y, const FVector& X)
{
	return FRotationMatrix::MakeFromYX(Y, X).Rotator();
}

KISMET_MATH_FORCEINLINE
FRotator UKismetMathLibrary::MakeRotFromYZ(const FVector& Y, const FVector& Z)
{
	return FRotationMatrix::MakeFromYZ(Y, Z).Rotator();
}

KISMET_MATH_FORCEINLINE
FRotator UKismetMathLibrary::MakeRotFromZX(const FVector& Z, const FVector& X)
{
	return FRotationMatrix::MakeFromZX(Z, X).Rotator();
}

KISMET_MATH_FORCEINLINE
FRotator UKismetMathLibrary::MakeRotFromZY(const FVector& Z, const FVector& Y)
{
	return FRotationMatrix::MakeFromZY(Z, Y).Rotator();
}

KISMET_MATH_FORCEINLINE
void UKismetMathLibrary::BreakRotator(FRotator InRot, float& Roll, float& Pitch, float& Yaw)
{
	Pitch = InRot.Pitch;
	Yaw = InRot.Yaw;
	Roll = InRot.Roll;
}

KISMET_MATH_FORCEINLINE
FTransform UKismetMathLibrary::MakeTransform(FVector Translation, FRotator Rotation, FVector Scale)
{
	return FTransform(Rotation,Translation,Scale);
}

KISMET_MATH_FORCEINLINE
void UKismetMathLibrary::BreakTransform(const FTransform& InTransform, FVector& Translation, FRotator& Rotation, FVector& Scale)
{
	Translation = InTransform.GetLocation();
	Rotation = InTransform.Rotator();
	Scale = InTransform.GetScale3D();
}

KISMET_MATH_FORCEINLINE
FLinearColor UKismetMathLibrary::MakeColor(float R, float G, float B, float A)
{
	return FLinearColor(R,G,B,A);
}

KISMET_MATH_FORCEINLINE
void UKismetMathLibrary::BreakColor(const FLinearColor InColor, float& R, float& G, float& B, float& A)
{
	R = InColor.R;
	G = InColor.G;
	B = InColor.B;
	A = InColor.A;
}

KISMET_MATH_FORCEINLINE
FLinearColor UKismetMathLibrary::HSVToRGB(float H, float S, float V, float A)
{
	const FLinearColor HSV(H, S, V, A);
	return HSV.HSVToLinearRGB();
}

KISMET_MATH_FORCEINLINE
void UKismetMathLibrary::RGBToHSV(const FLinearColor InColor, float& H, float& S, float& V, float& A)
{
	const FLinearColor HSV(InColor.LinearRGBToHSV());
	H = HSV.R;
	S = HSV.G;
	V = HSV.B;
	A = HSV.A;
}

KISMET_MATH_FORCEINLINE
void UKismetMathLibrary::HSVToRGB_Vector(const FLinearColor HSV, FLinearColor& RGB)
{
	RGB = HSV.HSVToLinearRGB();
}

KISMET_MATH_FORCEINLINE
void UKismetMathLibrary::RGBToHSV_Vector(const FLinearColor RGB, FLinearColor& HSV)
{
	HSV = RGB.LinearRGBToHSV();
}

KISMET_MATH_FORCEINLINE
FString UKismetMathLibrary::SelectString(const FString& A, const FString& B, bool bSelectA)
{
	return bSelectA ? A : B;
}

KISMET_MATH_FORCEINLINE
int32 UKismetMathLibrary::SelectInt(int32 A, int32 B, bool bSelectA)
{
	return bSelectA ? A : B;
}

KISMET_MATH_FORCEINLINE
float UKismetMathLibrary::SelectFloat(float A, float B, bool bSelectA)
{
	return bSelectA ? A : B;
}

KISMET_MATH_FORCEINLINE
FVector UKismetMathLibrary::SelectVector(FVector A, FVector B, bool bSelectA)
{
	return bSelectA ? A : B;
}

KISMET_MATH_FORCEINLINE
FRotator UKismetMathLibrary::SelectRotator(FRotator A, FRotator B, bool bSelectA)
{
	return bSelectA ? A : B;
}

KISMET_MATH_FORCEINLINE
FLinearColor UKismetMathLibrary::SelectColor(FLinearColor A, FLinearColor B, bool bSelectA)
{
	return bSelectA ? A : B;
}

KISMET_MATH_FORCEINLINE
FTransform UKismetMathLibrary::SelectTransform(const FTransform& A, const FTransform& B, bool bSelectA)
{
	return bSelectA ? A : B;
}

KISMET_MATH_FORCEINLINE
UObject* UKismetMathLibrary::SelectObject(UObject* A, UObject* B, bool bSelectA)
{
	return bSelectA ? A : B;
}

KISMET_MATH_FORCEINLINE
UClass* UKismetMathLibrary::SelectClass(UClass* A, UClass* B, bool bSelectA)
{
	return bSelectA ? A : B;
}

KISMET_MATH_FORCEINLINE
bool UKismetMathLibrary::PointsAreCoplanar(const TArray<FVector>& Points, float Tolerance)
{
	return FMath::PointsAreCoplanar(Points, Tolerance);
}
