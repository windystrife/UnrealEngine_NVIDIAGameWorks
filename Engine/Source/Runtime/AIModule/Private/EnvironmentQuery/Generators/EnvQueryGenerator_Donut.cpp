// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EnvironmentQuery/Generators/EnvQueryGenerator_Donut.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"

#define LOCTEXT_NAMESPACE "EnvQueryGenerator"

UEnvQueryGenerator_Donut::UEnvQueryGenerator_Donut(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Center = UEnvQueryContext_Querier::StaticClass();
	InnerRadius.DefaultValue = 300.0f;
	OuterRadius.DefaultValue = 1000.0f;
	NumberOfRings.DefaultValue = 3;
	PointsPerRing.DefaultValue = 8;
	ArcDirection.DirMode = EEnvDirection::TwoPoints;
	ArcDirection.LineFrom = UEnvQueryContext_Querier::StaticClass();
	ArcDirection.Rotation = UEnvQueryContext_Querier::StaticClass();
	ArcAngle.DefaultValue = 360.f;
	bUseSpiralPattern = false;

	ProjectionData.TraceMode = EEnvQueryTrace::None;
}

float UEnvQueryGenerator_Donut::GetArcBisectorAngle(FEnvQueryInstance& QueryInstance) const
{
	float BisectAngle = 0.0f;

	FVector Direction;
	if (bDefineArc)
	{
		if (ArcDirection.DirMode == EEnvDirection::TwoPoints)
		{
			TArray<FVector> Start;
			TArray<FVector> End;
			QueryInstance.PrepareContext(ArcDirection.LineFrom, Start);
			QueryInstance.PrepareContext(ArcDirection.LineTo, End);

			if (Start.Num() > 0 && End.Num() > 0)
			{
				const FVector LineDir = (End[0] - Start[0]).GetSafeNormal();
				const FRotator LineRot = LineDir.Rotation();
				BisectAngle = LineRot.Yaw;
			}
		}
		else
		{
			TArray<FRotator> Rot;
			QueryInstance.PrepareContext(ArcDirection.Rotation, Rot);

			if (Rot.Num() > 0)
			{
				BisectAngle = Rot[0].Yaw;
			}
		}
	}

	return BisectAngle;
}

bool UEnvQueryGenerator_Donut::IsAngleAllowed(float TestAngleRad, float BisectAngleDeg, float AngleRangeDeg, bool bConstrainAngle) const
{
	if (bConstrainAngle)
	{
		const float TestAngleDeg = FMath::RadiansToDegrees(TestAngleRad);
		const float AngleDelta = FRotator::NormalizeAxis(TestAngleDeg - BisectAngleDeg);
		return (FMath::Abs(AngleDelta) - 0.01f) < AngleRangeDeg;
	}

	return true;	
}

void UEnvQueryGenerator_Donut::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
	TArray<FVector> CenterPoints;
	QueryInstance.PrepareContext(Center, CenterPoints);

	if (CenterPoints.Num() <= 0)
	{
		return;
	}

	UObject* BindOwner = QueryInstance.Owner.Get();
	InnerRadius.BindData(BindOwner, QueryInstance.QueryID);
	OuterRadius.BindData(BindOwner, QueryInstance.QueryID);
	NumberOfRings.BindData(BindOwner, QueryInstance.QueryID);
	PointsPerRing.BindData(BindOwner, QueryInstance.QueryID);
	ArcAngle.BindData(BindOwner, QueryInstance.QueryID);

	float ArcAngleValue = ArcAngle.GetValue();
	float InnerRadiusValue = InnerRadius.GetValue();
	float OuterRadiusValue = OuterRadius.GetValue();
	int32 NumRings = NumberOfRings.GetValue();
	int32 NumPoints = PointsPerRing.GetValue();

	if ((InnerRadiusValue <= 0.f) || (OuterRadiusValue <= 0.f) ||
		(InnerRadiusValue > OuterRadiusValue) ||
		(NumRings < 1) || (NumPoints < 1))
	{
		return;
	}

	const float ArcBisectDeg = GetArcBisectorAngle(QueryInstance);
	const float ArcAngleDeg = FMath::Clamp(ArcAngleValue, 0.0f, 360.0f);

	const float RadiusDelta = (OuterRadiusValue - InnerRadiusValue) / (NumRings - 1);
	const float AngleDelta = 2.0 * PI / NumPoints;
	float SectionAngle = FMath::DegreesToRadians(ArcBisectDeg);

	TArray<FNavLocation> Points;
	Points.Reserve(NumPoints * NumRings);

	if (!bUseSpiralPattern)
	{
		for (int32 SectionIdx = 0; SectionIdx < NumPoints; SectionIdx++, SectionAngle += AngleDelta)
		{
			if (IsAngleAllowed(SectionAngle, ArcBisectDeg, ArcAngleDeg, bDefineArc))
			{
				const float SinValue = FMath::Sin(SectionAngle);
				const float CosValue = FMath::Cos(SectionAngle);

				float RingRadius = InnerRadiusValue;
				for (int32 RingIdx = 0; RingIdx < NumRings; RingIdx++, RingRadius += RadiusDelta)
				{
					const FVector RingPos(RingRadius * CosValue, RingRadius * SinValue, 0.0f);
					for (int32 ContextIdx = 0; ContextIdx < CenterPoints.Num(); ContextIdx++)
					{
						const FNavLocation PointPos = FNavLocation(CenterPoints[ContextIdx] + RingPos);
						Points.Add(PointPos);
					}
				}
			}
		}
	}
	else
	{	// In order to spiral, we need to change the angle for each ring as well as each spoke.  By changing it as a
		// fraction of the SectionAngle, we guarantee that the offset won't cross to the starting point of the next
		// spoke.
		const float RingAngleDelta = AngleDelta / NumRings;
		float RingRadius = InnerRadiusValue;
		float CurrentRingAngleDelta = 0.f;

		// So, start with ring angle and then add section angle.
		for (int32 RingIdx = 0; RingIdx < NumRings; RingIdx++, RingRadius += RadiusDelta, CurrentRingAngleDelta += RingAngleDelta)
		{
			for (int32 SectionIdx = 0; SectionIdx < NumPoints; SectionIdx++, SectionAngle += AngleDelta)
			{
				float RingSectionAngle = CurrentRingAngleDelta + SectionAngle;
				if (IsAngleAllowed(RingSectionAngle, ArcBisectDeg, ArcAngleDeg, bDefineArc))
				{
					const float SinValue = FMath::Sin(RingSectionAngle);
					const float CosValue = FMath::Cos(RingSectionAngle);

					const FVector RingPos(RingRadius * CosValue, RingRadius * SinValue, 0.0f);
					for (int32 ContextIdx = 0; ContextIdx < CenterPoints.Num(); ContextIdx++)
					{
						const FNavLocation PointPos = FNavLocation(CenterPoints[ContextIdx] + RingPos);
						Points.Add(PointPos);
					}
				}
			}
		}
	}

	ProjectAndFilterNavPoints(Points, QueryInstance);
	StoreNavPoints(Points, QueryInstance);
}

FText UEnvQueryGenerator_Donut::GetDescriptionTitle() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("DescriptionTitle"), Super::GetDescriptionTitle());
	Args.Add(TEXT("DescribeContext"), UEnvQueryTypes::DescribeContext(Center));

	return FText::Format(LOCTEXT("DescriptionGenerateDonutAroundContext", "{DescriptionTitle}: generate items around {DescribeContext}"), Args);
}

FText UEnvQueryGenerator_Donut::GetDescriptionDetails() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("InnerRadius"), FText::FromString(InnerRadius.ToString()));
	Args.Add(TEXT("OuterRadius"), FText::FromString(OuterRadius.ToString()));
	Args.Add(TEXT("NumRings"), FText::FromString(NumberOfRings.ToString()));
	Args.Add(TEXT("NumPerRing"), FText::FromString(PointsPerRing.ToString()));

	FText Desc = FText::Format(LOCTEXT("DonutDescription", "radius: {InnerRadius} to {OuterRadius}\nrings: {NumRings}, points per ring: {NumPerRing}"), Args);

	if (bDefineArc)
	{
		FFormatNamedArguments ArcArgs;
		ArcArgs.Add(TEXT("Description"), Desc);
		ArcArgs.Add(TEXT("AngleValue"), ArcAngle.DefaultValue);
		ArcArgs.Add(TEXT("ArcDirection"), ArcDirection.ToText());
		Desc = FText::Format(LOCTEXT("DescriptionWithArc", "{Description}\nLimit to {AngleValue} angle both sides on {ArcDirection}"), ArcArgs);
	}

	FText ProjDesc = ProjectionData.ToText(FEnvTraceData::Brief);
	if (!ProjDesc.IsEmpty())
	{
		FFormatNamedArguments ProjArgs;
		ProjArgs.Add(TEXT("Description"), Desc);
		ProjArgs.Add(TEXT("ProjectionDescription"), ProjDesc);
		Desc = FText::Format(LOCTEXT("OnCircle_DescriptionWithProjection", "{Description}, {ProjectionDescription}"), ProjArgs);
	}

	return Desc;
}

#if WITH_EDITOR

void UEnvQueryGenerator_Donut::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property != NULL)
	{
		const FName PropName = PropertyChangedEvent.MemberProperty->GetFName();
		if (PropName == GET_MEMBER_NAME_CHECKED(UEnvQueryGenerator_Donut, ArcAngle))
		{
			ArcAngle.DefaultValue = FMath::Clamp(ArcAngle.DefaultValue, 0.0f, 360.f);
			bDefineArc = (ArcAngle.DefaultValue < 360.f) && (ArcAngle.DefaultValue > 0.f);
		}
		else if (PropName == GET_MEMBER_NAME_CHECKED(UEnvQueryGenerator_Donut, NumberOfRings))
		{
			NumberOfRings.DefaultValue = FMath::Max(1, NumberOfRings.DefaultValue);
		}
		else if (PropName == GET_MEMBER_NAME_CHECKED(UEnvQueryGenerator_Donut, PointsPerRing))
		{
			PointsPerRing.DefaultValue = FMath::Max(1, PointsPerRing.DefaultValue);
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
