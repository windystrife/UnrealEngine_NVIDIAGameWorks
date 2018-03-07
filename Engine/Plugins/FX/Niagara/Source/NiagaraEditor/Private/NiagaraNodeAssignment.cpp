// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraNodeAssignment.h"
#include "UObject/UnrealType.h"
#include "NiagaraGraph.h"
#include "NiagaraScriptSource.h"
#include "NiagaraScript.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "EdGraphSchema_Niagara.h"
#include "ModuleManager.h"
#include "AssetRegistryModule.h"
#include "NiagaraComponent.h"
#include "NiagaraHlslTranslator.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraNodeParameterMapGet.h"
#include "NiagaraNodeParameterMapSet.h"
#include "NiagaraConstants.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"

#define LOCTEXT_NAMESPACE "NiagaraNodeAssigment"

void UNiagaraNodeAssignment::AllocateDefaultPins()
{
	GenerateScript();
	Super::AllocateDefaultPins();
}

FText UNiagaraNodeAssignment::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::Format(LOCTEXT("NodeTitle", "Set {0}"), FText::FromName(AssignmentTarget.GetName()));
}

bool UNiagaraNodeAssignment::RefreshFromExternalChanges()
{
	FunctionScript = nullptr;
	GenerateScript();
	ReallocatePins();
	return true;
}

void UNiagaraNodeAssignment::GenerateScript()
{
	FString NodeName = TEXT("Set_") + AssignmentTarget.GetName().ToString();
	NodeName = NodeName.Replace(TEXT("."), TEXT("_"));

	if (FunctionScript == nullptr)
	{
		FunctionScript = NewObject<UNiagaraScript>(this, FName(*NodeName), RF_Transactional);
		FunctionScript->SetUsage(ENiagaraScriptUsage::Module);
		InitializeScript(FunctionScript);
		ComputeNodeName();
	}
	if (FunctionScript->GetName() != NodeName)
	{
		FunctionScript->Rename(*NodeName);
		ComputeNodeName();
	}
}

void UNiagaraNodeAssignment::InitializeScript(UNiagaraScript* NewScript)
{
	if (NewScript != NULL)
	{		
		UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(NewScript->GetSource());

		if (nullptr == Source)
		{
			Source = NewObject<UNiagaraScriptSource>(NewScript, NAME_None, RF_Transactional);
			NewScript->SetSource(Source);
		}

		UNiagaraGraph* CreatedGraph = Source->NodeGraph;
		if (nullptr == CreatedGraph)
		{
			CreatedGraph = NewObject<UNiagaraGraph>(Source, NAME_None, RF_Transactional);
			Source->NodeGraph = CreatedGraph;
		}
		
		TArray<UNiagaraNodeInput*> InputNodes;
		CreatedGraph->FindInputNodes(InputNodes);
		if (InputNodes.Num() == 0)
		{
			FGraphNodeCreator<UNiagaraNodeInput> InputNodeCreator(*CreatedGraph);
			UNiagaraNodeInput* InputNode = InputNodeCreator.CreateNode();
			InputNode->Input = FNiagaraVariable(FNiagaraTypeDefinition::GetParameterMapDef(), TEXT("InputMap"));
			InputNode->Usage = ENiagaraInputNodeUsage::Parameter;
			InputNodeCreator.Finalize();
			InputNodes.Add(InputNode);
		}

		UNiagaraNodeOutput* OutputNode = CreatedGraph->FindOutputNode(ENiagaraScriptUsage::Module);
		if (OutputNode == nullptr)
		{
			FGraphNodeCreator<UNiagaraNodeOutput> OutputNodeCreator(*CreatedGraph);
			OutputNode = OutputNodeCreator.CreateNode();
			FNiagaraVariable ParamMapAttrib(FNiagaraTypeDefinition::GetParameterMapDef(), TEXT("OutputMap"));
			OutputNode->SetUsage(ENiagaraScriptUsage::Module);
			OutputNode->Outputs.Add(ParamMapAttrib);
			OutputNodeCreator.Finalize();
		}

		TArray<UNiagaraNodeParameterMapGet*> GetNodes;
		CreatedGraph->GetNodesOfClass(GetNodes);

		TArray<UNiagaraNodeParameterMapSet*> SetNodes;
		CreatedGraph->GetNodesOfClass(SetNodes);

		if (SetNodes.Num() == 0)
		{
			FGraphNodeCreator<UNiagaraNodeParameterMapSet> InputNodeCreator(*CreatedGraph);
			UNiagaraNodeParameterMapSet* InputNode = InputNodeCreator.CreateNode();
			InputNodeCreator.Finalize();
			SetNodes.Add(InputNode);

			InputNodes[0]->GetOutputPin(0)->MakeLinkTo(SetNodes[0]->GetInputPin(0));
			SetNodes[0]->GetOutputPin(0)->MakeLinkTo(OutputNode->GetInputPin(0));
		}

		// We create two get nodes. The first is for the direct values.
		// The second is in the case of referencing other parameters that we want to use as defaults.
		if (GetNodes.Num() == 0)
		{
			FGraphNodeCreator<UNiagaraNodeParameterMapGet> InputNodeCreator(*CreatedGraph);
			UNiagaraNodeParameterMapGet* InputNode = InputNodeCreator.CreateNode();
			InputNodeCreator.Finalize();
			GetNodes.Add(InputNode);

			InputNodes[0]->GetOutputPin(0)->MakeLinkTo(GetNodes[0]->GetInputPin(0));
		}
		if (GetNodes.Num() == 1)
		{
			FGraphNodeCreator<UNiagaraNodeParameterMapGet> InputNodeCreator(*CreatedGraph);
			UNiagaraNodeParameterMapGet* InputNode = InputNodeCreator.CreateNode();
			InputNodeCreator.Finalize();
			GetNodes.Add(InputNode);

			InputNodes[0]->GetOutputPin(0)->MakeLinkTo(GetNodes[1]->GetInputPin(0));
		}

		// Clean out existing pins
		while (!SetNodes[0]->IsAddPin(SetNodes[0]->GetInputPin(1)))
		{
			SetNodes[0]->RemovePin(SetNodes[0]->GetInputPin(1));
		}

		while (!GetNodes[0]->IsAddPin(GetNodes[0]->GetOutputPin(0)))
		{
			GetNodes[0]->RemovePin(GetNodes[0]->GetInputPin(0));
		}

		while (!GetNodes[1]->IsAddPin(GetNodes[1]->GetOutputPin(0)))
		{
			GetNodes[1]->RemovePin(GetNodes[1]->GetInputPin(0));
		}

		// Now create the proper new pins and connect them.
		if (AssignmentTarget.GetName() != NAME_None)
		{
			FNiagaraParameterHandle TargetHandle(AssignmentTarget.GetName().ToString());
			UEdGraphPin* SetPin = SetNodes[0]->RequestNewTypedPin(EGPD_Input, AssignmentTarget.GetType(), AssignmentTarget.GetName().ToString());
			UEdGraphPin* GetPin = GetNodes[0]->RequestNewTypedPin(EGPD_Output, AssignmentTarget.GetType(), TEXT("Module.") + TargetHandle.GetName());

			GetPin->MakeLinkTo(SetPin);

			if (!AssignmentDefaultValue.IsEmpty())
			{
				UEdGraphPin* DefaultInputPin = GetNodes[0]->GetDefaultPin(GetPin);

				bool isEngineConstant = false;
				FNiagaraVariable SeekVar = FNiagaraVariable(AssignmentTarget.GetType(), FName(*AssignmentDefaultValue));
				const FNiagaraVariable* FoundVar = FNiagaraConstants::FindEngineConstant(SeekVar);
				if (FoundVar != nullptr)
				{
					UEdGraphPin* DefaultGetPin = GetNodes[1]->RequestNewTypedPin(EGPD_Output, AssignmentTarget.GetType(), FoundVar->GetName().ToString());
					DefaultGetPin->MakeLinkTo(DefaultInputPin);
				}
				else
				{
					DefaultInputPin->bDefaultValueIsIgnored = false;
					DefaultInputPin->DefaultValue = AssignmentDefaultValue;
				}
			}
		}


	}
}

#undef LOCTEXT_NAMESPACE
