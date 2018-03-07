// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "K2Node_ControlRig.h"
#include "K2Node_ControlRigOutput.generated.h"

class FBlueprintActionDatabaseRegistrar;
class UEdGraph;
struct FSlateIcon;
class UK2Node_Event;

/** Writes output to this animation node */
UCLASS()
class UK2Node_ControlRigOutput : public UK2Node_ControlRig
{
	GENERATED_UCLASS_BODY()

public:
	// UEdGraphNode Interface.
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;

	// UK2Node Interface
	virtual bool IsNodeSafeToIgnore() const override { return true; }
	virtual bool IsNodeRootSet() const override { return true; }

protected:
	// UK2Node_ControlRig Interface
	virtual bool HasInputs() const { return false; }
	virtual const UClass* GetControlRigClassImpl() const override { return GetControlRigClassFromBlueprint(GetBlueprint()); }
	virtual EEdGraphPinDirection GetInputDirection() const override { return EGPD_Output; }
	virtual EEdGraphPinDirection GetOutputDirection() const override { return EGPD_Input; }
	virtual TArray<TSharedRef<IControlRigField>> GetInputVariableInfo(const TArray<FName>& DisabledPins) const { return TArray<TSharedRef<IControlRigField>>(); }

private:
	/** Helper function for ExpandNode() */
	static UK2Node_Event* FindExistingEvaluateNode(UEdGraph* Graph);

private:
	/** Tooltip text for this node. */
	FText NodeTooltip;
};
