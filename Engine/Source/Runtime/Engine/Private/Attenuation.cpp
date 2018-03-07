// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/Attenuation.h"

float FBaseAttenuationSettings::GetMaxDimension() const
{
	float MaxDimension = FalloffDistance;

	switch(AttenuationShape)
	{
	case EAttenuationShape::Sphere:
	case EAttenuationShape::Cone:

		MaxDimension += AttenuationShapeExtents.X;
		break;

	case EAttenuationShape::Box:

		MaxDimension += FMath::Max3(AttenuationShapeExtents.X, AttenuationShapeExtents.Y, AttenuationShapeExtents.Z);
		break;

	case EAttenuationShape::Capsule:
		
		MaxDimension += FMath::Max(AttenuationShapeExtents.X, AttenuationShapeExtents.Y);
		break;

	default:
		check(false);
	}

	return MaxDimension;
}

float FBaseAttenuationSettings::Evaluate(const FTransform& Origin, const FVector Location, const float DistanceScale) const
{
	float AttenuationMultiplier = 1.f;

	switch (AttenuationShape)
	{
	case EAttenuationShape::Sphere:
	{
		const float Distance = FMath::Max(FVector::Dist( Origin.GetTranslation(), Location ) - AttenuationShapeExtents.X, 0.f);
		AttenuationMultiplier = AttenuationEval(Distance, FalloffDistance, DistanceScale);
		break;
	}

	case EAttenuationShape::Box:
		AttenuationMultiplier = AttenuationEvalBox(Origin, Location, DistanceScale);
		break;

	case EAttenuationShape::Capsule:
		AttenuationMultiplier = AttenuationEvalCapsule(Origin, Location, DistanceScale);
		break;

	case EAttenuationShape::Cone:
		AttenuationMultiplier = AttenuationEvalCone(Origin, Location, DistanceScale);
		break;

	default:
		check(false);
	}

	return AttenuationMultiplier;
}

float FBaseAttenuationSettings::AttenuationEval(const float Distance, const float Falloff, const float DistanceScale) const
{
	// Clamp the input distance between 0.0f and Falloff. If the Distance
	// is actually less than the min value, it will use the min-value of the algorithm/curve
	// rather than assume it's 1.0 (i.e. it could be 0.0 for an inverse curve). Similarly, if the distance
	// is greater than the falloff value, it'll use the algorithm/curve value evaluated at Falloff distance,
	// which could be 1.0 (and not 0.0f).

	const float FalloffCopy = FMath::Max(Falloff, 1.0f);
	const float DistanceCopy = FMath::Clamp(Distance, 0.0f, FalloffCopy) * DistanceScale;

	float Result = 0.0f;
	switch(DistanceAlgorithm)
	{
	case EAttenuationDistanceModel::Linear:

		Result = (1.0f - (DistanceCopy / FalloffCopy));
		break;

	case EAttenuationDistanceModel::Logarithmic:

		Result = 0.5f * - FMath::Loge(DistanceCopy / FalloffCopy);
		break;

	case EAttenuationDistanceModel::Inverse:

		Result = 0.02f / (DistanceCopy / FalloffCopy);
		break;

	case EAttenuationDistanceModel::LogReverse:

		Result = 1.0f + 0.5f * FMath::Loge(1.0f - (DistanceCopy / FalloffCopy));
		break;

	case EAttenuationDistanceModel::NaturalSound:
	{
		check( dBAttenuationAtMax <= 0.0f );
		Result = FMath::Pow(10.0f, ((DistanceCopy / FalloffCopy) * dBAttenuationAtMax) / 20.0f);
		break;
	}

	case EAttenuationDistanceModel::Custom:

		Result = CustomAttenuationCurve.GetRichCurveConst()->Eval(DistanceCopy / FalloffCopy);
		break;

	default:
		checkf(false, TEXT("Uknown attenuation distance algorithm!"))
		break;
	}

	// Make sure the output is clamped between 0.0 and 1.0f. Some of the algorithms above can
	// result in bad values at the edges.
	return FMath::Clamp(Result, 0.0f, 1.0f);
}

float FBaseAttenuationSettings::AttenuationEvalBox(const FTransform& Origin, const FVector Location, const float DistanceScale) const
{
	const float DistanceSq = ComputeSquaredDistanceFromBoxToPoint(-AttenuationShapeExtents, AttenuationShapeExtents, Origin.InverseTransformPositionNoScale(Location));
	if (DistanceSq < FalloffDistance * FalloffDistance)
	{ 
		return AttenuationEval(FMath::Sqrt(DistanceSq), FalloffDistance, DistanceScale);
	}

	return 0.f;
}

float FBaseAttenuationSettings::AttenuationEvalCapsule(const FTransform& Origin, const FVector Location, const float DistanceScale) const
{
	float Distance = 0.f;
	const float CapsuleHalfHeight = AttenuationShapeExtents.X;
	const float CapsuleRadius = AttenuationShapeExtents.Y;

	// Capsule devolves to a sphere if HalfHeight <= Radius
	if (CapsuleHalfHeight <= CapsuleRadius )
	{
		Distance = FMath::Max(FVector::Dist( Origin.GetTranslation(), Location ) - CapsuleRadius, 0.f);
	}
	else
	{
		const FVector PointOffset = (CapsuleHalfHeight - CapsuleRadius) * Origin.GetUnitAxis( EAxis::Z );
		const FVector StartPoint = Origin.GetTranslation() + PointOffset;
		const FVector EndPoint = Origin.GetTranslation() - PointOffset;

		Distance = FMath::PointDistToSegment(Location, StartPoint, EndPoint) - CapsuleRadius;
	}

	return AttenuationEval(Distance, FalloffDistance, DistanceScale);
}

float FBaseAttenuationSettings::AttenuationEvalCone(const FTransform& Origin, const FVector Location, const float DistanceScale) const
{
	const FVector Forward = Origin.GetUnitAxis( EAxis::X );

	float AttenuationMultiplier = 1.f;

	const FVector ConeOrigin = Origin.GetTranslation() - (Forward * ConeOffset);

	const float Distance = FMath::Max(FVector::Dist( ConeOrigin, Location ) - AttenuationShapeExtents.X, 0.f);
	AttenuationMultiplier *= AttenuationEval(Distance, FalloffDistance, DistanceScale);

	if (AttenuationMultiplier > 0.f)
	{
		const float theta = FMath::RadiansToDegrees(FMath::Abs(FMath::Acos( FVector::DotProduct(Forward, (Location - ConeOrigin).GetSafeNormal()))));
		AttenuationMultiplier *= AttenuationEval(theta - AttenuationShapeExtents.Y, AttenuationShapeExtents.Z, 1.0f);
	}

	return AttenuationMultiplier;
}

void FBaseAttenuationSettings::CollectAttenuationShapesForVisualization(TMultiMap<EAttenuationShape::Type, AttenuationShapeDetails>& ShapeDetailsMap) const
{
	AttenuationShapeDetails ShapeDetails;
	ShapeDetails.Extents = AttenuationShapeExtents;
	ShapeDetails.Falloff = FalloffDistance;
	ShapeDetails.ConeOffset = ConeOffset;

	ShapeDetailsMap.Add(AttenuationShape, ShapeDetails);
}