// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_ClearDelegate.generated.h"

UCLASS(MinimalAPI)
class UK2Node_ClearDelegate : public UK2Node_BaseMCDelegate
{
	GENERATED_UCLASS_BODY()

public:
	//~ Begin UEdGraphNode Interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	//~ End UEdGraphNode Interface

	//~ Begin of UK2Node Interface
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	//~ End UK2Node Interface
};
