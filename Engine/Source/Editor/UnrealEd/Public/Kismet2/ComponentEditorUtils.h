// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectHash.h"

class FMenuBuilder;
class UMaterialInterface;

class UNREALED_API FComponentEditorUtils
{
public:
	/** Tests whether the native component is editable */
	static bool CanEditNativeComponent(const UActorComponent* NativeComponent);

	/** Test whether or not the given string is a valid variable name string for the given component instance */
	static bool IsValidVariableNameString(const UActorComponent* InComponent, const FString& InString);

	/** 
	 * Test whether or not the given string is already the name string of a component on the the actor
	 * Optionally excludes an existing component from the check (ex. a component currently being renamed)
	 * @return True if the InString is an available name for a component of ComponentOwner
	 */
	static bool IsComponentNameAvailable(const FString& InString, AActor* ComponentOwner, const UActorComponent* ComponentToIgnore = nullptr);
		
	/** Generate a valid variable name string for the given component instance */
	static FString GenerateValidVariableName(TSubclassOf<UActorComponent> InComponentClass, AActor* ComponentOwner);

	/** Generate a valid variable name string for the given component instance based on the name of the asset referenced by the component */
	static FString GenerateValidVariableNameFromAsset(UObject* Asset, AActor* ComponentOwner);

	/**
	 * Checks whether it is valid to copy the indicated components
	 * @param ComponentsToCopy The list of components to check
	 * @return Whether the indicated components can be copied
	 */
	static bool CanCopyComponents(const TArray<UActorComponent*>& ComponentsToCopy);

	/**
	 * Copies the selected components to the clipboard
	 * @param ComponentsToCopy The list of components to copy
	 */
	static void CopyComponents(const TArray<UActorComponent*>& ComponentsToCopy);

	/**
	 * Determines whether the current contents of the clipboard contain paste-able component information
	 * @param RootComponent The root component of the actor being pasted on
	 * @param bOverrideCanAttach Optional override declaring that components can be attached and a check is not needed
	 * @return Whether components can be pasted
	 */
	static bool CanPasteComponents(USceneComponent* RootComponent, bool bOverrideCanAttach = false, bool bPasteAsArchetypes = false);

	/**
	 * Attempts to paste components from the clipboard as siblings of the target component
	 * @param OutPastedComponents List of all the components that were pasted
	 * @param TargetActor The actor to attach the pasted components to
	 * @param TargetComponent The component the paste is targeting (will attempt to paste components as siblings). If null, will attach pasted components to the root.
	 */
	static void PasteComponents(TArray<UActorComponent*>& OutPastedComponents, AActor* TargetActor, USceneComponent* TargetComponent = nullptr);

	/**
	 * Gets the copied components from the clipboard without attempting to paste/apply them in any way
	 * @param OutParentMap Contains the child->parent name relationships of the copied components
	 * @param OutNewObjectMap Contains the name->instance object mapping of the copied components
	 */
	static void GetComponentsFromClipboard(TMap<FName, FName>& OutParentMap, TMap<FName, UActorComponent*>& OutNewObjectMap, bool bGetComponentsAsArchetypes);

	/**
	 * Determines whether the indicated components can be deleted
	 * @param ComponentsToDelete The list of components to determine can be deleted
	 * @return Whether the indicated components can be deleted
	 */
	static bool CanDeleteComponents(const TArray<UActorComponent*>& ComponentsToDelete);

	/**
	 * Deletes the indicated components and identifies the component that should be selected following the operation.
	 * Note: Does not take care of the actual selection of a new component. It only identifies which component should be selected.
	 * 
	 * @param ComponentsToDelete The list of components to delete
	 * @param OutComponentToSelect The component that should be selected after the deletion
	 * @return The number of components that were actually deleted
	 */
	static int32 DeleteComponents(const TArray<UActorComponent*>& ComponentsToDelete, UActorComponent*& OutComponentToSelect);

	/** 
	 * Duplicates a component instance and takes care of attachment and registration.
	 * @param TemplateComponent The component to duplicate
	 * @return The created clone of the provided TemplateComponent. nullptr if the duplication failed.
	 */
	static UActorComponent* DuplicateComponent(UActorComponent* TemplateComponent);

	/**
	 * Ensures that the selection override delegate is properly bound for the supplied component
	* This includes any attached editor-only primitive components (such as billboard visualizers)
	 * 
	 * @param SceneComponent The component to set the selection override for
	 * @param bBind Whether the override should be bound
	*/
	static void BindComponentSelectionOverride(USceneComponent* SceneComponent, bool bBind);

	/**
	 * Attempts to apply a material to a component at the specified slot.
	 *
	 * @param SceneComponent The component to which we should attempt to apply the material
	 * @param MaterialToApply The material to apply to the component
	 * @param OptionalMaterialSlot The material slot on the component to which the material should be applied. -1 to apply to all slots on the component.
	 *
	 * @return	True if the material was successfully applied to the component.
	 */
	static bool AttemptApplyMaterialToComponent( USceneComponent* SceneComponent, UMaterialInterface* MaterialToApply, int32 OptionalMaterialSlot = -1 );

	/** Potentially transforms the delta to be applied to a component into the appropriate space */
	static void AdjustComponentDelta(USceneComponent* Component, FVector& Drag, FRotator& Rotation);

	// Given a template and a property, propagates a default value change to all instances (only if applicable)
	template<typename T>
	static void PropagateDefaultValueChange(class USceneComponent* InSceneComponentTemplate, const class UProperty* InProperty, const T& OldDefaultValue, const T& NewDefaultValue, TSet<class USceneComponent*>& UpdatedInstances, int32 PropertyOffset = INDEX_NONE)
	{
		TArray<UObject*> ArchetypeInstances;
		if(InSceneComponentTemplate->HasAnyFlags(RF_ArchetypeObject))
		{
			InSceneComponentTemplate->GetArchetypeInstances(ArchetypeInstances);
			for(int32 InstanceIndex = 0; InstanceIndex < ArchetypeInstances.Num(); ++InstanceIndex)
			{
				USceneComponent* InstancedSceneComponent = static_cast<USceneComponent*>(ArchetypeInstances[InstanceIndex]);
				if(InstancedSceneComponent != nullptr && !UpdatedInstances.Contains(InstancedSceneComponent) && ApplyDefaultValueChange(InstancedSceneComponent, InProperty, OldDefaultValue, NewDefaultValue, PropertyOffset))
				{
					UpdatedInstances.Add(InstancedSceneComponent);
				}
			}
		}
		else if(UObject* Outer = InSceneComponentTemplate->GetOuter())
		{
			Outer->GetArchetypeInstances(ArchetypeInstances);
			for(int32 InstanceIndex = 0; InstanceIndex < ArchetypeInstances.Num(); ++InstanceIndex)
			{
				USceneComponent* InstancedSceneComponent = static_cast<USceneComponent*>(FindObjectWithOuter(ArchetypeInstances[InstanceIndex], InSceneComponentTemplate->GetClass(), InSceneComponentTemplate->GetFName()));
				if(InstancedSceneComponent != nullptr && !UpdatedInstances.Contains(InstancedSceneComponent) && ApplyDefaultValueChange(InstancedSceneComponent, InProperty, OldDefaultValue, NewDefaultValue, PropertyOffset))
				{
					UpdatedInstances.Add(InstancedSceneComponent);
				}
			}
		}
	}

	// Given an instance of a template and a property, set a default value change to the instance (only if applicable)
	template<typename T>
	static bool ApplyDefaultValueChange(class USceneComponent* InSceneComponent, const class UProperty* InProperty, const T& OldDefaultValue, const T& NewDefaultValue, int32 PropertyOffset)
	{
		check(InProperty != nullptr);
		check(InSceneComponent != nullptr);

		ensureMsgf(Cast<UBoolProperty>(InProperty) == nullptr, TEXT("ApplyDefaultValueChange cannot be safely called on a bool property with a non-bool value, becuase of bitfields"));

		T* CurrentValue = PropertyOffset == INDEX_NONE ? InProperty->ContainerPtrToValuePtr<T>(InSceneComponent) : (T*)((uint8*)InSceneComponent + PropertyOffset);
		check(CurrentValue);

		return ApplyDefaultValueChange(InSceneComponent, *CurrentValue, OldDefaultValue, NewDefaultValue);
	}

	// Bool specialization so it can properly handle bitfields
	static bool ApplyDefaultValueChange(class USceneComponent* InSceneComponent, const class UProperty* InProperty, const bool& OldDefaultValue, const bool& NewDefaultValue, int32 PropertyOffset)
	{
		check(InProperty != nullptr);
		check(InSceneComponent != nullptr);
		
		// Only bool properties can have bool values
		const UBoolProperty* BoolProperty = Cast<UBoolProperty>(InProperty);
		check(BoolProperty);

		uint8* CurrentValue = PropertyOffset == INDEX_NONE ? InProperty->ContainerPtrToValuePtr<uint8>(InSceneComponent) : ((uint8*)InSceneComponent + PropertyOffset);
		check(CurrentValue);

		bool CurrentBool = BoolProperty->GetPropertyValue(CurrentValue);
		if (ApplyDefaultValueChange(InSceneComponent, CurrentBool, OldDefaultValue, NewDefaultValue, false))
		{
			BoolProperty->SetPropertyValue(CurrentValue, CurrentBool);

			InSceneComponent->ReregisterComponent();

			return true;
		}

		return false;
	}

	// Given an instance of a template and a current value, propagates a default value change to the instance (only if applicable)
	template<typename T>
	static bool ApplyDefaultValueChange(class USceneComponent* InSceneComponent, T& CurrentValue, const T& OldDefaultValue, const T& NewDefaultValue, bool bReregisterComponent = true)
	{
		check(InSceneComponent != nullptr);

		// Propagate the change only if the current instanced value matches the previous default value (otherwise this could overwrite any per-instance override)
		if(NewDefaultValue != OldDefaultValue && CurrentValue == OldDefaultValue)
		{
			// Ensure that this instance will be included in any undo/redo operations, and record it into the transaction buffer.
			// Note: We don't do this for components that originate from script, because they will be re-instanced from the template after an undo, so there is no need to record them.
			if (!InSceneComponent->IsCreatedByConstructionScript())
			{
				InSceneComponent->SetFlags(RF_Transactional);
				InSceneComponent->Modify();
			}

			// We must also modify the owner, because we'll need script components to be reconstructed as part of an undo operation.
			AActor* Owner = InSceneComponent->GetOwner();
			if(Owner != nullptr)
			{
				Owner->Modify();
			}

			// Modify the value
			CurrentValue = NewDefaultValue;

			if (bReregisterComponent && InSceneComponent->IsRegistered())
			{
				// Re-register the component with the scene so that transforms are updated for display
				InSceneComponent->ReregisterComponent();
			}
			
			return true;
		}

		return false;
	}

	// Try to find the correct variable name for a given native component template or instance (which can have a mismatch)
	static FName FindVariableNameGivenComponentInstance(UActorComponent* ComponentInstance);

	/**
	* Populates the given menu with basic options for operations on components in the world.
	* @param MenuBuilder Used to create the menu options
	* @param SelectedComponents The selected components to create menu options for
	*/
	static void FillComponentContextMenuOptions(FMenuBuilder& MenuBuilder, const TArray<UActorComponent*>& SelectedComponents);

private:	
	static USceneComponent* FindClosestParentInList(UActorComponent* ChildComponent, const TArray<UActorComponent*>& ComponentList);

	static void OnGoToComponentAssetInBrowser(UObject* Asset);
	static void OnOpenComponentCodeFile(const FString CodeFileName);
	static void OnEditBlueprintComponent(UObject* Blueprint);
};
