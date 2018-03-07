// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "Tickable.h"
#include "LevelBounds.generated.h"

/**
 *  Ticks only in the editor, regardless of viewport 'Realtime' option
 */
class FEditorTickableLevelBounds 
#if WITH_EDITOR
	: FTickableGameObject
#endif
{
};

/**
 *
 * Defines level bounds
 * Updates bounding box automatically based on actors transformation changes or holds fixed user defined bounding box
 * Uses only actors where AActor::IsLevelBoundsRelevant() == true
 */
UCLASS(hidecategories=(Advanced, Collision, Display, Rendering, Physics, Input), showcategories=("Input|MouseInput", "Input|TouchInput"), MinimalAPI)
class ALevelBounds 
	: public AActor
	, public FEditorTickableLevelBounds 
{
	GENERATED_UCLASS_BODY()
		
	/** Whether to automatically update actor bounds based on all relevant actors bounds belonging to the same level */
	UPROPERTY(EditAnywhere, Category=LevelBounds)
	bool bAutoUpdateBounds;

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface
	
	//~ Begin AActor Interface.
	virtual FBox GetComponentsBoundingBox(bool bNonColliding = false) const override;
	virtual bool IsLevelBoundsRelevant() const override { return false; }
	//~ End AActor Interface.

	/** @return Bounding box which includes all relevant actors bounding boxes belonging to specified level */
	ENGINE_API static FBox CalculateLevelBounds(ULevel* InLevel);

#if WITH_EDITOR
	virtual void PostEditUndo() override;
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostRegisterAllComponents() override;
	virtual void PostUnregisterAllComponents() override;
	
	/** Marks level bounds as dirty so they will be recalculated on next tick */
	void MarkLevelBoundsDirty();

	/** @return True if there were no actors contributing to bounds and we are currently using the default bounds */
	ENGINE_API bool IsUsingDefaultBounds() const;

	/** Update level bounds immediately so the bounds are accurate when returning. Use only when needed because updating the bounds is slow */
	ENGINE_API void UpdateLevelBoundsImmediately();
	
private:
	/** FTickableGameObject interface */
	virtual void Tick(float DeltaTime) override;
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override;
	
	/** Updates this actor bounding box by summing all level actors bounding boxes  */
	void UpdateLevelBounds();

	/** Broadcasts LevelBoundsActorUpdatedEvent in case this actor acts as a level bounds */
	void BroadcastLevelBoundsUpdated();

	/** Called whenever any actor moved  */
	void OnLevelActorMoved(AActor* InActor);
	
	/** Called whenever any actor added or removed  */
	void OnLevelActorAddedRemoved(AActor* InActor);

	/** Subscribes for actors transformation events */
	void SubscribeToUpdateEvents();
	
	/** Unsubscribes from actors transformation events */
	void UnsubscribeFromUpdateEvents();
	
	/** Whether currently level bounds is dirty and needs to be updated  */
	bool bLevelBoundsDirty;

	/** True when there are no actors contributing to the bounds and we are currently using the default bounds instead */
	bool bUsingDefaultBounds;

	/** Handles to various registered delegates */
	FDelegateHandle OnLevelActorMovedDelegateHandle;
	FDelegateHandle OnLevelActorDeletedDelegateHandle;
	FDelegateHandle OnLevelActorAddedDelegateHandle;
#endif
};



