// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraNodeOutput.h"
#include "UObject/UnrealType.h"
#include "NiagaraHlslTranslator.h"
#include "NiagaraScript.h"
#include "NiagaraGraph.h"
#include "NiagaraScriptSource.h"
#include "NiagaraCustomVersion.h"
#include "ScopedTransaction.h"
#include "EdGraphSchema_Niagara.h"
#include "MultiBoxBuilder.h"
#include "SBox.h"
#include "SEditableTextBox.h"

#define LOCTEXT_NAMESPACE "NiagaraNodeOutput"

UNiagaraNodeOutput::UNiagaraNodeOutput(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer), ScriptTypeIndex(0)
{
}

void UNiagaraNodeOutput::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
// 	for (FNiagaraVariable& Output : Outputs)
// 	{
// 		//Ensure all pins are valid.
// 		if (Output.GetStruct() == nullptr)
// 		{
// 			Output = FNiagaraVariable();
// 		}
// 	}

	if (PropertyChangedEvent.Property != nullptr)
	{
		ReallocatePins();
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UNiagaraNodeOutput::RemoveOutputPin(UEdGraphPin* Pin)
{
	FScopedTransaction RemovePinTransaction(LOCTEXT("RemovePinTransaction", "Remove pin"));
	int32 Index = Outputs.IndexOfByPredicate([&](const FNiagaraVariable& InVar) { return Pin->PinName == InVar.GetName().ToString(); });
	if (Index >= 0)
	{
		Modify();
		Outputs.RemoveAt(Index);
		ReallocatePins();
		GetNiagaraGraph()->NotifyGraphNeedsRecompile();
	}
}

FText UNiagaraNodeOutput::GetPinNameText(UEdGraphPin* Pin) const
{
	return FText::FromString(Pin->PinName);
}


void UNiagaraNodeOutput::PinNameTextCommitted(const FText& Text, ETextCommit::Type CommitType, UEdGraphPin* Pin)
{
	if (CommitType == ETextCommit::OnEnter)
	{
		FScopedTransaction RenamePinTransaction(LOCTEXT("RenamePinTransaction", "Rename pin"));

		Modify();
		FNiagaraVariable* Var = Outputs.FindByPredicate([&](const FNiagaraVariable& InVar) {return Pin->PinName == InVar.GetName().ToString(); });
		check(Var != nullptr);
		Pin->PinName = Text.ToString();
		Var->SetName(FName(*Pin->PinName));
		GetNiagaraGraph()->NotifyGraphNeedsRecompile();
	}
}

void UNiagaraNodeOutput::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	Super::GetContextMenuActions(Context);
	if (Context.Pin != nullptr)
	{
		Context.MenuBuilder->BeginSection("EdGraphSchema_NiagaraPinActions", LOCTEXT("EditPinMenuHeader", "Edit Pin"));
		
		{
			UEdGraphPin* Pin = const_cast<UEdGraphPin*>(Context.Pin);
			TSharedRef<SWidget> RenameWidget =
				SNew(SBox)
				.WidthOverride(100)
				.Padding(FMargin(5, 0, 0, 0))
				[
					SNew(SEditableTextBox)
					.Text_UObject(this, &UNiagaraNodeOutput::GetPinNameText, Pin)
					.OnTextCommitted_UObject(this, &UNiagaraNodeOutput::PinNameTextCommitted, Pin)
				];
			Context.MenuBuilder->AddWidget(RenameWidget, LOCTEXT("NameMenuItem", "Name"));
		}

		
		{
			Context.MenuBuilder->AddMenuEntry(
				LOCTEXT("RemoveDynamicPin", "Remove pin"),
				LOCTEXT("RemoveDynamicPinToolTip", "Remove this pin and any connections."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateUObject(const_cast<UNiagaraNodeOutput*>(this), &UNiagaraNodeOutput::RemoveOutputPin, const_cast<UEdGraphPin*>(Context.Pin))));
		}
	}
}

void UNiagaraNodeOutput::AllocateDefaultPins()
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();

// 	if (Script->Usage == ENiagaraScriptUsage::ParticleSpawnScript || Script->Usage == ENiagaraScriptUsage::ParticleUpdateScript)
// 	{
// 		//For the outermost spawn and update scripts output nodes we need an extra pin telling whether the instance is still alive or not.
// 		CreatePin(EGPD_Input, Schema->TypeDefinitionToPinType(FNiagaraTypeDefinition::GetIntDef()), TEXT("Alive"));
// 	}

	for (const FNiagaraVariable& Output : Outputs)
	{
		UEdGraphPin* Pin = CreatePin(EGPD_Input, Schema->TypeDefinitionToPinType(Output.GetType()), Output.GetName().ToString());
		if (ScriptType == ENiagaraScriptUsage::ParticleUpdateScript)
		{
			Pin->bDefaultValueIsIgnored = true;
		}
	}
}

bool UNiagaraNodeOutput::CanUserDeleteNode() const 
{
	if (ScriptType == ENiagaraScriptUsage::ParticleEventScript)
	{
		return true;
	}
	return false; 
}

bool UNiagaraNodeOutput::CanDuplicateNode() const
{
	return false;
}

FText UNiagaraNodeOutput::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (ScriptType == ENiagaraScriptUsage::ParticleSpawnScript)
	{
		return NSLOCTEXT("NiagaraNodeOutput", "OutputSpawn", "Output Particle Spawn");
	}
	else if (ScriptType == ENiagaraScriptUsage::ParticleSpawnScriptInterpolated)
	{
		return NSLOCTEXT("NiagaraNodeOutput", "OutputSpawn", "Output Particle Spawn");
	}
	else if (ScriptType == ENiagaraScriptUsage::ParticleUpdateScript)
	{
		return NSLOCTEXT("NiagaraNodeOutput", "OutputUpdate", "Output Particle Update");
	}
	else if (ScriptType == ENiagaraScriptUsage::ParticleEventScript)
	{
		return FText::Format(NSLOCTEXT("NiagaraNodeOutput", "OutputEvent", "Output Event {0}"), FText::AsNumber(ScriptTypeIndex));
	}
	else if (ScriptType == ENiagaraScriptUsage::Function)
	{
		return NSLOCTEXT("NiagaraNodeOutput", "OutputFunction", "Output Function");
	}
	else if (ScriptType == ENiagaraScriptUsage::DynamicInput)
	{
		return NSLOCTEXT("NiagaraNodeOutput", "OutputDynamicInput", "Output Dynamic Input");
	}
	else if (ScriptType == ENiagaraScriptUsage::Module)
	{
		return NSLOCTEXT("NiagaraNodeOutput", "OutputModule", "Output Module");
	}
	else  if (ScriptType == ENiagaraScriptUsage::EmitterSpawnScript)
	{
		return NSLOCTEXT("NiagaraNodeOutput", "OutputEmitterSpawn", "Output Emitter Spawn");
	}
	else if (ScriptType == ENiagaraScriptUsage::EmitterUpdateScript)
	{
		return NSLOCTEXT("NiagaraNodeOutput", "OutputEmitterUpdate", "Output Emitter Update");
	}
	else  if (ScriptType == ENiagaraScriptUsage::SystemSpawnScript)
	{
		return NSLOCTEXT("NiagaraNodeOutput", "OutputSystemSpawn", "Output System Spawn");
	}
	else if (ScriptType == ENiagaraScriptUsage::SystemUpdateScript)
	{
		return NSLOCTEXT("NiagaraNodeOutput", "OutputSystemUpdate", "Output System Update");
	}

	return NSLOCTEXT("NiagaraNode", "Output", "Output");
}

FLinearColor UNiagaraNodeOutput::GetNodeTitleColor() const
{
	return CastChecked<UEdGraphSchema_Niagara>(GetSchema())->NodeTitleColor_Attribute;
}

void UNiagaraNodeOutput::NotifyOutputVariablesChanged()
{
	ReallocatePins();
}

int32 UNiagaraNodeOutput::CompileInputPin(FHlslNiagaraTranslator *Translator, UEdGraphPin* Pin)
{
	// If we are an update script, automatically fill in any unwired values with the previous frame's value...
	if (GetUsage() == ENiagaraScriptUsage::ParticleUpdateScript && Pin->LinkedTo.Num() == 0)
	{
		FNiagaraVariable OutputVariable;
		for (const FNiagaraVariable& Output : Outputs)
		{
			if (Output.GetName().ToString() == Pin->GetName() && Output.GetType() != FNiagaraTypeDefinition::GetParameterMapDef())
			{
				OutputVariable = Output;
				return Translator->GetAttribute(OutputVariable);
			}
		}
	}

	return Translator->CompilePin(Pin);
}

void UNiagaraNodeOutput::Compile(FHlslNiagaraTranslator *Translator, TArray<int32>& OutputExpressions)
{
	TArray<int32> Results;
	bool bError = CompileInputPins(Translator, Results);
	if (!bError)
	{
		Translator->Output(this, Results);
	}
}


void UNiagaraNodeOutput::PostLoad()
{
	Super::PostLoad();

	// In the version change for FNiagaraCustomVersion::UpdateSpawnEventGraphCombination we now require that output nodes specify the
	// compilation type that they support.
	const int32 NiagaraVer = GetLinkerCustomVersion(FNiagaraCustomVersion::GUID);
	if (NiagaraVer < FNiagaraCustomVersion::UpdateSpawnEventGraphCombination && NiagaraVer > 0)
	{
		UEdGraph* Graph = GetGraph();
		if (Graph != nullptr)
		{
			UNiagaraScriptSource* Src = Cast<UNiagaraScriptSource>(Graph->GetOuter());
			if (Src != nullptr)
			{
				UNiagaraScript* Script = Cast<UNiagaraScript>(Src->GetOuter());
				if (Script != nullptr)
				{
					SetUsage(Script->GetUsage());
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
