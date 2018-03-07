// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Interfaces/Interface_AssetUserData.h"
#include "RenderCommandFence.h"

#include "GeometryCache.generated.h"

class UGeometryCacheTrack;
class UMaterialInterface;

/**
* A Geometry Cache is a piece/set of geometry that consists of individual Mesh/Transformation samples.
* In contrast with Static Meshes they can have their vertices animated in certain ways. * 
*/
UCLASS(hidecategories = Object, BlueprintType, config = Engine)
class GEOMETRYCACHE_API UGeometryCache : public UObject, public IInterface_AssetUserData
{
	GENERATED_UCLASS_BODY()
public:
	//~ Begin UObject Interface.
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostInitProperties() override;
	virtual FString GetDesc() override;
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;

	virtual void BeginDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
#if WITH_EDITOR
	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
#endif // WITH_EDITOR
	//~ End UObject Interface.

	/**
	* AddTrack
	*
	* @param Track - GeometryCacheTrack instance that is a part of the GeometryCacheAsset
	* @return void
	*/
	void AddTrack(UGeometryCacheTrack* Track);
		
	/** Clears all stored data so the reimporting step can fill the instance again*/
	void ClearForReimporting();	

#if WITH_EDITORONLY_DATA
	/** Importing data and options used for this Geometry cache object*/
	UPROPERTY(Category = ImportSettings, VisibleAnywhere, Instanced)
	class UAssetImportData* AssetImportData;	

	/** Information for thumbnail rendering */
	UPROPERTY(VisibleAnywhere, Instanced, Category = Thumbnail)
	class UThumbnailInfo* ThumbnailInfo;
#endif
	UPROPERTY(VisibleAnywhere, Category = GeometryCache)
	TArray<UMaterialInterface*> Materials;
	
	/** GeometryCache track defining the samples/geometry data for this GeomCache instance */
	UPROPERTY(VisibleAnywhere, Category=GeometryCache)
	TArray<UGeometryCacheTrack*> Tracks;
private:
	/** Storing the number of Vertex and RigidBody track for asset meta tags*/
	uint32 NumVertexAnimationTracks;
	uint32 NumTransformAnimationTracks;

	/** A fence which is used to keep track of the rendering thread releasing the geometry cache resources. */
	FRenderCommandFence ReleaseResourcesFence;
};
