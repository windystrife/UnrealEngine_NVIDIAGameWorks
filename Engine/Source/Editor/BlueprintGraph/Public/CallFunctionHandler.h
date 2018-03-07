// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "KismetCompilerMisc.h"
#include "K2Node_CallFunction.h"

class FKismetCompilerContext;
struct FKismetFunctionContext;

//////////////////////////////////////////////////////////////////////////
// FKCHandler_CallFunction

class FKCHandler_CallFunction : public FNodeHandlingFunctor
{
public:
	FKCHandler_CallFunction(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	/**
	 * Searches for the function referenced by a graph node in the CallingContext class's list of functions,
	 * validates that the wiring matches up correctly, and creates an execution statement.
	 */
	void CreateFunctionCallStatement(FKismetFunctionContext& Context, UEdGraphNode* Node, UEdGraphPin* SelfPin);

	bool IsCalledFunctionPure(UEdGraphNode* Node)
	{
		if(UK2Node_CallFunction* CallFunctionNode = Cast<UK2Node_CallFunction>(Node))
		{
			return CallFunctionNode->bIsPureFunc;
		}
		return false;
	}

	bool IsCalledFunctionFinal(UEdGraphNode* Node)
	{
		if(UK2Node_CallFunction* CallFunctionNode = Cast<UK2Node_CallFunction>(Node))
		{
			return CallFunctionNode->bIsFinalFunction;
		}
		return false;
	}

	bool IsCalledFunctionFromInterface(UEdGraphNode* Node)
	{
		if(UK2Node_CallFunction* CallFunctionNode = Cast<UK2Node_CallFunction>(Node))
		{
			return CallFunctionNode->bIsInterfaceCall;
		}
		return false;
	}

private:
	// Get the name of the function to call from the node
	virtual FString GetFunctionNameFromNode(UEdGraphNode* Node) const;
	UClass* GetCallingContext(FKismetFunctionContext& Context, UEdGraphNode* Node);
	UClass* GetTrueCallingClass(FKismetFunctionContext& Context, UEdGraphPin* SelfPin);

public:

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override;
	virtual void RegisterNet(FKismetFunctionContext& Context, UEdGraphPin* Net) override;
	virtual UFunction* FindFunction(FKismetFunctionContext& Context, UEdGraphNode* Node);
	virtual void Transform(FKismetFunctionContext& Context, UEdGraphNode* Node) override;
	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override;
	virtual void CheckIfFunctionIsCallable(UFunction* Function, FKismetFunctionContext& Context, UEdGraphNode* Node);
	virtual void AdditionalCompiledStatementHandling(FKismetFunctionContext& Context, UEdGraphNode* Node, FBlueprintCompiledStatement& Statement) {}

protected:
	TMap<UEdGraphPin*, FBPTerminal*> InterfaceTermMap;
};
