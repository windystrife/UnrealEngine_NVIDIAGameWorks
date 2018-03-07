// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraNodeParameterCollection.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraEditorUtilities.h"
#include "SNiagaraGraphNodeConvert.h"
#include "NiagaraGraph.h"
#include "NiagaraHlslTranslator.h"
#include "ScopedTransaction.h"
#include "NiagaraParameterCollection.h"
#include "NiagaraCommon.h"
#include "ModuleManager.h"
#include "AssetRegistryModule.h"
#include "SNiagaraGraphPinAdd.h"
#include "MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "NiagaraNodeParameterCollection"

UNiagaraNodeParameterCollection::UNiagaraNodeParameterCollection() : UNiagaraNodeWithDynamicPins()
{

}

void UNiagaraNodeParameterCollection::AllocateDefaultPins()
{
	if (CollectionAssetObjectPath != NAME_None && Collection == nullptr)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(CollectionAssetObjectPath);
		if (AssetData.IsValid())
		{
			Collection = Cast<UNiagaraParameterCollection>(AssetData.GetAsset());
		}
		else
		{
			UE_LOG(LogNiagaraEditor, Warning, TEXT("Failed to load Niagara Parameter Collection: {%s}"), *CollectionAssetObjectPath.ToString());
		}
	}

 	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	if (Collection)
	{
		for (const FNiagaraVariable& Var : Variables)
		{
			if (Collection->IndexOfParameter(Var) != INDEX_NONE)
			{
				FString VarName = Collection->FriendlyNameFromParameterName(Var.GetName().ToString());
				CreatePin(EGPD_Output, Schema->TypeDefinitionToPinType(Var.GetType()), VarName);
			}
		}
	}

	CreateAddPin(EGPD_Output);
}

bool UNiagaraNodeParameterCollection::RefreshFromExternalChanges()
{
	//Update to reflect any collection changes.
	if (Collection)
	{
		const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
		
		for (UEdGraphPin* Pin : Pins)
		{
			if (IsAddPin(Pin))
			{
				continue;
			}
			FNiagaraVariable PinVar = Collection->CollectionParameterFromFriendlyParameter(Schema->PinToNiagaraVariable(Pin));
			if (Collection->IndexOfParameter(PinVar) == INDEX_NONE)
			{
				//Can't find this parameter in the collection so we must refresh.
				ReallocatePins();
				return true;
			}
		}
	}
	
	return false;
}

#if WITH_EDITOR

void UNiagaraNodeParameterCollection::PostEditImport()
{
	Super::PostEditImport();
	//Pins.Empty();
}

void UNiagaraNodeParameterCollection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	//RefreshFromExternalChanges();
}

#endif

void UNiagaraNodeParameterCollection::PostLoad()
{
	Super::PostLoad();
}

bool UNiagaraNodeParameterCollection::CanRenamePin(const UEdGraphPin* GraphPinObj) const
{
	return false;
}

void UNiagaraNodeParameterCollection::OnNewTypedPinAdded(UEdGraphPin* NewPin)
{
 	check(Collection);
 	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	FNiagaraVariable NewVar = Schema->PinToNiagaraVariable(NewPin);
	Variables.Add(Collection->CollectionParameterFromFriendlyParameter(NewVar));
}

FText UNiagaraNodeParameterCollection::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return Collection ? FText::FromString(Collection->GetName()) : LOCTEXT("ParameterCollectionNodeTitle", "Parameter Collection");
}

FText UNiagaraNodeParameterCollection::GetTooltipText()const
{
	return FText::FromString(Collection->GetPathName());
}

TSharedRef<SWidget> UNiagaraNodeParameterCollection::GenerateAddPinMenu(const FString& InWorkingPinName, SNiagaraGraphPinAdd* InPin)
{
	FMenuBuilder MenuBuilder(true, nullptr);
	if (Collection)
	{
		for (FNiagaraVariable Var : Collection->GetParameters())
		{
			if(!Variables.Contains(Var))
			{
				FNiagaraVariable FriendlyVar = Collection->FriendlyParameterFromCollectionParameter(Var);
				MenuBuilder.AddMenuEntry(
					FText::FromName(FriendlyVar.GetName()),
					FText::Format(LOCTEXT("AddButtonTypeEntryToolTipFormatCollection", "Add a reference to collection parameter {0}"), FText::FromName(FriendlyVar.GetName())),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateSP(InPin, &SNiagaraGraphPinAdd::OnAddType, FriendlyVar)));
			}
		}
	}
	return MenuBuilder.MakeWidget();
}

void UNiagaraNodeParameterCollection::RemoveDynamicPin(UEdGraphPin* Pin)
{
	Super::RemoveDynamicPin(Pin);

	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	FNiagaraVariable RemVar = Collection->CollectionParameterFromFriendlyParameter(Schema->PinToNiagaraVariable(Pin));
	Variables.Remove(RemVar);
}

// void UNiagaraNodeParameterCollection::BuildParameterMapHistory(const UEdGraphPin* ParameterMapPinFromNode, FNiagaraParameterMapHistory& OutHistory, bool bRecursive)
// {
// 	if (Collection)
// 	{
// 		const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
// 		OutHistory.RegisterNodeVisitation(this);
// 		if (GetInputPin(0) == ParameterMapPinFromNode)
// 		{
// 			FString Namespace = GetCollectionNamespace();
// 			TArray<UEdGraphPin*> OutputPins;
// 			GetOutputPins(OutputPins);
// 			for (int32 i = 0; i < OutputPins.Num(); i++)
// 			{
// 				if (IsAddPin(OutputPins[i]))
// 				{
// 					continue;
// 				}
// 
// 				FNiagaraVariable Var = Schema->PinToNiagaraVariable(OutputPins[i]);
// 				Var = FNiagaraParameterMapHistory::VariableToNamespacedVariable(Var, Namespace);
// 				OutHistory.AddExternalVariable(Var);
// 			}
// 		}
// 	}
// }

UObject* UNiagaraNodeParameterCollection::GetReferencedAsset()const 
{
	return Collection; 
}

void UNiagaraNodeParameterCollection::Compile(class FHlslNiagaraTranslator* Translator, TArray<int32>& Outputs)
{
	if (Collection)
	{
		UNiagaraGraph* Graph = Cast<UNiagaraGraph>(GetGraph());
		const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());

		TArray<UEdGraphPin*> InputPins;
		GetInputPins(InputPins);
		check(InputPins.Num() == 0);

		Translator->ParameterCollection(this, Outputs);
	}
	else
	{
		Translator->Error(LOCTEXT("Invalid Collection", "Parameter Collection is invalid."), this, nullptr);
	}
}

#undef LOCTEXT_NAMESPACE
