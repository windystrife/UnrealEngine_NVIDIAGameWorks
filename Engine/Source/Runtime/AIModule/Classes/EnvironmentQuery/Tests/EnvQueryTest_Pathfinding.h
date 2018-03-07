// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "AI/Navigation/NavFilters/NavigationQueryFilter.h"
#include "AI/Navigation/NavigationTypes.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "DataProviders/AIDataProvider.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvQueryTest_Pathfinding.generated.h"

class ANavigationData;
class UNavigationSystem;

UENUM()
namespace EEnvTestPathfinding
{
	enum Type
	{
		PathExist,
		PathCost,
		PathLength,
	};
}

UCLASS()
class UEnvQueryTest_Pathfinding : public UEnvQueryTest
{
	GENERATED_UCLASS_BODY()

	/** testing mode */
	UPROPERTY(EditDefaultsOnly, Category=Pathfinding)
	TEnumAsByte<EEnvTestPathfinding::Type> TestMode;

	/** context: other end of pathfinding test */
	UPROPERTY(EditDefaultsOnly, Category=Pathfinding)
	TSubclassOf<UEnvQueryContext> Context;

	/** pathfinding direction */
	UPROPERTY(EditDefaultsOnly, Category=Pathfinding)
	FAIDataProviderBoolValue PathFromContext;

	/** if set, items with failed path will be invalidated (PathCost, PathLength) */
	UPROPERTY(EditDefaultsOnly, Category=Pathfinding, AdvancedDisplay)
	FAIDataProviderBoolValue SkipUnreachable;

	/** navigation filter to use in pathfinding */
	UPROPERTY(EditDefaultsOnly, Category=Pathfinding)
	TSubclassOf<UNavigationQueryFilter> FilterClass;

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

#if WITH_EDITOR
	/** update test properties after changing mode */
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void PostLoad() override;

protected:

	DECLARE_DELEGATE_RetVal_SevenParams(bool, FTestPathSignature, const FVector&, const FVector&, EPathFindingMode::Type, const ANavigationData&, UNavigationSystem&, FSharedConstNavQueryFilter, const UObject*);
	DECLARE_DELEGATE_RetVal_SevenParams(float, FFindPathSignature, const FVector&, const FVector&, EPathFindingMode::Type, const ANavigationData&, UNavigationSystem&, FSharedConstNavQueryFilter, const UObject*);

	bool TestPathFrom(const FVector& ItemPos, const FVector& ContextPos, EPathFindingMode::Type Mode, const ANavigationData& NavData, UNavigationSystem& NavSys, FSharedConstNavQueryFilter NavFilter, const UObject* PathOwner) const;
	bool TestPathTo(const FVector& ItemPos, const FVector& ContextPos, EPathFindingMode::Type Mode, const ANavigationData& NavData, UNavigationSystem& NavSys, FSharedConstNavQueryFilter NavFilter, const UObject* PathOwner) const;
	float FindPathCostFrom(const FVector& ItemPos, const FVector& ContextPos, EPathFindingMode::Type Mode, const ANavigationData& NavData, UNavigationSystem& NavSys, FSharedConstNavQueryFilter NavFilter, const UObject* PathOwner) const;
	float FindPathCostTo(const FVector& ItemPos, const FVector& ContextPos, EPathFindingMode::Type Mode, const ANavigationData& NavData, UNavigationSystem& NavSys, FSharedConstNavQueryFilter NavFilter, const UObject* PathOwner) const;
	float FindPathLengthFrom(const FVector& ItemPos, const FVector& ContextPos, EPathFindingMode::Type Mode, const ANavigationData& NavData, UNavigationSystem& NavSys, FSharedConstNavQueryFilter NavFilter, const UObject* PathOwner) const;
	float FindPathLengthTo(const FVector& ItemPos, const FVector& ContextPos, EPathFindingMode::Type Mode, const ANavigationData& NavData, UNavigationSystem& NavSys, FSharedConstNavQueryFilter NavFilter, const UObject* PathOwner) const;

	ANavigationData* FindNavigationData(UNavigationSystem& NavSys, UObject* Owner) const;
};
