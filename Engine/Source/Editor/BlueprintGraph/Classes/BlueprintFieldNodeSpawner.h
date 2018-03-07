// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Templates/SubclassOf.h"
#include "BlueprintNodeSignature.h"
#include "K2Node.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintFieldNodeSpawner.generated.h"

class UEdGraph;

/**
 * Takes care of spawning various field related nodes (nodes associated with 
 * functions, enums, structs, properties, etc.). Acts as the "action" portion
 * for certain FBlueprintActionMenuItems. 
 */
UCLASS(Transient)
class BLUEPRINTGRAPH_API UBlueprintFieldNodeSpawner : public UBlueprintNodeSpawner
{
	GENERATED_UCLASS_BODY()
	DECLARE_DELEGATE_TwoParams(FSetNodeFieldDelegate, UEdGraphNode*, UField const*);

public:
	/**
	 * Creates a new UBlueprintFieldNodeSpawner for the supplied field.
	 * Does not do any compatibility checking to ensure that the field is
	 * viable for blueprint use.
	 * 
	 * @param  NodeClass	The type of node you want the spawner to create.
	 * @param  Field		The field you want assigned to new nodes.
	 * @param  Outer		Optional outer for the new spawner (if left null, the transient package will be used).
	 * @return A newly allocated instance of this class.
	 */
	static UBlueprintFieldNodeSpawner* Create(TSubclassOf<UK2Node> NodeClass, UField const* const Field, UObject* Outer = nullptr);

	// UBlueprintNodeSpawner interface
	virtual FBlueprintNodeSignature GetSpawnerSignature() const override;
	virtual UEdGraphNode* Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const override;
	// End UBlueprintNodeSpawner interface

	/** Callback to define how the field should be applied to new nodes */
	FSetNodeFieldDelegate SetNodeFieldDelegate;

	/**
	 * Retrieves the field that this assigns to spawned nodes (defines the 
	 * node's signature).
	 *
	 * @return The field that this class was initialized with.
	 */
	UField const* GetField() const;

protected:
	/** The field to configure new nodes with. */
	UPROPERTY()
	UField const* Field;
};
