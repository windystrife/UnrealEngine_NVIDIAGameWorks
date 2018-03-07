// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraNodeParameterMapGet.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraEditorUtilities.h"
#include "SNiagaraGraphNodeConvert.h"
#include "NiagaraGraph.h"
#include "NiagaraHlslTranslator.h"
#include "ScopedTransaction.h"
#include "SNiagaraGraphParameterMapGetNode.h"
#include "NiagaraEditorModule.h"
#include "INiagaraEditorTypeUtilities.h"
#include "ModuleManager.h"

#define LOCTEXT_NAMESPACE "NiagaraNodeParameterMapGet"

UNiagaraNodeParameterMapGet::UNiagaraNodeParameterMapGet() : UNiagaraNodeParameterMapBase(), PinPendingRename(nullptr)
{

}


void UNiagaraNodeParameterMapGet::AllocateDefaultPins()
{
	PinPendingRename = nullptr;
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	CreatePin(EGPD_Input, Schema->TypeDefinitionToPinType(FNiagaraTypeDefinition::GetParameterMapDef()), TEXT("Source"));
	CreateAddPin(EGPD_Output);
}


TSharedPtr<SGraphNode> UNiagaraNodeParameterMapGet::CreateVisualWidget()
{
	return SNew(SNiagaraGraphParameterMapGetNode, this);
}

bool UNiagaraNodeParameterMapGet::IsPinNameEditable(const UEdGraphPin* GraphPinObj) const
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	FNiagaraTypeDefinition TypeDef = Schema->PinToTypeDefinition(GraphPinObj);
	if (TypeDef.IsValid() && GraphPinObj && GraphPinObj->Direction == EGPD_Output && CanRenamePin(GraphPinObj))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool UNiagaraNodeParameterMapGet::IsPinNameEditableUponCreation(const UEdGraphPin* GraphPinObj) const
{
	if (GraphPinObj == PinPendingRename && GraphPinObj->Direction == EEdGraphPinDirection::EGPD_Output)
	{
		return true;
	}
	else
	{
		return false;
	}
}


bool UNiagaraNodeParameterMapGet::VerifyEditablePinName(const FText& InName, FText& OutErrorMessage, const UEdGraphPin* InGraphPinObj) const
{
	if (InName.IsEmptyOrWhitespace() && InGraphPinObj->Direction == EEdGraphPinDirection::EGPD_Output)
	{
		OutErrorMessage = LOCTEXT("InvalidName", "Invalid pin name");
		return false;
	}
	return true;
}

UEdGraphPin* UNiagaraNodeParameterMapGet::CreateDefaultPin(UEdGraphPin* OutputPin)
{
	if (OutputPin == nullptr)
	{
		return nullptr;
	}

	UEdGraphPin* DefaultPin = CreatePin(EEdGraphPinDirection::EGPD_Input, OutputPin->PinType, TEXT(""));
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	FNiagaraVariable Var = Schema->PinToNiagaraVariable(OutputPin);
	FNiagaraEditorUtilities::ResetVariableToDefaultValue(Var);

	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::GetModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
	if (Var.IsDataAllocated())
	{
		TSharedPtr<INiagaraEditorTypeUtilities> TypeEditorUtilities = NiagaraEditorModule.GetTypeUtilities(Var.GetType());

		if (TypeEditorUtilities.IsValid() && TypeEditorUtilities->CanHandlePinDefaults())
		{
			FString Value = TypeEditorUtilities->GetPinDefaultStringFromValue(Var);
			DefaultPin->DefaultValue = Value;
		}
	}
	
	if (!OutputPin->PersistentGuid.IsValid())
	{
		OutputPin->PersistentGuid = FGuid::NewGuid();
	}
	if (!DefaultPin->PersistentGuid.IsValid())
	{
		DefaultPin->PersistentGuid = FGuid::NewGuid();
	}
	PinOutputToPinDefaultPersistentId.Add(OutputPin->PersistentGuid, DefaultPin->PersistentGuid);

	SynchronizeDefaultInputPin(DefaultPin, OutputPin);
	return DefaultPin;
}

void UNiagaraNodeParameterMapGet::OnPinRenamed(UEdGraphPin* RenamedPin)
{
	RenamedPin->PinFriendlyName = FText::FromString(RenamedPin->PinName);
}


void UNiagaraNodeParameterMapGet::OnNewTypedPinAdded(UEdGraphPin* NewPin)
{
	if (NewPin->Direction == EEdGraphPinDirection::EGPD_Output)
	{
		const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
		FNiagaraTypeDefinition TypeDef = Schema->PinToTypeDefinition(NewPin);

		UEdGraphPin* MatchingDefault = GetDefaultPin(NewPin);
		if (MatchingDefault == nullptr)
		{
			UEdGraphPin* DefaultPin = CreateDefaultPin(NewPin);
		}
	}

	if (HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedInitialization))
	{
		return;
	}

	if (NewPin->Direction == EEdGraphPinDirection::EGPD_Output)
	{
		PinPendingRename = NewPin;
	}

}

void UNiagaraNodeParameterMapGet::RemoveDynamicPin(UEdGraphPin* Pin)
{
	FScopedTransaction RemovePinTransaction(LOCTEXT("RemovePinTransaction", "Remove pin"));

	UEdGraphPin* DefaultPin = nullptr;
	if (Pin->Direction == EEdGraphPinDirection::EGPD_Output)
	{
		DefaultPin = GetDefaultPin(Pin);
	}

	RemovePin(Pin);

	if (DefaultPin != nullptr)
	{
		RemovePin(DefaultPin);
	}

	GetGraph()->NotifyGraphChanged();
}

UEdGraphPin* UNiagaraNodeParameterMapGet::GetDefaultPin(UEdGraphPin* OutputPin)
{
	TArray<UEdGraphPin*> InputPins;
	GetInputPins(InputPins);

	const FGuid* InputGuid = PinOutputToPinDefaultPersistentId.Find(OutputPin->PersistentGuid);

	if (InputGuid != nullptr)
	{
		for (UEdGraphPin* InputPin : InputPins)
		{
			if ((*InputGuid) == InputPin->PersistentGuid)
			{
				return InputPin;
			}
		}
	}

	return nullptr;
}


UEdGraphPin* UNiagaraNodeParameterMapGet::GetOutputPinForDefault(UEdGraphPin* DefaultPin)
{
	TArray<UEdGraphPin*> OutputPins;
	GetOutputPins(OutputPins);

	const FGuid* OutputGuid = PinOutputToPinDefaultPersistentId.Find(DefaultPin->PersistentGuid);

	if (OutputGuid != nullptr)
	{
		for (UEdGraphPin* OutputPin : OutputPins)
		{
			if ((*OutputGuid) == OutputPin->PersistentGuid)
			{
				return OutputPin;
			}
		}
	}

	return nullptr;
}

void UNiagaraNodeParameterMapGet::PostLoad()
{
	Super::PostLoad();
	TArray<UEdGraphPin*> OutputPins;
	GetOutputPins(OutputPins);
	for (int32 i = 0; i < OutputPins.Num(); i++)
	{
		if (IsAddPin(OutputPins[i]))
		{
			continue;
		}
		UEdGraphPin* InputPin = GetDefaultPin(OutputPins[i]);
		if (InputPin == nullptr)
		{
			CreateDefaultPin(OutputPins[i]);
		}
		else
		{
			SynchronizeDefaultInputPin(InputPin, OutputPins[i]);
		}
	}
}


void UNiagaraNodeParameterMapGet::SynchronizeDefaultInputPin(UEdGraphPin* DefaultPin, UEdGraphPin* OutputPin)
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	if (!DefaultPin)
	{
		return;
	}

	if (FNiagaraParameterMapHistory::IsEngineParameter(Schema->PinToNiagaraVariable(OutputPin)))
	{
		DefaultPin->bDefaultValueIsIgnored = true;
		DefaultPin->bNotConnectable = true;
		DefaultPin->bHidden = true;
		DefaultPin->PinToolTip = FText::Format(LOCTEXT("DefaultValueTooltip", "Default value for {0}. Disabled for Engine Parameters."), FText::FromString(OutputPin->PinName)).ToString();
	}
	else
	{
		DefaultPin->bDefaultValueIsIgnored = false;
		DefaultPin->bNotConnectable = false;
		DefaultPin->bHidden = false;
		DefaultPin->PinToolTip = FText::Format(LOCTEXT("DefaultValueTooltip", "Default value for {0} if no other module has set it previously in the stack."), FText::FromString(OutputPin->PinName)).ToString();
	}
}


FText UNiagaraNodeParameterMapGet::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("UNiagaraNodeParameterMapGetName", "Map Get");
}

void UNiagaraNodeParameterMapGet::BuildParameterMapHistory(FNiagaraParameterMapHistoryBuilder& OutHistory, bool bRecursive)
{
	if (bRecursive)
	{
		OutHistory.VisitInputPin(GetInputPin(0), this);
	}

	int32 ParamMapIdx = INDEX_NONE;
	if (GetInputPin(0)->LinkedTo.Num() != 0)
	{
		ParamMapIdx = OutHistory.TraceParameterMapOutputPin(GetInputPin(0)->LinkedTo[0]);
	}

	if (ParamMapIdx != INDEX_NONE)
	{
		TArray<UEdGraphPin*> OutputPins;
		GetOutputPins(OutputPins);
		for (int32 i = 0; i < OutputPins.Num(); i++)
		{
			if (IsAddPin(OutputPins[i]))
			{
				continue;
			}

			bool bUsedDefaults = false;
			if (bRecursive)
			{
				OutHistory.HandleVariableRead(ParamMapIdx, OutputPins[i], true, GetDefaultPin(OutputPins[i]), bUsedDefaults);
			}
			else
			{
				OutHistory.HandleVariableRead(ParamMapIdx, OutputPins[i], true, nullptr, bUsedDefaults);
			}
		}
	}
}

void UNiagaraNodeParameterMapGet::Compile(class FHlslNiagaraTranslator* Translator, TArray<int32>& Outputs)
{
	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());

	TArray<UEdGraphPin*> InputPins;
	GetInputPins(InputPins);
	TArray<UEdGraphPin*> OutputPins;
	GetOutputPins(OutputPins);

	// Initialize the outputs to invalid values.
	check(Outputs.Num() == 0);
	for (int32 i = 0; i < OutputPins.Num(); i++)
	{
		if (IsAddPin(OutputPins[i]))
		{
			continue;
		}
		Outputs.Add(INDEX_NONE);
	}

	// First compile fully down the hierarchy for our predecessors..
	TArray<int32> CompileInputs;

	for (int32 i = 0; i < InputPins.Num(); i++)
	{
		UEdGraphPin* InputPin = InputPins[i];

		if (InputPin->PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryType || 
			InputPin->PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryEnum)
		{
			int32 CompiledInput = INDEX_NONE;
			if (i == 0) // Only the zeroth item is not an default value pin.
			{
				CompiledInput = Translator->CompilePin(InputPin);
				if (CompiledInput == INDEX_NONE)
				{
					Translator->Error(LOCTEXT("InputError", "Error compiling input for param map get node."), this, InputPin);
				}
			}
			CompileInputs.Add(CompiledInput);
		}
	}

	UNiagaraGraph* Graph = Cast<UNiagaraGraph>(GetGraph());
	// By this point, we've visited all of our child nodes in the call graph. We can mine them to find out everyone contributing to the parameter map (and when).
	if (GetInputPin(0) != nullptr && GetInputPin(0)->LinkedTo.Num() > 0)
	{
		Translator->ParameterMapGet(this, CompileInputs, Outputs);
	}
}


bool UNiagaraNodeParameterMapGet::CommitEditablePinName(const FText& InName, UEdGraphPin* InGraphPinObj) 
{
	if (Pins.Contains(InGraphPinObj) && InGraphPinObj->Direction == EEdGraphPinDirection::EGPD_Output)
	{
		FScopedTransaction AddNewPinTransaction(LOCTEXT("Rename Pin", "Renamed pin"));
		Modify();
		InGraphPinObj->Modify();
		UEdGraphPin* DefaultPin = GetDefaultPin(InGraphPinObj);

		InGraphPinObj->PinFriendlyName = InName;
		InGraphPinObj->PinName = InName.ToString();

		if (DefaultPin)
		{
			DefaultPin->Modify();
			SynchronizeDefaultInputPin(DefaultPin, InGraphPinObj);
		}


		if (PinPendingRename == InGraphPinObj)
		{
			PinPendingRename = nullptr;
		}	
		GetGraph()->NotifyGraphChanged();

		return true;
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
