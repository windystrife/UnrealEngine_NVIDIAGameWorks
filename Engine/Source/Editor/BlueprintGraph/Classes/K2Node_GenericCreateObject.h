// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "K2Node_ConstructObjectFromClass.h"
#include "K2Node_GenericCreateObject.generated.h"

class UEdGraph;

UCLASS()
class BLUEPRINTGRAPH_API UK2Node_GenericCreateObject : public UK2Node_ConstructObjectFromClass
{
	GENERATED_BODY()

	//~ Begin UEdGraphNode Interface.
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void EarlyValidation(class FCompilerResultsLog& MessageLog) const override;
	virtual bool IsCompatibleWithGraph(const UEdGraph* TargetGraph) const override;
	//~ End UEdGraphNode Interface.

	virtual bool UseWorldContext() const override { return false; }
	virtual bool UseOuter() const override { return true; }
};
