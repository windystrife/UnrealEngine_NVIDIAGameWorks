// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "K2Node.h"
#include "Textures/SlateIcon.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "NodeDependingOnEnumInterface.h"
#include "K2Node_EnumLiteral.generated.h"

class FBlueprintActionDatabaseRegistrar;
class FKismetCompilerContext;
class FNodeHandlingFunctor;

UCLASS(MinimalAPI)
class UK2Node_EnumLiteral : public UK2Node, public INodeDependingOnEnumInterface
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	UEnum* Enum;

	static BLUEPRINTGRAPH_API const FString& GetEnumInputPinName();

	//~ Begin UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	//~ End UEdGraphNode Interface

	//~ Begin UK2Node Interface
	virtual bool IsNodePure() const override { return true; }
	virtual FNodeHandlingFunctor* CreateNodeHandler(FKismetCompilerContext& CompilerContext) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	//~ End UK2Node Interface

	// INodeDependingOnEnumInterface
	virtual class UEnum* GetEnum() const override { return Enum; }
	virtual bool ShouldBeReconstructedAfterEnumChanged() const override { return true; }
	// End of INodeDependingOnEnumInterface

private:
	/** Constructing FText strings can be costly, so we cache the node's tootltip */
	FNodeTextCache CachedTooltip;
};

