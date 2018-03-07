// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "UObject/UnrealType.h"
#include "K2Node_BaseMCDelegate.h"
#include "BlueprintFieldNodeSpawner.h"
#include "BlueprintDelegateNodeSpawner.generated.h"

/**
 * Takes care of spawning various nodes associated with delegates. Serves as the 
 * "action" portion for certain FBlueprintActionMenuItems. Evolved from 
 * FEdGraphSchemaAction_K2Delegate, FEdGraphSchemaAction_K2AssignDelegate, etc.
 */
UCLASS(Transient)
class BLUEPRINTGRAPH_API UBlueprintDelegateNodeSpawner : public UBlueprintFieldNodeSpawner
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * Creates a new UBlueprintDelegateNodeSpawner for the specified property.
	 * Does not do any compatibility checking to ensure that the property is
	 * accessible from blueprints (do that before calling this).
	 *
	 * @param  NodeClass	The node type that you want the spawner to spawn.
	 * @param  Property		The property you want assigned to spawned nodes.
	 * @param  Outer		Optional outer for the new spawner (if left null, the transient package will be used).
	 * @return A newly allocated instance of this class.
	 */
	static UBlueprintDelegateNodeSpawner* Create(TSubclassOf<UK2Node_BaseMCDelegate> NodeClass, UMulticastDelegateProperty const* const Property, UObject* Outer = nullptr);

	/**
	 * Accessor to the delegate property that this spawner wraps (the delegate
	 * that this will assign spawned nodes with).
	 *
	 * @return The delegate property that this was initialized with.
	 */
	UMulticastDelegateProperty const* GetDelegateProperty() const;
};
