// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MapBuildData.cpp
=============================================================================*/

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Engine/Level.h"
#include "GameFramework/Actor.h"
#include "LightMap.h"
#include "UObject/UObjectAnnotation.h"
#include "PrecomputedLightVolume.h"
#include "PrecomputedVolumetricLightmap.h"
#include "Engine/MapBuildDataRegistry.h"
#include "ShadowMap.h"
#include "UObject/Package.h"
#include "EngineUtils.h"
#include "Components/ModelComponent.h"
#include "ComponentRecreateRenderStateContext.h"
#include "UObject/RenderingObjectVersion.h"
#include "ContentStreaming.h"

FArchive& operator<<(FArchive& Ar, FMeshMapBuildData& MeshMapBuildData)
{
	Ar << MeshMapBuildData.LightMap;
	Ar << MeshMapBuildData.ShadowMap;
	Ar << MeshMapBuildData.IrrelevantLights;
	MeshMapBuildData.PerInstanceLightmapData.BulkSerialize(Ar);

	return Ar;
}

ULevel* UWorld::GetActiveLightingScenario() const
{
	for (int32 LevelIndex = 0; LevelIndex < Levels.Num(); LevelIndex++)
	{
		ULevel* LocalLevel = Levels[LevelIndex];

		if (LocalLevel->bIsVisible && LocalLevel->bIsLightingScenario)
		{
			return LocalLevel;
		}
	}

	return NULL;
}

void UWorld::PropagateLightingScenarioChange(bool bLevelWasMadeVisible)
{
	for (FActorIterator It(this); It; ++It)
	{
		TInlineComponentArray<USceneComponent*> Components;
		(*It)->GetComponents(Components);

		for (int32 ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++)
		{
			USceneComponent* CurrentComponent = Components[ComponentIndex];
			CurrentComponent->PropagateLightingScenarioChange();
		}
	}

	for (ULevel* Level : GetLevels())
	{
		Level->ReleaseRenderingResources();
		Level->InitializeRenderingResources();

		for (UModelComponent* ModelComponent : Level->ModelComponents)
		{
			ModelComponent->PropagateLightingScenarioChange();
		}
	}

	IStreamingManager::Get().PropagateLightingScenarioChange();

	// Skipping the reflection capture update if made invisible, in most cases another lighting scenario level will be made visible shortly after, 
	// Or we're unloading all levels and then it doesn't matter if lighting is updated.
	if (bLevelWasMadeVisible)
	{
		//@todo - store reflection capture data in UMapBuildDataRegistry so it can work with multiple lighting scenarios without forcing a recapture
		UpdateAllReflectionCaptures();
	}
}

UMapBuildDataRegistry* CreateRegistryForLegacyMap(ULevel* Level)
{
	static FName RegistryName(TEXT("MapBuildDataRegistry"));
	// Create a new registry for legacy map build data, but put it in the level's package.  
	// This avoids creating a new package during cooking which the cooker won't know about.
	Level->MapBuildData = NewObject<UMapBuildDataRegistry>(Level->GetOutermost(), RegistryName, RF_NoFlags);
	return Level->MapBuildData;
}

void ULevel::HandleLegacyMapBuildData()
{
	if (GComponentsWithLegacyLightmaps.GetAnnotationMap().Num() > 0 
		|| GLevelsWithLegacyBuildData.GetAnnotationMap().Num() > 0
		|| GLightComponentsWithLegacyBuildData.GetAnnotationMap().Num() > 0)
	{
		FLevelLegacyMapBuildData LegacyLevelData = GLevelsWithLegacyBuildData.GetAndRemoveAnnotation(this);

		UMapBuildDataRegistry* Registry = NULL;
		if (LegacyLevelData.Id != FGuid())
		{
			Registry = CreateRegistryForLegacyMap(this);
			Registry->AddLevelPrecomputedLightVolumeBuildData(LegacyLevelData.Id, LegacyLevelData.Data);
		}

		for (int32 ActorIndex = 0; ActorIndex < Actors.Num(); ActorIndex++)
		{
			if (!Actors[ActorIndex])
			{
				continue;
			}

			TInlineComponentArray<UActorComponent*> Components;
			Actors[ActorIndex]->GetComponents(Components);

			for (int32 ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++)
			{
				UActorComponent* CurrentComponent = Components[ComponentIndex];
				FMeshMapBuildLegacyData LegacyMeshData = GComponentsWithLegacyLightmaps.GetAndRemoveAnnotation(CurrentComponent);

				for (int32 EntryIndex = 0; EntryIndex < LegacyMeshData.Data.Num(); EntryIndex++)
				{
					if (!Registry)
					{
						Registry = CreateRegistryForLegacyMap(this);
					}

					FMeshMapBuildData& DestData = Registry->AllocateMeshBuildData(LegacyMeshData.Data[EntryIndex].Key, false);
					DestData = *LegacyMeshData.Data[EntryIndex].Value;
					delete LegacyMeshData.Data[EntryIndex].Value;
				}

				FLightComponentLegacyMapBuildData LegacyLightData = GLightComponentsWithLegacyBuildData.GetAndRemoveAnnotation(CurrentComponent);

				if (LegacyLightData.Id != FGuid())
				{
					FLightComponentMapBuildData& DestData = Registry->FindOrAllocateLightBuildData(LegacyLightData.Id, false);
					DestData = *LegacyLightData.Data;
					delete LegacyLightData.Data;
				}
			}
		}

		for (UModelComponent* ModelComponent : ModelComponents)
		{
			ModelComponent->PropagateLightingScenarioChange();
			FMeshMapBuildLegacyData LegacyData = GComponentsWithLegacyLightmaps.GetAndRemoveAnnotation(ModelComponent);

			for (int32 EntryIndex = 0; EntryIndex < LegacyData.Data.Num(); EntryIndex++)
			{
				if (!Registry)
				{
					Registry = CreateRegistryForLegacyMap(this);
				}

				FMeshMapBuildData& DestData = Registry->AllocateMeshBuildData(LegacyData.Data[EntryIndex].Key, false);
				DestData = *LegacyData.Data[EntryIndex].Value;
				delete LegacyData.Data[EntryIndex].Value;
			}
		}
	}
}

FMeshMapBuildData::FMeshMapBuildData()
{}

FMeshMapBuildData::~FMeshMapBuildData()
{}

void FMeshMapBuildData::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (LightMap)
	{
		LightMap->AddReferencedObjects(Collector);
	}

	if (ShadowMap)
	{
		ShadowMap->AddReferencedObjects(Collector);
	}
}

void FStaticShadowDepthMapData::Empty()
{
	ShadowMapSizeX = 0;
	ShadowMapSizeY = 0;
	DepthSamples.Empty();
}

FArchive& operator<<(FArchive& Ar, FStaticShadowDepthMapData& ShadowMapData)
{
	Ar << ShadowMapData.WorldToLight;
	Ar << ShadowMapData.ShadowMapSizeX;
	Ar << ShadowMapData.ShadowMapSizeY;
	Ar << ShadowMapData.DepthSamples;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FLightComponentMapBuildData& LightBuildData)
{
	Ar << LightBuildData.ShadowMapChannel;
	Ar << LightBuildData.DepthMap;
	return Ar;
}

UMapBuildDataRegistry::UMapBuildDataRegistry(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LevelLightingQuality = Quality_MAX;
}

void UMapBuildDataRegistry::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	FStripDataFlags StripFlags(Ar, 0);

	Ar.UsingCustomVersion(FRenderingObjectVersion::GUID);

	if (!StripFlags.IsDataStrippedForServer())
	{
		Ar << MeshBuildData;
		Ar << LevelPrecomputedLightVolumeBuildData;

		if (Ar.CustomVer(FRenderingObjectVersion::GUID) >= FRenderingObjectVersion::VolumetricLightmaps)
		{
			Ar << LevelPrecomputedVolumetricLightmapBuildData;
		}

		Ar << LightBuildData;
	}
}

void UMapBuildDataRegistry::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);

	UMapBuildDataRegistry* TypedThis = Cast<UMapBuildDataRegistry>(InThis);
	check(TypedThis);

	for (TMap<FGuid, FMeshMapBuildData>::TIterator It(TypedThis->MeshBuildData); It; ++It)
	{
		It.Value().AddReferencedObjects(Collector);
	}
}

void UMapBuildDataRegistry::BeginDestroy()
{
	Super::BeginDestroy();

	ReleaseResources();

	// Start a fence to track when BeginReleaseResource has completed
	DestroyFence.BeginFence();
}

bool UMapBuildDataRegistry::IsReadyForFinishDestroy()
{
	return Super::IsReadyForFinishDestroy() && DestroyFence.IsFenceComplete();
}

void UMapBuildDataRegistry::FinishDestroy()
{
	Super::FinishDestroy();

	EmptyData();
}

FMeshMapBuildData& UMapBuildDataRegistry::AllocateMeshBuildData(const FGuid& MeshId, bool bMarkDirty)
{
	check(MeshId.IsValid());

	if (bMarkDirty)
	{
		MarkPackageDirty();
	}
	
	return MeshBuildData.Add(MeshId, FMeshMapBuildData());
}

const FMeshMapBuildData* UMapBuildDataRegistry::GetMeshBuildData(FGuid MeshId) const
{
	return MeshBuildData.Find(MeshId);
}

FMeshMapBuildData* UMapBuildDataRegistry::GetMeshBuildData(FGuid MeshId)
{
	return MeshBuildData.Find(MeshId);
}

FPrecomputedLightVolumeData& UMapBuildDataRegistry::AllocateLevelPrecomputedLightVolumeBuildData(const FGuid& LevelId)
{
	check(LevelId.IsValid());
	MarkPackageDirty();
	return *LevelPrecomputedLightVolumeBuildData.Add(LevelId, new FPrecomputedLightVolumeData());
}

void UMapBuildDataRegistry::AddLevelPrecomputedLightVolumeBuildData(const FGuid& LevelId, FPrecomputedLightVolumeData* InData)
{
	check(LevelId.IsValid());
	LevelPrecomputedLightVolumeBuildData.Add(LevelId, InData);
}

const FPrecomputedLightVolumeData* UMapBuildDataRegistry::GetLevelPrecomputedLightVolumeBuildData(FGuid LevelId) const
{
	const FPrecomputedLightVolumeData* const * DataPtr = LevelPrecomputedLightVolumeBuildData.Find(LevelId);
	
	if (DataPtr)
	{
		return *DataPtr;
	}

	return NULL;
}

FPrecomputedLightVolumeData* UMapBuildDataRegistry::GetLevelPrecomputedLightVolumeBuildData(FGuid LevelId)
{
	FPrecomputedLightVolumeData** DataPtr = LevelPrecomputedLightVolumeBuildData.Find(LevelId);

	if (DataPtr)
	{
		return *DataPtr;
	}

	return NULL;
}

FPrecomputedVolumetricLightmapData& UMapBuildDataRegistry::AllocateLevelPrecomputedVolumetricLightmapBuildData(const FGuid& LevelId)
{
	check(LevelId.IsValid());
	MarkPackageDirty();
	return *LevelPrecomputedVolumetricLightmapBuildData.Add(LevelId, new FPrecomputedVolumetricLightmapData());
}

void UMapBuildDataRegistry::AddLevelPrecomputedVolumetricLightmapBuildData(const FGuid& LevelId, FPrecomputedVolumetricLightmapData* InData)
{
	check(LevelId.IsValid());
	LevelPrecomputedVolumetricLightmapBuildData.Add(LevelId, InData);
}

const FPrecomputedVolumetricLightmapData* UMapBuildDataRegistry::GetLevelPrecomputedVolumetricLightmapBuildData(FGuid LevelId) const
{
	const FPrecomputedVolumetricLightmapData* const * DataPtr = LevelPrecomputedVolumetricLightmapBuildData.Find(LevelId);
	
	if (DataPtr)
	{
		return *DataPtr;
	}

	return NULL;
}

FPrecomputedVolumetricLightmapData* UMapBuildDataRegistry::GetLevelPrecomputedVolumetricLightmapBuildData(FGuid LevelId)
{
	FPrecomputedVolumetricLightmapData** DataPtr = LevelPrecomputedVolumetricLightmapBuildData.Find(LevelId);

	if (DataPtr)
	{
		return *DataPtr;
	}

	return NULL;
}

FLightComponentMapBuildData& UMapBuildDataRegistry::FindOrAllocateLightBuildData(FGuid LightId, bool bMarkDirty)
{
	check(LightId.IsValid());

	if (bMarkDirty)
	{
		MarkPackageDirty();
	}
	
	return LightBuildData.FindOrAdd(LightId);
}

const FLightComponentMapBuildData* UMapBuildDataRegistry::GetLightBuildData(FGuid LightId) const
{
	return LightBuildData.Find(LightId);
}

FLightComponentMapBuildData* UMapBuildDataRegistry::GetLightBuildData(FGuid LightId)
{
	return LightBuildData.Find(LightId);
}

void UMapBuildDataRegistry::InvalidateStaticLighting(UWorld* World)
{
	if (MeshBuildData.Num() > 0 || LightBuildData.Num() > 0)
	{
		FGlobalComponentRecreateRenderStateContext Context;
		MeshBuildData.Empty();
		LightBuildData.Empty();
	}
	
	if (LevelPrecomputedLightVolumeBuildData.Num() > 0 || LevelPrecomputedVolumetricLightmapBuildData.Num() > 0)
	{
		for (int32 LevelIndex = 0; LevelIndex < World->GetNumLevels(); LevelIndex++)
		{
			World->GetLevel(LevelIndex)->ReleaseRenderingResources();
		}

		ReleaseResources();

		// Make sure the RT has processed the release command before we delete any FPrecomputedLightVolume's
		FlushRenderingCommands();

		EmptyData();
	}
	
	MarkPackageDirty();
}

bool UMapBuildDataRegistry::IsLegacyBuildData() const
{
	return GetOutermost()->ContainsMap();
}

void UMapBuildDataRegistry::ReleaseResources()
{
	for (TMap<FGuid, FPrecomputedVolumetricLightmapData*>::TIterator It(LevelPrecomputedVolumetricLightmapBuildData); It; ++It)
	{
		BeginReleaseResource(It.Value());
	}
}

void UMapBuildDataRegistry::EmptyData()
{
	MeshBuildData.Empty();
	LightBuildData.Empty();

	for (TMap<FGuid, FPrecomputedLightVolumeData*>::TIterator It(LevelPrecomputedLightVolumeBuildData); It; ++It)
	{
		delete It.Value();
	}

	LevelPrecomputedLightVolumeBuildData.Empty();

	for (TMap<FGuid, FPrecomputedVolumetricLightmapData*>::TIterator It(LevelPrecomputedVolumetricLightmapBuildData); It; ++It)
	{
		delete It.Value();
	}

	LevelPrecomputedVolumetricLightmapBuildData.Empty();
}

FUObjectAnnotationSparse<FMeshMapBuildLegacyData, true> GComponentsWithLegacyLightmaps;
FUObjectAnnotationSparse<FLevelLegacyMapBuildData, true> GLevelsWithLegacyBuildData;
FUObjectAnnotationSparse<FLightComponentLegacyMapBuildData, true> GLightComponentsWithLegacyBuildData;
