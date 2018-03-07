// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "KismetCompilerMisc.h"

class FKismetCompilerContext;
class UK2Node_MathExpression;
struct FKismetFunctionContext;

class FKCHandler_MathExpression : public FNodeHandlingFunctor
{
	FBlueprintCompiledStatement* GenerateFunctionRPN(UEdGraphNode* CurrentNode
		, FKismetFunctionContext& Context
		, UK2Node_MathExpression& MENode
		, FBPTerminal* ResultTerm
		, TMap<UEdGraphPin*, UEdGraphPin*>& InnerToOuterInput);

public:
	FKCHandler_MathExpression(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* InNode) override;
	virtual void RegisterNet(FKismetFunctionContext& Context, UEdGraphPin* Net) override;
	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override;

	static bool CanBeCalledByMathExpression(const UFunction* Function);
};
