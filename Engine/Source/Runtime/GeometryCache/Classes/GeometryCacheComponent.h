// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Runtime/GeometryCache/Private/GeometryCacheSceneProxy.h"
#include "Components/MeshComponent.h"

#include "GeometryCacheComponent.generated.h"

class UGeometryCache;
struct FGeometryCacheMeshData;

/** Stores the RenderData for each individual track */
USTRUCT()
struct FTrackRenderData
{
	GENERATED_USTRUCT_BODY()

	~FTrackRenderData()
	{
		MeshData = nullptr;
	}
	
	/** Pointer to FGeometryCacheMeshData containing vertex-data, bounding box, index buffer and batch-info*/
	FGeometryCacheMeshData* MeshData;
	/** World matrix used to render this specific track */
	FMatrix WorldMatrix;

	void Reset()
	{
		MeshData = nullptr;
	}
};

/** GeometryCacheComponent, encapsulates a GeometryCache asset instance and implements functionality for rendering/and playback of GeometryCaches */
UCLASS(ClassGroup = (Rendering, Common), hidecategories = (Object, LOD), meta = (BlueprintSpawnableComponent), Experimental, ClassGroup = Experimental)
class GEOMETRYCACHE_API UGeometryCacheComponent : public UMeshComponent
{
	GENERATED_UCLASS_BODY()

	/** Required for access to (protected) TrackSections */
	friend FGeometryCacheSceneProxy;
		
	//~ Begin UObject Interface
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject Interface

	//~ Begin UActorComponent Interface.
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	//~ End UActorComponent Interface.

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	/** Update LocalBounds member from the local box of each section */
	void UpdateLocalBounds();
	//~ Begin USceneComponent Interface.	

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin UMeshComponent Interface.
	virtual int32 GetNumMaterials() const override;
	virtual UMaterialInterface* GetMaterial(int32 MaterialIndex) const override;
	//~ End UMeshComponent Interface.

	/**
	* CreateTrackSection, Create/replace a track section.
	*
	* @param WorldMatrix - World Matrix for the TrackSection
	* @param MeshData - MeshData for the TrackSection (containing vertex-data, batch-info and bounding box)
	*/
	void CreateTrackSection(int32 SectionIndex, const FMatrix& WorldMatrix, FGeometryCacheMeshData* MeshData);

	/**
	* UpdateTrackSectionMeshData, Update only the Mesh Data (Vertices) for a specific section
	*
	* @param SectionIndex - Index of the section we want to update.
	* @param MeshData - New MeshData pointer for the section.
	*/
	void UpdateTrackSectionMeshData(int32 SectionIndex, FGeometryCacheMeshData* MeshData);

	/**
	* UpdateTrackSectionMatrixData, Update only the World Matrix for a specific section
	*
	* @param SectionIndex - Index of the section we want to update
	* @param WorldMatrix - New world matrix for the section.
	*/
	void UpdateTrackSectionMatrixData(int32 SectionIndex, const FMatrix& WorldMatrix);	

	/**
	* UpdateTrackSectionVertexbuffer, Update only the Vertex Buffer for a specific section
	*
	* @param SectionIndex - Index of the section we want to update
	* @param MeshData - MeshData pointer to update the VB with
	*/
	void UpdateTrackSectionVertexbuffer(int32 SectionIndex, FGeometryCacheMeshData* MeshData);

	/**
	* UpdateTrackSectionIndexbuffer, Update only the Index Buffer for a specific section
	*
	* @param SectionIndex - Index of the section we want to update
	* @param Indices - Array with new indices
	*/
	void UpdateTrackSectionIndexbuffer(int32 SectionIndex, const TArray<uint32>& Indices );

	/**
	* OnObjectReimported, Callback function to refresh section data and update scene proxy.
	*
	* @param ImportedGeometryCache
	*/
	void OnObjectReimported(UGeometryCache* ImportedGeometryCache);

	/**
	* SetupTrackData
	* Setup data required for playback of geometry cache tracks
	*/
	void SetupTrackData();
	
	/**
	* ClearTrackData
	* Clean up data that was required for playback of geometry cache tracks
	*/
	void ClearTrackData();

	/** Start playback of GeometryCache */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	void Play();

	/** Start playback of GeometryCache from the start */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	void PlayFromStart();

	/** Start playback of GeometryCache in reverse*/
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	void PlayReversed();
	
	/** Start playback of GeometryCache from the end and play in reverse */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	void PlayReversedFromEnd();

	/** Pause playback of GeometryCache */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	void Pause();

	/** Stop playback of GeometryCache */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	void Stop();

	/** Get whether this GeometryCache is playing or not. */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	bool IsPlaying() const;

	/** Get whether this GeometryCache is playing in reverse or not. */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	bool IsPlayingReversed() const;

	/** Get whether this GeometryCache is looping or not. */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	bool IsLooping() const;

	/** Set whether this GeometryCache is looping or not. */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	void SetLooping( const bool bNewLooping);

	/** Get current playback speed for GeometryCache. */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	float GetPlaybackSpeed() const;

	/** Set new playback speed for GeometryCache. */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	void SetPlaybackSpeed(const float NewPlaybackSpeed);

	/** Change the Geometry Cache used by this instance. */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	virtual bool SetGeometryCache( UGeometryCache* NewGeomCache );
	
	/** Getter for Geometry cache instance referred by the component */
	UGeometryCache* GetGeometryCache() const;

	/** Get current start time offset for GeometryCache. */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	float GetStartTimeOffset() const;

	/** Set current start time offset for GeometryCache. */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	void SetStartTimeOffset(const float NewStartTimeOffset);


	/** Geometry Cache instance referenced by the component */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = GeometryCache)
	UGeometryCache* GeometryCache;
		
protected:
	/**
	* Invalidate both the Matrix and Mesh sample indices
	*/
	void InvalidateTrackSampleIndices();

	/**
	* ReleaseResources, clears and removes data stored/copied from GeometryCache instance	
	*/
	void ReleaseResources();

	UPROPERTY(EditAnywhere, Interp, Category = GeometryCache)
	bool bRunning;

	UPROPERTY(EditAnywhere, Interp, Category = GeometryCache)
	bool bLooping;

	UPROPERTY(EditAnywhere, Interp, Category = GeometryCache, meta = (UIMin = "-14400.0", UIMax = "14400.0", ClampMin = "-14400.0", ClampMax = "14400.0"))
	float StartTimeOffset;

	UPROPERTY(EditAnywhere, Interp, Category = GeometryCache, meta = (UIMin = "-4.0", UIMax = "4.0", ClampMin = "-512.0", ClampMax = "512.0"))
	float PlaybackSpeed;

	UPROPERTY(VisibleAnywhere, Category = GeometryCache)
	int32 NumTracks;

	UPROPERTY(VisibleAnywhere, Category = GeometryCache)
	float ElapsedTime;

	/** Local space bounds of mesh */
	FBoxSphereBounds LocalBounds;

	/** Matrix and Mesh sample index for each individual track */
	TArray<int32> TrackMeshSampleIndices;
	TArray<int32> TrackMatrixSampleIndices;
	
	/** Array containing the TrackData (used for rendering) for each individual track*/
	TArray<FTrackRenderData> TrackSections;

	/** Play (time) direction, either -1.0f or 1.0f */
	float PlayDirection;
	/** Duration of the animation (maximum time) */
	float Duration;
};
