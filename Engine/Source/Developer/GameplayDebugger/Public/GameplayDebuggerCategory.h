// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


// GAMEPLAY DEBUGGER CATEGORY
// 
// Single category of gameplay debugger tool, responsible for gathering and presenting data.
// Category instances are created on both server and local sides, and can use replication to
// show server's state on client.
// 
// It should be compiled and used only when module is included, so every category class
// needs be placed in #if WITH_GAMEPLAY_DEBUGGER block.
// 
// 
// Server side category :
// - CollectData() function will be called in category having authority (server / standalone)
// - set CollectDataInterval for adding delay between data collection, default value is 0, meaning: every tick
// - AddTextLine() and AddShape() adds new data to replicate, both arrays are cleared before calling CollectData()
// - SetDataPackReplication() marks struct member variable for replication
// - MarkDataPackDirty() forces data pack replication, sometimes changes can go unnoticed (CRC based)
// 
// Local category :
// - DrawData() function will be called in every tick to present gathered data
// - everything added by AddTextLine() and AddShape() will be shown before calling DrawData()
// - CreateSceneProxy() allows creating custom scene proxies, use with MarkRenderStateDirty()
// - OnDataPackReplicated() notifies about receiving new data, use with MarkRenderStateDirty() if needed
// - BindKeyPress() allows creating custom key bindings active only when category is being displayed
// 
// 
// Categories needs to be manually registered and unregistered with GameplayDebugger.
// It's best to do it in owning module's Startup / Shutdown, similar to detail view customizations.
// Check AIModule.cpp for examples.

#pragma once

#include "CoreMinimal.h"
#include "GameplayDebuggerTypes.h"
#include "GameplayDebuggerAddonBase.h"

class AActor;
class APlayerController;
class FDebugRenderSceneProxy;
class UPrimitiveComponent;
struct FDebugDrawDelegateHelper;

/**
 * Single category of visual debugger tool
 */
class GAMEPLAYDEBUGGER_API FGameplayDebuggerCategory : public FGameplayDebuggerAddonBase
{
public:

	FGameplayDebuggerCategory();
	virtual ~FGameplayDebuggerCategory();

	/** [AUTH] gather data for replication */
	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor);

	/** [LOCAL] draw collected data */
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext);

	/** [LOCAL] creates a scene proxy for more advanced debug rendering */
	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy(const UPrimitiveComponent* InComponent, FDebugDrawDelegateHelper*& OutDelegateHelper);

	/** [LOCAL] called after successful replication of entire data pack to client */
	virtual void OnDataPackReplicated(int32 DataPackId);

	/** [AUTH] adds line of text tagged with {color} to replicated data */
	void AddTextLine(const FString& TextLine);

	/** [AUTH] adds shape to replicated data */
	void AddShape(const FGameplayDebuggerShape& Shape);

	/** [LOCAL] draw category */
	void DrawCategory(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext);

	/** [LOCAL] check if category should be drawn */
	bool ShouldDrawCategory(bool bHasDebugActor) const { return IsCategoryEnabled() && (!bShowOnlyWithDebugActor || bHasDebugActor); }

	/** [LOCAL] check if data pack replication status  */
	bool ShouldDrawReplicationStatus() const { return bShowDataPackReplication; }

	/** [ALL] get name of category */
	FName GetCategoryName() const { return CategoryName; }

	/** [ALL] check if category header should be drawn */
	bool IsCategoryHeaderVisible() const { return bShowCategoryName; }

	/** [ALL] check if category is enabled */
	bool IsCategoryEnabled() const { return bIsEnabled; }

	/** [ALL] check if category is local (present data) */
	bool IsCategoryLocal() const { return bIsLocal; }

	/** [ALL] check if category has authority (collects data) */
	bool IsCategoryAuth() const { return bHasAuthority; }

	int32 GetNumDataPacks() const { return ReplicatedDataPacks.Num(); }
	float GetDataPackProgress(int32 DataPackId) const { return ReplicatedDataPacks.IsValidIndex(DataPackId) ? ReplicatedDataPacks[DataPackId].GetProgress() : 0.0f; }
	bool IsDataPackReplicating(int32 DataPackId) const { return ReplicatedDataPacks.IsValidIndex(DataPackId) && ReplicatedDataPacks[DataPackId].IsInProgress(); }
	FGameplayDebuggerDataPack::FHeader GetDataPackHeaderCopy(int32 DataPackId) const { return ReplicatedDataPacks.IsValidIndex(DataPackId) ? ReplicatedDataPacks[DataPackId].Header : FGameplayDebuggerDataPack::FHeader(); }

	// temporary functions for compatibility, will be removed soon
	TArray<FString> GetReplicatedLinesCopy() const { return ReplicatedLines; }
	TArray<FGameplayDebuggerShape> GetReplicatedShapesCopy() const { return ReplicatedShapes; }

protected:

	/** [AUTH] marks data pack as needing replication */
	void MarkDataPackDirty(int32 DataPackId);

	/** [LOCAL] requests new scene proxy */
	void MarkRenderStateDirty();

	/** [LOCAL] preferred view flag for creating scene proxy */
	FString GetSceneProxyViewFlag() const;

	/** [ALL] sets up DataPack replication, needs address of property holding data, DataPack's struct must define Serialize(FArchive& Ar) function
	 *  returns DataPackId
	 */
	template<typename T>
	int32 SetDataPackReplication(T* DataPackAddr, EGameplayDebuggerDataPack Flags = EGameplayDebuggerDataPack::ResetOnTick)
	{
		FGameplayDebuggerDataPack NewDataPack;
		NewDataPack.PackId = ReplicatedDataPacks.Num();
		NewDataPack.Flags = Flags;
		NewDataPack.SerializeDelegate.BindRaw(DataPackAddr, &T::Serialize);
		NewDataPack.ResetDelegate.BindLambda([=] { *DataPackAddr = T(); });

		ReplicatedDataPacks.Add(NewDataPack);
		return NewDataPack.PackId;
	}

	/** update interval, 0 = each tick */
	float CollectDataInterval;

	/** include data pack replication details in drawn messages */
	uint32 bShowDataPackReplication : 1;

	/** include remaining time to next data collection in drawn messages */
	uint32 bShowUpdateTimer : 1;

	/** include category name in drawn messages */
	uint32 bShowCategoryName : 1;

	/** draw category only when DebugActor is present */
	uint32 bShowOnlyWithDebugActor : 1;

private:

	friend class FGameplayDebuggerAddonManager;
	friend class AGameplayDebuggerCategoryReplicator;
	friend struct FGameplayDebuggerNetPack;

	/** if set, this category object can display data */
	uint32 bIsLocal : 1;

	/** if set, this category object can collect data */
	uint32 bHasAuthority : 1;

	/** if set, this category object is enabled in debugger */
	uint32 bIsEnabled : 1;

	/** Id number assigned to this category object */
	int32 CategoryId;

	/** timestamp of last update */
	float LastCollectDataTime;

	/** name of debugger category (auto assigned during category registration) */
	FName CategoryName;

	/** list of replicated text lines, will be reset before CollectData call on AUTH */
	TArray<FString> ReplicatedLines;

	/** list of replicated shapes, will be reset before CollectData call on AUTH */
	TArray<FGameplayDebuggerShape> ReplicatedShapes;

	/** list of replicated data packs */
	TArray<FGameplayDebuggerDataPack> ReplicatedDataPacks;
};
