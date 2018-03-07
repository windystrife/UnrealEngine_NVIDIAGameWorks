// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "K2Node_ControlRig.h"
#include "K2Node_ControlRigEvaluator.generated.h"

class UEdGraph;
class UControlRig;

/** Evaluates an animation ControlRig */
UCLASS()
class UK2Node_ControlRigEvaluator : public UK2Node_ControlRig
{
	GENERATED_UCLASS_BODY()

	static FString ControlRigPinName;

public:
	/** The type of the ControlRig we want to evaluate */
	UPROPERTY(EditAnywhere, Category = "ControlRig")
	TSubclassOf<UControlRig> ControlRigType;

public:
	// UObject interface.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	// UEdGraphNode Interface.
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;

	// UK2Node Interface
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FBlueprintNodeSignature GetSignature() const override;
	virtual bool ShouldShowNodeProperties() const { return true; }
	virtual bool IsNodePure() const { return true; }

protected:
	// UK2Node_ControlRig interface
	virtual const UClass* GetControlRigClassImpl() const override;

private:
	void ExpandInputs(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UEdGraphPin*& OutDelegateOutputPin, UEdGraphPin*& OutTempControlRigVariablePin);
	void ExpandOutputs(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UEdGraphPin* PreEvaluateDelegatePin, UEdGraphPin* TempControlRigVariablePin);
	UEdGraphPin* CreateAllocateSubControlRigNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, int32& OutSubControlRigIndex);

private:
	/** Tooltip text for this node. */
	FText NodeTooltip;
};
