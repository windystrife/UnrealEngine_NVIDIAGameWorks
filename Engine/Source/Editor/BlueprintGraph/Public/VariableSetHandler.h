// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "KismetCompilerMisc.h"

class FKismetCompilerContext;
struct FKismetFunctionContext;

//////////////////////////////////////////////////////////////////////////
// FKCHandler_VariableSet

class FKCHandler_VariableSet : public FNodeHandlingFunctor
{
public:
	FKCHandler_VariableSet(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	virtual void RegisterNet(FKismetFunctionContext& Context, UEdGraphPin* Net) override;
	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override;
	void InnerAssignment(FKismetFunctionContext& Context, UEdGraphNode* Node, UEdGraphPin* VariablePin, UEdGraphPin* ValuePin);
	void GenerateAssigments(FKismetFunctionContext& Context, UEdGraphNode* Node);
	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override;
	virtual void Transform(FKismetFunctionContext& Context, UEdGraphNode* Node) override;
};
