// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayDebuggerCategory.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameplayDebuggerCategoryReplicator.h"

FGameplayDebuggerCategory::FGameplayDebuggerCategory() :
	CollectDataInterval(0.0f),
	bShowDataPackReplication(false),
	bShowUpdateTimer(false),
	bShowCategoryName(true),
	bShowOnlyWithDebugActor(true),
	bIsLocal(false),
	bHasAuthority(true),
	bIsEnabled(true),
	CategoryId(INDEX_NONE),
	LastCollectDataTime(-FLT_MAX)
{
}

FGameplayDebuggerCategory::~FGameplayDebuggerCategory()
{
}

void FGameplayDebuggerCategory::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	// empty in base class
}

void FGameplayDebuggerCategory::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	// empty in base class
}

FDebugRenderSceneProxy* FGameplayDebuggerCategory::CreateDebugSceneProxy(const UPrimitiveComponent* InComponent, FDebugDrawDelegateHelper*& OutDelegateHelper)
{
	OutDelegateHelper = nullptr;
	// empty in base class
	return nullptr;
}

void FGameplayDebuggerCategory::OnDataPackReplicated(int32 DataPackId)
{
	// empty in base class
}

void FGameplayDebuggerCategory::AddTextLine(const FString& TextLine)
{
	if (bHasAuthority)
	{
		ReplicatedLines.Add(TextLine);
	}
}

void FGameplayDebuggerCategory::AddShape(const FGameplayDebuggerShape& Shape)
{
	if (bHasAuthority)
	{
		ReplicatedShapes.Add(Shape);
	}
}

void FGameplayDebuggerCategory::DrawCategory(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	UWorld* World = OwnerPC->GetWorld();

	FString CategoryPrefix;
	if (!bShowCategoryName)
	{
		CategoryPrefix = FString::Printf(TEXT("{green}[%s]{white}  "), *CategoryName.ToString());
	}

	if (bShowUpdateTimer && bHasAuthority)
	{
		const float GameTime = World->GetTimeSeconds();
		CanvasContext.Printf(TEXT("%sNext update in: {yellow}%.0fs"), *CategoryPrefix, CollectDataInterval - (GameTime - LastCollectDataTime));
	}

	if (bShowDataPackReplication)
	{
		for (int32 Idx = 0; Idx < ReplicatedDataPacks.Num(); Idx++)
		{
			FGameplayDebuggerDataPack& DataPack = ReplicatedDataPacks[Idx];
			if (DataPack.IsInProgress())
			{
				const FString DataPackMessage = (ReplicatedDataPacks.Num() == 1) ?
					FString::Printf(TEXT("%sReplicating: {red}%.0f%% {white}(ver:%d)"), *CategoryPrefix, DataPack.GetProgress() * 100.0f, DataPack.Header.DataVersion) :
					FString::Printf(TEXT("%sReplicating data[%d]: {red}%.0f%% {white}(ver:%d)"), *CategoryPrefix, Idx, DataPack.GetProgress() * 100.0f, DataPack.Header.DataVersion);

				CanvasContext.Print(DataPackMessage);
			}
		}
	}

	for (int32 Idx = 0; Idx < ReplicatedLines.Num(); Idx++)
	{
		CanvasContext.Print(ReplicatedLines[Idx]);
	}

	for (int32 Idx = 0; Idx < ReplicatedShapes.Num(); Idx++)
	{
		ReplicatedShapes[Idx].Draw(World, CanvasContext);
	}

	DrawData(OwnerPC, CanvasContext);
}

void FGameplayDebuggerCategory::MarkDataPackDirty(int32 DataPackId)
{
	if (ReplicatedDataPacks.IsValidIndex(DataPackId))
	{
		ReplicatedDataPacks[DataPackId].bIsDirty = true;
	}
}

void FGameplayDebuggerCategory::MarkRenderStateDirty()
{
	if (bIsLocal)
	{
		AGameplayDebuggerCategoryReplicator* RepOwnerOb = GetReplicator();
		if (RepOwnerOb)
		{
			RepOwnerOb->MarkComponentsRenderStateDirty();
		}
	}
}

FString FGameplayDebuggerCategory::GetSceneProxyViewFlag() const
{
	const bool bIsSimulate = FGameplayDebuggerAddonBase::IsSimulateInEditor();
	return bIsSimulate ? TEXT("DebugAI") : TEXT("Game");
}
