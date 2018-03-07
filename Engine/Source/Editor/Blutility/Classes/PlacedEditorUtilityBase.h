// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * Base class of all placed Blutility editor utilities.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "PlacedEditorUtilityBase.generated.h"

UCLASS(Abstract, hideCategories=(Object, Actor)/*, Blueprintable*/)
class BLUTILITY_API APlacedEditorUtilityBase : public AActor
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(Category=Config, EditDefaultsOnly, BlueprintReadWrite)
	FString HelpText;

	// AActor interface
	virtual void TickActor(float DeltaSeconds, ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;
	// End of AActor interface

	/**
	 * Returns the current selection set in the editor.  Note that for non-editor builds, this will return an empty array
	 */
	UFUNCTION(BlueprintPure, Category = "Development|Editor")
	TArray<AActor*> GetSelectionSet();

	/**
	 * Gets information about the camera position for the primary level editor viewport.  In non-editor builds, these will be zeroed
	 *
	 * @param	CameraLocation	(out) Current location of the level editing viewport camera, or zero if none found
	 * @param	CameraRotation	(out) Current rotation of the level editing viewport camera, or zero if none found
	 * @return	Whether or not we were able to get a camera for a level editing viewport
	 */
	UFUNCTION(BlueprintPure, Category = "Development|Editor")
	bool GetLevelViewportCameraInfo(FVector& CameraLocation, FRotator& CameraRotation);

	/**
	* Sets information about the camera position for the primary level editor viewport.
	*
	* @param	CameraLocation	Location the camera will be moved to.
	* @param	CameraRotation	Rotation the camera will be set to.
	*/
	UFUNCTION(BlueprintCallable, Category = "Development|Editor")
	void SetLevelViewportCameraInfo(FVector CameraLocation, FRotator CameraRotation);

	// Remove all actors from the selection set
	UFUNCTION(BlueprintCallable, Category = "Development|Editor")
	void ClearActorSelectionSet();

	// Selects nothing in the editor (another way to clear the selection)
	UFUNCTION(BlueprintCallable, Category = "Development|Editor")
	void SelectNothing();

	// Set the selection state for the selected actor
	UFUNCTION(BlueprintCallable, Category = "Development|Editor")
	void SetActorSelectionState(AActor* Actor, bool bShouldBeSelected);

	/**
	* Attempts to find the actor specified by PathToActor in the current editor world
	* @param	PathToActor	The path to the actor (e.g. PersistentLevel.PlayerStart)
	* @return	A reference to the actor, or none if it wasn't found
	*/
	UFUNCTION(BlueprintPure, Category = "Development|Editor")
	AActor* GetActorReference(FString PathToActor);
};
