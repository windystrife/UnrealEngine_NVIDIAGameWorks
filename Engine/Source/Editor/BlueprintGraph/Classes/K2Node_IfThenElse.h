// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Textures/SlateIcon.h"
#include "K2Node.h"
#include "K2Node_IfThenElse.generated.h"

class FBlueprintActionDatabaseRegistrar;
class UEdGraphPin;

UCLASS(MinimalAPI, meta=(Keywords = "if bool branch"))
class UK2Node_IfThenElse : public UK2Node
{
	GENERATED_UCLASS_BODY()

	//~ Begin UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	//~ End UEdGraphNode Interface

	//~ Begin K2Node Interface.
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	//~ End K2Node Interface.

	/** Get the then output pin */
	BLUEPRINTGRAPH_API UEdGraphPin* GetThenPin() const;
	/** Get the return value pin */
	BLUEPRINTGRAPH_API UEdGraphPin* GetElsePin() const;
	/** Get the condition pin */
	BLUEPRINTGRAPH_API UEdGraphPin* GetConditionPin() const;
};

