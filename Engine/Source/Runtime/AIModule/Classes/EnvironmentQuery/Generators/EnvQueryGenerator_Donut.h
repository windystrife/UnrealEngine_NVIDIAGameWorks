// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "DataProviders/AIDataProvider.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_ProjectedPoints.h"
#include "EnvQueryGenerator_Donut.generated.h"

UCLASS(meta = (DisplayName = "Points: Donut"))
class AIMODULE_API UEnvQueryGenerator_Donut : public UEnvQueryGenerator_ProjectedPoints
{
	GENERATED_UCLASS_BODY()

	/** min distance between point and context */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
	FAIDataProviderFloatValue InnerRadius;

	/** max distance between point and context */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
	FAIDataProviderFloatValue OuterRadius;

	/** number of rings to generate */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
	FAIDataProviderIntValue NumberOfRings;

	/** number of items to generate for each ring */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
	FAIDataProviderIntValue PointsPerRing;

	/** If you generate items on a piece of circle you define direction of Arc cut here */
	UPROPERTY(EditDefaultsOnly, Category = Generator, meta = (EditCondition = "bDefineArc"))
	FEnvDirection ArcDirection;

	/** If you generate items on a piece of circle you define angle of Arc cut here */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
	FAIDataProviderFloatValue ArcAngle;

	/** If true, the rings of the wheel will be rotated in a spiral pattern.  If false, they will all be at a zero
	  * rotation, looking more like the spokes on a wheel.  */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
	bool bUseSpiralPattern;

	/** context */
	UPROPERTY(EditAnywhere, Category = Generator)
	TSubclassOf<class UEnvQueryContext> Center;

	UPROPERTY(EditAnywhere, Category = Generator, meta=(InlineEditConditionToggle))
	uint32 bDefineArc : 1;

	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

protected:
	float GetArcBisectorAngle(FEnvQueryInstance& QueryInstance) const;
	bool IsAngleAllowed(float TestAngleRad, float BisectAngleDeg, float AngleRangeDeg, bool bConstrainAngle) const;
};
