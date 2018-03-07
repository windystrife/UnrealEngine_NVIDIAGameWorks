// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
DynamicTextureInstanceManager.h: Definitions of classes used for texture streaming.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Streaming/TextureInstanceManager.h"
#include "Streaming/TextureInstanceTask.h"
#include "ContentStreaming.h"

/** 
 * A texture instance manager to manage dynamic components. 
 * The async view generated is duplicated so that the state can change freely.
 */
class FDynamicTextureInstanceManager : public ITextureInstanceManager
{
public:

	/** Contructor. */
	FDynamicTextureInstanceManager();

	void RegisterTasks(TextureInstanceTask::FDoWorkTask& AsyncTask);

	void IncrementalUpdate(FRemovedTextureArray& RemovedTextures, float Percentage);

#if !UE_BUILD_SHIPPING
	// Get all (non removed) components refered by the manager. Debug only.
	void GetReferencedComponents(TArray<const UPrimitiveComponent*>& Components) { StateSync.SyncAndGetState()->GetReferencedComponents(Components); }
#endif

	/*-----------------------------------
	------ ITextureInstanceManager ------
	-----------------------------------*/

	/** Return whether this component can be managed by this manager. */
	bool IsReferenced(const UPrimitiveComponent* Component) const final;

	/** Return whether this component is be managed by this manager. */
	bool CanManage(const UPrimitiveComponent* Component) const final override;

	/** Refresh component data (bounds, last render time, min and max view distance) - see TextureInstanceView. */
	void Refresh(float Percentage) final override;

	/** Add a component streaming data, the LevelContext gives support for precompiled data. */
	 bool Add(const UPrimitiveComponent* Component, FStreamingTextureLevelContext& LevelContext) final override;

	/** Remove a component, the RemoveTextures is the list of textures not referred anymore. */
	 void Remove(const UPrimitiveComponent* Component, FRemovedTextureArray& RemovedTextures) final override;

	/** Notify the manager that an async view will be requested on the next frame. */
	void PrepareAsyncView() final override;

	/** Return a view of the data that has to be 100% thread safe. The content is allowed to be updated, but not memory must be reallocated. */
	const FTextureInstanceView* GetAsyncView(bool bCreateIfNull) final override;

	/** Return the size taken for sub-allocation. */
	uint32 GetAllocatedSize() const final override;

protected:

	void OnCreateViewDone(FTextureInstanceView* InView);
	void OnRefreshVisibilityDone(int32 BeginIndex, int32 EndIndex, const TArray<int32>& SkippedIndices, int32 FirstFreeBound, int32 LastUsedBound);

private:

	typedef TextureInstanceTask::FCreateViewWithUninitializedBoundsTask FCreateViewTask;
	typedef TextureInstanceTask::FRefreshFullTask FRefreshFullTask;

	struct FTasks
	{
		~FTasks() { SyncResults(); }
		void SyncResults();
		TRefCountPtr<FCreateViewTask> CreateViewTask;
		TRefCountPtr<FRefreshFullTask> RefreshFullTask;
	};

	/** The texture instances. Shared with the async task. */
	FTextureInstanceStateTaskSync<FTasks> StateSync;

	/** A duplicate view for the async streaming task. */
	TRefCountPtr<const FTextureInstanceView> AsyncView;

	/** Ranges from 0 to Bounds4Components.Num(). Used in the incremental update to update bounds and visibility. */
	int32 DirtyIndex;

	/** The valid bound index to be moved for defrag. */
	int32 PendingDefragSrcBoundIndex;
	/** The free bound index to be used as defrag destination. */
	int32 PendingDefragDstBoundIndex;

	/** The list of components to be processed (could have duplicates). */
	TArray<const UPrimitiveComponent*> PendingComponents;
};
