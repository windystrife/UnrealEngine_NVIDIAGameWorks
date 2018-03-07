// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "K2Node.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_AssignmentStatement.generated.h"

class FBlueprintActionDatabaseRegistrar;
class UEdGraphPin;

UCLASS(MinimalAPI)
class UK2Node_AssignmentStatement : public UK2Node
{
	GENERATED_UCLASS_BODY()


	// Name of the Variable pin for this node
	static FString VariablePinName;
	// Name of the Value pin for this node
	static FString ValuePinName;

	//~ Begin UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* TargetGraph) const override;
	virtual bool CanPasteHere(const UEdGraph* TargetGraph) const override;
	//~ End UEdGraphNode Interface

	//~ Begin UK2Node Interface
	virtual void PostReconstructNode() override;
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual int32 GetNodeRefreshPriority() const override { return EBaseNodeRefreshPriority::Low_UsesDependentWildcard; }
	//~ End UK2Node Interface

	/** Get the Then output pin */
	BLUEPRINTGRAPH_API UEdGraphPin* GetThenPin() const;
	/** Get the Variable input pin */
	BLUEPRINTGRAPH_API UEdGraphPin* GetVariablePin() const;
	/** Get the Value input pin */
	BLUEPRINTGRAPH_API UEdGraphPin* GetValuePin() const;
};

