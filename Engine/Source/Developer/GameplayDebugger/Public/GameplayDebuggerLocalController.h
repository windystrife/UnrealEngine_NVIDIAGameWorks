// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "GameplayDebuggerLocalController.generated.h"

class AActor;
class AGameplayDebuggerCategoryReplicator;
class AGameplayDebuggerPlayerManager;
class FGameplayDebuggerCanvasContext;
class FGameplayDebuggerCategory;
class UInputComponent;
struct FKey;

UCLASS(NotBlueprintable, NotBlueprintType, noteditinlinenew, hidedropdown, Transient)
class UGameplayDebuggerLocalController : public UObject
{
	GENERATED_UCLASS_BODY()

	virtual void BeginDestroy() override;

	/** initialize controller with replicator owner */
	void Initialize(AGameplayDebuggerCategoryReplicator& Replicator, AGameplayDebuggerPlayerManager& Manager);

	/** remove from world */
	void Cleanup();

	/** drawing event */
	void OnDebugDraw(class UCanvas* Canvas, class APlayerController* PC);

	/** binds input actions */
	void BindInput(UInputComponent& InputComponent);

	/** checks if key is bound by any action */
	bool IsKeyBound(const FName KeyName) const;

protected:

	UPROPERTY()
	AGameplayDebuggerCategoryReplicator* CachedReplicator;

	UPROPERTY()
	AGameplayDebuggerPlayerManager* CachedPlayerManager;

	UPROPERTY()
	AActor* DebugActorCandidate;

	TArray<TArray<int32> > DataPackMap;
	TArray<TArray<int32> > SlotCategoryIds;
	TArray<FString> SlotNames;

	TSet<FName> UsedBindings;

	uint32 bSimulateMode : 1;
	uint32 bNeedsCleanup : 1;
	uint32 bIsSelectingActor : 1;
	uint32 bIsLocallyEnabled : 1;
	uint32 bPrevLocallyEnabled : 1;

	FString ActivationKeyDesc;
	FString RowUpKeyDesc;
	FString RowDownKeyDesc;
	FString CategoryKeysDesc;

	int32 ActiveRowIdx;
	int32 NumCategorySlots;
	int32 NumCategories;

	float PaddingLeft;
	float PaddingRight;
	float PaddingTop;
	float PaddingBottom;

	FTimerHandle StartSelectingActorHandle;
	FTimerHandle SelectActorTickHandle;

	void OnActivationPressed();
	void OnActivationReleased();
	void OnCategory0Pressed();
	void OnCategory1Pressed();
	void OnCategory2Pressed();
	void OnCategory3Pressed();
	void OnCategory4Pressed();
	void OnCategory5Pressed();
	void OnCategory6Pressed();
	void OnCategory7Pressed();
	void OnCategory8Pressed();
	void OnCategory9Pressed();
	void OnCategoryRowUpPressed();
	void OnCategoryRowDownPressed();
	void OnCategoryBindingEvent(int32 CategoryId, int32 HandlerId);
	void OnExtensionBindingEvent(int32 ExtensionId, int32 HandlerId);

	/** called short time after activation key was pressed and hold */
	void OnStartSelectingActor();

	/** called in tick during actor selection */
	void OnSelectActorTick();

	/** toggle state of categories in given slot */
	void ToggleSlotState(int32 SlotIdx);

	/** draw header row */
	void DrawHeader(FGameplayDebuggerCanvasContext& CanvasContext);

	/** draw header for category */
	void DrawCategoryHeader(int32 CategoryId, TSharedRef<FGameplayDebuggerCategory> Category, FGameplayDebuggerCanvasContext& CanvasContext);

	/** event for simulate in editor mode */
	void OnSelectionChanged(UObject* Object);

	FString GetKeyDescriptionShort(const FKey& KeyBind) const;
	FString GetKeyDescriptionLong(const FKey& KeyBind) const;

	/** called when known category set has changed */
	void OnCategoriesChanged();

	/** build DataPackMap for replication details */
	void RebuildDataPackMap();
};
