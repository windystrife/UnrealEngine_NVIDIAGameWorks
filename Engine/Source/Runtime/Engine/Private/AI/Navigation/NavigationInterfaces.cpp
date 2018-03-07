// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Templates/Casts.h"
#include "AI/Navigation/NavAgentInterface.h"
#include "AI/Navigation/NavRelevantInterface.h"
#include "AI/Navigation/NavigationPathGenerator.h"
#include "AI/Navigation/NavNodeInterface.h"
#include "AI/Navigation/NavLinkDefinition.h"
#include "AI/Navigation/NavLinkHostInterface.h"
#include "AI/Navigation/NavPathObserverInterface.h"
#include "AI/Navigation/NavLinkCustomInterface.h"
#include "AI/Navigation/NavEdgeProviderInterface.h"
#include "AI/RVOAvoidanceInterface.h"

uint32 INavLinkCustomInterface::NextUniqueId = 1;

URVOAvoidanceInterface::URVOAvoidanceInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UNavAgentInterface::UNavAgentInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UNavigationPathGenerator::UNavigationPathGenerator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UNavNodeInterface::UNavNodeInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UNavLinkHostInterface::UNavLinkHostInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UNavLinkCustomInterface::UNavLinkCustomInterface(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

UObject* INavLinkCustomInterface::GetLinkOwner() const
{
	return Cast<UObject>((INavLinkCustomInterface*)this);
}

uint32 INavLinkCustomInterface::GetUniqueId()
{
	return NextUniqueId++;
}

void INavLinkCustomInterface::UpdateUniqueId(uint32 AlreadyUsedId)
{
	NextUniqueId = FMath::Max(NextUniqueId, AlreadyUsedId + 1);
}

FNavigationLink INavLinkCustomInterface::GetModifier(const INavLinkCustomInterface* CustomNavLink)
{
	FNavigationLink LinkMod;
	LinkMod.SetAreaClass(CustomNavLink->GetLinkAreaClass());
	LinkMod.UserId = CustomNavLink->GetLinkId();

	ENavLinkDirection::Type LinkDirection = ENavLinkDirection::BothWays;
	CustomNavLink->GetLinkData(LinkMod.Left, LinkMod.Right, LinkDirection);
	LinkMod.Direction = LinkDirection;

	return LinkMod;
}

UNavPathObserverInterface::UNavPathObserverInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UNavRelevantInterface::UNavRelevantInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UNavEdgeProviderInterface::UNavEdgeProviderInterface(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

