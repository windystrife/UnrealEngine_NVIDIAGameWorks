// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AI/Navigation/NavLinkCustomComponent.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"
#include "CollisionQueryParams.h"
#include "WorldCollision.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "AI/Navigation/NavigationSystem.h"
#include "AI/Navigation/NavAreas/NavArea_Null.h"
#include "AI/Navigation/NavAreas/NavArea_Default.h"
// @todo to be addressed when removing AIModule circular dependency
#include "Navigation/PathFollowingComponent.h"
#include "AI/NavigationModifier.h"
#include "AI/NavigationOctree.h"
#include "AI/NavigationSystemHelpers.h"

UNavLinkCustomComponent::UNavLinkCustomComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NavLinkUserId = 0;
	LinkRelativeStart = FVector(70, 0, 0);
	LinkRelativeEnd = FVector(-70, 0, 0);
	LinkDirection = ENavLinkDirection::BothWays;
	EnabledAreaClass = UNavArea_Default::StaticClass();
	DisabledAreaClass = UNavArea_Null::StaticClass();
	ObstacleAreaClass = UNavArea_Null::StaticClass();
	ObstacleExtent = FVector(50, 50, 50);
	bLinkEnabled = true;
	bNotifyWhenEnabled = false;
	bNotifyWhenDisabled = false;
	bCreateBoxObstacle = false;
	BroadcastRadius = 0.0f;
	BroadcastChannel = ECC_Pawn;
	BroadcastInterval = 0.0f;
}

void UNavLinkCustomComponent::PostLoad()
{
	Super::PostLoad();

	INavLinkCustomInterface::UpdateUniqueId(NavLinkUserId);
}

#if WITH_EDITOR
void UNavLinkCustomComponent::PostEditImport()
{
	Super::PostEditImport();

	NavLinkUserId = INavLinkCustomInterface::GetUniqueId();
}
#endif

void UNavLinkCustomComponent::GetLinkData(FVector& LeftPt, FVector& RightPt, ENavLinkDirection::Type& Direction) const
{
	LeftPt = LinkRelativeStart;
	RightPt = LinkRelativeEnd;
	Direction = LinkDirection;
}

TSubclassOf<UNavArea> UNavLinkCustomComponent::GetLinkAreaClass() const
{
	return bLinkEnabled ? EnabledAreaClass : DisabledAreaClass;
}

uint32 UNavLinkCustomComponent::GetLinkId() const
{
	return NavLinkUserId;
}

void UNavLinkCustomComponent::UpdateLinkId(uint32 NewUniqueId)
{
	NavLinkUserId = NewUniqueId;
}

bool UNavLinkCustomComponent::IsLinkPathfindingAllowed(const UObject* Querier) const
{
	return true;
}

bool UNavLinkCustomComponent::OnLinkMoveStarted(UPathFollowingComponent* PathComp, const FVector& DestPoint)
{
	TWeakObjectPtr<UPathFollowingComponent> WeakPathComp = PathComp;
	MovingAgents.Add(WeakPathComp);

	if (OnMoveReachedLink.IsBound())
	{
		OnMoveReachedLink.Execute(this, PathComp, DestPoint);
		return true;
	}

	return false;
}

void UNavLinkCustomComponent::OnLinkMoveFinished(UPathFollowingComponent* PathComp)
{
	TWeakObjectPtr<UPathFollowingComponent> WeakPathComp(PathComp);
	MovingAgents.Remove(WeakPathComp);
}

void UNavLinkCustomComponent::GetNavigationData(FNavigationRelevantData& Data) const
{
	TArray<FNavigationLink> NavLinks;
	FNavigationLink LinkMod = GetLinkModifier();
	LinkMod.MaxFallDownLength = 0.f;
	LinkMod.LeftProjectHeight = 0.f;
	NavLinks.Add(LinkMod);
	NavigationHelper::ProcessNavLinkAndAppend(&Data.Modifiers, GetOwner(), NavLinks);

	if (bCreateBoxObstacle)
	{
		Data.Modifiers.Add(FAreaNavModifier(FBox::BuildAABB(ObstacleOffset, ObstacleExtent), GetOwner()->GetTransform(), ObstacleAreaClass));
	}
}

void UNavLinkCustomComponent::CalcAndCacheBounds() const
{
	Bounds = FBox(ForceInit);
	Bounds += GetStartPoint();
	Bounds += GetEndPoint();

	if (bCreateBoxObstacle)
	{
		FBox ObstacleBounds = FBox::BuildAABB(ObstacleOffset, ObstacleExtent);
		Bounds += ObstacleBounds.TransformBy(GetOwner()->GetTransform());
	}
}

void UNavLinkCustomComponent::OnRegister()
{
	Super::OnRegister();

	if (NavLinkUserId == 0)
	{
		NavLinkUserId = INavLinkCustomInterface::GetUniqueId();
	}

	UNavigationSystem::RequestCustomLinkRegistering(*this, this);
}

void UNavLinkCustomComponent::OnUnregister()
{
	Super::OnUnregister();

	UNavigationSystem::RequestCustomLinkUnregistering(*this, this);
}

void UNavLinkCustomComponent::SetLinkData(const FVector& RelativeStart, const FVector& RelativeEnd, ENavLinkDirection::Type Direction)
{
	LinkRelativeStart = RelativeStart;
	LinkRelativeEnd = RelativeEnd;
	LinkDirection = Direction;
	
	RefreshNavigationModifiers();
	MarkRenderStateDirty();
}

FNavigationLink UNavLinkCustomComponent::GetLinkModifier() const
{
	return INavLinkCustomInterface::GetModifier(this);
}

void UNavLinkCustomComponent::SetEnabledArea(TSubclassOf<UNavArea> AreaClass)
{
	EnabledAreaClass = AreaClass;
	if (IsNavigationRelevant() && bLinkEnabled && GetWorld() != nullptr )
	{
		UNavigationSystem* NavSys = GetWorld()->GetNavigationSystem();
		NavSys->UpdateCustomLink(this);
	}
}

void UNavLinkCustomComponent::SetDisabledArea(TSubclassOf<UNavArea> AreaClass)
{
	DisabledAreaClass = AreaClass;
	if (IsNavigationRelevant() && !bLinkEnabled && GetWorld() != nullptr )
	{
		UNavigationSystem* NavSys = GetWorld()->GetNavigationSystem();
		NavSys->UpdateCustomLink(this);
	}
}

void UNavLinkCustomComponent::AddNavigationObstacle(TSubclassOf<UNavArea> AreaClass, const FVector& BoxExtent, const FVector& BoxOffset)
{
	ObstacleOffset = BoxOffset;
	ObstacleExtent = BoxExtent;
	ObstacleAreaClass = AreaClass;
	bCreateBoxObstacle = true;

	RefreshNavigationModifiers();
}

void UNavLinkCustomComponent::ClearNavigationObstacle()
{
	ObstacleAreaClass = NULL;
	bCreateBoxObstacle = false;

	RefreshNavigationModifiers();
}

void UNavLinkCustomComponent::SetEnabled(bool bNewEnabled)
{
	if (bLinkEnabled != bNewEnabled)
	{
		bLinkEnabled = bNewEnabled;

		UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
		if (NavSys)
		{
			NavSys->UpdateCustomLink(this);
		}

		if (GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(TimerHandle_BroadcastStateChange);

			if ((bLinkEnabled && bNotifyWhenEnabled) || (!bLinkEnabled && bNotifyWhenDisabled))
			{
				BroadcastStateChange();
			}
		}
	}
}

void UNavLinkCustomComponent::SetMoveReachedLink(FOnMoveReachedLink const& InDelegate)
{
	OnMoveReachedLink = InDelegate;
}

bool UNavLinkCustomComponent::HasMovingAgents() const
{
	for (int32 i = 0; i < MovingAgents.Num(); i++)
	{
		if (MovingAgents[i].IsValid())
		{
			return true;
		}
	}

	return false;
}

void UNavLinkCustomComponent::SetBroadcastData(float Radius, ECollisionChannel TraceChannel, float Interval)
{
	BroadcastRadius = Radius;
	BroadcastChannel = TraceChannel;
	BroadcastInterval = Interval;
}

void UNavLinkCustomComponent::SendBroadcastWhenEnabled(bool bEnabled)
{
	bNotifyWhenEnabled = bEnabled;
}

void UNavLinkCustomComponent::SendBroadcastWhenDisabled(bool bEnabled)
{
	bNotifyWhenDisabled = bEnabled;
}

void UNavLinkCustomComponent::CollectNearbyAgents(TArray<UPathFollowingComponent*>& NotifyList)
{
	AActor* MyOwner = GetOwner();
	if (BroadcastRadius < KINDA_SMALL_NUMBER || MyOwner == NULL)
	{
		return;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SmartLinkBroadcastTrace), false, MyOwner);
	TArray<FOverlapResult> OverlapsL, OverlapsR;

	const FVector LocationL = GetStartPoint();
	const FVector LocationR = GetEndPoint();
	const float LinkDistSq = (LocationL - LocationR).SizeSquared();
	const float DistThresholdSq = FMath::Square(BroadcastRadius * 0.25f);
	if (LinkDistSq > DistThresholdSq)
	{
		GetWorld()->OverlapMultiByChannel(OverlapsL, LocationL, FQuat::Identity, BroadcastChannel, FCollisionShape::MakeSphere(BroadcastRadius), Params);
		GetWorld()->OverlapMultiByChannel(OverlapsR, LocationR, FQuat::Identity, BroadcastChannel, FCollisionShape::MakeSphere(BroadcastRadius), Params);
	}
	else
	{
		const FVector MidPoint = (LocationL + LocationR) * 0.5f;
		GetWorld()->OverlapMultiByChannel(OverlapsL, MidPoint, FQuat::Identity, BroadcastChannel, FCollisionShape::MakeSphere(BroadcastRadius), Params);
	}

	TArray<APawn*> PawnList;
	for (int32 i = 0; i < OverlapsL.Num(); i++)
	{
		APawn* MovingPawn = Cast<APawn>(OverlapsL[i].GetActor());
		if (MovingPawn && MovingPawn->GetController())
		{
			PawnList.Add(MovingPawn);
		}
	}
	for (int32 i = 0; i < OverlapsR.Num(); i++)
	{
		APawn* MovingPawn = Cast<APawn>(OverlapsR[i].GetActor());
		if (MovingPawn && MovingPawn->GetController())
		{
			PawnList.AddUnique(MovingPawn);
		}
	}

	for (int32 i = 0; i < PawnList.Num(); i++)
	{
		UPathFollowingComponent* NavComp = PawnList[i]->GetController()->FindComponentByClass<UPathFollowingComponent>();
		if (NavComp)// && NavComp->WantsSmartLinkUpdates())
		{
			NotifyList.Add(NavComp);
		}
	}
}

void UNavLinkCustomComponent::BroadcastStateChange()
{
	TArray<UPathFollowingComponent*> NearbyAgents;

	CollectNearbyAgents(NearbyAgents);
	OnBroadcastFilter.ExecuteIfBound(this, NearbyAgents);

	for (int32 i = 0; i < NearbyAgents.Num(); i++)
	{
//		NearbyAgents[i]->OnCustomLinkBroadcast(this);
	}

	if (BroadcastInterval > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(TimerHandle_BroadcastStateChange, this, &UNavLinkCustomComponent::BroadcastStateChange, BroadcastInterval);
	}
}

FVector UNavLinkCustomComponent::GetStartPoint() const
{
	return GetOwner()->GetTransform().TransformPosition(LinkRelativeStart);
}

FVector UNavLinkCustomComponent::GetEndPoint() const
{
	return GetOwner()->GetTransform().TransformPosition(LinkRelativeEnd);
}
