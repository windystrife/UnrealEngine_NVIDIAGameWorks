// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AI/Navigation/NavFilters/NavigationQueryFilter.h"
#include "AI/Navigation/NavigationTypes.h"
#include "AI/Navigation/NavigationData.h"
#include "AbstractNavData.generated.h"

struct ENGINE_API FAbstractNavigationPath : public FNavigationPath
{
	typedef FNavigationPath Super;

	FAbstractNavigationPath();

	static const FNavPathType Type;
};

class ENGINE_API FAbstractQueryFilter : public INavigationQueryFilterInterface
{
public:
	virtual void Reset() override {}
	virtual void SetAreaCost(uint8 AreaType, float Cost) override {}
	virtual void SetFixedAreaEnteringCost(uint8 AreaType, float Cost) override {}
	virtual void SetExcludedArea(uint8 AreaType) override {}
	virtual void SetAllAreaCosts(const float* CostArray, const int32 Count) override {}
	virtual void GetAllAreaCosts(float* CostArray, float* FixedCostArray, const int32 Count) const override {}
	virtual void SetBacktrackingEnabled(const bool bBacktracking) override {}
	virtual bool IsBacktrackingEnabled() const override { return false; }
	virtual bool IsEqual(const INavigationQueryFilterInterface* Other) const override { return true; }
	virtual void SetIncludeFlags(uint16 Flags) override {}
	virtual uint16 GetIncludeFlags() const override { return 0; }
	virtual void SetExcludeFlags(uint16 Flags) override {}
	virtual uint16 GetExcludeFlags() const override { return 0; }
	virtual FVector GetAdjustedEndLocation(const FVector& EndLocation) const override { return EndLocation; }
	virtual INavigationQueryFilterInterface* CreateCopy() const override;
};

UCLASS()
class ENGINE_API AAbstractNavData : public ANavigationData
{
	GENERATED_BODY()

public:
	AAbstractNavData(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PostLoad() override;

	// Begin ANavigationData overrides
	virtual void BatchRaycast(TArray<FNavigationRaycastWork>& Workload, FSharedConstNavQueryFilter QueryFilter, const UObject* Querier = NULL) const override {};
	virtual FNavLocation GetRandomPoint(FSharedConstNavQueryFilter Filter = NULL, const UObject* Querier = NULL) const override { return FNavLocation();  }
	virtual bool GetRandomReachablePointInRadius(const FVector& Origin, float Radius, FNavLocation& OutResult, FSharedConstNavQueryFilter Filter = NULL, const UObject* Querier = NULL) const override { return false; }
	virtual bool GetRandomPointInNavigableRadius(const FVector& Origin, float Radius, FNavLocation& OutResult, FSharedConstNavQueryFilter Filter = NULL, const UObject* Querier = NULL) const override { return false; }
	virtual bool ProjectPoint(const FVector& Point, FNavLocation& OutLocation, const FVector& Extent, FSharedConstNavQueryFilter Filter = NULL, const UObject* Querier = NULL) const override { return false; }
	virtual void BatchProjectPoints(TArray<FNavigationProjectionWork>& Workload, const FVector& Extent, FSharedConstNavQueryFilter Filter = NULL, const UObject* Querier = NULL) const override {};
	virtual void BatchProjectPoints(TArray<FNavigationProjectionWork>& Workload, FSharedConstNavQueryFilter Filter = NULL, const UObject* Querier = NULL) const override {}
	virtual ENavigationQueryResult::Type CalcPathCost(const FVector& PathStart, const FVector& PathEnd, float& OutPathCost, FSharedConstNavQueryFilter QueryFilter = NULL, const UObject* Querier = NULL) const override { return ENavigationQueryResult::Invalid; }
	virtual ENavigationQueryResult::Type CalcPathLength(const FVector& PathStart, const FVector& PathEnd, float& OutPathLength, FSharedConstNavQueryFilter QueryFilter = NULL, const UObject* Querier = NULL) const override { return ENavigationQueryResult::Invalid; }
	virtual ENavigationQueryResult::Type CalcPathLengthAndCost(const FVector& PathStart, const FVector& PathEnd, float& OutPathLength, float& OutPathCost, FSharedConstNavQueryFilter QueryFilter = NULL, const UObject* Querier = NULL) const override { return ENavigationQueryResult::Invalid;	}
	virtual bool DoesNodeContainLocation(NavNodeRef NodeRef, const FVector& WorldSpaceLocation) const override { return true; }
	virtual void OnNavAreaAdded(const UClass* NavAreaClass, int32 AgentIndex) override {}
	virtual void OnNavAreaRemoved(const UClass* NavAreaClass) override {};
	// End ANavigationData overrides

	static FPathFindingResult FindPathAbstract(const FNavAgentProperties& AgentProperties, const FPathFindingQuery& Query);
	static bool TestPathAbstract(const FNavAgentProperties& AgentProperties, const FPathFindingQuery& Query, int32* NumVisitedNodes);
	static bool RaycastAbstract(const ANavigationData* NavDataInstance, const FVector& RayStart, const FVector& RayEnd, FVector& HitLocation, FSharedConstNavQueryFilter QueryFilter, const UObject* Querier);
};
