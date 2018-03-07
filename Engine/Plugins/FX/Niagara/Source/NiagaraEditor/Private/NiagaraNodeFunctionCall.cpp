// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraNodeFunctionCall.h"
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
#include "NiagaraConstants.h"

#define LOCTEXT_NAMESPACE "NiagaraNodeFunctionCall"

void UNiagaraNodeFunctionCall::PostLoad()
{
	Super::PostLoad();

	if (FunctionScript)
	{
		FunctionScript->ConditionalPostLoad();

		// We need to make sure that the variables that could potentially be used in AllocateDefaultPins have been properly
		// loaded. Otherwise, we could be out of date.
		if (FunctionScript->GetSource())
		{
			UNiagaraScriptSource* Source = CastChecked<UNiagaraScriptSource>(FunctionScript->GetSource());
			Source->ConditionalPostLoad();
			UNiagaraGraph* Graph = Source->NodeGraph;
			Graph->ConditionalPostLoad();
		}
	}

	if (FunctionDisplayName.IsEmpty())
	{
		ComputeNodeName();
	}
}

void UNiagaraNodeFunctionCall::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property != nullptr)
	{
		ReallocatePins();
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);

	GetGraph()->NotifyGraphChanged();
}

void UNiagaraNodeFunctionCall::AllocateDefaultPins()
{
	if (FunctionScriptAssetObjectPath != NAME_None && FunctionScript == nullptr)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		FAssetData ScriptAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FunctionScriptAssetObjectPath);
		if (ScriptAssetData.IsValid())
		{
			FunctionScript = Cast<UNiagaraScript>(ScriptAssetData.GetAsset());
		}
	}

	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());
	UNiagaraGraph* CallerGraph = GetNiagaraGraph();
	if (FunctionScript)
	{
		UNiagaraScriptSource* Source = CastChecked<UNiagaraScriptSource>(FunctionScript->GetSource());
		UNiagaraGraph* Graph = Source->NodeGraph;

		//These pins must be refreshed and kept in the correct order for the function
		TArray<FNiagaraVariable> Inputs;
		TArray<FNiagaraVariable> Outputs;
		Graph->GetParameters(Inputs, Outputs);

		TArray<UNiagaraNodeInput*> InputNodes;
		UNiagaraGraph::FFindInputNodeOptions Options;
		Options.bSort = true;
		Options.bFilterDuplicates = true;
		Graph->FindInputNodes(InputNodes, Options);

		bool bHasAdvancePins = false;
		for (UNiagaraNodeInput* InputNode : InputNodes)
		{
			if (InputNode->IsExposed())
			{
				UEdGraphPin* NewPin = CreatePin(EGPD_Input, Schema->TypeDefinitionToPinType(InputNode->Input.GetType()), InputNode->Input.GetName().ToString());
					
				//An inline pin default only makes sense if we are required. 
				//Non exposed or optional inputs will used their own function input nodes defaults when not directly provided by a link.
				//Special class types cannot have an inline default.
				NewPin->bDefaultValueIsIgnored = !(InputNode->IsRequired() && InputNode->Input.GetType().GetClass() == nullptr);

				//TODO: Some visual indication of Auto bound pins.
				//I tried just linking to null but
// 				FNiagaraVariable AutoBoundVar;
// 				ENiagaraInputNodeUsage AutBoundUsage = ENiagaraInputNodeUsage::Undefined;
// 				bool bCanAutoBind = FindAutoBoundInput(InputNode->AutoBindOptions, NewPin, AutoBoundVar, AutBoundUsage);
// 				if (bCanAutoBind)
// 				{
// 
// 				}

				if (InputNode->IsHidden())
				{
					NewPin->bAdvancedView = true;
					bHasAdvancePins = true;
				}
				else
				{
					NewPin->bAdvancedView = false;
				}
			}
		}

		AdvancedPinDisplay = bHasAdvancePins ? ENodeAdvancedPins::Hidden : ENodeAdvancedPins::NoPins;

		for (FNiagaraVariable& Output : Outputs)
		{
			UEdGraphPin* NewPin = CreatePin(EGPD_Output, Schema->TypeDefinitionToPinType(Output.GetType()), Output.GetName().ToString());
			NewPin->bDefaultValueIsIgnored = true;
		}

		// Make sure to note that we've synchronized with the external version.
		CachedChangeId = FunctionScript->GetChangeID();
	}
	else
	{
		
		for (FNiagaraVariable& Input : Signature.Inputs)
		{
			UEdGraphPin* NewPin = CreatePin(EGPD_Input, Schema->TypeDefinitionToPinType(Input.GetType()), Input.GetName().ToString());
			NewPin->bDefaultValueIsIgnored = false;
		}

		for (FNiagaraVariable& Output : Signature.Outputs)
		{
			UEdGraphPin* NewPin = CreatePin(EGPD_Output, Schema->TypeDefinitionToPinType(Output.GetType()), Output.GetName().ToString());
			NewPin->bDefaultValueIsIgnored = true;
		}

		// We don't reference an external function, so set an invalid id.
		CachedChangeId = FGuid();
	}

	if (FunctionDisplayName.IsEmpty())
	{
		ComputeNodeName();
	}
}

FText UNiagaraNodeFunctionCall::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FString DetectedName = FunctionScript ? FunctionScript->GetName() : Signature.GetName();
	if (DetectedName.IsEmpty())
	{
		return FText::FromString(TEXT("Missing ( Was\"") + FunctionDisplayName + TEXT("\")"));
	}
	else
	{
		return FText::FromString(FName::NameToDisplayString(FunctionDisplayName, false));
	}
}

FText UNiagaraNodeFunctionCall::GetTooltipText()const
{
	if (FunctionScript != nullptr)
	{
		return FunctionScript->GetDescription();
	}
	else if (Signature.IsValid())
	{
		return Signature.Description;
	} 
	else
	{
		return LOCTEXT("NiagaraFuncCallUnknownSignatureTooltip", "Unknown function call");
	}
}

FLinearColor UNiagaraNodeFunctionCall::GetNodeTitleColor() const
{
	return UEdGraphSchema_Niagara::NodeTitleColor_FunctionCall;
}

bool UNiagaraNodeFunctionCall::CanAddToGraph(UNiagaraGraph* TargetGraph, FString& OutErrorMsg) const
{
	if (Super::CanAddToGraph(TargetGraph, OutErrorMsg) == false)
	{
		return false;
	}
	UPackage* TargetPackage = TargetGraph->GetOutermost();

	TArray<const UNiagaraGraph*> FunctionGraphs;
	UNiagaraScript* SpawningFunctionScript = FunctionScript;
	
	// We probably haven't loaded the script yet. Let's do so now so that we can trace its lineage.
	if (FunctionScriptAssetObjectPath != NAME_None && FunctionScript == nullptr)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		FAssetData ScriptAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FunctionScriptAssetObjectPath);
		if (ScriptAssetData.IsValid())
		{
			SpawningFunctionScript = Cast<UNiagaraScript>(ScriptAssetData.GetAsset());
		}
	}

	// Now we need to get the graphs referenced by the script that we are about to spawn in.
	if (SpawningFunctionScript && SpawningFunctionScript->GetSource())
	{
		UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(SpawningFunctionScript->GetSource());
		if (Source)
		{
			UNiagaraGraph* FunctionGraph = Source->NodeGraph;
			if (FunctionGraph)
			{
				FunctionGraph->GetAllReferencedGraphs(FunctionGraphs);
			}
		}
	}

	// Iterate over each graph referenced by this spawning function call and see if any of them reference the graph that we are about to be spawned into. If 
	// a match is found, then adding us would introduce a cycle and we need to abort the add.
	for (const UNiagaraGraph* Graph : FunctionGraphs)
	{
		UPackage* FunctionPackage = Graph->GetOutermost();
		if (FunctionPackage != nullptr && TargetPackage != nullptr && FunctionPackage == TargetPackage)
		{
			OutErrorMsg = LOCTEXT("NiagaraFuncCallCannotAddToGraph", "Cannot add to graph because the Function Call used by this node would lead to a cycle.").ToString();
			return false;
		}
	}

	return true;
}

void UNiagaraNodeFunctionCall::Compile(class FHlslNiagaraTranslator* Translator, TArray<int32>& Outputs)
{
	TArray<int32> Inputs;

	bool bError = false;

	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());
	UNiagaraGraph* CallerGraph = GetNiagaraGraph();
	if (FunctionScript)
	{
		TArray<UEdGraphPin*> CallerInputPins;
		GetInputPins(CallerInputPins);

		UNiagaraScriptSource* Source = CastChecked<UNiagaraScriptSource>(FunctionScript->GetSource());
		UNiagaraGraph* FunctionGraph = Source->NodeGraph;

		TArray<UNiagaraNodeInput*> FunctionInputNodes;
		UNiagaraGraph::FFindInputNodeOptions Options;
		Options.bSort = true;
		Options.bFilterDuplicates = true;
		FunctionGraph->FindInputNodes(FunctionInputNodes, Options);

		for (UNiagaraNodeInput* FunctionInputNode : FunctionInputNodes)
		{
			//Finds the matching Pin in the caller.
			UEdGraphPin** PinPtr = CallerInputPins.FindByPredicate([&](UEdGraphPin* InPin) { return Schema->PinToNiagaraVariable(InPin).IsEquivalent(FunctionInputNode->Input); });
			if (!PinPtr)
			{
				if (FunctionInputNode->IsExposed())
				{
					//Couldn't find the matching pin for an exposed input. Probably a stale function call node that needs to be refreshed.
					Translator->Error(LOCTEXT("StaleFunctionCallError", "Function call is stale and needs to be refreshed."), this, nullptr);
					bError = true;
				}
				else if (FunctionInputNode->ExposureOptions.bRequired == true)
				{
					// Not exposed, but required. This means we should just add as a constant.
					Inputs.Add(Translator->GetConstant(FunctionInputNode->Input));
					continue;
				}


				Inputs.Add(INDEX_NONE);
				continue;
			}

			UEdGraphPin* CallerPin = *PinPtr;
			UEdGraphPin* CallerLinkedTo = CallerPin->LinkedTo.Num() > 0 ? CallerPin->LinkedTo[0] : nullptr;			
			FNiagaraVariable PinVar = Schema->PinToNiagaraVariable(CallerPin);
			if (!CallerLinkedTo)
			{
				//if (Translator->CanReadAttributes())
				{
					//Try to auto bind if we're not linked to by the caller.
					FNiagaraVariable AutoBoundVar;
					ENiagaraInputNodeUsage AutBoundUsage = ENiagaraInputNodeUsage::Undefined;
					if (FindAutoBoundInput(FunctionInputNode, CallerPin, AutoBoundVar, AutBoundUsage))
					{
						UNiagaraNodeInput* NewNode = NewObject<UNiagaraNodeInput>(CallerGraph);
						NewNode->Input = PinVar;
						NewNode->Usage = AutBoundUsage;
						NewNode->AllocateDefaultPins();
						CallerLinkedTo = NewNode->GetOutputPin(0);
						CallerPin->BreakAllPinLinks();
						CallerPin->MakeLinkTo(CallerLinkedTo);
					}
				}
			}

			if (CallerLinkedTo)
			{
				//Param is provided by the caller. Typical case.
				Inputs.Add(Translator->CompilePin(CallerPin));
				continue;
			}
			else
			{
				if (FunctionInputNode->IsRequired())
				{
					if (CallerPin->bDefaultValueIsIgnored)
					{
						//This pin can't use a default and it is required so flag an error.
						Translator->Error(FText::Format(LOCTEXT("RequiredInputUnboundErrorFmt", "Required input {0} was not bound and could not be automatically bound."), CallerPin->GetDisplayName()),
							this, CallerPin);
						bError = true;
						//We weren't linked to anything and we couldn't auto bind so tell the compiler this input isn't provided and it should use it's local default.
						Inputs.Add(INDEX_NONE);
					}
					else
					{
						//We also compile the pin anyway if it is required as we'll be attempting to use it's inline default.
						Inputs.Add(Translator->CompilePin(CallerPin));
					}
				}
				else
				{
					//We optional, weren't linked to anything and we couldn't auto bind so tell the compiler this input isn't provided and it should use it's local default.
					Inputs.Add(INDEX_NONE);
				}
			}
		}
	}
	else
	{
		bError = CompileInputPins(Translator, Inputs);
	}		

	if (!bError)
	{
		Translator->FunctionCall(this, Inputs, Outputs);
	}
}

UObject*  UNiagaraNodeFunctionCall::GetReferencedAsset() const
{
	if (FunctionScript && FunctionScript->GetOutermost() != GetOutermost())
	{
		return FunctionScript;
	}
	else
	{
		return nullptr;
	}
}

bool UNiagaraNodeFunctionCall::RefreshFromExternalChanges()
{
	bool bReload = false;
	if (FunctionScript)
	{
		bReload = CachedChangeId != FunctionScript->GetChangeID();
		if (bReload)
		{
			bReload = true;
			check(FunctionScript->GetOutermost()->HasAnyFlags(RF_WasLoaded));
			UE_LOG(LogNiagaraEditor, Log, TEXT("RefreshFromExternalChanges %s"), *(FunctionScript->GetPathName()));
		}
	}
	else
	{
		if (Signature.IsValid())
		{
			bReload = true;
		}
	}

	if (bReload)
	{
		// TODO - Leverage code in reallocate pins to determine if any pins have changed...
		ReallocatePins();
		return true;
	}
	else
	{
		return false;
	}
}

void UNiagaraNodeFunctionCall::SubsumeExternalDependencies(TMap<const UObject*, UObject*>& ExistingConversions)
{
	if (FunctionScript && FunctionScript->GetOutermost() != this->GetOutermost())
	{
		if (ExistingConversions.Contains(FunctionScript))
		{
			FunctionScript = CastChecked<UNiagaraScript>(ExistingConversions[FunctionScript]);
			check(FunctionScript->HasAnyFlags(RF_Standalone) == false);
			check(FunctionScript->HasAnyFlags(RF_Public) == false);
		}
		else
		{
			FunctionScript = FunctionScript->MakeRecursiveDeepCopy(this, ExistingConversions);
		}
	}
}

void UNiagaraNodeFunctionCall::BuildParameterMapHistory(FNiagaraParameterMapHistoryBuilder& OutHistory, bool bRecursive)
{
	Super::BuildParameterMapHistory(OutHistory, bRecursive);

	if (FunctionScript)
	{
		UNiagaraScriptSource* Source = CastChecked<UNiagaraScriptSource>(FunctionScript->GetSource());
		UNiagaraGraph* FunctionGraph = CastChecked<UNiagaraGraph>(Source->NodeGraph);
		
		UNiagaraNodeOutput* OutputNode = FunctionGraph->FindOutputNode(ENiagaraScriptUsage::Function);
		if (OutputNode == nullptr)
		{
			OutputNode = FunctionGraph->FindOutputNode(ENiagaraScriptUsage::Module);
		}
		if (OutputNode == nullptr)
		{
			OutputNode = FunctionGraph->FindOutputNode(ENiagaraScriptUsage::DynamicInput);
		}
		
		OutHistory.EnterFunction(GetFunctionName(), FunctionScript, this);
		OutputNode->BuildParameterMapHistory(OutHistory, true);

		// Since we're about to lose the pin calling context, we finish up the function call parameter map pin wiring
		// here when we have the calling context and the child context still available to us...
		TArray<UEdGraphPin*> OutputPins;
		GetOutputPins(OutputPins);

		TArray<TPair<UEdGraphPin*, int32> > MatchedPairs;

		const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());

		// Find the matches of names and types of the sub-graph output pins and this function call nodes' outputs.
		for (UEdGraphPin* ChildOutputNodePin : OutputNode->GetAllPins())
		{
			FNiagaraVariable VarChild = Schema->PinToNiagaraVariable(ChildOutputNodePin);
			
			if (ChildOutputNodePin->LinkedTo.Num() > 0 && VarChild.GetType() == FNiagaraTypeDefinition::GetParameterMapDef())
			{
				for (int32 i = 0; i < OutputPins.Num(); i++)
				{
					FNiagaraVariable OutputVar = Schema->PinToNiagaraVariable(OutputPins[i]);
					if (OutputVar.IsEquivalent(VarChild))
					{
						TPair<UEdGraphPin*, int32> Pair;
						Pair.Key = OutputPins[i];
						Pair.Value = OutHistory.TraceParameterMapOutputPin(ChildOutputNodePin->LinkedTo[0]);
						MatchedPairs.Add(Pair);
					}
				}
			}
		}
		OutHistory.ExitFunction(GetFunctionName(), FunctionScript, this);

		for (int32 i = 0; i < MatchedPairs.Num(); i++)
		{
			OutHistory.RegisterParameterMapPin(MatchedPairs[i].Value, MatchedPairs[i].Key);
		}
	}
}


UEdGraphPin* UNiagaraNodeFunctionCall::FindParameterMapDefaultValuePin(const FString& VariableName)
{
	if (FunctionScript)
	{
		UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(FunctionScript->GetSource());
		if (ScriptSource != nullptr)
		{
			return ScriptSource->NodeGraph->FindParameterMapDefaultValuePin(VariableName);
		}
	}
	return nullptr;
}

ENiagaraNumericOutputTypeSelectionMode UNiagaraNodeFunctionCall::GetNumericOutputTypeSelectionMode() const
{
	if (FunctionScript)
	{
		return FunctionScript->NumericOutputTypeSelectionMode;
	}	
	return ENiagaraNumericOutputTypeSelectionMode::None;
}

void UNiagaraNodeFunctionCall::AutowireNewNode(UEdGraphPin* FromPin)
{
	UNiagaraNode::AutowireNewNode(FromPin);
	ComputeNodeName();
}

void UNiagaraNodeFunctionCall::ComputeNodeName()
{
	const FString CurrentName = FunctionDisplayName;
	FName ProposedName = *CurrentName;
	FString FunctionName = FunctionScript ? FunctionScript->GetName() : Signature.GetName();
	if (!FunctionName.IsEmpty())
	{
		ProposedName = *FunctionName;
	}

	UNiagaraGraph* Graph = GetNiagaraGraph();
	TArray<UNiagaraNodeFunctionCall*> Nodes;
	Graph->GetNodesOfClass(Nodes);

	TSet<FName> Names;
	for (UNiagaraNodeFunctionCall* Node : Nodes)
	{
		CA_ASSUME(Node != nullptr);
		if (Node != this)
		{
			Names.Add(*Node->GetFunctionName());
		}
	}

	FString NewName = FNiagaraEditorUtilities::GetUniqueName(ProposedName, Names).ToString();
	if (!FunctionDisplayName.Equals(NewName))
	{
		FunctionDisplayName = NewName;
	}
}

bool UNiagaraNodeFunctionCall::FindAutoBoundInput(UNiagaraNodeInput* InputNode, UEdGraphPin* PinToAutoBind, FNiagaraVariable& OutFoundVar, ENiagaraInputNodeUsage& OutNodeUsage)
{
	check(InputNode && InputNode->IsExposed());
	if (PinToAutoBind->LinkedTo.Num() > 0 || !InputNode->CanAutoBind())
		return false;

	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());
	FNiagaraVariable PinVar = Schema->PinToNiagaraVariable(PinToAutoBind);

	//See if we can auto bind this pin to something in the caller script.
	UNiagaraGraph* CallerGraph = GetNiagaraGraph();
	check(CallerGraph);
	UNiagaraNodeOutput* CallerOutputNodeSpawn = CallerGraph->FindOutputNode(ENiagaraScriptUsage::ParticleSpawnScript);
	UNiagaraNodeOutput* CallerOutputNodeUpdate = CallerGraph->FindOutputNode(ENiagaraScriptUsage::ParticleUpdateScript);

	//First, lest see if we're an attribute of this emitter. Only valid if we're a module call off the primary script.
	if (CallerOutputNodeSpawn || CallerOutputNodeUpdate)
	{
		UNiagaraNodeOutput* CallerOutputNode = CallerOutputNodeSpawn != nullptr ? CallerOutputNodeSpawn : CallerOutputNodeUpdate;
		check(CallerOutputNode);
		{
			FNiagaraVariable* AttrVarPtr = CallerOutputNode->Outputs.FindByPredicate([&](const FNiagaraVariable& Attr) { return PinVar.IsEquivalent(Attr); });
			if (AttrVarPtr)
			{
				OutFoundVar = *AttrVarPtr;
				OutNodeUsage = ENiagaraInputNodeUsage::Attribute;
				return true;
			}
		}
	}
	
	//Next, lets see if we are a system constant.
	//Do we need a smarter (possibly contextual) handling of system constants?
	const TArray<FNiagaraVariable>& SysConstants = FNiagaraConstants::GetEngineConstants();
	if (SysConstants.Contains(PinVar))
	{
		OutFoundVar = PinVar;
		OutNodeUsage = ENiagaraInputNodeUsage::SystemConstant;
		return true;
	}

	//Not sure it's a good idea to allow binding to user made parameters.
// 	if (AutoBindOptions.bBindToParameters)
// 	{
// 		//Finally, lets see if we're a parameter of this emitter.
// 		TArray<UNiagaraNodeInput*> CallerInputNodes;
// 		CallerGraph->FindInputNodes(CallerInputNodes);
// 		UNiagaraNodeInput** MatchingParamPtr = CallerInputNodes.FindByPredicate([&](UNiagaraNodeInput*& CallerInputNode)
// 		{
// 			return CallerInputNode->Input.IsEquivalent(PinVar);
// 		});
// 
// 		if (MatchingParamPtr)
// 		{
// 			OutFoundVar = (*MatchingParamPtr)->Input;
// 			OutNodeUsage = ENiagaraInputNodeUsage::Parameter;
// 			return true;
// 		}
// 	}

	//Unable to auto bind.
	return false;
}

#undef LOCTEXT_NAMESPACE
