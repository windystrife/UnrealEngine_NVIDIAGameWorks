// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorUtilities.h"
#include "NiagaraEditorModule.h"
#include "INiagaraEditorTypeUtilities.h"
#include "NiagaraNodeInput.h"
#include "NiagaraDataInterface.h"
#include "NiagaraComponent.h"
#include "ModuleManager.h"
#include "StructOnScope.h"
#include "NiagaraGraph.h"
#include "NiagaraSystem.h"
#include "NiagaraScriptSource.h"
#include "NiagaraScript.h"
#include "NiagaraNodeOutput.h"
#include "EdGraphUtilities.h"
#include "NiagaraConstants.h"
#include "SWidget.h"
#include "STextBlock.h"
#include "SImage.h"
#include "SBoxPanel.h"
#include "HAL/PlatformApplicationMisc.h"
#include "NiagaraEditorStyle.h"
#include "EditorStyleSet.h"
#include "NiagaraSystemViewModel.h"
#include "NiagaraEmitterViewModel.h"
#include "PlatformApplicationMisc.h"

#define LOCTEXT_NAMESPACE "FNiagaraEditorUtilities"

FName FNiagaraEditorUtilities::GetUniqueName(FName CandidateName, const TSet<FName>& ExistingNames)
{
	if (ExistingNames.Contains(CandidateName) == false)
	{
		return CandidateName;
	}

	FString CandidateNameString = CandidateName.ToString();
	FString BaseNameString = CandidateNameString;
	if (CandidateNameString.Len() >= 3 && CandidateNameString.Right(3).IsNumeric())
	{
		BaseNameString = CandidateNameString.Left(CandidateNameString.Len() - 3);
	}

	FName UniqueName = FName(*BaseNameString);
	int32 NameIndex = 1;
	while (ExistingNames.Contains(UniqueName))
	{
		UniqueName = FName(*FString::Printf(TEXT("%s%03i"), *BaseNameString, NameIndex));
		NameIndex++;
	}

	return UniqueName;
}

TSet<FName> FNiagaraEditorUtilities::GetSystemConstantNames()
{
	TSet<FName> SystemConstantNames;
	for (const FNiagaraVariable& SystemConstant : FNiagaraConstants::GetEngineConstants())
	{
		SystemConstantNames.Add(SystemConstant.GetName());
	}
	return SystemConstantNames;
}

void FNiagaraEditorUtilities::GetTypeDefaultValue(const FNiagaraTypeDefinition& Type, TArray<uint8>& DefaultData)
{
	if (const UScriptStruct* ScriptStruct = Type.GetScriptStruct())
	{
		int32 TypeSize = Type.GetSize();
		DefaultData.SetNumUninitialized(TypeSize);

		FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::GetModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
		TSharedPtr<INiagaraEditorTypeUtilities> TypeEditorUtilities = NiagaraEditorModule.GetTypeUtilities(Type);
		if (TypeEditorUtilities.IsValid() && TypeEditorUtilities->CanProvideDefaultValue())
		{
			TSharedRef<FStructOnScope> Struct = MakeShareable(new FStructOnScope(ScriptStruct));
			TypeEditorUtilities->UpdateStructWithDefaultValue(Struct);	
			FMemory::Memcpy(DefaultData.GetData(), Struct->GetStructMemory(), TypeSize);
			
		}
		else
		{
			ScriptStruct->InitializeDefaultValue(DefaultData.GetData());
		}
	}
}

void FNiagaraEditorUtilities::ResetVariableToDefaultValue(FNiagaraVariable& Variable)
{
	if (const UScriptStruct* ScriptStruct = Variable.GetType().GetScriptStruct())
	{
		FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::GetModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
		TSharedPtr<INiagaraEditorTypeUtilities> TypeEditorUtilities = NiagaraEditorModule.GetTypeUtilities(Variable.GetType());
		if (TypeEditorUtilities.IsValid() && TypeEditorUtilities->CanProvideDefaultValue())
		{
			TSharedRef<FStructOnScope> Struct = MakeShareable(new FStructOnScope(ScriptStruct));
			TypeEditorUtilities->UpdateStructWithDefaultValue(Struct);
			Variable.SetData(Struct->GetStructMemory());
		}
		else
		{
			Variable.AllocateData();
			ScriptStruct->InitializeDefaultValue(Variable.GetData());
		}
	}
}

void FNiagaraEditorUtilities::InitializeParameterInputNode(UNiagaraNodeInput& InputNode, const FNiagaraTypeDefinition& Type, const UNiagaraGraph* InGraph, FName InputName)
{
	InputNode.Usage = ENiagaraInputNodeUsage::Parameter;
	InputNode.bCanRenameNode = true;
	InputName = UNiagaraNodeInput::GenerateUniqueName(InGraph, InputName, ENiagaraInputNodeUsage::Parameter);
	InputNode.Input.SetName(InputName);
	InputNode.Input.SetType(Type);
	if (InGraph) // Only compute sort priority if a graph was passed in, similar to the way that GenrateUniqueName works above.
	{
		InputNode.CallSortPriority = UNiagaraNodeInput::GenerateNewSortPriority(InGraph, InputName, ENiagaraInputNodeUsage::Parameter);
	}
	if (Type.GetScriptStruct() != nullptr)
	{
		ResetVariableToDefaultValue(InputNode.Input);
		InputNode.DataInterface = nullptr;
	}
	else
	{
		InputNode.Input.AllocateData(); // Frees previously used memory if we're switching from a struct to a class type.
		InputNode.DataInterface = NewObject<UNiagaraDataInterface>(&InputNode, const_cast<UClass*>(Type.GetClass()), NAME_None, RF_Transactional);
	}
}

void FNiagaraEditorUtilities::GetParameterVariablesFromSystem(UNiagaraSystem& System, TArray<FNiagaraVariable>& ParameterVariables,
	FNiagaraEditorUtilities::FGetParameterVariablesFromSystemOptions Options)
{
	UNiagaraScript* SystemScript = System.GetSystemSpawnScript();
	if (SystemScript != nullptr)
	{
		UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(SystemScript->GetSource());
		if (ScriptSource != nullptr)
		{
			UNiagaraGraph* SystemGraph = ScriptSource->NodeGraph;
			if (SystemGraph != nullptr)
			{
				UNiagaraGraph::FFindInputNodeOptions FindOptions;
				FindOptions.bIncludeAttributes = false;
				FindOptions.bIncludeSystemConstants = false;
				FindOptions.bFilterDuplicates = true;

				TArray<UNiagaraNodeInput*> InputNodes;
				SystemGraph->FindInputNodes(InputNodes, FindOptions);
				for (UNiagaraNodeInput* InputNode : InputNodes)
				{
					bool bIsStructParameter = InputNode->Input.GetType().GetScriptStruct() != nullptr;
					bool bIsDataInterfaceParameter = InputNode->Input.GetType().GetClass() != nullptr;
					if ((bIsStructParameter && Options.bIncludeStructParameters) || (bIsDataInterfaceParameter && Options.bIncludeDataInterfaceParameters))
					{
						ParameterVariables.Add(InputNode->Input);
					}
				}
			}
		}
	}
}

bool FNiagaraEditorUtilities::ConvertToMergedGraph(UNiagaraEmitter* InEmitter)
{
	if (InEmitter->GraphSource == nullptr)
	{
		UNiagaraScriptSource* Source = NewObject<UNiagaraScriptSource>(InEmitter, NAME_None, RF_Transactional);
		if (Source)
		{
			UNiagaraGraph* CreatedGraph = NewObject<UNiagaraGraph>(Source, NAME_None, RF_Transactional);
			Source->NodeGraph = CreatedGraph;

			TArray<UNiagaraGraph*> GraphsToConvert;
			TArray<ENiagaraScriptUsage> GraphUsages;

			int32 yMaxPrevious = 0;

			GraphsToConvert.Add(CastChecked<UNiagaraScriptSource>(InEmitter->SpawnScriptProps.Script->GetSource())->NodeGraph);
			GraphUsages.Add(ENiagaraScriptUsage::ParticleSpawnScript);
			GraphsToConvert.Add(CastChecked<UNiagaraScriptSource>(InEmitter->UpdateScriptProps.Script->GetSource())->NodeGraph);
			GraphUsages.Add(ENiagaraScriptUsage::ParticleUpdateScript);

			for (int32 i = 0; i < InEmitter->EventHandlerScriptProps.Num(); i++)
			{
				if (InEmitter->EventHandlerScriptProps[i].Script != nullptr)
				{
					GraphsToConvert.Add(CastChecked<UNiagaraScriptSource>(InEmitter->EventHandlerScriptProps[i].Script->GetSource())->NodeGraph);
					GraphUsages.Add(ENiagaraScriptUsage::ParticleEventScript);
				}
			}

			for (int32 i = 0; i < GraphsToConvert.Num(); i++)
			{
				UNiagaraGraph* Graph = GraphsToConvert[i];
				ENiagaraScriptUsage GraphUsage = GraphUsages[i];

				TSet<UObject*> NodesToCopy;
				TArray<UNiagaraNode*> SourceNodes;
				Graph->GetNodesOfClass<UNiagaraNode>(SourceNodes);

				int32 HighestY = -INT_MAX;
				int32 EstimatedHeight = 300;
				for (UNiagaraNode* SelectedGraphNode : SourceNodes)
				{
					if (SelectedGraphNode != nullptr)
					{
						if (SelectedGraphNode->NodePosY + EstimatedHeight > HighestY)
						{
							HighestY = SelectedGraphNode->NodePosY + EstimatedHeight;
						}

						if (SelectedGraphNode->CanDuplicateNode())
						{
							SelectedGraphNode->PrepareForCopying();
							NodesToCopy.Add(SelectedGraphNode);
						}
						else
						{
							UE_LOG(LogNiagaraEditor, Error, TEXT("Could not clone node! %s"), *SelectedGraphNode->GetName());
						}
					}
				}

				FString ExportedText;
				FEdGraphUtilities::ExportNodesToText(NodesToCopy, ExportedText);
				FPlatformApplicationMisc::ClipboardCopy(*ExportedText);

				// Grab the text to paste from the clipboard.
				FPlatformApplicationMisc::ClipboardPaste(ExportedText);

				// Import the nodes
				TSet<UEdGraphNode*> PastedNodes;
				FEdGraphUtilities::ImportNodesFromText(CreatedGraph, ExportedText, PastedNodes);


				for (UEdGraphNode* PastedNode : PastedNodes)
				{
					PastedNode->CreateNewGuid();
					PastedNode->NodePosY += yMaxPrevious;

					UNiagaraNodeOutput* Output = Cast<UNiagaraNodeOutput>(PastedNode);
					if (Output != nullptr)
					{
						Output->SetUsage(GraphUsage);
					}
				}

				FixUpPastedInputNodes(CreatedGraph, PastedNodes);
				yMaxPrevious = yMaxPrevious + HighestY;
			}

			InEmitter->GraphSource = Source;
			InEmitter->SpawnScriptProps.Script->SetSource(Source);
			InEmitter->UpdateScriptProps.Script->SetSource(Source);
			for (int32 i = 0; i < InEmitter->EventHandlerScriptProps.Num(); i++)
			{
				if (InEmitter->EventHandlerScriptProps[i].Script != nullptr)
				{
					InEmitter->EventHandlerScriptProps[i].Script->SetSource(Source);
				}
			}

			// Also fix up any dependencies' referenced script type..
			TArray<const UNiagaraGraph*> ReferencedGraphs;
			CreatedGraph->GetAllReferencedGraphs(ReferencedGraphs);
			for (const UNiagaraGraph* Graph : ReferencedGraphs)
			{
				TArray<UNiagaraNodeOutput*> Outputs;
				Graph->FindOutputNodes(Outputs);
				UNiagaraScript* Script = Cast<UNiagaraScript>(Graph->GetOuter());
				if (Script)
				{
					for (UNiagaraNodeOutput* OutputNode : Outputs)
					{
						OutputNode->SetUsage(Script->GetUsage());
					}
				}
			}
			
			// Now make sure that anyone referencing these graphs knows that they are out-of-date.
			Source->MarkNotSynchronized();
			InEmitter->ChangeId = FGuid::NewGuid();

			//FString ErrorMessages;
			//FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::Get().LoadModuleChecked<FNiagaraEditorModule>(TEXT("NiagaraEditor"));
			//NiagaraEditorModule.CompileScript(NewScript, ErrorMessages);
			return true;
		}
	}
	return false;
}


// TODO: This is overly complicated.
void FNiagaraEditorUtilities::FixUpPastedInputNodes(UEdGraph* Graph, TSet<UEdGraphNode*> PastedNodes)
{
	// Collect existing inputs.
	TArray<UNiagaraNodeInput*> CurrentInputs;
	Graph->GetNodesOfClass<UNiagaraNodeInput>(CurrentInputs);
	TSet<FNiagaraVariable> ExistingInputs;
	TMap<FNiagaraVariable, UNiagaraNodeInput*> ExistingNodes;
	int32 HighestSortOrder = -1; // Set to -1 initially, so that in the event of no nodes, we still get zero.
	for (UNiagaraNodeInput* CurrentInput : CurrentInputs)
	{
		if (PastedNodes.Contains(CurrentInput) == false && CurrentInput->Usage == ENiagaraInputNodeUsage::Parameter)
		{
			ExistingInputs.Add(CurrentInput->Input);
			ExistingNodes.Add(CurrentInput->Input) = CurrentInput;
			if (CurrentInput->CallSortPriority > HighestSortOrder)
			{
				HighestSortOrder = CurrentInput->CallSortPriority;
			}
		}
	}

	// Collate pasted inputs nodes by their input for further processing.
	TMap<FNiagaraVariable, TArray<UNiagaraNodeInput*>> InputToPastedInputNodes;
	for (UEdGraphNode* PastedNode : PastedNodes)
	{
		UNiagaraNodeInput* PastedInputNode = Cast<UNiagaraNodeInput>(PastedNode);
		if (PastedInputNode != nullptr && PastedInputNode->Usage == ENiagaraInputNodeUsage::Parameter && ExistingInputs.Contains(PastedInputNode->Input) == false)
		{
			TArray<UNiagaraNodeInput*>* NodesForInput = InputToPastedInputNodes.Find(PastedInputNode->Input);
			if (NodesForInput == nullptr)
			{
				NodesForInput = &InputToPastedInputNodes.Add(PastedInputNode->Input);
			}
			NodesForInput->Add(PastedInputNode);
		}
	}

	// Fix up the nodes based on their relationship to the existing inputs.
	for (auto PastedPairIterator = InputToPastedInputNodes.CreateIterator(); PastedPairIterator; ++PastedPairIterator)
	{
		FNiagaraVariable PastedInput = PastedPairIterator.Key();
		TArray<UNiagaraNodeInput*>& PastedNodesForInput = PastedPairIterator.Value();

		// Try to find an existing input which matches the pasted input by both name and type so that the pasted nodes
		// can be assigned the same id and value, to facilitate pasting multiple times from the same source graph.
		FNiagaraVariable* MatchingInputByNameAndType = nullptr;
		UNiagaraNodeInput* MatchingNode = nullptr;
		for (FNiagaraVariable& ExistingInput : ExistingInputs)
		{
			if (PastedInput.GetName() == ExistingInput.GetName() && PastedInput.GetType() == ExistingInput.GetType())
			{
				MatchingInputByNameAndType = &ExistingInput;
				UNiagaraNodeInput** FoundNode = ExistingNodes.Find(ExistingInput);
				if (FoundNode != nullptr)
				{
					MatchingNode = *FoundNode;
				}
				break;
			}
		}

		if (MatchingInputByNameAndType != nullptr && MatchingNode != nullptr)
		{
			// Update the id and value on the matching pasted nodes.
			for (UNiagaraNodeInput* PastedNodeForInput : PastedNodesForInput)
			{
				if (nullptr == PastedNodeForInput)
				{
					continue;
				}
				PastedNodeForInput->CallSortPriority = MatchingNode->CallSortPriority;
				PastedNodeForInput->ExposureOptions = MatchingNode->ExposureOptions;
				PastedNodeForInput->Input.AllocateData();
				PastedNodeForInput->Input.SetData(MatchingInputByNameAndType->GetData());
			}
		}
		else
		{
			// Check for duplicate names
			TSet<FName> ExistingNames;
			for (FNiagaraVariable& ExistingInput : ExistingInputs)
			{
				ExistingNames.Add(ExistingInput.GetName());
			}
			if (ExistingNames.Contains(PastedInput.GetName()))
			{
				FName UniqueName = FNiagaraEditorUtilities::GetUniqueName(PastedInput.GetName(), ExistingNames.Union(FNiagaraEditorUtilities::GetSystemConstantNames()));
				for (UNiagaraNodeInput* PastedNodeForInput : PastedNodesForInput)
				{
					PastedNodeForInput->Input.SetName(UniqueName);
				}
			}

			// Assign the pasted inputs the same new id and add them to the end of the parameters list.
			int32 NewSortOrder = ++HighestSortOrder;
			for (UNiagaraNodeInput* PastedNodeForInput : PastedNodesForInput)
			{
				PastedNodeForInput->CallSortPriority = NewSortOrder;
			}
		}
	}
}

FText FNiagaraEditorUtilities::StatusToText(ENiagaraScriptCompileStatus Status)
{
	switch (Status)
	{
	default:
	case ENiagaraScriptCompileStatus::NCS_Unknown:
		return LOCTEXT("Recompile_Status", "Unknown status; should recompile");
	case ENiagaraScriptCompileStatus::NCS_Dirty:
		return LOCTEXT("Dirty_Status", "Dirty; needs to be recompiled");
	case ENiagaraScriptCompileStatus::NCS_Error:
		return LOCTEXT("CompileError_Status", "There was an error during compilation, see the log for details");
	case ENiagaraScriptCompileStatus::NCS_UpToDate:
		return LOCTEXT("GoodToGo_Status", "Good to go");
	case ENiagaraScriptCompileStatus::NCS_UpToDateWithWarnings:
		return LOCTEXT("GoodToGoWarning_Status", "There was a warning during compilation, see the log for details");
	}
}

ENiagaraScriptCompileStatus FNiagaraEditorUtilities::UnionCompileStatus(const ENiagaraScriptCompileStatus& StatusA, const ENiagaraScriptCompileStatus& StatusB)
{
	if (StatusA != StatusB)
	{
		if (StatusA == ENiagaraScriptCompileStatus::NCS_Unknown || StatusB == ENiagaraScriptCompileStatus::NCS_Unknown)
		{
			return ENiagaraScriptCompileStatus::NCS_Unknown;
		}
		else if (StatusA >= ENiagaraScriptCompileStatus::NCS_MAX || StatusB >= ENiagaraScriptCompileStatus::NCS_MAX)
		{
			return ENiagaraScriptCompileStatus::NCS_MAX;
		}
		else if (StatusA == ENiagaraScriptCompileStatus::NCS_Dirty || StatusB == ENiagaraScriptCompileStatus::NCS_Dirty)
		{
			return ENiagaraScriptCompileStatus::NCS_Dirty;
		}
		else if (StatusA == ENiagaraScriptCompileStatus::NCS_Error || StatusB == ENiagaraScriptCompileStatus::NCS_Error)
		{
			return ENiagaraScriptCompileStatus::NCS_Error;
		}
		else if (StatusA == ENiagaraScriptCompileStatus::NCS_UpToDateWithWarnings || StatusB == ENiagaraScriptCompileStatus::NCS_UpToDateWithWarnings)
		{
			return ENiagaraScriptCompileStatus::NCS_UpToDateWithWarnings;
		}
		else if (StatusA == ENiagaraScriptCompileStatus::NCS_BeingCreated || StatusB == ENiagaraScriptCompileStatus::NCS_BeingCreated)
		{
			return ENiagaraScriptCompileStatus::NCS_BeingCreated;
		}
		else if (StatusA == ENiagaraScriptCompileStatus::NCS_UpToDate || StatusB == ENiagaraScriptCompileStatus::NCS_UpToDate)
		{
			return ENiagaraScriptCompileStatus::NCS_UpToDate;
		}
		else
		{
			return ENiagaraScriptCompileStatus::NCS_Unknown;
		}
	}
	else
	{
		return StatusA;
	}
}

bool FNiagaraEditorUtilities::DataMatches(const FNiagaraVariable& Variable, const FStructOnScope& StructOnScope)
{
	if (Variable.GetType().GetScriptStruct() != StructOnScope.GetStruct() ||
		Variable.IsDataAllocated() == false)
	{
		return false;
	}

	const uint8* VariableMemory = Variable.GetData();
	const uint8* StructOnScopeMemory = StructOnScope.GetStructMemory();
	int32 Size = Variable.GetSizeInBytes();
	for (int32 i = 0; i < Size; i++)
	{
		if (VariableMemory[i] != StructOnScopeMemory[i])
		{
			return false;
		}
	}

	return true;
}

bool FNiagaraEditorUtilities::DataMatches(const FStructOnScope& StructOnScopeA, const FStructOnScope& StructOnScopeB)
{
	if (StructOnScopeA.GetStruct() != StructOnScopeB.GetStruct())
	{
		return false;
	}

	const uint8* StructOnScopeAMemory = StructOnScopeA.GetStructMemory();
	const uint8* StructOnScopeBMemory = StructOnScopeB.GetStructMemory();
	int32 Size = StructOnScopeA.GetStruct()->GetStructureSize();
	for (int32 i = 0; i < Size; i++)
	{
		if (StructOnScopeAMemory[i] != StructOnScopeBMemory[i])
		{
			return false;
		}
	}

	return true;
}

TSharedPtr<SWidget> FNiagaraEditorUtilities::CreateInlineErrorText(TAttribute<FText> ErrorMessage, TAttribute<FText> ErrorTooltip)
{
	TSharedPtr<SHorizontalBox> ErrorInternalBox = SNew(SHorizontalBox);
		ErrorInternalBox->AddSlot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterText")
				.Text(ErrorMessage)
			];

		return SNew(SHorizontalBox)
			.ToolTipText(ErrorTooltip)
			+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("Icons.Error"))
				]
			+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					ErrorInternalBox.ToSharedRef()
				];
}

void FNiagaraEditorUtilities::UpdateExistingEmitters(const TArray<UNiagaraEmitter*>& AffectedEmitters)
{
	// Compile the existing emitters. Also determine which Systems need to be properly updated.
	TArray<UNiagaraSystem*> AffectedSystemSystems;
	for (UNiagaraEmitter* Emitter : AffectedEmitters)
	{
		if (Emitter->IsPendingKillOrUnreachable())
		{
			continue;
		}

		TSharedPtr<FNiagaraEmitterViewModel> EmitterViewModel = FNiagaraEmitterViewModel::GetExistingViewModelForObject(Emitter);
		if (!EmitterViewModel.IsValid())
		{
			EmitterViewModel = MakeShareable(new FNiagaraEmitterViewModel(Emitter, nullptr));
		}
		EmitterViewModel->CompileScripts();

		for (TObjectIterator<UNiagaraSystem> It; It; ++It)
		{
			if (It->GetAutoImportChangedEmitters() && It->ReferencesSourceEmitter(Emitter))
			{
				AffectedSystemSystems.AddUnique(*It);
			}
		}
	}

	// Now iterate over the affected Systems.
	for (UNiagaraSystem* System : AffectedSystemSystems)
	{
		TSharedPtr<FNiagaraSystemViewModel> SystemViewModel = FNiagaraSystemViewModel::GetExistingViewModelForObject(System);
		if (!SystemViewModel.IsValid())
		{
			FNiagaraSystemViewModelOptions Options;
			Options.bCanRemoveEmittersFromTimeline = false;
			Options.bCanRenameEmittersFromTimeline = false;
			Options.bCanAddEmittersFromTimeline = false;
			Options.bUseSystemExecStateForTimelineReset = false;
			SystemViewModel = MakeShareable(new FNiagaraSystemViewModel(*System, Options));
		}

		SystemViewModel->ResynchronizeAllHandles();
	}
}


#undef LOCTEXT_NAMESPACE
