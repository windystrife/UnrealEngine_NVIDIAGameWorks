// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "GameFramework/Actor.h"
#include "GameplayDebuggerTypes.h"
#include "GameplayDebuggerCategoryReplicator.generated.h"

class AGameplayDebuggerCategoryReplicator;
class APlayerController;
class FGameplayDebuggerCategory;
class FGameplayDebuggerExtension;
class UGameplayDebuggerRenderingComponent;

USTRUCT()
struct FGameplayDebuggerNetPack
{
	GENERATED_USTRUCT_BODY()

	AGameplayDebuggerCategoryReplicator* Owner;

	FGameplayDebuggerNetPack() : Owner(nullptr) {}
	bool NetDeltaSerialize(FNetDeltaSerializeInfo & DeltaParms);
	void OnCategoriesChanged();

private:
	struct FCategoryData
	{
		TArray<FString> TextLines;
		TArray<FGameplayDebuggerShape> Shapes;
		TArray<FGameplayDebuggerDataPack::FHeader> DataPacks;
		bool bIsEnabled;
	};
	TArray<FCategoryData> SavedData;
};

template<>
struct TStructOpsTypeTraits<FGameplayDebuggerNetPack> : public TStructOpsTypeTraitsBase2<FGameplayDebuggerNetPack>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};

USTRUCT()
struct FGameplayDebuggerDebugActor
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	AActor* Actor;

	UPROPERTY()
	FName ActorName;

	UPROPERTY()
	int32 SyncCounter;
};

UCLASS(NotBlueprintable, NotBlueprintType, notplaceable, noteditinlinenew, hidedropdown, Transient)
class GAMEPLAYDEBUGGER_API AGameplayDebuggerCategoryReplicator : public AActor
{
	GENERATED_UCLASS_BODY()

	virtual class UNetConnection* GetNetConnection() const override;
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;

protected:
	virtual void BeginPlay() override;

public:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;
	virtual void PostNetReceive() override;

	/** [AUTH] set new owner */
	void SetReplicatorOwner(APlayerController* InOwnerPC);

	/** [ALL] set replicator state */
	void SetEnabled(bool bEnable);

	/** [ALL] set category state */
	void SetCategoryEnabled(int32 CategoryId, bool bEnable);

	/** [ALL] set actor for debugging */
	void SetDebugActor(AActor* Actor);

	/** [ALL] send input event to category */
	void SendCategoryInputEvent(int32 CategoryId, int32 HandlerId);

	/** [ALL] send input event to extension */
	void SendExtensionInputEvent(int32 ExtensionId, int32 HandlerId);

	/** [AUTH] starts data collection */
	void CollectCategoryData(bool bForce = false);

	/** get current debug actor */
	AActor* GetDebugActor() const { return IsValid(DebugActor.Actor) ? DebugActor.Actor : nullptr; }
	
	/** get name of debug actor */
	FName GetDebugActorName() const { return DebugActor.ActorName; }

	/** get sync counter, increased with every change of DebugActor */
	int16 GetDebugActorCounter() const { return DebugActor.SyncCounter; }

	/** get player controller owning this replicator */
	APlayerController* GetReplicationOwner() const { return OwnerPC; }

	/** get replicator state */
	bool IsEnabled() const { return bIsEnabled; }

	/** get category state */
	bool IsCategoryEnabled(int32 CategoryId) const;

	/** check if debug actor was selected */
	bool HasDebugActor() const { return DebugActor.ActorName != NAME_None; }

	/** get category count */
	int32 GetNumCategories() const { return Categories.Num(); }

	/** get extension count */
	int32 GetNumExtensions() const { return Extensions.Num(); }

	/** get category object */
	TSharedRef<FGameplayDebuggerCategory> GetCategory(int32 CategoryId) const { return Categories[CategoryId]; }

	/** get category object */
	TSharedRef<FGameplayDebuggerExtension> GetExtension(int32 ExtensionId) const { return Extensions[ExtensionId]; }

	/** returns true if object was created for local player (client / standalone) */
	bool IsLocal() const { return bIsLocal; }

protected:

	friend FGameplayDebuggerNetPack;

	UPROPERTY(Replicated)
	APlayerController* OwnerPC;

	UPROPERTY(Replicated)
	bool bIsEnabled;

	UPROPERTY(Replicated)
	FGameplayDebuggerNetPack ReplicatedData;

	UPROPERTY(Replicated)
	FGameplayDebuggerDebugActor	DebugActor;

	/** rendering component needs to attached to some actor, and this is as good as any */
	UPROPERTY()
	UGameplayDebuggerRenderingComponent* RenderingComp;

	/** category objects */
	TArray<TSharedRef<FGameplayDebuggerCategory> > Categories;

	/** extension objects */
	TArray<TSharedRef<FGameplayDebuggerExtension> > Extensions;

	uint32 bIsEnabledLocal : 1;
	uint32 bHasAuthority : 1;
	uint32 bIsLocal : 1;

	/** notify about changes in known category set */
	void OnCategoriesChanged();

	/** notify about changes in known extensions set */
	void OnExtensionsChanged();

	/** send notifies to all categories about current tool state */
	void NotifyCategoriesToolState(bool bIsActive);

	/** send notifies to all categories about current tool state */
	void NotifyExtensionsToolState(bool bIsActive);

	UFUNCTION(Server, Reliable, WithValidation, meta = (CallInEditor = "true"))
	void ServerSetEnabled(bool bEnable);

	UFUNCTION(Server, Reliable, WithValidation, meta = (CallInEditor = "true"))
	void ServerSetDebugActor(AActor* Actor);

	UFUNCTION(Server, Reliable, WithValidation, meta = (CallInEditor = "true"))
	void ServerSetCategoryEnabled(int32 CategoryId, bool bEnable);

	/** helper function for replicating input for category handlers */
	UFUNCTION(Server, Reliable, WithValidation, meta = (CallInEditor = "true"))
	void ServerSendCategoryInputEvent(int32 CategoryId, int32 HandlerId);

	/** helper function for replicating input for extension handlers */
	UFUNCTION(Server, Reliable, WithValidation, meta = (CallInEditor = "true"))
	void ServerSendExtensionInputEvent(int32 ExtensionId, int32 HandlerId);

	/** [LOCAL] notify from CategoryData replication */
	void OnReceivedDataPackPacket(int32 CategoryId, int32 DataPackId, const FGameplayDebuggerDataPack& DataPacket);
};
