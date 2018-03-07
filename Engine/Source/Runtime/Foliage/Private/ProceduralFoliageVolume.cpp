// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ProceduralFoliageVolume.h"
#include "Components/BrushComponent.h"
#include "ProceduralFoliageComponent.h"
#include "ProceduralFoliageSpawner.h"

AProceduralFoliageVolume::AProceduralFoliageVolume(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	ProceduralComponent = ObjectInitializer.CreateDefaultSubobject<UProceduralFoliageComponent>(this, TEXT("ProceduralFoliageComponent"));
	ProceduralComponent->SetSpawningVolume(this);

	if (UBrushComponent* MyBrushComponent = GetBrushComponent())
	{
		MyBrushComponent->SetCollisionObjectType(ECC_WorldStatic);
		MyBrushComponent->SetCollisionResponseToAllChannels(ECR_Ignore);

		// This is important because the volume overlaps with all procedural foliage
		// That means during streaming we'll get a huge hitch for UpdateOverlaps
		MyBrushComponent->bGenerateOverlapEvents = false;
	}
}

#if WITH_EDITOR

void AProceduralFoliageVolume::PostEditImport()
{
	// Make sure that this is the component's spawning volume
	ProceduralComponent->SetSpawningVolume(this);
}

bool AProceduralFoliageVolume::GetReferencedContentObjects(TArray<UObject*>& Objects) const
{
	Super::GetReferencedContentObjects(Objects);

	if (ProceduralComponent && ProceduralComponent->FoliageSpawner)
	{
		Objects.Add(ProceduralComponent->FoliageSpawner);
	}
	return true;
}

#endif
