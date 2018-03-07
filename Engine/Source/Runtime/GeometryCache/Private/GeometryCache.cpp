// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GeometryCache.h"
#include "EditorFramework/AssetImportData.h"
#include "Materials/MaterialInterface.h"
#include "GeometryCacheTrackTransformAnimation.h"
#include "GeometryCacheTrackFlipbookAnimation.h"
#include "UObject/FrameworkObjectVersion.h"
#include "Interfaces/ITargetPlatform.h"

UGeometryCache::UGeometryCache(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NumTransformAnimationTracks = 0;
	NumVertexAnimationTracks = 0;
}

void UGeometryCache::PostInitProperties()
{
#if WITH_EDITORONLY_DATA
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
#endif
	Super::PostInitProperties();
}

void UGeometryCache::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);
#if WITH_EDITORONLY_DATA
	if ( !Ar.IsCooking() || (Ar.CookingTarget() && Ar.CookingTarget()->HasEditorOnlyData()))
	{
		Ar << AssetImportData;
	}
#endif
	Ar << Tracks;
	Ar << NumVertexAnimationTracks;
	Ar << NumTransformAnimationTracks;	

	if (Ar.CustomVer(FFrameworkObjectVersion::GUID) >= FFrameworkObjectVersion::GeometryCacheMissingMaterials)
	{
		Ar << Materials;
	}
}

FString UGeometryCache::GetDesc()
{
	const int32 NumTracks = Tracks.Num();
	return FString("%d Tracks", NumTracks);
}

void UGeometryCache::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

#if WITH_EDITORONLY_DATA
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(sizeof(*AssetImportData));
#endif
	// Calculate Resource Size according to what is serialized
	const int32 NumTracks = Tracks.Num();
	for (int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
	{
		Tracks[TrackIndex]->GetResourceSizeEx(CumulativeResourceSize);
	}

	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(sizeof(Tracks));
	CumulativeResourceSize.AddUnknownMemoryBytes(sizeof(NumVertexAnimationTracks));
	CumulativeResourceSize.AddUnknownMemoryBytes(sizeof(NumTransformAnimationTracks));
}

void UGeometryCache::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	// Information on number total of (per type) tracks
	const int32 NumTracks = Tracks.Num();	
	OutTags.Add(FAssetRegistryTag("Total Tracks", FString::FromInt(NumTracks), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("Vertex Animation Tracks", FString::FromInt(NumVertexAnimationTracks), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("Transform Animation Tracks", FString::FromInt(NumTransformAnimationTracks), FAssetRegistryTag::TT_Numerical));

#if WITH_EDITORONLY_DATA
	if (AssetImportData)
	{
		OutTags.Add(FAssetRegistryTag(SourceFileTagName(), AssetImportData->GetSourceData().ToJson(), FAssetRegistryTag::TT_Hidden));
	}
#endif

	Super::GetAssetRegistryTags(OutTags);
}

void UGeometryCache::BeginDestroy()
{
	Super::BeginDestroy();
	NumVertexAnimationTracks = NumTransformAnimationTracks = 0;
	Tracks.Empty();
	ReleaseResourcesFence.BeginFence();
}

void UGeometryCache::ClearForReimporting()
{
	NumVertexAnimationTracks = NumTransformAnimationTracks = 0;
	Tracks.Empty();

	// Flush the resource release commands to the rendering thread to ensure that the edit change doesn't occur while a resource is still allocated
	ReleaseResourcesFence.Wait();
}

bool UGeometryCache::IsReadyForFinishDestroy()
{
	return ReleaseResourcesFence.IsFenceComplete();
}

#if WITH_EDITOR
void UGeometryCache::PreEditChange(UProperty* PropertyAboutToChange)
{
	// Release the Geometry Cache resources.
	NumVertexAnimationTracks = NumTransformAnimationTracks = 0;
	Tracks.Empty();

	// Flush the resource release commands to the rendering thread to ensure that the edit change doesn't occur while a resource is still allocated
	ReleaseResourcesFence.Wait();
}
#endif

void UGeometryCache::AddTrack(UGeometryCacheTrack* Track)
{
	if (Track->GetClass() == UGeometryCacheTrack_TransformAnimation::StaticClass())
	{
		NumTransformAnimationTracks++;
	}
	else if (Track->GetClass() == UGeometryCacheTrack_FlipbookAnimation::StaticClass())
	{
		NumVertexAnimationTracks++;
	}

	Tracks.Add(Track);
}
