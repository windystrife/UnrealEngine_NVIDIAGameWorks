// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraNodeWriteDataSet.h"
#include "UObject/UnrealType.h"
#include "NiagaraHlslTranslator.h"
#include "NiagaraEvents.h"
#include "SNiagaraGraphNodeWriteDataSet.h"
#include "EdGraphSchema_Niagara.h"

#define LOCTEXT_NAMESPACE "NiagaraNodeWriteDataSet"

UNiagaraNodeWriteDataSet::UNiagaraNodeWriteDataSet(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void UNiagaraNodeWriteDataSet::AddConditionPin(int32 PinIndex)
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	const bool ConditionPinDefaulValue = true;
	UEdGraphPin* ConditionPin = CreatePin(EGPD_Input, Schema->TypeDefinitionToPinType(FNiagaraTypeDefinition::GetBoolDef()), ConditionVarName, PinIndex);
	ConditionPin->bDefaultValueIsIgnored = false;
	ConditionPin->DefaultValue = ConditionPinDefaulValue ? TEXT("true") : TEXT("false");
	ConditionPin->PinFriendlyName = LOCTEXT("UNiagaraNodeWriteDataSetConditionPin", "Condition");

}


void UNiagaraNodeWriteDataSet::AllocateDefaultPins()
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();

	if (DataSet.Type == ENiagaraDataSetType::Event)
	{
		//Probably need this for all data set types tbh!
		//UEdGraphPin* Pin = CreatePin(EGPD_Input, Schema->TypeDefinitionToPinType(FNiagaraTypeDefinition::GetBoolDef()), TEXT("Valid"));
		//Pin->bDefaultValueIsIgnored = true;
	}

	AddConditionPin();
	
	bool useFriendlyNames = (VariableFriendlyNames.Num() == Variables.Num());
	for (int32 i = 0; i < Variables.Num(); i++)
	{
		FNiagaraVariable& Var = Variables[i];
		UEdGraphPin* NewPin = CreatePin(EGPD_Input, Schema->TypeDefinitionToPinType(Var.GetType()), Var.GetName().ToString());
		if (useFriendlyNames && VariableFriendlyNames[i].IsEmpty() == false)
		{
			NewPin->PinFriendlyName = FText::FromString(VariableFriendlyNames[i]);
		}
	}
}

TSharedPtr<SGraphNode> UNiagaraNodeWriteDataSet::CreateVisualWidget()
{
	return SNew(SNiagaraGraphNodeWriteDataSet, this);
}

FText UNiagaraNodeWriteDataSet::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::Format(LOCTEXT("NiagaraDataSetWriteFormat", "{0} Write"), FText::FromName(DataSet.Name));
}

void UNiagaraNodeWriteDataSet::Compile(class FHlslNiagaraTranslator* Translator, TArray<int32>& Outputs)
{
	bool bError = false;

	//TODO implement writing to data sets in hlsl compiler and vm.

	TArray<int32> Inputs;
	CompileInputPins(Translator, Inputs);

	FString IssuesWithStruct;
	if (!IsSynchronizedWithStruct(true, &IssuesWithStruct,false))
	{
		Translator->Error(FText::FromString(IssuesWithStruct), this, nullptr);
	}
	
	if (EventName.IsNone())
	{
		EventName = DataSet.Name;
	}

	FNiagaraDataSetID AlteredDataSet = DataSet;
	AlteredDataSet.Name = EventName;
	Translator->WriteDataSet(AlteredDataSet, Variables, ENiagaraDataSetAccessMode::AppendConsume, Inputs);

}

void UNiagaraNodeWriteDataSet::PostLoad()
{
	Super::PostLoad();

	bool bFoundMatchingPin = false;
	for (int32 PinIndex = 0; PinIndex < Pins.Num(); PinIndex++)
	{
		if (Pins[PinIndex]->Direction == EGPD_Input && Pins[PinIndex]->PinName == ConditionVarName)
		{
			bFoundMatchingPin = true;
			break;
		}
	}

	if (!bFoundMatchingPin)
	{
		AddConditionPin(0);
	}

	if (EventName.IsNone())
	{
		EventName = DataSet.Name;
	}
}

#undef LOCTEXT_NAMESPACE





