// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "EdGraph/EdGraphNode.h"
#include "BlueprintNodeSignature.h"
#include "BlueprintNodeBinder.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintBoundNodeSpawner.generated.h"

class UBlueprint;
class UEdGraph;

/**
 * Takes care of spawning various bound nodes. Acts as the 
 * "action" portion of certain FBlueprintActionMenuItems. 
 */
UCLASS(Transient)
class BLUEPRINTGRAPH_API UBlueprintBoundNodeSpawner : public UBlueprintNodeSpawner
{
	GENERATED_UCLASS_BODY()
	DECLARE_DELEGATE_RetVal_OneParam(bool, FCanBindObjectDelegate, UObject const*);
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnBindObjectDelegate, UEdGraphNode*, UObject*);
	DECLARE_DELEGATE_RetVal_TwoParams(UEdGraphNode*, FFindPreExistingNodeDelegate, const UBlueprint*, IBlueprintNodeBinder::FBindingSet const& );

public:
	/**
	 * @return A newly allocated instance of this class.
	 */
	static UBlueprintBoundNodeSpawner* Create(TSubclassOf<UEdGraphNode> NodeClass, UObject* Outer = nullptr);

	// UBlueprintNodeSpawner interface
	virtual FBlueprintNodeSignature GetSpawnerSignature() const override;
	virtual UEdGraphNode* Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const override;
	// End UBlueprintNodeSpawner interface

	// FBlueprintNodeBinder interface
	virtual bool IsBindingCompatible(UObject const* BindingCandidate) const override;
	virtual bool BindToNode(UEdGraphNode* Node, UObject* Binding) const override;
	// End FBlueprintNodeBinder interface

public:
	/**
	 * A delegate to perform specialized node binding verification
	 */
	FCanBindObjectDelegate CanBindObjectDelegate;

	/**
	 * A delegate to perform specialized node setup during binding
	 */
	FOnBindObjectDelegate OnBindObjectDelegate;

	/**
	 * A delegate to find a node that is already spawned, instead of spawning a node
	 * we will focus on the pre-existing node found by the delegate
	 */
	FFindPreExistingNodeDelegate FindPreExistingNodeDelegate;
};
