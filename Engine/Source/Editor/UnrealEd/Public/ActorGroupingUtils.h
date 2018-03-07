// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ActorGroupingUtils.generated.h"

/**
 * Helper class for grouping actors in the level editor
 */
UCLASS(transient)
class UNREALED_API UActorGroupingUtils : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Actor Grouping")
	static bool IsGroupingActive() { return bGroupingActive; }

	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Actor Grouping")
	static void SetGroupingActive(bool bInGroupingActive);

	/**
	 * Convenience method for accessing grouping utils in a blueprint or script
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Actor Grouping", DisplayName="Get Actor Grouping Utils")
	static UActorGroupingUtils* Get();

	/**
	 * Creates a new group from the current selection removing the actors from any existing groups they are already in
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Actor Grouping")
	virtual void GroupSelected();

	/**
	 * Creates a new group from the provided list of actors removing the actors from any existing groups they are already in
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Actor Grouping")
	virtual void GroupActors(const TArray<AActor*>& ActorsToGroup);

	/**
	 * Disbands any groups in the current selection, does not attempt to maintain any hierarchy
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Actor Grouping")
	virtual void UngroupSelected();

	/**
	 * Disbands any groups that the provided actors belong to, does not attempt to maintain any hierarchy
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Actor Grouping")
	virtual void UngroupActors(const TArray<AActor*>& ActorsToUngroup);

	/**
	* Locks any groups in the current selection
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Actor Grouping")
	virtual void LockSelectedGroups();

	/**
	 * Unlocks any groups in the current selection
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Actor Grouping")
	virtual void UnlockSelectedGroups();

	/**
	 * Activates "Add to Group" mode which allows the user to select a group to append current selection
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Actor Grouping")
	virtual void AddSelectedToGroup();

	/**
	 * Removes any groups or actors in the current selection from their immediate parent.
	 * If all actors/subgroups are removed, the parent group will be destroyed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Actor Grouping")
	virtual void RemoveSelectedFromGroup();

protected:
	static bool bGroupingActive;
	static FSoftClassPath ClassToUse;
};


