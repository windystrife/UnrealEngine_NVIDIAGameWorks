// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Templates/SubclassOf.h"
#include "BlueprintNodeSignature.h"
#include "BlueprintNodeSpawner.h"
#include "K2Node_Event.h"
#include "BlueprintEventNodeSpawner.generated.h"

class UEdGraph;

/**
 * Takes care of spawning UK2Node_Event nodes. Acts as the "action" portion of
 * certain FBlueprintActionMenuItems. Will not spawn a new event node if one
 * associated with the specified function already exits (instead, Invoke() will
 * return the existing one). Evolved from FEdGraphSchemaAction_K2AddEvent and 
 * FEdGraphSchemaAction_K2ViewNode.
 */
UCLASS(Transient)
class BLUEPRINTGRAPH_API UBlueprintEventNodeSpawner : public UBlueprintNodeSpawner
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * Creates a new UBlueprintEventNodeSpawner for the specified function.
	 * Does not do any compatibility checking to ensure that the function is
	 * viable as a blueprint event (do that before calling this).
	 *
	 * @param  EventFunc	The function you want assigned to new nodes.
	 * @param  Outer		Optional outer for the new spawner (if left null, the transient package will be used).
	 * @return A newly allocated instance of this class.
	 */
	static UBlueprintEventNodeSpawner* Create(UFunction const* const EventFunc, UObject* Outer = nullptr);

	/**
	 * Creates a new UBlueprintEventNodeSpawner for custom events. The
	 * CustomEventName can be left blank if the node will pick one itself on
	 * instantiation.
	 *
	 * @param  NodeClass		The event node type that you want this to spawn.
	 * @param  CustomEventName	The name you want assigned to the event.
	 * @param  Outer			Optional outer for the new spawner (if left null, the transient package will be used).
	 * @return A newly allocated instance of this class.
	 */
	static UBlueprintEventNodeSpawner* Create(TSubclassOf<UK2Node_Event> NodeClass, FName CustomEventName, UObject* Outer = nullptr);

	// UBlueprintNodeSpawner interface
	virtual FBlueprintNodeSignature GetSpawnerSignature() const override;
	virtual UEdGraphNode* Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const override;
	// End UBlueprintNodeSpawner interface

	/**
	 * 
	 * 
	 * @return 
	 */
	bool IsForCustomEvent() const;
	
	/**
	 * Retrieves the function that this assigns to spawned nodes (defines the
	 * event's signature). Can be null if this spawner was for a custom event.
	 *
	 * @return The event function that this class was initialized with.
	 */
	UFunction const* GetEventFunction() const;

	/**
	 * 
	 * 
	 * @return 
	 */
	virtual UK2Node_Event const* FindPreExistingEvent(UBlueprint* Blueprint, FBindingSet const& Bindings) const;

private:
	/** The function to configure new nodes with. */
	UPROPERTY()
    UFunction const* EventFunc;

	/** The custom name to configure new event nodes with. */
	UPROPERTY()
	FName CustomEventName;	
};
