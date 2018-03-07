// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
StaticTextureInstanceManager.h: Definitions of classes used for texture streaming.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Streaming/TextureInstanceManager.h"
#include "Streaming/TextureInstanceTask.h"

/** 
 * A texture instance manager to manage fully static components. 
 * Once an async view has been requested, nothing can be changed as the async view and the internal state point to the same data. 
 * This allows for quicker refreshes and also prevent state duplication.
 */
class FStaticTextureInstanceManager : public ITextureInstanceManager
{
public:

	/** Contructor. */
	FStaticTextureInstanceManager(TextureInstanceTask::FDoWorkTask& AsyncTask);

	/** Normalize lightmap texel factors, this is ran on an async tasks. */
	void NormalizeLightmapTexelFactor();

	FORCEINLINE int32 CompileElements() { return StateSync.SyncAndGetState()->CompileElements(); }
	FORCEINLINE int32 CheckRegistrationAndUnpackBounds(TArray<const UPrimitiveComponent*>& RemovedComponents) { return StateSync.SyncAndGetState()->CheckRegistrationAndUnpackBounds(RemovedComponents); }
	FORCEINLINE FTextureInstanceState::FTextureIterator GetTextureIterator( ) {  return StateSync.SyncAndGetState()->GetTextureIterator(); }

	/*-----------------------------------
	------ ITextureInstanceManager ------
	-----------------------------------*/

	/** Return whether this component can be managed by this manager. */
	FORCEINLINE bool IsReferenced(const UPrimitiveComponent* Component) const final override { return StateSync.GetState()->HasComponentReferences(Component); }

	/** Return whether this component can be managed by this manager. */
	FORCEINLINE void GetReferencedComponents(TArray<const UPrimitiveComponent*>& Components) const { StateSync.GetState()->GetReferencedComponents(Components); }

	/** Return whether this component is be managed by this manager. */
	bool CanManage(const UPrimitiveComponent* Component) const final override;

	/** Refresh component data (bounds, last render time, min and max view distance) - see TextureInstanceView. */
	void Refresh(float Percentage) final override;

	/** Add a component streaming data, the LevelContext gives support for precompiled data. */
	 bool Add(const UPrimitiveComponent* Component, FStreamingTextureLevelContext& LevelContext) final override;

	/** Remove a component, the RemoveTextures is the list of textures not referred anymore. */
	 void Remove(const UPrimitiveComponent* Component, FRemovedTextureArray& RemovedTextures) final override;

	/** Notify the manager that an async view will be requested on the next frame. */
	FORCEINLINE void PrepareAsyncView() final override {}

	/** Return a view of the data that has to be 100% thread safe. The content is allowed to be updated, but not memory must be reallocated. */
	const FTextureInstanceView* GetAsyncView(bool bCreateIfNull) final override;

	/** Return the size taken for sub-allocation. */
	uint32 GetAllocatedSize() const final override;

	/** Apply specified offset to all cached primitives bounds */
	void OffsetBounds(const FVector& Offset);

protected:

	void OnRefreshVisibilityDone(int32 InBeginIndex, int32 InEndIndex);

private:

	typedef TextureInstanceTask::FRefreshVisibilityTask FRefreshVisibilityTask;
	typedef TextureInstanceTask::FNormalizeLightmapTexelFactorTask FNormalizeLightmapTexelFactorTask;

	struct FTasks
	{
		~FTasks() { SyncResults(); }
		void SyncResults();
		TRefCountPtr<FRefreshVisibilityTask> RefreshVisibilityTask;
		TRefCountPtr<FNormalizeLightmapTexelFactorTask> NormalizeLightmapTexelFactorTask;
	};

	/** The texture instances. Shared with the async task. */
	FTextureInstanceStateTaskSync<FTasks> StateSync;

	/** A duplicate view for the async streaming task. */
	TRefCountPtr<const FTextureInstanceView> AsyncView;

	/** Ranges from 0 to Bounds4Components.Num(). Used in the incremental update to update visibility. */
	int32 DirtyIndex;
};
