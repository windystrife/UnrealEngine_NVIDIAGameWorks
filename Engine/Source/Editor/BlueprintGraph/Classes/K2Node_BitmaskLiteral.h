// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "K2Node.h"
#include "NodeDependingOnEnumInterface.h"
#include "K2Node_BitmaskLiteral.generated.h"

class FBlueprintActionDatabaseRegistrar;
class UEdGraph;

UCLASS(MinimalAPI)
class UK2Node_BitmaskLiteral : public UK2Node, public INodeDependingOnEnumInterface
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	UEnum* BitflagsEnum;

	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

	//~ Begin UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual void ReconstructNode() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	//~ End UEdGraphNode Interface
	
	//~ Begin UK2Node Interface
	virtual bool IsNodePure() const override { return true; }
	virtual bool ShouldShowNodeProperties() const override { return true; }
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	//~ End UK2Node Interface

	//~ Begin INodeDependingOnEnumInterface
	virtual class UEnum* GetEnum() const override { return BitflagsEnum; }
	virtual bool ShouldBeReconstructedAfterEnumChanged() const override { return true; }
	//~ End INodeDependingOnEnumInterface

	BLUEPRINTGRAPH_API static const FString& GetBitmaskInputPinName();

protected:
	/** Internal helper method used to validate the current enum type */
	void ValidateBitflagsEnumType();
};

