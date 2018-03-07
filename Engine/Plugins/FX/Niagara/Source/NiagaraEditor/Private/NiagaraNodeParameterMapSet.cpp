// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraNodeParameterMapSet.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraEditorUtilities.h"
#include "SNiagaraGraphNodeConvert.h"
#include "NiagaraHlslTranslator.h"

#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "NiagaraNodeParameterMapSet"

UNiagaraNodeParameterMapSet::UNiagaraNodeParameterMapSet() : UNiagaraNodeParameterMapBase(), PinPendingRename(nullptr)
{

}

void UNiagaraNodeParameterMapSet::AllocateDefaultPins()
{
	PinPendingRename = nullptr;
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	CreatePin(EGPD_Input, Schema->TypeDefinitionToPinType(FNiagaraTypeDefinition::GetParameterMapDef()), TEXT("Source"));
	CreatePin(EGPD_Output, Schema->TypeDefinitionToPinType(FNiagaraTypeDefinition::GetParameterMapDef()), TEXT("Dest"));
	CreateAddPin(EGPD_Input);
}

bool UNiagaraNodeParameterMapSet::IsPinNameEditable(const UEdGraphPin* GraphPinObj) const
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	FNiagaraTypeDefinition TypeDef = Schema->PinToTypeDefinition(GraphPinObj);
	if (TypeDef.IsValid() && GraphPinObj && GraphPinObj->Direction == EGPD_Input && CanRenamePin(GraphPinObj))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool UNiagaraNodeParameterMapSet::IsPinNameEditableUponCreation(const UEdGraphPin* GraphPinObj) const
{
	if (GraphPinObj == PinPendingRename)
	{
		return true;
	}
	else
	{
		return false;
	}
}


bool UNiagaraNodeParameterMapSet::VerifyEditablePinName(const FText& InName, FText& OutErrorMessage, const UEdGraphPin* InGraphPinObj) const
{
	if (InName.IsEmptyOrWhitespace())
	{
		OutErrorMessage = LOCTEXT("InvalidName", "Invalid pin name");
		return false;
	}
	return true;
}

void UNiagaraNodeParameterMapSet::OnNewTypedPinAdded(UEdGraphPin* NewPin)
{
	if (HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedInitialization))
	{
		return;
	}

	PinPendingRename = NewPin;
}

void UNiagaraNodeParameterMapSet::OnPinRenamed(UEdGraphPin* RenamedPin) 
{
	RenamedPin->PinFriendlyName = FText::FromString(RenamedPin->PinName);

}

bool UNiagaraNodeParameterMapSet::CommitEditablePinName(const FText& InName, UEdGraphPin* InGraphPinObj) 
{
	if (Pins.Contains(InGraphPinObj))
	{
		FScopedTransaction AddNewPinTransaction(LOCTEXT("Rename Pin", "Renamed pin"));
		Modify();
		InGraphPinObj->Modify();
		InGraphPinObj->PinFriendlyName = InName;
		InGraphPinObj->PinName = InName.ToString();

		if (InGraphPinObj == PinPendingRename)
		{
			PinPendingRename = nullptr;
		}
		GetGraph()->NotifyGraphChanged();
		return true;
	}
	return false;
}

void UNiagaraNodeParameterMapSet::Compile(class FHlslNiagaraTranslator* Translator, TArray<int32>& Outputs)
{
	TArray<UEdGraphPin*> InputPins;
	GetInputPins(InputPins);

	TArray<UEdGraphPin*> OutputPins;
	GetOutputPins(OutputPins);

	// Initialize the outputs to invalid values.
	check(Outputs.Num() == 0);
	for (int32 i = 0; i < OutputPins.Num(); i++)
	{
		Outputs.Add(INDEX_NONE);
	}

	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());

	// First compile fully down the hierarchy for our predecessors..
	TArray<int32> CompileInputs;
	for (UEdGraphPin* InputPin : InputPins)
	{
		if (IsAddPin(InputPin))
		{
			continue;
		}

		int32 CompiledInput = Translator->CompilePin(InputPin);
		if (CompiledInput == INDEX_NONE)
		{
			Translator->Error(LOCTEXT("InputError", "Error compiling input for get node."), this, InputPin);
		}
		CompileInputs.Add(CompiledInput);
	}

	if (GetInputPin(0) != nullptr && GetInputPin(0)->LinkedTo.Num() > 0)
	{
		Translator->ParameterMapSet(this, CompileInputs, Outputs);
	}
}

FText UNiagaraNodeParameterMapSet::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("UNiagaraNodeParameterMapSetName", "Map Set");
}


void UNiagaraNodeParameterMapSet::BuildParameterMapHistory(FNiagaraParameterMapHistoryBuilder& OutHistory, bool bRecursive)
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	TArray<UEdGraphPin*> InputPins;
	GetInputPins(InputPins);
	
	int32 ParamMapIdx = INDEX_NONE;
	for (int32 i = 0; i < InputPins.Num(); i++)
	{
		if (IsAddPin(InputPins[i]))
		{
			continue;
		}

		OutHistory.VisitInputPin(InputPins[i], this);

		FNiagaraTypeDefinition VarTypeDef = Schema->PinToTypeDefinition(InputPins[i]);
		if (i == 0 && InputPins[i] != nullptr && VarTypeDef == FNiagaraTypeDefinition::GetParameterMapDef())
		{
			UEdGraphPin* PriorParamPin = nullptr;
			if (InputPins[i]->LinkedTo.Num() > 0)
			{
				PriorParamPin = InputPins[i]->LinkedTo[0];
			}

			// Now plow into our ancestor node
			if (PriorParamPin)
			{
				ParamMapIdx = OutHistory.TraceParameterMapOutputPin(PriorParamPin);
			}
		}
		else if (i > 0 && InputPins[i] != nullptr && ParamMapIdx != INDEX_NONE)
		{
			OutHistory.HandleVariableWrite(ParamMapIdx, InputPins[i]);
		}
	}

	OutHistory.RegisterParameterMapPin(ParamMapIdx, GetOutputPin(0));
}

#undef LOCTEXT_NAMESPACE
