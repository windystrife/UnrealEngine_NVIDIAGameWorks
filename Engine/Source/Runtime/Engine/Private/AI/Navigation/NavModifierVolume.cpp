// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AI/Navigation/NavModifierVolume.h"
#include "AI/Navigation/NavigationSystem.h"
#include "AI/NavigationModifier.h"
#include "AI/Navigation/NavAreas/NavArea_Null.h"
#include "AI/NavigationOctree.h"
#include "Components/BrushComponent.h"
#include "AI/NavigationSystemHelpers.h"
#include "Engine/CollisionProfile.h"

//----------------------------------------------------------------------//
// ANavModifierVolume
//----------------------------------------------------------------------//
ANavModifierVolume::ANavModifierVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, AreaClass(UNavArea_Null::StaticClass())
{
	if (GetBrushComponent())
	{
		GetBrushComponent()->bGenerateOverlapEvents = false;
		GetBrushComponent()->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	}
}

void ANavModifierVolume::GetNavigationData(FNavigationRelevantData& Data) const
{
	if (Brush && AreaClass && AreaClass != UNavigationSystem::GetDefaultWalkableArea())
	{
		FAreaNavModifier AreaMod(GetBrushComponent(), AreaClass);
		Data.Modifiers.Add(AreaMod);
	}
}

FBox ANavModifierVolume::GetNavigationBounds() const
{
	return GetComponentsBoundingBox(/*bNonColliding=*/ true);
}

void ANavModifierVolume::SetAreaClass(TSubclassOf<UNavArea> NewAreaClass)
{
	if (NewAreaClass != AreaClass)
	{
		AreaClass = NewAreaClass;

		UNavigationSystem::UpdateActorInNavOctree(*this);
	}
}

void ANavModifierVolume::RebuildNavigationData()
{
	UNavigationSystem::UpdateActorInNavOctree(*this);
}

#if WITH_EDITOR

void ANavModifierVolume::PostEditUndo()
{
	Super::PostEditUndo();

	UNavigationSystem::UpdateActorInNavOctree(*this);
}

void ANavModifierVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	static const FName NAME_AreaClass = GET_MEMBER_NAME_CHECKED(ANavModifierVolume, AreaClass);
	static const FName NAME_BrushComponent = TEXT("BrushComponent");

	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	const FName PropName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropName == NAME_AreaClass)
	{
		UNavigationSystem::UpdateActorInNavOctree(*this);
	}
	else if (PropName == NAME_BrushComponent)
	{
		if (GetBrushComponent())
		{
			if (GetBrushComponent()->GetBodySetup() && NavigationHelper::IsBodyNavigationRelevant(*GetBrushComponent()->GetBodySetup()))
			{
				UNavigationSystem::UpdateActorInNavOctree(*this);
			}
			else
			{
				UNavigationSystem::OnActorUnregistered(this);
			}
		}
	}
}

#endif
