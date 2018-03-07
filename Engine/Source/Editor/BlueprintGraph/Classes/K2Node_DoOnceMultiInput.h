// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node.h"
#include "K2Node_AddPinInterface.h"
#include "K2Node_DoOnceMultiInput.generated.h"

class FBlueprintActionDatabaseRegistrar;
class UEdGraph;

UCLASS(MinimalAPI)
class UK2Node_DoOnceMultiInput : public UK2Node, public IK2Node_AddPinInterface
{
	GENERATED_UCLASS_BODY()

	/** The number of additional input pins to generate for this node (2 base pins are not included) */
	UPROPERTY()
	int32 NumAdditionalInputs;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	/** Reference to the integer that contains */
	UPROPERTY(transient)
	class UK2Node_TemporaryVariable* DataNode;

	virtual ERedirectType DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const override;

private:

	const static int32 NumBaseInputs = 1;

	static int32 GetMaxInputPinsNum();
	static FText GetNameForPin(int32 PinIndex, bool In);

	FEdGraphPinType GetInType() const;
	FEdGraphPinType GetOutType() const;

	void AddPinsInner(int32 AdditionalPinIndex);
	bool CanRemovePin(const UEdGraphPin* Pin) const;
public:
	BLUEPRINTGRAPH_API UEdGraphPin* FindOutPin() const;
	BLUEPRINTGRAPH_API UEdGraphPin* FindSelfPin() const;

	/** Get TRUE input type (self, etc.. are skipped) */
	BLUEPRINTGRAPH_API UEdGraphPin* GetInputPin(int32 InputPinIndex);
	BLUEPRINTGRAPH_API UEdGraphPin* GetOutputPin(int32 InputPinIndex);

	BLUEPRINTGRAPH_API void RemoveInputPin(UEdGraphPin* Pin);

	// UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	// End of UEdGraphNode interface

	// UK2Node interface
	virtual void GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	// End of UK2Node interface

	// IK2Node_AddPinInterface interface
	BLUEPRINTGRAPH_API virtual void AddInputPin() override;
	virtual bool CanAddPin() const override;
	// End of IK2Node_AddPinInterface interface
};
