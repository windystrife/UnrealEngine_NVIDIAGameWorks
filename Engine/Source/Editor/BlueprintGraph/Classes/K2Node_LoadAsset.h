// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "K2Node.h"
#include "K2Node_LoadAsset.generated.h"

class FBlueprintActionDatabaseRegistrar;
class UEdGraph;

UCLASS(MinimalAPI)
class UK2Node_LoadAsset : public UK2Node
{
	GENERATED_BODY()
public:
	// UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool IsCompatibleWithGraph(const UEdGraph* TargetGraph) const override;
	// End of UEdGraphNode interface

	// UK2Node interface
	virtual bool IsNodePure() const override { return false; }
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual FName GetCornerIcon() const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual bool NodeCausesStructuralBlueprintChange() const { return true; }
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	// End of UK2Node interface

protected:
	virtual FName NativeFunctionName() const;

	virtual const FString& GetInputCategory() const;
	virtual const FString& GetOutputCategory() const;

	virtual const FString& GetInputPinName() const;
	virtual const FString& GetOutputPinName() const;
};

UCLASS(MinimalAPI)
class UK2Node_LoadAssetClass : public UK2Node_LoadAsset
{
	GENERATED_BODY()
public:
	// UEdGraphNode interface
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	// End of UEdGraphNode interface

protected:
	virtual FName NativeFunctionName() const override;

	virtual const FString& GetInputCategory() const override;
	virtual const FString& GetOutputCategory() const override;

	virtual const FString& GetInputPinName() const override;
	virtual const FString& GetOutputPinName() const override;
};
