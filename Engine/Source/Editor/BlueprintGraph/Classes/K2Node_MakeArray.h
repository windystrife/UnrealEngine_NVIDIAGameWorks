// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "K2Node_MakeContainer.h"
#include "K2Node_MakeArray.generated.h"

class FBlueprintActionDatabaseRegistrar;
class UEdGraphPin;

UCLASS(MinimalAPI)
class UK2Node_MakeArray : public UK2Node_MakeContainer
{
	GENERATED_UCLASS_BODY()

public:
	// UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	// End of UEdGraphNode interface

	// UK2Node interface
	virtual void GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const override;
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual FText GetMenuCategory() const override;
	// End of UK2Node interface

	// UK2Node_MakeContainer interface
	virtual FString GetOutputPinName() const override;
	// UK2Node_MakeContainer interface
};
