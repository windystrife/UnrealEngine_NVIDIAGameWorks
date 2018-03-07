// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "AI/Navigation/NavFilters/NavigationQueryFilter.h"
#include "AI/Navigation/NavigationTypes.h"

#if WITH_RECAST
#include "Detour/DetourNavMesh.h"
#include "Detour/DetourNavMeshQuery.h"

#define RECAST_VERY_SMALL_AGENT_RADIUS 0.0f

class UNavigationSystem;

class ENGINE_API FRecastQueryFilter : public INavigationQueryFilterInterface, public dtQueryFilter
{
public:
	FRecastQueryFilter(bool bIsVirtual = true);
	virtual ~FRecastQueryFilter(){}

	virtual void Reset() override;

	virtual void SetAreaCost(uint8 AreaType, float Cost) override;
	virtual void SetFixedAreaEnteringCost(uint8 AreaType, float Cost) override;
	virtual void SetExcludedArea(uint8 AreaType) override;
	virtual void SetAllAreaCosts(const float* CostArray, const int32 Count) override;
	virtual void GetAllAreaCosts(float* CostArray, float* FixedCostArray, const int32 Count) const override;
	virtual void SetBacktrackingEnabled(const bool bBacktracking) override;
	virtual bool IsBacktrackingEnabled() const override;
	virtual bool IsEqual(const INavigationQueryFilterInterface* Other) const override;
	virtual void SetIncludeFlags(uint16 Flags) override;
	virtual uint16 GetIncludeFlags() const override;
	virtual void SetExcludeFlags(uint16 Flags) override;
	virtual uint16 GetExcludeFlags() const override;
	virtual FVector GetAdjustedEndLocation(const FVector& EndLocation) const override { return EndLocation; }
	virtual INavigationQueryFilterInterface* CreateCopy() const override;

	const dtQueryFilter* GetAsDetourQueryFilter() const { return this; }

	/** note that it results in losing all area cost setup. Call it before setting anything else */
	void SetIsVirtual(bool bIsVirtual);
};

struct ENGINE_API FRecastSpeciaLinkFilter : public dtQuerySpecialLinkFilter
{
	FRecastSpeciaLinkFilter(UNavigationSystem* NavSystem, const UObject* Owner) : NavSys(NavSystem), SearchOwner(Owner), CachedOwnerOb(nullptr) {}
	virtual bool isLinkAllowed(const int32 UserId) const override;
	virtual void initialize() override;

	UNavigationSystem* NavSys;
	FWeakObjectPtr SearchOwner;
	UObject* CachedOwnerOb;
};

#endif	// WITH_RECAST
