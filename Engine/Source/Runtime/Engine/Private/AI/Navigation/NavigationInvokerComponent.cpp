// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AI/Navigation/NavigationInvokerComponent.h"
#include "AI/Navigation/NavigationSystem.h"

UNavigationInvokerComponent::UNavigationInvokerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TileGenerationRadius(3000)
	, TileRemovalRadius(5000)
{
	bAutoActivate = true;
}

void UNavigationInvokerComponent::Activate(bool bReset)
{
	Super::Activate(bReset);

	AActor* Owner = GetOwner();
	if (Owner)
	{
		UNavigationSystem::RegisterNavigationInvoker(*Owner, TileGenerationRadius, TileRemovalRadius);
	}
}

void UNavigationInvokerComponent::Deactivate()
{
	Super::Deactivate();

	AActor* Owner = GetOwner();
	if (Owner)
	{
		UNavigationSystem::UnregisterNavigationInvoker(*Owner);
	}
}

void UNavigationInvokerComponent::RegisterWithNavigationSystem(UNavigationSystem& NavSys)
{
	if (IsActive())
	{
		AActor* Owner = GetOwner();
		if (Owner)
		{
			NavSys.RegisterInvoker(*Owner, TileGenerationRadius, TileRemovalRadius);
		}
	}
}
