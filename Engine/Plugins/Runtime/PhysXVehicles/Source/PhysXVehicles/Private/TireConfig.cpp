// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TireConfig.h"
#include "EngineDefines.h"
#include "PhysXPublic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#if WITH_PHYSX
#include "PhysXVehicleManager.h"
#endif

TArray<TWeakObjectPtr<UTireConfig>> UTireConfig::AllTireConfigs;

UTireConfig::UTireConfig()
{
	// Property initialization
	FrictionScale = 1.0f;
}

void UTireConfig::SetFrictionScale(float NewFrictionScale)
{
	if (NewFrictionScale != FrictionScale)
	{
		FrictionScale = NewFrictionScale;

		NotifyTireFrictionUpdated();
	}
}

void UTireConfig::SetPerMaterialFrictionScale(UPhysicalMaterial* PhysicalMaterial, float NewFrictionScale)
{
	// See if we already have an entry for this material
	bool bFoundEntry = false;
	for (FTireConfigMaterialFriction MatFriction : TireFrictionScales)
	{
		if (MatFriction.PhysicalMaterial == PhysicalMaterial)
		{
			// We do, update it
			MatFriction.FrictionScale = NewFrictionScale;
			bFoundEntry = true;
			break;
		}
	}

	// We don't have an entry, add one
	if (!bFoundEntry)
	{
		FTireConfigMaterialFriction MatFriction;
		MatFriction.PhysicalMaterial = PhysicalMaterial;
		MatFriction.FrictionScale = NewFrictionScale;
		TireFrictionScales.Add(MatFriction);
	}

	// Update friction table
	NotifyTireFrictionUpdated();
}


void UTireConfig::PostInitProperties()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		// Set our TireConfigID - either by finding an available slot or creating a new one
		int32 TireConfigIndex = AllTireConfigs.Find(NULL);

		if (TireConfigIndex == INDEX_NONE)
		{
			TireConfigIndex = AllTireConfigs.Add(this);
		}
		else
		{
			AllTireConfigs[TireConfigIndex] = this;
		}

		TireConfigID = (int32)TireConfigIndex;

		NotifyTireFrictionUpdated();
	}

	Super::PostInitProperties();
}

void UTireConfig::BeginDestroy()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		// free our TireTypeID
		check(AllTireConfigs.IsValidIndex(TireConfigID));
		check(AllTireConfigs[TireConfigID] == this);
		AllTireConfigs[TireConfigID] = NULL;

		NotifyTireFrictionUpdated();
	}

	Super::BeginDestroy();
}

void UTireConfig::NotifyTireFrictionUpdated()
{
#if WITH_PHYSX
	FPhysXVehicleManager::UpdateTireFrictionTable();
#endif // WITH_PHYSX
}

#if WITH_EDITOR
void UTireConfig::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	NotifyTireFrictionUpdated();
}
#endif //WITH_EDITOR

float UTireConfig::GetTireFriction(UPhysicalMaterial* PhysicalMaterial)
{
	// Get friction from tire config
	float Friction = (PhysicalMaterial != nullptr) ? PhysicalMaterial->Friction : 1.f;

	// Scale by tire config scale
	Friction *= FrictionScale;

	// See if we have a material-specific scale as well
	for (FTireConfigMaterialFriction MatFriction : TireFrictionScales)
	{
		if (MatFriction.PhysicalMaterial == PhysicalMaterial)
		{
			Friction *= MatFriction.FrictionScale;
			break;
		}
	}

	return Friction;
}


