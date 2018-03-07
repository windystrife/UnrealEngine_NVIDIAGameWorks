// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * Base class of all global Blutility editor utilities.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "GlobalEditorUtilityBase.generated.h"

class AActor;
class UEditorPerProjectUserSettings;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams( FForEachActorIteratorSignature, class AActor*, Actor, int32, Index );
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams( FForEachAssetIteratorSignature, class UObject*, Asset, int32, Index );


UCLASS(Abstract, hideCategories=(Object), Blueprintable)
class BLUTILITY_API UGlobalEditorUtilityBase : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(Category=Config, EditDefaultsOnly, BlueprintReadWrite, AssetRegistrySearchable)
	FString HelpText;

	UPROPERTY(Transient)
	bool bDirtiedSelectionSet;

	/** UObject interface */
	virtual UWorld* GetWorld() const override;

	UFUNCTION(BlueprintCallable, Category="Development|Editor")
	TArray<AActor*> GetSelectionSet();

	/**
	 * Attempts to find the actor specified by PathToActor in the current editor world
	 * @param	PathToActor	The path to the actor (e.g. PersistentLevel.PlayerStart)
	 * @return	A reference to the actor, or none if it wasn't found
	 */
	UFUNCTION(BlueprintPure, Category = "Development|Editor")
	AActor* GetActorReference(FString PathToActor);

	////////////////////////////

	// Should this blueprint automatically run OnDefaultActionClicked, or should it open up a details panel to edit properties and/or offer multiple buttons
	UPROPERTY(Category=Settings, EditDefaultsOnly, BlueprintReadOnly)
	bool bAutoRunDefaultAction;

	// The default action called when the blutility is invoked if bAutoRunDefaultAction=true (it is never called otherwise)
	UFUNCTION(BlueprintImplementableEvent)
	void OnDefaultActionClicked();

	////////////////////////////

	// Calls OnEachSelectedActor for each selected actor
	UFUNCTION(BlueprintCallable, Category="Development|Editor")
	void ForEachSelectedActor();

	// The method called for each selected actor when ForEachSelectedActor is called
	UPROPERTY(BlueprintAssignable)
	FForEachActorIteratorSignature OnEachSelectedActor;

	////////////////////////////

	// Calls OnEachSelectedAsset for each selected asset
	UFUNCTION(BlueprintCallable, Category="Development|Editor")
	void ForEachSelectedAsset();

	// The method called for each selected actor when ForEachSelectedAsset is called
	UPROPERTY(BlueprintAssignable)
	FForEachAssetIteratorSignature OnEachSelectedAsset;

	///////////////////////////

	UFUNCTION(BlueprintPure, Category="Development|Editor")
	UEditorPerProjectUserSettings* GetEditorUserSettings();

	// Remove all actors from the selection set
	UFUNCTION(BlueprintCallable, Category="Development|Editor")
	void ClearActorSelectionSet();

	// Selects nothing in the editor (another way to clear the selection)
	UFUNCTION(BlueprintCallable, Category = "Development|Editor")
	void SelectNothing();

	// Set the selection state for the selected actor
	UFUNCTION(BlueprintCallable, Category="Development|Editor")
	void SetActorSelectionState(AActor* Actor, bool bShouldBeSelected);

	UFUNCTION(BlueprintCallable, Category="Development|Editor")
	void GetSelectionBounds(FVector& Origin, FVector& BoxExtent, float& SphereRadius);

	// Renames an asset (cannot move folders)
	UFUNCTION(BlueprintCallable, Category = "Development|Editor")
	void RenameAsset(UObject* Asset, const FString& NewName);

	////////////////////////////

	// Run the default action
	void ExecuteDefaultAction();

	// Handles notifying the editor if the recent command mucked with the selection set
	void PostExecutionCleanup();
};
