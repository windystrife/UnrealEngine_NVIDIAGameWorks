// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsK2Node_MultiCompareGameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"

#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "GameplayTagAssetInterface.h"
#include "BlueprintGameplayTagLibrary.h"
#include "KismetCompiler.h"

UGameplayTagsK2Node_MultiCompareGameplayTagAssetInterface::UGameplayTagsK2Node_MultiCompareGameplayTagAssetInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UGameplayTagsK2Node_MultiCompareGameplayTagAssetInterface::AllocateDefaultPins()
{
	PinNames.Empty();
	for (int32 Index = 0; Index < NumberOfPins; ++Index)
	{
		AddPinToSwitchNode();
	}

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	CreatePin(EGPD_Input, K2Schema->PC_Interface, FString(), UGameplayTagAssetInterface::StaticClass(), TEXT("Gameplay Tag Asset Interface"));
}

void UGameplayTagsK2Node_MultiCompareGameplayTagAssetInterface::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	// Get The input and output pins to our node
	UEdGraphPin* InPinSwitch = FindPin(TEXT("Gameplay Tag Asset Interface"));

	// For Each Pin Compare against the Interface
	for (int32 Index = 0; Index < NumberOfPins; ++Index)
	{
		FString InPinName = TEXT("TagCase_") + FString::FormatAsNumber(Index);
		FString OutPinName = TEXT("Case_") + FString::FormatAsNumber(Index) + TEXT(" True");
		UEdGraphPin* InPinCase = FindPin(InPinName);
		UEdGraphPin* OutPinCase = FindPin(OutPinName);

		// Create call function node for the Compare function HasAllMatchingGameplayTags
		UK2Node_CallFunction* PinCallFunction = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		const UFunction* Function = UBlueprintGameplayTagLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UBlueprintGameplayTagLibrary, HasAllMatchingGameplayTags));
		PinCallFunction->SetFromFunction(Function);
		PinCallFunction->AllocateDefaultPins();

		UEdGraphPin *InterfacePin = PinCallFunction->FindPinChecked(FString(TEXT("TagContainerInterface")));
		CompilerContext.CopyPinLinksToIntermediate(*InPinSwitch, *InterfacePin);

		UEdGraphPin *OtherTagContainerPin = PinCallFunction->FindPinChecked(FString(TEXT("OtherContainer")));
		CompilerContext.MovePinLinksToIntermediate(*InPinCase, *OtherTagContainerPin);
		
		UEdGraphPin *OutPin = PinCallFunction->FindPinChecked(K2Schema->PN_ReturnValue);

		if (OutPinCase && OutPin)
		{
			OutPin->PinType = OutPinCase->PinType; // Copy type so it uses the right actor subclass
			CompilerContext.MovePinLinksToIntermediate(*OutPinCase, *OutPin);
		}
	}

	// Break any links to the expanded node
	BreakAllNodeLinks();
}

FText UGameplayTagsK2Node_MultiCompareGameplayTagAssetInterface::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("K2Node", "MultiCompare_AssetInterface", "Compare Tag Asset Interface to Other Tag Containers");
}

void UGameplayTagsK2Node_MultiCompareGameplayTagAssetInterface::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// actions get registered under specific object-keys; the idea is that 
	// actions might have to be updated (or deleted) if their object-key is  
	// mutated (or removed)... here we use the node's class (so if the node 
	// type disappears, then the action should go with it)
	UClass* ActionKey = GetClass();
	// to keep from needlessly instantiating a UBlueprintNodeSpawner, first   
	// check to make sure that the registrar is looking for actions of this type
	// (could be regenerating actions for a specific asset, and therefore the 
	// registrar would only accept actions corresponding to that asset)
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

void UGameplayTagsK2Node_MultiCompareGameplayTagAssetInterface::AddPinToSwitchNode()
{
	FString PinName = GetUniquePinName();
	FString InPin = TEXT("Tag") + PinName;
	FString OutPin = PinName + TEXT(" True");
	PinNames.Add(FName(*PinName));

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	CreatePin(EGPD_Input, K2Schema->PC_Struct, FString(), FGameplayTagContainer::StaticStruct(), InPin, EPinContainerType::None, true);
	CreatePin(EGPD_Output, K2Schema->PC_Boolean, FString(), nullptr, OutPin);
}
