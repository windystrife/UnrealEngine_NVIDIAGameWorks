// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MathExpressionHandler.h"
#include "UObject/UnrealType.h"
#include "UObject/Interface.h"
#include "K2Node_MathExpression.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "EdGraphUtilities.h"
#include "KismetCompiler.h"

#define LOCTEXT_NAMESPACE "KCHandler_MathExpression"

struct KCHandler_MathExpressionHelper
{
	static bool CreateMap(UEdGraphNode& KeyNode, UEdGraphNode& ValueNode, EEdGraphPinDirection KeyPinDirection, const UEdGraphSchema_K2* Schema, TMap<UEdGraphPin*, UEdGraphPin*>& OutMap)
	{
		check(Schema);
		for (auto InnerInputPin : KeyNode.Pins)
		{
			if (!InnerInputPin || InnerInputPin->Direction != KeyPinDirection)
			{
				return false;
			}

			auto OuterInputPin = ValueNode.FindPin(InnerInputPin->PinName);
			if (!OuterInputPin
				|| !Schema->ArePinsCompatible(InnerInputPin, OuterInputPin, nullptr, false))
			{
				return false;
			}
			OutMap.Add(InnerInputPin, OuterInputPin);
		}
		return true;
	}
};

bool FKCHandler_MathExpression::CanBeCalledByMathExpression(const UFunction* Function)
{
	bool bGoodFunction = (nullptr != Function)
		&& Function->HasAllFunctionFlags(FUNC_Static | FUNC_BlueprintPure | FUNC_Final | FUNC_Native)
		&& !Function->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly | FUNC_BlueprintCosmetic)
		&& !Function->GetOuterUClass()->IsChildOf(UInterface::StaticClass());
	if (!bGoodFunction)
	{
		return false;
	}

	for (TFieldIterator<const UProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
	{
		const UProperty* Property = *It;
		const bool bGoodProperty = Property && (!Property->HasAnyPropertyFlags(CPF_OutParm) || Property->HasAllPropertyFlags(CPF_ReturnParm));
		if (!bGoodProperty)
		{
			return false;
		}
	}
	return true;
}

FBlueprintCompiledStatement* FKCHandler_MathExpression::GenerateFunctionRPN(UEdGraphNode* CurrentNode, FKismetFunctionContext& Context, UK2Node_MathExpression& MENode, FBPTerminal* ResultTerm, TMap<UEdGraphPin*, UEdGraphPin*>& InnerToOuterInput)
{
	UK2Node_CallFunction* CallFunctionNode = Cast<UK2Node_CallFunction>(CurrentNode);
	UFunction* Function = CallFunctionNode ? CallFunctionNode->GetTargetFunction() : nullptr;
	if (!CanBeCalledByMathExpression(Function))
	{
		CompilerContext.MessageLog.Error(*FString::Printf(*LOCTEXT("WrongFunction_Error", "Function '%s' cannot be called inside Math Expression @@ - @@").ToString(), *GetNameSafe(Function)), CallFunctionNode, &MENode);
		return nullptr;
	}

	FBlueprintCompiledStatement* Statement = new FBlueprintCompiledStatement();
	Statement->FunctionToCall = Function;
	Statement->FunctionContext = nullptr;
	Statement->Type = KCST_CallFunction;
	Statement->LHS = ResultTerm; // required only for the first node

	check(CallFunctionNode);

	TArray<FBPTerminal*> RHSTerms;
	for (TFieldIterator<UProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
	{
		UProperty* Property = *It;
		if (Property && !Property->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm))
		{
			UEdGraphPin* PinToTry = nullptr;
			{
				UEdGraphPin* PinMatch = CallFunctionNode->FindPin(Property->GetName());
				const bool bGoodPin = PinMatch && FKismetCompilerUtilities::IsTypeCompatibleWithProperty(PinMatch, Property, CompilerContext.MessageLog, CompilerContext.GetSchema(), Context.NewClass);
				PinToTry = bGoodPin ? FEdGraphUtilities::GetNetFromPin(PinMatch) : nullptr;
			}

			FBPTerminal* RHSTerm = nullptr;
			{
				UEdGraphPin** OuterInputPtr = PinToTry ? InnerToOuterInput.Find(PinToTry) : nullptr;
				UEdGraphPin* OuterInputNet = (OuterInputPtr && *OuterInputPtr) ? FEdGraphUtilities::GetNetFromPin(*OuterInputPtr) : nullptr;
				FBPTerminal** OuterTerm = OuterInputNet ? Context.NetMap.Find(OuterInputNet) : nullptr;
				// Input is an outer term
				if (OuterTerm && *OuterTerm)
				{
					RHSTerm = *OuterTerm;
				}
			}

			if (!RHSTerm)
			{
				FBPTerminal** Term = PinToTry ? Context.NetMap.Find(PinToTry) : nullptr;
				const bool bValidTerm = Term && *Term;
				// Input is a literal term
				// Input is a variable
				if (bValidTerm && ((*Term)->bIsLiteral || (*Term)->AssociatedVarProperty))
				{
					RHSTerm = *Term;
				}
				// Input is an InlineGeneratedParameter
				else if (bValidTerm)
				{
					ensure(!(*Term)->InlineGeneratedParameter);
					UEdGraphNode* SourceNode = PinToTry ? PinToTry->GetOwningNodeUnchecked() : nullptr;
					FBlueprintCompiledStatement* InlineGeneratedParameterStatement = GenerateFunctionRPN(SourceNode, Context, MENode, nullptr, InnerToOuterInput);
					if (InlineGeneratedParameterStatement)
					{
						Context.AllGeneratedStatements.Add(InlineGeneratedParameterStatement);
						RHSTerm = *Term;
						RHSTerm->InlineGeneratedParameter = InlineGeneratedParameterStatement;
					}
				}
			}

			if (RHSTerm)
			{
				RHSTerms.Add(RHSTerm);
			}
			else
			{
				CompilerContext.MessageLog.Error(*FString::Printf(*LOCTEXT("FindPinParameter_Error", "Could not find a pin for the parameter %s of %s on @@").ToString(), *GetNameSafe(Property), *GetNameSafe(Function)), CallFunctionNode);
			}
		}
	}
	Statement->RHS = RHSTerms;
	return Statement;
}

void FKCHandler_MathExpression::RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* InNode)
{
	FNodeHandlingFunctor::RegisterNets(Context, InNode);

	auto Node_MathExpression = CastChecked<UK2Node_MathExpression>(InNode);
	auto InnerEntryNode = Node_MathExpression->GetEntryNode();
	auto InnerExitNode = Node_MathExpression->GetExitNode();
	check(Node_MathExpression->BoundGraph && InnerEntryNode && InnerExitNode);

	for (auto InnerGraphNode : Node_MathExpression->BoundGraph->Nodes)
	{
		if (!InnerGraphNode || (InnerGraphNode == InnerEntryNode) || (InnerGraphNode == InnerExitNode))
		{
			continue;
		}

		if (auto GetVarNode = Cast<UK2Node_VariableGet>(InnerGraphNode))
		{
			auto VarHandler = GetVarNode->CreateNodeHandler(CompilerContext);
			if (ensure(VarHandler))
			{
				VarHandler->RegisterNets(Context, GetVarNode);
			}
			continue;
		}

		for (auto Pin : InnerGraphNode->Pins)
		{
			// Register fake terms for InlineGeneratedValues
			if (Pin && (Pin->Direction == EEdGraphPinDirection::EGPD_Output) && Pin->LinkedTo.Num())
			{
				auto Linked = Pin->LinkedTo[0];
				auto LinkedOwnerNode = Linked ? Linked->GetOwningNodeUnchecked() : nullptr;
				if (LinkedOwnerNode && (InnerExitNode != LinkedOwnerNode))
				{
					FBPTerminal* Term = new (Context.InlineGeneratedValues) FBPTerminal();
					Term->CopyFromPin(Pin, Context.NetNameMap->MakeValidName(Pin));
					Context.NetMap.Add(Pin, Term);
				}
			}

			// Register Literals
			if (Pin && (Pin->Direction == EEdGraphPinDirection::EGPD_Input) && !Pin->LinkedTo.Num())
			{
				RegisterLiteral(Context, Pin);
			}
		}
	}
}

void FKCHandler_MathExpression::RegisterNet(FKismetFunctionContext& Context, UEdGraphPin* Net) 
{
	FBPTerminal* Term = Context.CreateLocalTerminalFromPinAutoChooseScope(Net, Context.NetNameMap->MakeValidName(Net));
	Context.NetMap.Add(Net, Term);
}

void FKCHandler_MathExpression::Compile(FKismetFunctionContext& Context, UEdGraphNode* Node)
{
	UK2Node_MathExpression* Node_MathExpression = CastChecked<UK2Node_MathExpression>(Node);
	check(Context.Schema);

	auto InnerExitNode = Node_MathExpression->GetExitNode();
	auto InnerEntryNode = Node_MathExpression->GetEntryNode();

	if (!InnerExitNode || !InnerEntryNode || (InnerExitNode->Pins.Num() != 1) || ((InnerExitNode->Pins.Num() + InnerEntryNode->Pins.Num()) != Node->Pins.Num()))
	{
		Context.MessageLog.Error(*LOCTEXT("Compile_PinError", "ICE - wrong inner pins - @@").ToString(), Node);
		return;
	}

	TMap<UEdGraphPin*, UEdGraphPin*> InnerToOuterInput;
	bool bProperMap = KCHandler_MathExpressionHelper::CreateMap(*InnerEntryNode, *Node_MathExpression, EEdGraphPinDirection::EGPD_Output, Context.Schema, InnerToOuterInput);

	TMap<UEdGraphPin*, UEdGraphPin*> InnerToOuterOutput;
	bProperMap &= KCHandler_MathExpressionHelper::CreateMap(*InnerExitNode, *Node_MathExpression, EEdGraphPinDirection::EGPD_Input, Context.Schema, InnerToOuterOutput);

	if (!bProperMap)
	{
		Context.MessageLog.Error(*LOCTEXT("Compile_WrongMap", "ICE - cannot map pins - @@").ToString(), Node);
		return;
	}

	auto InnerOutputPin = InnerExitNode->Pins[0];
	auto OuterOutputPinPtr = InnerToOuterOutput.Find(InnerOutputPin);
	if (!InnerOutputPin || (InnerOutputPin->LinkedTo.Num() != 1) || !InnerOutputPin->LinkedTo[0] || !OuterOutputPinPtr || !*OuterOutputPinPtr)
	{
		Context.MessageLog.Error(*LOCTEXT("Compile_WrongOutputLink", "ICE - wrong output link - @@").ToString(), Node);
		return;
	}

	FBPTerminal** OutputTermPtr = Context.NetMap.Find(FEdGraphUtilities::GetNetFromPin(*OuterOutputPinPtr));
	FBPTerminal* OutputTerm = OutputTermPtr ? *OutputTermPtr : nullptr;
	auto LastInnerNode = InnerOutputPin->LinkedTo[0]->GetOwningNodeUnchecked();

	FBlueprintCompiledStatement* DetachedStatement = GenerateFunctionRPN(LastInnerNode, Context, *Node_MathExpression, OutputTerm, InnerToOuterInput);
	if (DetachedStatement)
	{
		Context.AllGeneratedStatements.Add(DetachedStatement);
		TArray<FBlueprintCompiledStatement*>& StatementList = Context.StatementsPerNode.FindOrAdd(Node);
		StatementList.Add(DetachedStatement);
	}
	else
	{
		Context.MessageLog.Error(*LOCTEXT("Compile_CannotGenerateFunction", "ICE - cannot generate function - @@").ToString(), Node);
		return;
	}
}

#undef LOCTEXT_NAMESPACE
