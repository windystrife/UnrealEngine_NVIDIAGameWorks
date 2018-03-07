// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "K2Node_ControlRigComponentInputOutput.h"
#include "K2Node_ControlRigComponentOutput.generated.h"

class UEdGraph;

/** Gets outputs from this component's animation controller */
UCLASS()
class UK2Node_ControlRigComponentOutput : public UK2Node_ControlRigComponentInputOutput
{
	GENERATED_UCLASS_BODY()

public:
	// UEdGraphNode Interface.
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;

	// UK2Node Interface
	virtual bool IsNodePure() const override { return true; }
	
protected:
	// UK2Node_ControlRig Interface
	virtual bool HasInputs() const { return false; }
	virtual TArray<TSharedRef<IControlRigField>> GetInputVariableInfo(const TArray<FName>& DisabledPins) const { return TArray<TSharedRef<IControlRigField>>(); }

private:
	void CreateOutputPins();

private:
	/** Tooltip text for this node. */
	FText NodeTooltip;
};
