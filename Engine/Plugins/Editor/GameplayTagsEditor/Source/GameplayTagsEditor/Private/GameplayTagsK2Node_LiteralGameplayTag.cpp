// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsK2Node_LiteralGameplayTag.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_MakeArray.h"
#include "K2Node_MakeStruct.h"

#include "KismetCompiler.h"
#include "GameplayTagContainer.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintGameplayTagLibrary.h"

#define LOCTEXT_NAMESPACE "GameplayTagsK2Node_LiteralGameplayTag"

UGameplayTagsK2Node_LiteralGameplayTag::UGameplayTagsK2Node_LiteralGameplayTag(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UGameplayTagsK2Node_LiteralGameplayTag::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	CreatePin(EGPD_Input, K2Schema->PC_String, TEXT("LiteralGameplayTagContainer"), nullptr, TEXT("TagIn"));
	CreatePin(EGPD_Output, K2Schema->PC_Struct, FString(), FGameplayTagContainer::StaticStruct(), K2Schema->PN_ReturnValue);
}

FLinearColor UGameplayTagsK2Node_LiteralGameplayTag::GetNodeTitleColor() const
{
	return FLinearColor(1.0f, 0.51f, 0.0f);
}

FText UGameplayTagsK2Node_LiteralGameplayTag::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("K2Node", "LiteralGameplayTag", "Make Literal GameplayTagContainer");
}

bool UGameplayTagsK2Node_LiteralGameplayTag::CanCreateUnderSpecifiedSchema( const UEdGraphSchema* Schema ) const
{
	return Schema->IsA(UEdGraphSchema_K2::StaticClass());
}

void UGameplayTagsK2Node_LiteralGameplayTag::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	ensureMsgf(0, TEXT("GameplayTagsK2Node_LiteralGameplayTag is deprecated and should never make it to compile time"));
}

void UGameplayTagsK2Node_LiteralGameplayTag::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	
}

FText UGameplayTagsK2Node_LiteralGameplayTag::GetMenuCategory() const
{
	return LOCTEXT("ActionMenuCategory", "Gameplay Tags");
}

FString UGameplayTagsK2Node_LiteralGameplayTag::GetDeprecationMessage() const
{
	return LOCTEXT("NodeDeprecated_Warning", "@@ is deprecated, replace with Make Literal GameplayTagContainer function call").ToString();
}

void UGameplayTagsK2Node_LiteralGameplayTag::ConvertDeprecatedNode(UEdGraph* Graph, bool bOnlySafeChanges)
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	TMap<FString, FString> OldPinToNewPinMap;

	UFunction* MakeFunction = UBlueprintGameplayTagLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UBlueprintGameplayTagLibrary, MakeLiteralGameplayTagContainer));
	OldPinToNewPinMap.Add(TEXT("TagIn"), TEXT("Value"));

	ensure(Schema->ConvertDeprecatedNodeToFunctionCall(this, MakeFunction, OldPinToNewPinMap, Graph) != nullptr);
}

#undef LOCTEXT_NAMESPACE
