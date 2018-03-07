// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AI/Navigation/NavAreas/NavAreaMeta.h"
#include "AI/Navigation/NavigationSystem.h"

UNavAreaMeta::UNavAreaMeta(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

TSubclassOf<UNavArea> UNavAreaMeta::PickAreaClass(const AActor* Actor, const FNavAgentProperties& NavAgent) const 
{ 
	UE_LOG(LogNavigation, Warning, TEXT("UNavAreaMeta::PickAreaClass called. UNavAreaMeta is an abstract class and should never get used directly!"));

	return GetClass(); 
}

int8 UNavAreaMeta::GetNavAgentIndex(const FNavAgentProperties& NavAgent) const
{
	const UNavigationSystem* DefNavSys = (UNavigationSystem*)(UNavigationSystem::StaticClass()->GetDefaultObject());
	return DefNavSys->GetSupportedAgentIndex(NavAgent);
}
