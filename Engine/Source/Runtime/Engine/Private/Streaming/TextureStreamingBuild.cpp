// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
TextureStreamingBuild.cpp : Contains definitions to build texture streaming data.
=============================================================================*/

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Misc/ScopedSlowTask.h"
#include "Engine/Level.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/TextureStreamingTypes.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/FeedbackContext.h"
#include "Engine/Texture2D.h"
#include "DebugViewModeMaterialProxy.h"
#include "ShaderCompiler.h"
#include "Engine/StaticMesh.h"
#include "Streaming/TextureStreamingHelpers.h"
#include "UObject/UObjectIterator.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"

DEFINE_LOG_CATEGORY(TextureStreamingBuild);
#define LOCTEXT_NAMESPACE "TextureStreamingBuild"

ENGINE_API bool BuildTextureStreamingComponentData(UWorld* InWorld, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel, bool bFullRebuild, FSlowTask& BuildTextureStreamingTask)
{
#if WITH_EDITORONLY_DATA
	if (!InWorld)
	{
		return false;
	}

	const int32 NumActorsInWorld = GetNumActorsInWorld(InWorld);
	if (!NumActorsInWorld)
	{
		BuildTextureStreamingTask.EnterProgressFrame();
		// Can't early exit here as Level might need reset.
		// return true;
	}

	const double StartTime = FPlatformTime::Seconds();
	const float OneOverNumActorsInWorld = 1.f / (float)FMath::Max<int32>(NumActorsInWorld, 1); // Prevent div by 0

	// Used to reset per level index for textures.
	TArray<UTexture2D*> AllTextures;
	for (FObjectIterator Iter(UTexture2D::StaticClass()); Iter && bFullRebuild; ++Iter)
	{
		UTexture2D* Texture2D = Cast<UTexture2D>(*Iter);
		if (Texture2D)
		{
			AllTextures.Add(Texture2D);
		}
	}

	FScopedSlowTask SlowTask(1.f, (LOCTEXT("TextureStreamingBuild_ComponentDataUpdate", "Updating Component Data")));
	
	for (int32 LevelIndex = 0; LevelIndex < InWorld->GetNumLevels(); LevelIndex++)
	{
		ULevel* Level = InWorld->GetLevel(LevelIndex);
		if (!Level)
		{
			continue;
		}

		const bool bHadBuildData = Level->StreamingTextureGuids.Num() > 0 || Level->TextureStreamingResourceGuids.Num() > 0;

		Level->NumTextureStreamingUnbuiltComponents = 0;

		// When not rebuilding everything, we can't update those as we don't know how the current build data was computed.
		// Consequently, partial rebuilds are not allowed to recompute everything. What something is missing and can not be built,
		// the BuildStreamingData will return false in which case we increment NumTextureStreamingUnbuiltComponents.
		// This allows to keep track of full rebuild requirements.
		if (bFullRebuild)
		{
			Level->bTextureStreamingRotationChanged = false;
			Level->StreamingTextureGuids.Empty();
			Level->TextureStreamingResourceGuids.Empty();
			Level->NumTextureStreamingDirtyResources = 0; // This is persistent in order to be able to notify if a rebuild is required when running a cooked build.
		}

		TSet<FGuid> ResourceGuids;
		TSet<FGuid> DummyResourceGuids;

		for (AActor* Actor : Level->Actors)
		{
			BuildTextureStreamingTask.EnterProgressFrame(OneOverNumActorsInWorld);
			SlowTask.EnterProgressFrame(OneOverNumActorsInWorld);
			if (GWarn->ReceivedUserCancel())
			{
				return false;
			}

			// Check the actor after incrementing the progress.
			if (!Actor)
			{
				continue; 
			}

			TInlineComponentArray<UPrimitiveComponent*> Primitives;
			Actor->GetComponents<UPrimitiveComponent>(Primitives);

			for (UPrimitiveComponent* Primitive : Primitives)
			{
				if (!Primitive)
				{
					continue;
				}
				else if (!Primitive->HasAnyFlags(RF_Transactional))
				{
					// For non transactional primitives, like the one created from blueprints, we tolerate fails and we also don't store the guids.
					Primitive->BuildTextureStreamingData(bFullRebuild ? TSB_MapBuild : TSB_ViewMode, QualityLevel, FeatureLevel, DummyResourceGuids);
				}
				else if (!Primitive->BuildTextureStreamingData(bFullRebuild ? TSB_MapBuild : TSB_ViewMode, QualityLevel, FeatureLevel, ResourceGuids))
				{
					++Level->NumTextureStreamingUnbuiltComponents;
				}
			}
		}

		if (bFullRebuild)
		{
			// Reset LevelIndex to default for next use and build the level Guid array.
			for (UTexture2D* Texture2D : AllTextures)
			{
				checkSlow(Texture2D);
				Texture2D->LevelIndex = INDEX_NONE;
			}

			// Cleanup the asset references.
			ResourceGuids.Remove(FGuid()); // Remove the invalid guid.
			for (const FGuid& ResourceGuid : ResourceGuids)
			{
				Level->TextureStreamingResourceGuids.Add(ResourceGuid);
			}

			// Mark for resave if and only if rebuilding everything and also if data was updated.
			const bool bHasBuildData = Level->StreamingTextureGuids.Num() > 0 || Level->TextureStreamingResourceGuids.Num() > 0;
			if (bHadBuildData || bHasBuildData)
			{
				Level->MarkPackageDirty();
			}
		}
	}

	// Update TextureStreamer
	ULevel::BuildStreamingData(InWorld);

	UE_LOG(TextureStreamingBuild, Display, TEXT("Build Texture Streaming took %.3f seconds."), FPlatformTime::Seconds() - StartTime);
	return true;
#else
	UE_LOG(TextureStreamingBuild, Fatal,TEXT("Build Texture Streaming should not be called on a console"));
	return false;
#endif
}

#undef LOCTEXT_NAMESPACE

/**
 * Checks whether a UTexture2D is supposed to be streaming.
 * @param Texture	Texture to check
 * @return			true if the UTexture2D is supposed to be streaming
 */
bool IsStreamingTexture( const UTexture2D* Texture2D )
{
	return Texture2D && Texture2D->bIsStreamable && !Texture2D->NeverStream && Texture2D->GetNumMips() > Texture2D->GetNumNonStreamingMips();
}

uint32 PackRelativeBox(const FVector& RefOrigin, const FVector& RefExtent, const FVector& Origin, const FVector& Extent)
{
	const FVector RefMin = RefOrigin - RefExtent;
	// 15.5 and 31.5 have the / 2 scale included 
	const FVector PackScale = FVector(15.5f, 15.5f, 31.5f) / RefExtent.ComponentMax(FVector(KINDA_SMALL_NUMBER, KINDA_SMALL_NUMBER, KINDA_SMALL_NUMBER));

	const FVector Min = Origin - Extent;
	const FVector Max = Origin + Extent;

	const FVector RelMin = (Min - RefMin) * PackScale;
	const FVector RelMax = (Max - RefMin) * PackScale;

	const uint32 PackedMinX = (uint32)FMath::Clamp<int32>(FMath::FloorToInt(RelMin.X), 0, 31);
	const uint32 PackedMinY = (uint32)FMath::Clamp<int32>(FMath::FloorToInt(RelMin.Y), 0, 31);
	const uint32 PackedMinZ = (uint32)FMath::Clamp<int32>(FMath::FloorToInt(RelMin.Z), 0, 63);

	const uint32 PackedMaxX = (uint32)FMath::Clamp<int32>(FMath::CeilToInt(RelMax.X), 0, 31);
	const uint32 PackedMaxY = (uint32)FMath::Clamp<int32>(FMath::CeilToInt(RelMax.Y), 0, 31);
	const uint32 PackedMaxZ = (uint32)FMath::Clamp<int32>(FMath::CeilToInt(RelMax.Z), 0, 63);

	return PackedMinX | (PackedMinY << 5) | (PackedMinZ << 10) | (PackedMaxX << 16) | (PackedMaxY << 21) | (PackedMaxZ << 26);
}

uint32 PackRelativeBox(const FBox& RefBox, const FBox& Box)
{
	// 15.5 and 31.5 have the / 2 scale included 
	const FVector PackScale = FVector(15.5f, 15.5f, 31.5f) / RefBox.GetExtent().ComponentMax(FVector(KINDA_SMALL_NUMBER, KINDA_SMALL_NUMBER, KINDA_SMALL_NUMBER));

	const FVector RelMin = (Box.Min - RefBox.Min) * PackScale;
	const FVector RelMax = (Box.Max - RefBox.Min) * PackScale;

	const uint32 PackedMinX = (uint32)FMath::Clamp<int32>(FMath::FloorToInt(RelMin.X), 0, 31);
	const uint32 PackedMinY = (uint32)FMath::Clamp<int32>(FMath::FloorToInt(RelMin.Y), 0, 31);
	const uint32 PackedMinZ = (uint32)FMath::Clamp<int32>(FMath::FloorToInt(RelMin.Z), 0, 63);

	const uint32 PackedMaxX = (uint32)FMath::Clamp<int32>(FMath::CeilToInt(RelMax.X), 0, 31);
	const uint32 PackedMaxY = (uint32)FMath::Clamp<int32>(FMath::CeilToInt(RelMax.Y), 0, 31);
	const uint32 PackedMaxZ = (uint32)FMath::Clamp<int32>(FMath::CeilToInt(RelMax.Z), 0, 63);

	return PackedMinX | (PackedMinY << 5) | (PackedMinZ << 10) | (PackedMaxX << 16) | (PackedMaxY << 21) | (PackedMaxZ << 26);
}

void UnpackRelativeBox(const FBoxSphereBounds& InRefBounds, uint32 InPackedRelBox, FBoxSphereBounds& OutBounds)
{
	if (InPackedRelBox == PackedRelativeBox_Identity)
	{
		OutBounds = InRefBounds;
	}
	else if (InRefBounds.SphereRadius > 0)
	{
		const uint32 PackedMinX = InPackedRelBox & 31;
		const uint32 PackedMinY = (InPackedRelBox >> 5) & 31;
		const uint32 PackedMinZ = (InPackedRelBox >> 10) & 63;

		const uint32 PackedMaxX = (InPackedRelBox >> 16) & 31;
		const uint32 PackedMaxY = (InPackedRelBox >> 21) & 31;
		const uint32 PackedMaxZ = (InPackedRelBox >> 26) & 63;

		const FVector RefMin = InRefBounds.Origin - InRefBounds.BoxExtent;
		// 15.5 and 31.5 have the / 2 scale included 
		const FVector UnpackScale = InRefBounds.BoxExtent.ComponentMax(FVector(KINDA_SMALL_NUMBER, KINDA_SMALL_NUMBER, KINDA_SMALL_NUMBER)) / FVector(15.5f, 15.5f, 31.5f);

		const FVector Min = FVector((float)PackedMinX, (float)PackedMinY, (float)PackedMinZ) * UnpackScale + RefMin;
		const FVector Max = FVector((float)PackedMaxX, (float)PackedMaxY, (float)PackedMaxZ) * UnpackScale + RefMin;

		OutBounds.Origin = .5 * (Min + Max);
		OutBounds.BoxExtent = .5 * (Max - Min);
		OutBounds.SphereRadius = OutBounds.BoxExtent.Size();
	}
	else // In that case the ref bounds is 0, so any relative bound is also 0.
	{
		OutBounds.Origin = FVector::ZeroVector;
		OutBounds.BoxExtent = FVector::ZeroVector;
		OutBounds.SphereRadius = 0;
	}
}

void FStreamingTextureBuildInfo::PackFrom(ULevel* Level, const FBoxSphereBounds& RefBounds, const FStreamingTexturePrimitiveInfo& Info)
{
	check(Level);

	PackedRelativeBox = PackRelativeBox(RefBounds.Origin, RefBounds.BoxExtent, Info.Bounds.Origin, Info.Bounds.BoxExtent);

	UTexture2D* Texture2D = Info.Texture;
	if (Texture2D->LevelIndex == INDEX_NONE)
	{
		// If this is the first time this texture gets processed in the packing process, encode it.
		Texture2D->LevelIndex = Level->StreamingTextureGuids.Add(Texture2D->GetLightingGuid());
	}
	TextureLevelIndex = (uint16)Texture2D->LevelIndex;

	TexelFactor = Info.TexelFactor;
}

FStreamingTextureLevelContext::FStreamingTextureLevelContext(EMaterialQualityLevel::Type InQualityLevel, const UPrimitiveComponent* Primitive)
: TextureGuidToLevelIndex(nullptr)
, bUseRelativeBoxes(false)
, BuildDataTimestamp(0)
, ComponentBuildData(nullptr)
, QualityLevel(InQualityLevel)
, FeatureLevel(GMaxRHIFeatureLevel)
{
	if (Primitive)
	{
		const UWorld* World = Primitive->GetWorld();
		if (World)
		{
			FeatureLevel = World->FeatureLevel;
		}
	}
}

FStreamingTextureLevelContext::FStreamingTextureLevelContext(EMaterialQualityLevel::Type InQualityLevel, ERHIFeatureLevel::Type InFeatureLevel, bool InUseRelativeBoxes)
: TextureGuidToLevelIndex(nullptr)
, bUseRelativeBoxes(InUseRelativeBoxes)
, BuildDataTimestamp(0)
, ComponentBuildData(nullptr)
, QualityLevel(InQualityLevel)
, FeatureLevel(InFeatureLevel)
{
}

FStreamingTextureLevelContext::FStreamingTextureLevelContext(EMaterialQualityLevel::Type InQualityLevel, const ULevel* InLevel, const TMap<FGuid, int32>* InTextureGuidToLevelIndex) 
: TextureGuidToLevelIndex(nullptr)
, bUseRelativeBoxes(false)
, BuildDataTimestamp(0)
, ComponentBuildData(nullptr)
, QualityLevel(InQualityLevel)
, FeatureLevel(GMaxRHIFeatureLevel)
{
	if (InLevel)
	{
		const UWorld* World = InLevel->GetWorld();
		if (World)
		{
			FeatureLevel = World->FeatureLevel;
		}

		if (InLevel && InTextureGuidToLevelIndex && InLevel->StreamingTextureGuids.Num() > 0 && InLevel->StreamingTextureGuids.Num() == InTextureGuidToLevelIndex->Num())
		{
			bUseRelativeBoxes = !InLevel->bTextureStreamingRotationChanged;
			TextureGuidToLevelIndex = InTextureGuidToLevelIndex;

			// Extra transient data for each texture.
			BoundStates.AddZeroed(InLevel->StreamingTextureGuids.Num());
		}
	}
}

FStreamingTextureLevelContext::~FStreamingTextureLevelContext()
{
	// Reset the level indices for the next use.
	for (const FTextureBoundState& BoundState : BoundStates)
	{
		if (BoundState.Texture)
		{
			BoundState.Texture->LevelIndex = INDEX_NONE;
		}
	}
}

void FStreamingTextureLevelContext::BindBuildData(const TArray<FStreamingTextureBuildInfo>* BuildData)
{
	// Increment the component timestamp, used to know when a texture is processed by a component for the first time.
	// Using a timestamp allows to not reset state in between components.
	++BuildDataTimestamp;

	if (TextureGuidToLevelIndex && CVarStreamingUseNewMetrics.GetValueOnGameThread() != 0) // No point in binding data if there is no possible remapping.
	{
		// Process the build data in order to be able to map a texture object to the build data entry.
		ComponentBuildData = BuildData;
		if (BuildData && BoundStates.Num() > 0)
		{
			for (int32 Index = 0; Index < BuildData->Num(); ++Index)
			{
				int32 TextureLevelIndex = (*BuildData)[Index].TextureLevelIndex;
				if (BoundStates.IsValidIndex(TextureLevelIndex))
				{
					FTextureBoundState& BoundState = BoundStates[TextureLevelIndex];
					BoundState.BuildDataIndex = Index; // The index of this texture in the component build data.
					BoundState.BuildDataTimestamp = BuildDataTimestamp; // The component timestamp will indicate that the index is valid to be used.
				}
			}
		}
	}
	else
	{
		ComponentBuildData = nullptr;
	}
}

int32* FStreamingTextureLevelContext::GetBuildDataIndexRef(UTexture2D* Texture2D)
{
	if (ComponentBuildData) // If there is some build data to map to.
	{
		if (Texture2D->LevelIndex == INDEX_NONE)
		{
			check(TextureGuidToLevelIndex); // Can't bind ComponentData without the remapping.
			const int32* LevelIndex = TextureGuidToLevelIndex->Find(Texture2D->GetLightingGuid());
			if (LevelIndex) // If the index is found in the map, the index is valid in BoundStates
			{
				// Here we need to support the invalid case where 2 textures have the same GUID.
				// If this happens, BoundState.Texture will already be set.
				FTextureBoundState& BoundState = BoundStates[*LevelIndex];
				if (!BoundState.Texture)
				{
					Texture2D->LevelIndex = *LevelIndex;
					BoundState.Texture = Texture2D; // Update the mapping now!
				}
				else // Don't allow 2 textures to be using the same level index otherwise UTexture2D::LevelIndex won't be reset properly in the destructor.
				{
					FMessageLog("AssetCheck").Error()
						->AddToken(FUObjectToken::Create(BoundState.Texture))
						->AddToken(FUObjectToken::Create(Texture2D))
						->AddToken(FTextToken::Create( NSLOCTEXT("AssetCheck", "TextureError_NonUniqueLightingGuid", "Same lighting guid, modify or touch any property in the texture editor to generate a new guid and fix the issue.") ) );

					// This will fallback not using the precomputed data. Note also that the other texture might be using the wrong precomputed data.
					return nullptr;
				}
			}
			else // Otherwise add a dummy entry to prevent having to search in the map multiple times.
			{
				Texture2D->LevelIndex = BoundStates.Add(FTextureBoundState(Texture2D));
			}
		}

		FTextureBoundState& BoundState = BoundStates[Texture2D->LevelIndex];
		check(BoundState.Texture == Texture2D);

		if (BoundState.BuildDataTimestamp == BuildDataTimestamp)
		{
			return &BoundState.BuildDataIndex; // Only return the bound static if it has data relative to this component.
		}
	}
	return nullptr;
}

void FStreamingTextureLevelContext::ProcessMaterial(const FBoxSphereBounds& ComponentBounds, const FPrimitiveMaterialInfo& MaterialData, float ComponentScaling, TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures)
{
	ensure(MaterialData.IsValid());

	TArray<UTexture*> Textures;
	MaterialData.Material->GetUsedTextures(Textures, QualityLevel, false, FeatureLevel, false);

	for (UTexture* Texture : Textures)
	{
		UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
		if (!IsStreamingTexture(Texture2D))
		{
			continue;
		}

		int32* BuildDataIndex = GetBuildDataIndexRef(Texture2D);
		if (BuildDataIndex)
		{
			if (*BuildDataIndex != INDEX_NONE)
			{
				FStreamingTexturePrimitiveInfo& Info = *new(OutStreamingTextures) FStreamingTexturePrimitiveInfo();
				const FStreamingTextureBuildInfo& BuildInfo = (*ComponentBuildData)[*BuildDataIndex];

				Info.Texture = Texture2D;
				Info.TexelFactor = BuildInfo.TexelFactor * ComponentScaling;
				Info.PackedRelativeBox = bUseRelativeBoxes ? BuildInfo.PackedRelativeBox : PackedRelativeBox_Identity;
				UnpackRelativeBox(ComponentBounds, Info.PackedRelativeBox, Info.Bounds);

				// Indicate that this texture build data has already been processed.
				// The build data use the merged results of all material so it only needs to be processed once.
				*BuildDataIndex = INDEX_NONE;
			}
		}
		else // Otherwise create an entry using the available data.
		{
			float TextureDensity = MaterialData.Material->GetTextureDensity(Texture->GetFName(), *MaterialData.UVChannelData);

			if (!TextureDensity)
			{
				// Fallback assuming a sampling scale of 1 using the UV channel 0;
				TextureDensity = MaterialData.UVChannelData->LocalUVDensities[0];
			}

			if (TextureDensity)
			{
				FStreamingTexturePrimitiveInfo& Info = *new(OutStreamingTextures) FStreamingTexturePrimitiveInfo();

				Info.Texture = Texture2D;
				Info.TexelFactor = TextureDensity * ComponentScaling;
				Info.PackedRelativeBox = bUseRelativeBoxes ? MaterialData.PackedRelativeBox : PackedRelativeBox_Identity;
				UnpackRelativeBox(ComponentBounds, Info.PackedRelativeBox, Info.Bounds);
			}
		}
	}
}

void CheckTextureStreamingBuildValidity(UWorld* InWorld)
{
	if (!InWorld)
	{
		return;
	}

	InWorld->NumTextureStreamingUnbuiltComponents = 0;
	InWorld->NumTextureStreamingDirtyResources = 0;

	if (CVarStreamingCheckBuildStatus.GetValueOnAnyThread() > 0)
	{
		for (int32 LevelIndex = 0; LevelIndex < InWorld->GetNumLevels(); LevelIndex++)
		{
			ULevel* Level = InWorld->GetLevel(LevelIndex);
			if (!Level)
			{
				continue;
			}

#if WITH_EDITORONLY_DATA // Only rebuild the data in editor 
			if (FPlatformProperties::HasEditorOnlyData())
			{
				TSet<FGuid> ResourceGuids;
				Level->NumTextureStreamingUnbuiltComponents = 0;

				for (AActor* Actor : Level->Actors)
				{
					// Check the actor after incrementing the progress.
					if (!Actor)
					{
						continue;
					}

					TInlineComponentArray<UPrimitiveComponent*> Primitives;
					Actor->GetComponents<UPrimitiveComponent>(Primitives);

					for (UPrimitiveComponent* Primitive : Primitives)
					{
						// Non transactional primitives like blueprints, can not invalidate the texture build for now.
						if (!Primitive || !Primitive->HasAnyFlags(RF_Transactional))
						{
							continue;
						}

						// Quality and feature level irrelevant in validation.
						if (!Primitive->BuildTextureStreamingData(TSB_ValidationOnly, EMaterialQualityLevel::Num, ERHIFeatureLevel::Num, ResourceGuids))
						{
							++Level->NumTextureStreamingUnbuiltComponents;
						}
					}
				}

				for (const FGuid& Guid : Level->TextureStreamingResourceGuids)
				{
					// If some Guid does not exists anymore, that means the resource changed.
					if (!ResourceGuids.Contains(Guid))
					{
						Level->NumTextureStreamingDirtyResources += 1;
					}
					ResourceGuids.Add(Guid);
				}

				// Don't mark package dirty as we avoid marking package dirty unless user changes something.
			}
#endif
			InWorld->NumTextureStreamingUnbuiltComponents += Level->NumTextureStreamingUnbuiltComponents;
			InWorld->NumTextureStreamingDirtyResources += Level->NumTextureStreamingDirtyResources;
		}
	}
}
