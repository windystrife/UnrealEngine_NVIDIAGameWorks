// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LandscapeModule.h"
#include "Serialization/CustomVersion.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectHash.h"
#include "UObject/Package.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "LandscapeComponent.h"
#include "LandscapeVersion.h"
#include "LandscapeInfoMap.h"
#include "Materials/MaterialInstance.h"

// Register the custom version with core
FCustomVersionRegistration GRegisterLandscapeCustomVersion(FLandscapeCustomVersion::GUID, FLandscapeCustomVersion::LatestVersion, TEXT("Landscape"));

class FLandscapeModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	void StartupModule() override;
	void ShutdownModule() override;
};

/**
 * Add landscape-specific per-world data.
 *
 * @param World A pointer to world that this data should be created for.
 */
void AddPerWorldLandscapeData(UWorld* World)
{
	EObjectFlags NewLandscapeDataFlags = RF_NoFlags;
	if (!World->PerModuleDataObjects.FindItemByClass<ULandscapeInfoMap>())
	{
		if (World->HasAnyFlags(RF_Transactional))
		{
			NewLandscapeDataFlags = RF_Transactional;
		}
		ULandscapeInfoMap* InfoMap = NewObject<ULandscapeInfoMap>(GetTransientPackage(), NAME_None, NewLandscapeDataFlags);
		InfoMap->World = World;
		World->PerModuleDataObjects.Add(InfoMap);
	}
}

/**
 * Gets landscape-specific material's static parameters values.
 *
 * @param OutStaticParameterSet A set that should be updated with found parameters values.
 * @param Material Material instance to look for parameters.
 */
void LandscapeMaterialsParameterValuesGetter(FStaticParameterSet &OutStaticParameterSet, UMaterialInstance* Material);

/**
 * Updates landscape-specific material parameters.
 *
 * @param OutStaticParameterSet A set of parameters.
 * @param Material A material to update.
 */
bool LandscapeMaterialsParameterSetUpdater(FStaticParameterSet &OutStaticParameterSet, UMaterial* Material);

/**
 * Function that will fire every time a world is created.
 *
 * @param World A world that was created.
 * @param IVS Initialization values.
 */
void WorldCreationEventFunction(UWorld* World)
{
	AddPerWorldLandscapeData(World);
}

/**
 * Function that will fire every time a world is destroyed.
 *
 * @param World A world that's being destroyed.
 */
void WorldDestroyEventFunction(UWorld* World)
{
	World->PerModuleDataObjects.RemoveAll(
		[](UObject* Object)
		{
			return Object != nullptr && Object->IsA(ULandscapeInfoMap::StaticClass());
		}
	);
}

#if WITH_EDITOR
/**
 * Gets array of Landscape-specific textures and materials connected with given
 * level.
 *
 * @param Level Level to search textures and materials in.
 * @param OutTexturesAndMaterials (Output parameter) Array to fill.
 */
void GetLandscapeTexturesAndMaterials(ULevel* Level, TArray<UObject*>& OutTexturesAndMaterials)
{
	TArray<UObject*> ObjectsInLevel;
	const bool bIncludeNestedObjects = true;
	GetObjectsWithOuter(Level, ObjectsInLevel, bIncludeNestedObjects);
	for (auto* ObjInLevel : ObjectsInLevel)
	{
		ULandscapeComponent* LandscapeComponent = Cast<ULandscapeComponent>(ObjInLevel);
		if (LandscapeComponent)
		{
			LandscapeComponent->GetGeneratedTexturesAndMaterialInstances(OutTexturesAndMaterials);
		}
	}
}

/**
 * A function that fires everytime a world is renamed.
 *
 * @param World A world that was renamed.
 * @param InName New world name.
 * @param NewOuter New outer of the world after rename.
 * @param Flags Rename flags.
 * @param bShouldFailRename (Output parameter) If you set it to true, then the renaming process should fail.
 */
void WorldRenameEventFunction(UWorld* World, const TCHAR* InName, UObject* NewOuter, ERenameFlags Flags, bool& bShouldFailRename)
{
	// Also rename all textures and materials used by landscape components
	TArray<UObject*> LandscapeTexturesAndMaterials;
	GetLandscapeTexturesAndMaterials(World->PersistentLevel, LandscapeTexturesAndMaterials);
	UPackage* PersistentLevelPackage = World->PersistentLevel->GetOutermost();
	for (auto* OldTexOrMat : LandscapeTexturesAndMaterials)
	{
		if (OldTexOrMat && OldTexOrMat->GetOuter() == PersistentLevelPackage)
		{
			// The names for these objects are not important, just generate a new name to avoid collisions
			if (!OldTexOrMat->Rename(nullptr, NewOuter, Flags))
			{
				bShouldFailRename = true;
			}
		}
	}
}
#endif

/**
 * A function that fires everytime a world is duplicated.
 *
 * If there are some objects duplicated during this event fill out
 * ReplacementMap and ObjectsToFixReferences in order to properly fix
 * references in objects created during this duplication.
 *
 * @param World A world that was duplicated.
 * @param bDuplicateForPIE If this duplication was done for PIE.
 * @param ReplacementMap Replacement map (i.e. old object -> new object).
 * @param ObjectsToFixReferences Array of objects that may contain bad
 *		references to old objects.
 */
void WorldDuplicateEventFunction(UWorld* World, bool bDuplicateForPIE, TMap<UObject*, UObject*>& ReplacementMap, TArray<UObject*>& ObjectsToFixReferences)
{
	int32 Index;
	ULandscapeInfoMap* InfoMap;
	if (World->PerModuleDataObjects.FindItemByClass(&InfoMap, &Index))
	{
		ULandscapeInfoMap* NewInfoMap = Cast<ULandscapeInfoMap>( StaticDuplicateObject(InfoMap, InfoMap->GetOuter()) );
		NewInfoMap->World = World;

		World->PerModuleDataObjects[Index] = NewInfoMap;
	}
	else
	{
		AddPerWorldLandscapeData(World);
	}

#if WITH_EDITOR
	if (!bDuplicateForPIE)
	{
		UPackage* WorldPackage = World->GetOutermost();

		// Also duplicate all textures and materials used by landscape components
		TArray<UObject*> LandscapeTexturesAndMaterials;
		GetLandscapeTexturesAndMaterials(World->PersistentLevel, LandscapeTexturesAndMaterials);
		for (auto* OldTexOrMat : LandscapeTexturesAndMaterials)
		{
			if (OldTexOrMat && OldTexOrMat->GetOuter() != WorldPackage)
			{
				// The names for these objects are not important, just generate a new name to avoid collisions
				UObject* NewTextureOrMaterial = StaticDuplicateObject(OldTexOrMat, WorldPackage);
				ReplacementMap.Add(OldTexOrMat, NewTextureOrMaterial);

				// Materials hold references to the textures being moved, so they will need references fixed up as well
				if (OldTexOrMat->IsA(UMaterialInterface::StaticClass()))
				{
					ObjectsToFixReferences.Add(NewTextureOrMaterial);
				}
			}
		}
	}
#endif // WITH_EDITOR
}

void FLandscapeModule::StartupModule()
{
	// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)
	UMaterialInstance::CustomStaticParametersGetters.AddStatic(
		&LandscapeMaterialsParameterValuesGetter
	);

	UMaterialInstance::CustomParameterSetUpdaters.Add(
		UMaterialInstance::FCustomParameterSetUpdaterDelegate::CreateStatic(
			&LandscapeMaterialsParameterSetUpdater
		)
	);

#if WITH_EDITORONLY_DATA
	FWorldDelegates::OnPostWorldCreation.AddStatic(
		&WorldCreationEventFunction
	);
	FWorldDelegates::OnPreWorldFinishDestroy.AddStatic(
		&WorldDestroyEventFunction
	);
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	FWorldDelegates::OnPreWorldRename.AddStatic(
		&WorldRenameEventFunction
	);
#endif // WITH_EDITOR

	FWorldDelegates::OnPostDuplicate.AddStatic(
		&WorldDuplicateEventFunction
	);
}

void FLandscapeModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

IMPLEMENT_MODULE(FLandscapeModule, Landscape);
