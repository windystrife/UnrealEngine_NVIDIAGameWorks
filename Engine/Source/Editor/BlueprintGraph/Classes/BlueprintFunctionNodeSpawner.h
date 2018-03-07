// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Templates/SubclassOf.h"
#include "BlueprintActionFilter.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintFieldNodeSpawner.h"
#include "BlueprintFunctionNodeSpawner.generated.h"

class UEdGraph;
class UK2Node_CallFunction;

/**
 * Takes care of spawning various UK2Node_CallFunction nodes. Acts as the 
 * "action" portion of certain FBlueprintActionMenuItems. 
 */
UCLASS(Transient)
class BLUEPRINTGRAPH_API UBlueprintFunctionNodeSpawner : public UBlueprintFieldNodeSpawner
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * Creates a new UBlueprintFunctionNodeSpawner for the specified function. 
	 * Does not do any compatibility checking to ensure that the function is 
	 * viable as a blueprint function call (do that before calling this).
	 *
	 * @param  Function		The function you want assigned to new nodes.
	 * @param  Outer		Optional outer for the new spawner (if left null, the transient package will be used).
	 * @return A newly allocated instance of this class.
	 */
	static UBlueprintFunctionNodeSpawner* Create(UFunction const* const Function, UObject* Outer = nullptr);

	/**
	 * Creates a new UBlueprintFunctionNodeSpawner for the specified function.
	 * Does not do any compatibility checking to ensure that the function is
	 * viable as a blueprint function call (do that before calling this).
	 * 
	 * @param  NodeClass	The type of node you want the spawner to create.
	 * @param  Function		The function you want assigned to new nodes.
	 * @param  Outer		Optional outer for the new spawner (if left null, the transient package will be used).
	 * @return A newly allocated instance of this class.
	 */
	static UBlueprintFunctionNodeSpawner* Create(TSubclassOf<UK2Node_CallFunction> NodeClass, UFunction const* const Function, UObject* Outer = nullptr);

	// UBlueprintNodeSpawner interface
	virtual void Prime() override;
	virtual FBlueprintActionUiSpec GetUiSpec(FBlueprintActionContext const& Context, FBindingSet const& Bindings) const override;
	virtual UEdGraphNode* Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const override;
	// End UBlueprintNodeSpawner interface

	// IBlueprintNodeBinder interface
	virtual bool IsBindingCompatible(UObject const* BindingCandidate) const override;
	virtual bool CanBindMultipleObjects() const override;
	virtual bool BindToNode(UEdGraphNode* Node, UObject* Binding) const override;
	// End IBlueprintNodeBinder interface

	/**
	 * Retrieves the function that this assigns to spawned nodes (defines the 
	 * node's signature).
	 *
	 * @return The function that this class was initialized with.
	 */
	UFunction const* GetFunction() const;
};
