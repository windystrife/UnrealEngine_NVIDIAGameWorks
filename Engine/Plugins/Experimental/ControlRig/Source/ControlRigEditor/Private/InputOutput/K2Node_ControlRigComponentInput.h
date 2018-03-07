// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "K2Node_ControlRigComponentInputOutput.h"
#include "K2Node_ControlRigComponentInput.generated.h"

class UEdGraph;

/** Sets inputs on this component's animation controller */
UCLASS()
class UK2Node_ControlRigComponentInput : public UK2Node_ControlRigComponentInputOutput
{
	GENERATED_UCLASS_BODY()

public:
	// UEdGraphNode Interface.
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;

protected:
	// UK2Node_ControlRig Interface
	virtual bool HasOutputs() const { return false; }
	virtual TArray<TSharedRef<IControlRigField>> GetOutputVariableInfo(const TArray<FName>& DisabledPins) const { return TArray<TSharedRef<IControlRigField>>(); }

private:
	void CreateInputPins();

private:
	/** Tooltip text for this node. */
	FText NodeTooltip;
};
