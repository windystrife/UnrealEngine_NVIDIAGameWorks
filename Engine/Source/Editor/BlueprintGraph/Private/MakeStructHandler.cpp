// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MakeStructHandler.h"
#include "UObject/UnrealType.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_MakeStruct.h"
#include "EdGraphUtilities.h"
#include "KismetCompiler.h"
#include "PropertyCustomizationHelpers.h"

#define LOCTEXT_NAMESPACE "FKCHandler_MakeStruct"

//////////////////////////////////////////////////////////////////////////
// FKCHandler_MakeStruct

UEdGraphPin* FKCHandler_MakeStruct::FindStructPinChecked(UEdGraphNode* Node) const
{
	check(Node);
	UEdGraphPin* OutputPin = nullptr;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && (EGPD_Output == Pin->Direction) && !CompilerContext.GetSchema()->IsMetaPin(*Pin))
		{
			OutputPin = Pin;
			break;
		}
	}
	check(OutputPin);
	return OutputPin;
}

FKCHandler_MakeStruct::FKCHandler_MakeStruct(FKismetCompilerContext& InCompilerContext)
	: FNodeHandlingFunctor(InCompilerContext)
	, bAutoGenerateGotoForPure(true)
{
}

void FKCHandler_MakeStruct::RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* InNode)
{
	UK2Node_MakeStruct* Node = CastChecked<UK2Node_MakeStruct>(InNode);
	if (nullptr == Node->StructType)
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("MakeStruct_UnknownStructure_Error", "Unknown structure to break for @@").ToString(), Node);
		return;
	}

	if (!UK2Node_MakeStruct::CanBeMade(Node->StructType, Node->IsIntermediateNode()))
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("MakeStruct_Error", "The structure @@ is not a BlueprintType. ").ToString(), Node);
		return;
	}

	FNodeHandlingFunctor::RegisterNets(Context, Node);

	UEdGraphPin* OutputPin = FindStructPinChecked(Node);
	UEdGraphPin* Net = FEdGraphUtilities::GetNetFromPin(OutputPin);
	check(Net);
	FBPTerminal** FoundTerm = Context.NetMap.Find(Net);
	FBPTerminal* Term = FoundTerm ? *FoundTerm : nullptr;

	if (Term == nullptr)
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("MakeStruct_NoTerm_Error", "Failed to generate a term for the @@ pin; was it a struct reference that was left unset?").ToString(), OutputPin);
	}
	else
	{
		UStruct* StructInTerm = Cast<UStruct>(Term->Type.PinSubCategoryObject.Get());
		if (nullptr == StructInTerm || !StructInTerm->IsChildOf(Node->StructType))
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("MakeStruct_NoMatch_Error", "Structures don't match for @@").ToString(), Node);
		}
	}
}
	

void FKCHandler_MakeStruct::RegisterNet(FKismetFunctionContext& Context, UEdGraphPin* Net)
{
	if (!Net->bDefaultValueIsIgnored)
	{
		FBPTerminal* Term = Context.CreateLocalTerminalFromPinAutoChooseScope(Net, Context.NetNameMap->MakeValidName(Net));
		Context.NetMap.Add(Net, Term);
	}
}

FBPTerminal* FKCHandler_MakeStruct::RegisterLiteral(FKismetFunctionContext& Context, UEdGraphPin* Net)
{
	FBPTerminal* ReturnTerm = nullptr;
	if (!Net->bDefaultValueIsIgnored)
	{
		ReturnTerm = FNodeHandlingFunctor::RegisterLiteral(Context, Net);
	}
	return ReturnTerm;
}

void FKCHandler_MakeStruct::Compile(FKismetFunctionContext& Context, UEdGraphNode* InNode)
{
	UK2Node_MakeStruct* Node = CastChecked<UK2Node_MakeStruct>(InNode);
	if (NULL == Node->StructType)
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("MakeStruct_UnknownStructure_Error", "Unknown structure to break for @@").ToString(), Node);
		return;
	}

	UEdGraphPin* StructPin = FindStructPinChecked(Node);
	UEdGraphPin* OutputStructNet = FEdGraphUtilities::GetNetFromPin(StructPin);
	FBPTerminal** FoundTerm = Context.NetMap.Find(OutputStructNet);
	FBPTerminal* OutputStructTerm = FoundTerm ? *FoundTerm : NULL;
	check(OutputStructTerm);

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && !Pin->bOrphanedPin && (Pin != StructPin) && (Pin->Direction == EGPD_Input) && !CompilerContext.GetSchema()->IsMetaPin(*Pin))
		{
			UProperty* BoundProperty = FindField<UProperty>(Node->StructType, *(Pin->PinName));
			check(BoundProperty);

			// If the pin is not connectible, do not forward the net
			if (!Pin->bNotConnectable)
			{
				if (FBPTerminal** FoundSrcTerm = Context.NetMap.Find(FEdGraphUtilities::GetNetFromPin(Pin)))
				{
					FBPTerminal* SrcTerm = FoundSrcTerm ? *FoundSrcTerm : nullptr;
					check(SrcTerm);

					FBPTerminal* DstTerm = Context.CreateLocalTerminal();
					DstTerm->CopyFromPin(Pin, Context.NetNameMap->MakeValidName(Pin));
					DstTerm->AssociatedVarProperty = BoundProperty;
					DstTerm->Context = OutputStructTerm;

					FKismetCompilerUtilities::CreateObjectAssignmentStatement(Context, Node, SrcTerm, DstTerm);
				}
			}

			// Handle injecting the override property values into the node if the property has any
			bool bNegate = false;
			UProperty* OverrideProperty = PropertyCustomizationHelpers::GetEditConditionProperty(BoundProperty, bNegate);
			if (OverrideProperty)
			{
				const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
				FEdGraphPinType PinType;
				Schema->ConvertPropertyToPinType(OverrideProperty, /*out*/ PinType);

				// Create the term in the list
				FBPTerminal* OverrideTerm = new (Context.VariableReferences) FBPTerminal();
				OverrideTerm->Type = PinType;
				OverrideTerm->AssociatedVarProperty = OverrideProperty;
				OverrideTerm->Context = OutputStructTerm;

				FBlueprintCompiledStatement* AssignBoolStatement = new FBlueprintCompiledStatement;
				AssignBoolStatement->Type = KCST_Assignment;

				// Literal Bool Term to set the OverrideProperty to
				FBPTerminal* BoolTerm = Context.CreateLocalTerminal(ETerminalSpecification::TS_Literal);
				BoolTerm->Type.PinCategory = CompilerContext.GetSchema()->PC_Boolean;
				BoolTerm->bIsLiteral = true;
				// If we are showing the pin, then we are overriding the property

				// Assigning the OverrideProperty to the literal bool term
				AssignBoolStatement->LHS = OverrideTerm;
				AssignBoolStatement->RHS.Add(BoolTerm);

				// Need to dig up what the state of the override property should be
				for (FOptionalPinFromProperty& PropertyEntry : Node->ShowPinForProperties)
				{
					if (PropertyEntry.bHasOverridePin && PropertyEntry.bShowPin && PropertyEntry.PropertyName == BoundProperty->GetFName())
					{
						if (!PropertyEntry.bIsOverridePinVisible || (PropertyEntry.bIsOverridePinVisible && !PropertyEntry.bIsOverrideEnabled && PropertyEntry.bIsSetValuePinVisible))
						{
							CompilerContext.MessageLog.Warning(*LOCTEXT("MakeStruct_InvalidOverrideSetting", "Selected override setting on @@ in @@ is no longer a supported workflow and it is advised that you refactor your Blueprint to not use it!").ToString(), Pin, Node->GetBlueprint());
						}

						if (PropertyEntry.bIsOverridePinVisible)
						{
							BoolTerm->Name = PropertyEntry.bIsOverrideEnabled ? TEXT("true") : TEXT("false");
							Context.AllGeneratedStatements.Add(AssignBoolStatement);

							TArray<FBlueprintCompiledStatement*>& StatementList = Context.StatementsPerNode.FindOrAdd(Node);
							StatementList.Add(AssignBoolStatement);
						}
						break;
					}
				}
			}
		}
	}

	if (bAutoGenerateGotoForPure && !Node->IsNodePure())
	{
		GenerateSimpleThenGoto(Context, *Node);
	}
}

#undef LOCTEXT_NAMESPACE
