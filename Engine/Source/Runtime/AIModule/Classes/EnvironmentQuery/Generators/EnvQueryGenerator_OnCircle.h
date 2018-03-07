// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "AI/Navigation/NavigationTypes.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "DataProviders/AIDataProvider.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_ProjectedPoints.h"
#include "EnvQueryGenerator_OnCircle.generated.h"

class AActor;

UENUM()
enum class EPointOnCircleSpacingMethod :uint8
{
	// Use the SpaceBetween value to determine how far apart points should be.
	BySpaceBetween,
	// Use a fixed number of points
	ByNumberOfPoints
};

UCLASS(meta = (DisplayName = "Points: Circle"))
class AIMODULE_API UEnvQueryGenerator_OnCircle : public UEnvQueryGenerator_ProjectedPoints
{
	GENERATED_UCLASS_BODY()

	/** max distance of path between point and context */
	UPROPERTY(EditDefaultsOnly, Category=Generator)
	FAIDataProviderFloatValue CircleRadius;

	/** items will be generated on a circle this much apart */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
	FAIDataProviderFloatValue SpaceBetween;

	/** this many items will be generated on a circle */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
	FAIDataProviderIntValue NumberOfPoints;

	/** how we are choosing where the points are in the circle */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
	EPointOnCircleSpacingMethod PointOnCircleSpacingMethod;

	/** If you generate items on a piece of circle you define direction of Arc cut here */
	UPROPERTY(EditDefaultsOnly, Category=Generator, meta=(EditCondition="bDefineArc"))
	FEnvDirection ArcDirection;

	/** If you generate items on a piece of circle you define angle of Arc cut here */
	UPROPERTY(EditDefaultsOnly, Category=Generator)
	FAIDataProviderFloatValue ArcAngle;
	
	UPROPERTY()
	mutable float AngleRadians;

	/** context */
	UPROPERTY(EditAnywhere, Category=Generator)
	TSubclassOf<class UEnvQueryContext> CircleCenter;

	/** ignore tracing into context actors when generating the circle */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
	bool bIgnoreAnyContextActorsWhenGeneratingCircle;

	/** context offset */
	UPROPERTY(EditAnywhere, Category = Generator)
	FAIDataProviderFloatValue CircleCenterZOffset;

	/** horizontal trace for nearest obstacle */
	UPROPERTY(EditAnywhere, Category=Generator)
	FEnvTraceData TraceData;

	UPROPERTY(EditAnywhere, Category=Generator, meta=(InlineEditConditionToggle))
	uint32 bDefineArc:1;

	virtual void PostLoad() override;

	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

protected:
	FVector CalcDirection(FEnvQueryInstance& QueryInstance) const;

	void GenerateItemsForCircle(uint8* ContextRawData, UEnvQueryItemType* ContextItemType,
		const FVector& CenterLocation, const FVector& StartDirection,
		const TArray<AActor*>& IgnoredActors,
		int32 StepsCount, float AngleStep, FEnvQueryInstance& OutQueryInstance) const;

	virtual void AddItemDataForCircle(uint8* ContextRawData, UEnvQueryItemType* ContextItemType, 
		const TArray<FNavLocation>& Locations, FEnvQueryInstance& OutQueryInstance) const;
};
