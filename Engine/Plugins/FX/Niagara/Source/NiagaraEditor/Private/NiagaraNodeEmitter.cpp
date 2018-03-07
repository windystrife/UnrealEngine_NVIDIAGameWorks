// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraNodeEmitter.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEditorUtilities.h"
#include "EdGraphSchema_Niagara.h"
#include "EdGraph/EdGraph.h"
#include "NiagaraCommon.h"
#include "NiagaraDataInterface.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeInput.h"
#include "SNiagaraGraphPinAdd.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeParameterMapBase.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraHlslTranslator.h"
#include "MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "NiagaraNodeEmitter"

void UNiagaraNodeEmitter::PostInitProperties()
{
	Super::PostInitProperties();
	PinPendingRename = nullptr;
}

UNiagaraSystem* UNiagaraNodeEmitter::GetOwnerSystem() const
{
	return OwnerSystem;
}

void UNiagaraNodeEmitter::SetOwnerSystem(UNiagaraSystem* InOwnerSystem)
{
	OwnerSystem = InOwnerSystem;
	DisplayName = GetNameFromEmitter();
}

FGuid UNiagaraNodeEmitter::GetEmitterHandleId() const
{
	return EmitterHandleId;
}

void UNiagaraNodeEmitter::SetEmitterHandleId(FGuid InEmitterHandleId)
{
	EmitterHandleId = InEmitterHandleId;
	DisplayName = GetNameFromEmitter();
}

void UNiagaraNodeEmitter::PostLoad()
{
	Super::PostLoad();

	ReallocatePins();
}

bool UNiagaraNodeEmitter::IsPinNameEditable(const UEdGraphPin* GraphPinObj) const
{
	return false;
}

bool UNiagaraNodeEmitter::IsPinNameEditableUponCreation(const UEdGraphPin* GraphPinObj) const
{
	return false;
}


bool UNiagaraNodeEmitter::VerifyEditablePinName(const FText& InName, FText& OutErrorMessage, const UEdGraphPin* InGraphPinObj) const
{
	if (InName.IsEmptyOrWhitespace())
	{
		OutErrorMessage = LOCTEXT("InvalidName", "Invalid pin name");
		return false;
	}
	return true;
}

bool UNiagaraNodeEmitter::CommitEditablePinName(const FText& InName, UEdGraphPin* InGraphPinObj)
{
	return false;
}


void UNiagaraNodeEmitter::AllocateDefaultPins()
{
	UNiagaraEmitter* Emitter = nullptr;
	if (OwnerSystem)
	{
		for (int32 i = 0; i < OwnerSystem->GetNumEmitters(); ++i)
		{
			if (OwnerSystem->GetEmitterHandle(i).GetId() == EmitterHandleId)
			{
				Emitter = OwnerSystem->GetEmitterHandle(i).GetInstance();
			}
		}
	}

	if (Emitter != nullptr)
	{
		const UEdGraphSchema_Niagara* NiagaraSchema = Cast<UEdGraphSchema_Niagara>(GetSchema());
		CreatePin(EGPD_Input, NiagaraSchema->TypeDefinitionToPinType(FNiagaraTypeDefinition::GetParameterMapDef()), TEXT("InputMap"));
		CreatePin(EGPD_Output, NiagaraSchema->TypeDefinitionToPinType(FNiagaraTypeDefinition::GetParameterMapDef()), TEXT("OutputMap"));
		CachedEmitterChangeId = Emitter->ChangeId;
	}
}

bool UNiagaraNodeEmitter::CanUserDeleteNode() const
{
	return false;
}

bool UNiagaraNodeEmitter::CanDuplicateNode() const
{
	return false;
}

FText UNiagaraNodeEmitter::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FText UsageText;
	if (ScriptType == ENiagaraScriptUsage::EmitterSpawnScript)
	{
		UsageText = LOCTEXT("SpawnTitle", "Spawn");
	}
	else if (ScriptType == ENiagaraScriptUsage::EmitterUpdateScript)
	{
		UsageText = LOCTEXT("UpdateTitle", "Update");
	}
	else
	{
		UsageText = LOCTEXT("Unknown Title", "Unknown");
	}
	return FText::Format(LOCTEXT("EmitterNameTitle","Emitter {0} {1}"), DisplayName, UsageText);
}

FLinearColor UNiagaraNodeEmitter::GetNodeTitleColor() const
{
	return CastChecked<UEdGraphSchema_Niagara>(GetSchema())->NodeTitleColor_Attribute;
}

void UNiagaraNodeEmitter::NodeConnectionListChanged()
{
	GetGraph()->NotifyGraphChanged();
}

FString UNiagaraNodeEmitter::GetEmitterUniqueName() const
{
	if (OwnerSystem != nullptr && EmitterHandleId.IsValid())
	{
		for (const FNiagaraEmitterHandle& EmitterHandle : OwnerSystem->GetEmitterHandles())
		{
			if (EmitterHandle.GetId() == EmitterHandleId)
			{
				return EmitterHandle.GetUniqueInstanceName();
			}
		}
	}
	return FString();
}

UNiagaraScriptSource* UNiagaraNodeEmitter::GetScriptSource() const
{
	// First get the emitter that we're referencing..
	UNiagaraEmitter* Emitter = nullptr;
	if (OwnerSystem)
	{
		for (int32 i = 0; i < OwnerSystem->GetNumEmitters(); ++i)
		{
			if (OwnerSystem->GetEmitterHandle(i).GetId() == EmitterHandleId)
			{
				Emitter = OwnerSystem->GetEmitterHandle(i).GetInstance();
			}
		}
	}

	// Now get the graph off that emitter
	if (Emitter != nullptr && Emitter->GraphSource != nullptr)
	{
		UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Emitter->GraphSource);
		return Source;
	}

	return nullptr;
}

UNiagaraGraph* UNiagaraNodeEmitter::GetCalledGraph() const
{
	// First get the emitter that we're referencing..
	UNiagaraEmitter* Emitter = nullptr;
	if (OwnerSystem)
	{
		for (int32 i = 0; i < OwnerSystem->GetNumEmitters(); ++i)
		{
			if (OwnerSystem->GetEmitterHandle(i).GetId() == EmitterHandleId)
			{
				Emitter = OwnerSystem->GetEmitterHandle(i).GetInstance();
			}
		}
	}

	// Now get the graph off that emitter
	if (Emitter != nullptr && Emitter->GraphSource != nullptr)
	{
		UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Emitter->GraphSource);
		if (Source)
		{
			return Source->NodeGraph;
		}
	}
	return nullptr;
}

bool UNiagaraNodeEmitter::RefreshFromExternalChanges()
{
	// First get the emitter that we're referencing..
	UNiagaraEmitter* Emitter = nullptr;
	if (OwnerSystem)
	{
		for (int32 i = 0; i < OwnerSystem->GetNumEmitters(); ++i)
		{
			if (OwnerSystem->GetEmitterHandle(i).GetId() == EmitterHandleId)
			{
				Emitter = OwnerSystem->GetEmitterHandle(i).GetInstance();
			}
		}
	}

	if (Emitter && Emitter->ChangeId != CachedEmitterChangeId)
	{
		// TODO - Leverage code in reallocate pins to determine if any pins have changed...
		ReallocatePins();
		DisplayName = GetNameFromEmitter();
		return true;
	}
	return false;
}

FText UNiagaraNodeEmitter::GetNameFromEmitter()
{
	if (OwnerSystem != nullptr && EmitterHandleId.IsValid())
	{
		for (const FNiagaraEmitterHandle& EmitterHandle : OwnerSystem->GetEmitterHandles())
		{
			if (EmitterHandle.GetId() == EmitterHandleId)
			{
				return FText::FromName(EmitterHandle.GetName());
			}
		}
	}
	return FText();
}

void UNiagaraNodeEmitter::BuildParameterMapHistory(FNiagaraParameterMapHistoryBuilder& OutHistory, bool bRecursive)
{
	Super::BuildParameterMapHistory(OutHistory, bRecursive);

	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	TArray<UEdGraphPin*> InputPins;
	GetInputPins(InputPins);
	TArray<UEdGraphPin*> OutputPins;
	GetOutputPins(OutputPins);
	
	int32 ParamMapIdx = INDEX_NONE;
	if (GetInputPin(0)->LinkedTo.Num() != 0)
	{
		if (bRecursive)
		{
			ParamMapIdx = OutHistory.TraceParameterMapOutputPin(GetInputPin(0)->LinkedTo[0]);
		}
		else
		{
			ParamMapIdx = OutHistory.CreateParameterMap();
		}
	}

	FString EmitterUniqueName = GetEmitterUniqueName();
	UNiagaraGraph* Graph = GetCalledGraph();
	if (Graph && ParamMapIdx != INDEX_NONE)
	{
		OutHistory.EnterEmitter(EmitterUniqueName, this);

		TArray<ENiagaraScriptUsage> Usages;
		Usages.Add(ENiagaraScriptUsage::EmitterSpawnScript);
		Usages.Add(ENiagaraScriptUsage::EmitterUpdateScript);
		Usages.Add(ENiagaraScriptUsage::ParticleSpawnScript);
		Usages.Add(ENiagaraScriptUsage::ParticleSpawnScriptInterpolated);
		Usages.Add(ENiagaraScriptUsage::ParticleUpdateScript);
		Usages.Add(ENiagaraScriptUsage::ParticleEventScript);
		
		for (ENiagaraScriptUsage OutputNodeUsage : Usages)
		{
			TArray<UNiagaraNodeOutput*> OutputNodes;

			Graph->FindOutputNodes(OutputNodeUsage, OutputNodes);

			// Build up a new parameter map history with all the child graph nodes..
			FNiagaraParameterMapHistoryBuilder ChildBuilder;

			ChildBuilder.EnableScriptWhitelist(true, GetUsage());
			ChildBuilder.EnterEmitter(EmitterUniqueName, this);
			for (UNiagaraNodeOutput* OutputNode : OutputNodes)
			{
				ChildBuilder.BuildParameterMaps(OutputNode, true);
			}
			ChildBuilder.ExitEmitter(EmitterUniqueName, this);
			 
			for (FNiagaraParameterMapHistory& History : ChildBuilder.Histories)
			{
				OutHistory.Histories[ParamMapIdx].MapPinHistory.Append(History.MapPinHistory);
				for (int32 SrcVarIdx = 0; SrcVarIdx < History.Variables.Num(); SrcVarIdx++)
				{
					FNiagaraVariable& Var = History.Variables[SrcVarIdx];
					int32 ExistingIdx = OutHistory.Histories[ParamMapIdx].FindVariable(Var.GetName(), Var.GetType());
					if (ExistingIdx == INDEX_NONE)
					{
						ExistingIdx = OutHistory.Histories[ParamMapIdx].Variables.Add(Var);
						OutHistory.Histories[ParamMapIdx].PerVariableReadHistory.AddDefaulted(1);
						OutHistory.Histories[ParamMapIdx].PerVariableWriteHistory.AddDefaulted(1);
						OutHistory.Histories[ParamMapIdx].PerVariableWarnings.AddDefaulted(1);
					}
					ensure(ExistingIdx < OutHistory.Histories[ParamMapIdx].PerVariableWarnings.Num());
					ensure(ExistingIdx < OutHistory.Histories[ParamMapIdx].PerVariableReadHistory.Num());
					ensure(ExistingIdx < OutHistory.Histories[ParamMapIdx].PerVariableWriteHistory.Num());
					OutHistory.Histories[ParamMapIdx].PerVariableReadHistory[ExistingIdx].Append(History.PerVariableReadHistory[SrcVarIdx]);
					OutHistory.Histories[ParamMapIdx].PerVariableWriteHistory[ExistingIdx].Append(History.PerVariableWriteHistory[SrcVarIdx]);
					OutHistory.Histories[ParamMapIdx].PerVariableWarnings[ExistingIdx].Append(History.PerVariableWarnings[SrcVarIdx]);	
				}
			}
		}

		OutHistory.ExitEmitter(EmitterUniqueName, this);
	}

	for (UEdGraphPin* Pin : OutputPins)
	{
		FNiagaraTypeDefinition Type = Schema->PinToTypeDefinition(Pin);
		if (Type == FNiagaraTypeDefinition::GetParameterMapDef())
		{
			OutHistory.RegisterParameterMapPin(ParamMapIdx, Pin);
		}
	}
}

void UNiagaraNodeEmitter::Compile(FHlslNiagaraTranslator *Translator, TArray<int32>& Outputs)
{
	TArray<UEdGraphPin*> InputPins;
	GetInputPins(InputPins);
	InputPins.RemoveAll([](UEdGraphPin* InputPin) { return (InputPin->PinType.PinCategory != UEdGraphSchema_Niagara::PinCategoryType) && (InputPin->PinType.PinCategory != UEdGraphSchema_Niagara::PinCategoryEnum); });

	TArray<UEdGraphPin*> OutputPins;
	GetOutputPins(OutputPins);

	check(Outputs.Num() == 0);

	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());

	// First compile fully down the hierarchy for our predecessors..
	UNiagaraGraph* CalledGraph = GetCalledGraph();
	TArray<UNiagaraNodeInput*> InputsNodes;
	UNiagaraGraph::FFindInputNodeOptions Options;
	Options.bSort = true;
	Options.bFilterDuplicates = true;
	Options.bFilterByScriptUsage = true;
	Options.TargetScriptUsage = Translator->GetTargetUsage() == ENiagaraScriptUsage::SystemSpawnScript ? ENiagaraScriptUsage::EmitterSpawnScript : ENiagaraScriptUsage::EmitterUpdateScript;
	CalledGraph->FindInputNodes(InputsNodes, Options);

	TArray<int32> CompileInputs;
	CompileInputs.Reserve(InputsNodes.Num());

	if (InputsNodes.Num() <= 0)
	{
		Translator->Error(LOCTEXT("InputNodesNotFound", "Input nodes on called graph not found"), this, nullptr);
		return;
	}

	if (InputPins.Num() > 1)
	{
		Translator->Error(LOCTEXT("TooManyOutputPinsError", "Too many input pins on node."), this, nullptr);
		return;
	}

	bool bError = false;
	for (UNiagaraNodeInput* EmitterInputNode : InputsNodes)
	{
		if (EmitterInputNode->Input.IsEquivalent(FNiagaraVariable(FNiagaraTypeDefinition::GetParameterMapDef(), TEXT("InputMap"))))
		{
			CompileInputs.Add(Translator->CompilePin(InputPins[0]));
		}
		else
		{
			CompileInputs.Add(INDEX_NONE);
		}
	}

	if (!bError)
	{
		Translator->Emitter(this, CompileInputs, Outputs);
	}
}

TSharedRef<SWidget> UNiagaraNodeEmitter::GenerateAddPinMenu(const FString& InWorkingPinName, SNiagaraGraphPinAdd* InPin)
{
	FMenuBuilder MenuBuilder(true, nullptr);
	
	UNiagaraGraph* Graph = GetCalledGraph();
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();

	TArray<UEdGraphPin*> InputPins;
	GetInputPins(InputPins);

	FString EmitterNamespace = FNiagaraParameterMapHistory::MakeSafeNamespaceString(GetNameFromEmitter().ToString());

	FNiagaraParameterMapHistoryBuilder HistoryBuilder;
	BuildParameterMapHistory(HistoryBuilder, false);

	if (HistoryBuilder.Histories.Num() == 0)
	{
		return SNullWidget::NullWidget;
	}

	TArray<FNiagaraVariable> ExistingVariables;
	for (UEdGraphPin* InputPin : InputPins)
	{
		ExistingVariables.Add(Schema->PinToNiagaraVariable(InputPin));
	}

	TArray<FNiagaraVariable> Variables;
	for (const FNiagaraVariable& Var : HistoryBuilder.Histories[0].Variables)
	{
		if (ExistingVariables.ContainsByPredicate([Var](const FNiagaraVariable& A) { return A.GetName().ToString().Equals(Var.GetName().ToString()); }))
		{
			continue;
		}

		if (FNiagaraParameterMapHistory::IsInNamespace(Var, TEXT("EmitterUniforms")) || FNiagaraParameterMapHistory::IsInNamespace(Var, EmitterNamespace))
		{
			Variables.AddUnique(Var);
		}
	}

	Variables.Sort([](const FNiagaraVariable& A, const FNiagaraVariable& B) { return (A.GetName() < B.GetName()); });

	for (FNiagaraVariable& NamespacedVar : Variables)
	{
		FString NamespacedName = NamespacedVar.GetName().ToString();
		MenuBuilder.AddMenuEntry(
			FText::FromString(NamespacedName),
			FText::Format(LOCTEXT("AddButtonTypeEntryToolTipFormatSystem", "Add a reference to {0}"), FText::FromName(NamespacedVar.GetName())),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(InPin, &SNiagaraGraphPinAdd::OnAddType, NamespacedVar)));
	}
	
	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE // NiagaraNodeEmitter
