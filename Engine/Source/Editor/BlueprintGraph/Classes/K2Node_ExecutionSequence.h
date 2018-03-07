// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Textures/SlateIcon.h"
#include "K2Node.h"
#include "K2Node_AddPinInterface.h"
#include "K2Node_ExecutionSequence.generated.h"

class FBlueprintActionDatabaseRegistrar;
class UEdGraphPin;

UCLASS(MinimalAPI)
class UK2Node_ExecutionSequence : public UK2Node, public IK2Node_AddPinInterface
{
	GENERATED_UCLASS_BODY()

	//~ Begin UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	//~ End UEdGraphNode Interface

	//~ Begin UK2Node Interface
	void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual bool CanEverRemoveExecutionPin() const override { return true; }
	//~ End UK2Node Interface

	// IK2Node_AddPinInterface interface
	BLUEPRINTGRAPH_API virtual void AddInputPin() override;
	// End of IK2Node_AddPinInterface interface

	//~ Begin K2Node_ExecutionSequence Interface

	/** Gets a unique pin name, the next in the sequence */
	FString GetUniquePinName();

	/**
	 * Adds a new execution pin to an execution node
	 */
	DEPRECATED(4.17, "Use AddInputPin instead.")
	BLUEPRINTGRAPH_API void AddPinToExecutionNode() { AddInputPin(); }

	/**
	 * Removes the specified execution pin from an execution node
	 *
	 * @param	TargetPin	The pin to remove from the node
	 */
	BLUEPRINTGRAPH_API void RemovePinFromExecutionNode(UEdGraphPin* TargetPin);

	/** Whether an execution pin can be removed from the node or not */
	BLUEPRINTGRAPH_API bool CanRemoveExecutionPin() const;

	// @todo document
	BLUEPRINTGRAPH_API UEdGraphPin* GetThenPinGivenIndex(int32 Index);

private:
	// Returns the exec output pin name for a given 0-based index
	virtual FString GetPinNameGivenIndex(int32 Index) const;
};

