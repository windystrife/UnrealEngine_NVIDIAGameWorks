// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	StaticTextureInstanceManager.cpp: Implementation of content streaming classes.
=============================================================================*/

#include "Streaming/StaticTextureInstanceManager.h"
#include "Components/PrimitiveComponent.h"

void FStaticTextureInstanceManager::FTasks::SyncResults()
{
	RefreshVisibilityTask->TryWork(false);
	NormalizeLightmapTexelFactorTask->TryWork(false);
	// All (async) work must be completed before synching the results as the work assume a constant state.
	RefreshVisibilityTask->TrySync();
	NormalizeLightmapTexelFactorTask->TrySync();

}

FStaticTextureInstanceManager::FStaticTextureInstanceManager(TextureInstanceTask::FDoWorkTask& AsyncTask)
	: DirtyIndex(0)
{
	FTasks& Tasks = StateSync.GetTasks();

	Tasks.RefreshVisibilityTask = new FRefreshVisibilityTask(TextureInstanceTask::FRefreshVisibility::FOnWorkDone::CreateLambda([this](int32 InBeginIndex, int32 InEndIndex){ this->OnRefreshVisibilityDone(InBeginIndex, InEndIndex); }));
	AsyncTask.Add(Tasks.RefreshVisibilityTask.GetReference());

	Tasks.NormalizeLightmapTexelFactorTask = new FNormalizeLightmapTexelFactorTask();
	AsyncTask.Add(Tasks.NormalizeLightmapTexelFactorTask.GetReference());

}

void FStaticTextureInstanceManager::NormalizeLightmapTexelFactor()
{
	if (!AsyncView)
	{
		FTextureInstanceState* State = StateSync.SyncAndGetState();
		if (State->NumBounds() > 0)
		{
			StateSync.GetTasks().NormalizeLightmapTexelFactorTask->Init(State);
		}
	}
}

void FStaticTextureInstanceManager::OnRefreshVisibilityDone(int32 InBeginIndex, int32 InEndIndex)
{
	// Make sure there are no wholes between the DirtyIndex and the first updated index.
	if (InBeginIndex <= DirtyIndex)
	{
		DirtyIndex = FMath::Max<int32>(InEndIndex, DirtyIndex);
	}
}

/*-----------------------------------
------ ITextureInstanceManager ------
-----------------------------------*/

bool FStaticTextureInstanceManager::CanManage(const UPrimitiveComponent* Component) const
{
	// This manager only manages static components from static actors.
	// Note that once the view has been shared, no other modifications are allowed.
	// Also, the manager allows to add unregistered components until the first shared view is required.
	if (!AsyncView && Component && Component->Mobility == EComponentMobility::Static)
	{
		// Primitive->IsPendingKill() || Primitive->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed)
		const AActor* Owner = Component->GetOwner();
		return Owner && Owner->IsRootComponentStatic();
	}
	return false;
}

void FStaticTextureInstanceManager::Refresh(float Percentage)
{
	// Since this is only managing static components, only visibility needs to be refreshed.

	FTextureInstanceState* State = StateSync.SyncAndGetState();
	if (DirtyIndex < State->NumBounds())
	{
		const int32 EndIndex = FMath::Min(State->NumBounds(), DirtyIndex + FMath::CeilToInt((float)State->NumBounds() * Percentage));
		StateSync.GetTasks().RefreshVisibilityTask->Init(State, DirtyIndex, EndIndex);
	}
}

bool FStaticTextureInstanceManager::Add(const UPrimitiveComponent* Component, FStreamingTextureLevelContext& LevelContext)
{
	if (!AsyncView)
	{
		FTextureInstanceState* State = StateSync.SyncAndGetState();
		return State->AddComponent(Component, LevelContext);
	}
	return false;
}

void FStaticTextureInstanceManager::Remove(const UPrimitiveComponent* Component, FRemovedTextureArray& RemovedTextures)
{
	FTextureInstanceState* State = StateSync.SyncAndGetState();
	// If the view is shared, we are limited to clearing the references (no realloc)
	if (AsyncView)
	{
		State->RemoveComponentReferences(Component);
	}
	else // Otherwise it can be cleaned properly
	{
		State->RemoveComponent(Component, RemovedTextures);
	}
}

const FTextureInstanceView* FStaticTextureInstanceManager::GetAsyncView(bool bCreateIfNull)
{
	FTextureInstanceState* State = StateSync.SyncAndGetState();
	if (!AsyncView && bCreateIfNull)
	{
		AsyncView = State;
	}
	DirtyIndex = 0; // Force a full refresh!
	return AsyncView.GetReference();
}

uint32 FStaticTextureInstanceManager::GetAllocatedSize() const
{
	const FTextureInstanceState* State = StateSync.GetState();
	return  State ? (sizeof(FTextureInstanceState) + State->GetAllocatedSize()) : 0;
}


void FStaticTextureInstanceManager::OffsetBounds(const FVector& Offset)
{
	FTextureInstanceState* State = StateSync.SyncAndGetState();
	if (State)
	{
		State->OffsetBounds(Offset);
	}
}