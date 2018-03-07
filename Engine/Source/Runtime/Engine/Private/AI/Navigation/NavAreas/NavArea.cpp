// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AI/Navigation/NavAreas/NavArea.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Modules/ModuleManager.h"


UNavArea::UNavArea(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DefaultCost = 1.f;
	FixedAreaEnteringCost = 0.f;
	DrawColor = FColor::Magenta;
	SupportedAgentsBits = 0xffffffff;
	// NOTE! AreaFlags == 0 means UNWALKABLE!
	AreaFlags = 1;
}

void UNavArea::FinishDestroy()
{
	if (HasAnyFlags(RF_ClassDefaultObject)
#if WITH_HOT_RELOAD
		&& !GIsHotReload
#endif // WITH_HOT_RELOAD
		)
	{
		UNavigationSystem::RequestAreaUnregistering(GetClass());
	}

	Super::FinishDestroy();
}

void UNavArea::PostLoad()
{
	Super::PostLoad();
	RegisterArea();
}

void UNavArea::PostInitProperties()
{
	Super::PostInitProperties();
	RegisterArea();
}

void UNavArea::RegisterArea()
{
	if (HasAnyFlags(RF_ClassDefaultObject)
#if WITH_HOT_RELOAD
		&& !GIsHotReload
#endif // WITH_HOT_RELOAD
		)
	{
		UNavigationSystem::RequestAreaRegistering(GetClass());
	}

	if (!SupportedAgents.IsInitialized())
	{
		SupportedAgents.bSupportsAgent0 = bSupportsAgent0;
		SupportedAgents.bSupportsAgent1 = bSupportsAgent1;
		SupportedAgents.bSupportsAgent2 = bSupportsAgent2;
		SupportedAgents.bSupportsAgent3 = bSupportsAgent3;
		SupportedAgents.bSupportsAgent4 = bSupportsAgent4;
		SupportedAgents.bSupportsAgent5 = bSupportsAgent5;
		SupportedAgents.bSupportsAgent6 = bSupportsAgent6;
		SupportedAgents.bSupportsAgent7 = bSupportsAgent7;
		SupportedAgents.bSupportsAgent8 = bSupportsAgent8;
		SupportedAgents.bSupportsAgent9 = bSupportsAgent9;
		SupportedAgents.bSupportsAgent10 = bSupportsAgent10;
		SupportedAgents.bSupportsAgent11 = bSupportsAgent11;
		SupportedAgents.bSupportsAgent12 = bSupportsAgent12;
		SupportedAgents.bSupportsAgent13 = bSupportsAgent13;
		SupportedAgents.bSupportsAgent14 = bSupportsAgent14;
		SupportedAgents.bSupportsAgent15 = bSupportsAgent15;
		SupportedAgents.MarkInitialized();
	}
}

void UNavArea::Serialize(FArchive& Ar)
{
	if (Ar.IsSaving() && !SupportedAgents.IsInitialized())
	{
		SupportedAgents.MarkInitialized();
	}
		
	Super::Serialize(Ar);
}

FColor UNavArea::GetColor(UClass* AreaDefinitionClass)
{
	return AreaDefinitionClass ? AreaDefinitionClass->GetDefaultObject<UNavArea>()->DrawColor : FColor::Black;
}

void UNavArea::CopyFrom(TSubclassOf<UNavArea> AreaClass)
{
	if (AreaClass)
	{
		UNavArea* DefArea = (UNavArea*)AreaClass->GetDefaultObject();

		DefaultCost = DefArea->DefaultCost;
		FixedAreaEnteringCost = DefArea->GetFixedAreaEnteringCost();
		AreaFlags = DefArea->GetAreaFlags();
		DrawColor = DefArea->DrawColor;

		// don't copy supported agents bits
	}
}

#if WITH_EDITOR
void UNavArea::UpdateAgentConfig()
{
	// empty in base class
}
#endif
