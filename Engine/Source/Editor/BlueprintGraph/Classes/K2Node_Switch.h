// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node.h"
#include "Textures/SlateIcon.h"
#include "K2Node_Switch.generated.h"

UCLASS(MinimalAPI, abstract)
class UK2Node_Switch : public UK2Node
{
	GENERATED_UCLASS_BODY()

	/** If true switch has a default pin */
	UPROPERTY(EditAnywhere, Category=PinOptions)
	uint32 bHasDefaultPin:1;

	/* The function underpining the switch, if required */
	UPROPERTY()
	FName FunctionName;

	/** The class that the function is from. */
	UPROPERTY()
	TSubclassOf<class UObject> FunctionClass;

	// UObject interface
	BLUEPRINTGRAPH_API virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	// End of UObject interface

	// UEdGraphNode interface
	BLUEPRINTGRAPH_API virtual void AllocateDefaultPins() override;
	BLUEPRINTGRAPH_API virtual FLinearColor GetNodeTitleColor() const override;
	BLUEPRINTGRAPH_API virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	// End of UEdGraphNode interface

	// UK2Node interface
	BLUEPRINTGRAPH_API virtual ERedirectType DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const override;
	BLUEPRINTGRAPH_API virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	BLUEPRINTGRAPH_API virtual FText GetMenuCategory() const override;
	virtual bool CanEverRemoveExecutionPin() const override { return true; }
	// End of UK2Node interface

	// UK2Node_Switch interface

	/** Gets a unique pin name, the next in the sequence */
	virtual FString GetUniquePinName() { return FString(); }

	/** Gets the pin type from the schema for the subclass */
	virtual FEdGraphPinType GetPinType() const { check(false); return FEdGraphPinType(); }

	BLUEPRINTGRAPH_API virtual FEdGraphPinType GetInnerCaseType() const;

	/**
	 * Adds a new execution pin to a switch node
	 */
	BLUEPRINTGRAPH_API virtual void AddPinToSwitchNode();

	/**
	 * Removes the specified execution pin from an switch node
	 *
	 * @param	TargetPin	The pin to remove from the node
	 */
	BLUEPRINTGRAPH_API virtual void RemovePinFromSwitchNode(UEdGraphPin* TargetPin);

	/** Whether an execution pin can be removed from the node or not */
	BLUEPRINTGRAPH_API virtual bool CanRemoveExecutionPin(UEdGraphPin* TargetPin) const;

	/** Getting pin access */
	BLUEPRINTGRAPH_API UEdGraphPin* GetSelectionPin() const;
	BLUEPRINTGRAPH_API UEdGraphPin* GetDefaultPin() const;
	BLUEPRINTGRAPH_API UEdGraphPin* GetFunctionPin() const;

	BLUEPRINTGRAPH_API static FString	GetSelectionPinName();

	virtual FString GetPinNameGivenIndex(int32 Index);

protected:
	virtual void CreateSelectionPin() {}
	virtual void CreateCasePins() {}
	virtual void CreateFunctionPin();
	virtual void RemovePin(UEdGraphPin* TargetPin) {}

private:
	// Editor-only field that signals a default pin setting change
	bool bHasDefaultPinValueChanged;
};

