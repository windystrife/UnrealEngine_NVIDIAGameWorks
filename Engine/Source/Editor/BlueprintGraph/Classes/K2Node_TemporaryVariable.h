// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraph.h"
#include "BlueprintNodeSignature.h"
#include "K2Node.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "K2Node_TemporaryVariable.generated.h"

class FBlueprintActionDatabaseRegistrar;

UCLASS(MinimalAPI)
class UK2Node_TemporaryVariable : public UK2Node
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	struct FEdGraphPinType VariableType;

	/** Whether or not this variable should be flagged with CPF_SaveGame, and inherit its name from the GUID of the macro that that gave rise to it */
	UPROPERTY()
	bool bIsPersistent;

	// get variable pin
	BLUEPRINTGRAPH_API UEdGraphPin* GetVariablePin();

	//~ Begin UEdGraphNode Interface.
	virtual void AllocateDefaultPins() override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FString GetDescriptiveCompiledName() const override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* TargetGraph) const override;
	virtual bool CanPasteHere(const UEdGraph* TargetGraph) const override;
	//~ End UEdGraphNode Interface.

	//~ Begin UK2Node Interface.
	virtual bool IsNodePure() const override;
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual FBlueprintNodeSignature GetSignature() const override;
	//~ End UK2Node Interface.

private:
	/** Constructing FText strings can be costly, so we cache the node's title/tooltip */
	FNodeTextCache CachedTooltip;
	FNodeTextCache CachedNodeTitle;
};

