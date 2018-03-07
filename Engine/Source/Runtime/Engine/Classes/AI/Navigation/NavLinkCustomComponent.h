// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineTypes.h"
#include "AI/Navigation/NavAreas/NavArea.h"
#include "AI/Navigation/NavLinkDefinition.h"
#include "AI/Navigation/NavLinkCustomInterface.h"
#include "AI/Navigation/NavRelevantComponent.h"
#include "NavLinkCustomComponent.generated.h"

class UPathFollowingComponent;
struct FNavigationRelevantData;

/**
 *  Encapsulates NavLinkCustomInterface interface, can be used with Actors not relevant for navigation
 *  
 *  Additional functionality:
 *  - can be toggled
 *  - can create obstacle area for easier/forced separation of link end points
 *  - can broadcast state changes to nearby agents
 */

UCLASS()
class ENGINE_API UNavLinkCustomComponent : public UNavRelevantComponent, public INavLinkCustomInterface
{
	GENERATED_UCLASS_BODY()

	DECLARE_DELEGATE_ThreeParams(FOnMoveReachedLink, UNavLinkCustomComponent* /*ThisComp*/, UPathFollowingComponent* /*PathComp*/, const FVector& /*DestPoint*/);
	DECLARE_DELEGATE_TwoParams(FBroadcastFilter, UNavLinkCustomComponent* /*ThisComp*/, TArray<UPathFollowingComponent*>& /*NotifyList*/);

	// BEGIN INavLinkCustomInterface
	virtual void GetLinkData(FVector& LeftPt, FVector& RightPt, ENavLinkDirection::Type& Direction) const override;
	virtual TSubclassOf<UNavArea> GetLinkAreaClass() const override;
	virtual uint32 GetLinkId() const override;
	virtual void UpdateLinkId(uint32 NewUniqueId) override;
	virtual bool IsLinkPathfindingAllowed(const UObject* Querier) const override;
	virtual bool OnLinkMoveStarted(UPathFollowingComponent* PathComp, const FVector& DestPoint) override;
	virtual void OnLinkMoveFinished(UPathFollowingComponent* PathComp) override;
	// END INavLinkCustomInterface

	//~ Begin UNavRelevantComponent Interface
	virtual void GetNavigationData(FNavigationRelevantData& Data) const override;
	virtual void CalcAndCacheBounds() const override;
	//~ End UNavRelevantComponent Interface

	//~ Begin UActorComponent Interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	//~ End UActorComponent Interface

	//~ Begin UObject Interface
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditImport() override;
#endif
	//~ End UObject Interface

	/** set basic link data: end points and direction */
	void SetLinkData(const FVector& RelativeStart, const FVector& RelativeEnd, ENavLinkDirection::Type Direction);
	virtual FNavigationLink GetLinkModifier() const;

	/** set area class to use when link is enabled */
	void SetEnabledArea(TSubclassOf<UNavArea> AreaClass);
	TSubclassOf<UNavArea> GetEnabledArea() const { return EnabledAreaClass; }

	/** set area class to use when link is disabled */
	void SetDisabledArea(TSubclassOf<UNavArea> AreaClass);
	TSubclassOf<UNavArea> GetDisabledArea() const { return DisabledAreaClass; }

	/** add box obstacle during generation of navigation data
	  * this can be used to create empty area under doors */
	void AddNavigationObstacle(TSubclassOf<UNavArea> AreaClass, const FVector& BoxExtent, const FVector& BoxOffset = FVector::ZeroVector);

	/** removes simple obstacle */
	void ClearNavigationObstacle();

	/** set properties of trigger around link entry point(s), that will notify nearby agents about link state change */
	void SetBroadcastData(float Radius, ECollisionChannel TraceChannel = ECC_Pawn, float Interval = 0.0f);

	void SendBroadcastWhenEnabled(bool bEnabled);
	void SendBroadcastWhenDisabled(bool bEnabled);

	/** set delegate to filter  */
	void SetBroadcastFilter(FBroadcastFilter const& InDelegate);

	/** change state of smart link (used area class) */
	void SetEnabled(bool bNewEnabled);
	bool IsEnabled() const { return bLinkEnabled; }
	
	/** set delegate to notify about reaching this link during path following */
	void SetMoveReachedLink(FOnMoveReachedLink const& InDelegate);

	/** check is any agent is currently moving though this link */
	bool HasMovingAgents() const;

	/** get link start point in world space */
	FVector GetStartPoint() const;

	/** get link end point in world space */
	FVector GetEndPoint() const;

	//////////////////////////////////////////////////////////////////////////
	// helper functions for setting delegates

	template< class UserClass >	
	FORCEINLINE void SetMoveReachedLink(UserClass* TargetOb, typename FOnMoveReachedLink::TUObjectMethodDelegate< UserClass >::FMethodPtr InFunc)
	{
		SetMoveReachedLink(FOnMoveReachedLink::CreateUObject(TargetOb, InFunc));
	}
	template< class UserClass >	
	FORCEINLINE void SetMoveReachedLink(UserClass* TargetOb, typename FOnMoveReachedLink::TUObjectMethodDelegate_Const< UserClass >::FMethodPtr InFunc)
	{
		SetMoveReachedLink(FOnMoveReachedLink::CreateUObject(TargetOb, InFunc));
	}

	template< class UserClass >	
	FORCEINLINE void SetBroadcastFilter(UserClass* TargetOb, typename FBroadcastFilter::TUObjectMethodDelegate< UserClass >::FMethodPtr InFunc)
	{
		SetBroadcastFilter(FBroadcastFilter::CreateUObject(TargetOb, InFunc));
	}
	template< class UserClass >	
	FORCEINLINE void SetBroadcastFilter(UserClass* TargetOb, typename FBroadcastFilter::TUObjectMethodDelegate_Const< UserClass >::FMethodPtr InFunc)
	{
		SetBroadcastFilter(FBroadcastFilter::CreateUObject(TargetOb, InFunc));
	}
	
protected:

	/** link Id assigned by navigation system */
	UPROPERTY()
	uint32 NavLinkUserId;

	/** area class to use when link is enabled */
	UPROPERTY(EditAnywhere, Category=SmartLink)
	TSubclassOf<UNavArea> EnabledAreaClass;

	/** area class to use when link is disabled */
	UPROPERTY(EditAnywhere, Category=SmartLink)
	TSubclassOf<UNavArea> DisabledAreaClass;

	/** start point, relative to owner */
	UPROPERTY(EditAnywhere, Category=SmartLink)
	FVector LinkRelativeStart;

	/** end point, relative to owner */
	UPROPERTY(EditAnywhere, Category=SmartLink)
	FVector LinkRelativeEnd;

	/** direction of link */
	UPROPERTY(EditAnywhere, Category=SmartLink)
	TEnumAsByte<ENavLinkDirection::Type> LinkDirection;

	/** is link currently in enabled state? (area class) */
	UPROPERTY(EditAnywhere, Category=SmartLink)
	uint32 bLinkEnabled : 1;

	/** should link notify nearby agents when it changes state to enabled */
	UPROPERTY(EditAnywhere, Category=Broadcast)
	uint32 bNotifyWhenEnabled : 1;

	/** should link notify nearby agents when it changes state to disabled */
	UPROPERTY(EditAnywhere, Category=Broadcast)
	uint32 bNotifyWhenDisabled : 1;

	/** if set, box obstacle area will be added to generation */
	UPROPERTY(EditAnywhere, Category=Obstacle)
	uint32 bCreateBoxObstacle : 1;

	/** offset of simple box obstacle */
	UPROPERTY(EditAnywhere, Category=Obstacle)
	FVector ObstacleOffset;

	/** extent of simple box obstacle */
	UPROPERTY(EditAnywhere, Category=Obstacle)
	FVector ObstacleExtent;

	/** area class for simple box obstacle */
	UPROPERTY(EditAnywhere, Category=Obstacle)
	TSubclassOf<UNavArea> ObstacleAreaClass;

	/** radius of state change broadcast */
	UPROPERTY(EditAnywhere, Category=Broadcast)
	float BroadcastRadius;

	/** interval for state change broadcast (0 = single broadcast) */
	UPROPERTY(EditAnywhere, Category=Broadcast, Meta=(UIMin="0.0", ClampMin="0.0"))
	float BroadcastInterval;

	/** trace channel for state change broadcast */
	UPROPERTY(EditAnywhere, Category=Broadcast)
	TEnumAsByte<ECollisionChannel> BroadcastChannel;

	/** delegate to call when link is reached */
	FBroadcastFilter OnBroadcastFilter;

	/** list of agents moving though this link */
	TArray<TWeakObjectPtr<UPathFollowingComponent> > MovingAgents;

	/** delegate to call when link is reached */
	FOnMoveReachedLink OnMoveReachedLink;

	/** Handle for efficient management of BroadcastStateChange timer */
	FTimerHandle TimerHandle_BroadcastStateChange;

	/** notify nearby agents about link changing state */
	void BroadcastStateChange();
	
	/** gather agents to notify about state change */
	void CollectNearbyAgents(TArray<UPathFollowingComponent*>& NotifyList);
};
