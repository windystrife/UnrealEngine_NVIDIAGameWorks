// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "Components/ActorComponent.h"
#include "BlueprintNodeSignature.h"
#include "BlueprintActionFilter.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintComponentNodeSpawner.generated.h"

class UEdGraph;

/**
 * Takes care of spawning UK2Node_AddComponent nodes. Acts as the "action" 
 * portion of certain FBlueprintActionMenuItems. Evolved from 
 * FEdGraphSchemaAction_K2AddComponent.
 */
UCLASS(Transient)
class BLUEPRINTGRAPH_API UBlueprintComponentNodeSpawner : public UBlueprintNodeSpawner
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * Creates a new UBlueprintComponentNodeSpawner for the specified class. 
	 * Does not do any compatibility checking to ensure that the class is 
	 * viable as a spawnable component (do that before calling this).
	 *
	 * @param  ComponentClass	The component type you want spawned nodes to spawn.
	 * @return A newly allocated instance of this class.
	 */
	static UBlueprintComponentNodeSpawner* Create(const struct FComponentTypeEntry& Entry);

	// UBlueprintNodeSpawner interface
	virtual FBlueprintNodeSignature GetSpawnerSignature() const override;
	virtual FBlueprintActionUiSpec GetUiSpec(FBlueprintActionContext const& Context, FBindingSet const& Bindings) const override;
	virtual UEdGraphNode* Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const override;
	// End UBlueprintNodeSpawner interface
	
	// IBlueprintNodeBinder interface
	virtual bool IsBindingCompatible(UObject const* BindingCandidate) const override;
	virtual bool BindToNode(UEdGraphNode* Node, UObject* Binding) const override;
	// End IBlueprintNodeBinder interface

	/**
	 * Retrieves the component class that this configures spawned nodes with.
	 *
	 * @return The component type that this class was initialized with.
	 */
	TSubclassOf<UActorComponent> GetComponentClass() const;

private:
	/** The component class to configure new nodes with. */
	UPROPERTY()
	TSubclassOf<UActorComponent> ComponentClass;

	/** The name of the component class to configure new nodes with. */
	UPROPERTY()
	FString ComponentName;
	
	/** The name of the asset name that needs to be loaded */
	UPROPERTY()
	FString ComponentAssetName;
};
