// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "K2Node_GetNumEnumEntries.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "KismetCompiler.h"
#include "BlueprintFieldNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"

UK2Node_GetNumEnumEntries::UK2Node_GetNumEnumEntries(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_GetNumEnumEntries::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	// Create the return value pin
	CreatePin(EGPD_Output, Schema->PC_Int, FString(), nullptr, Schema->PN_ReturnValue);

	Super::AllocateDefaultPins();
}

FText UK2Node_GetNumEnumEntries::GetTooltipText() const
{
	if (Enum == nullptr)
	{
		return NSLOCTEXT("K2Node", "GetNumEnumEntries_BadTooltip", "Returns (bad enum)_MAX value");
	}
	else if (CachedTooltip.IsOutOfDate(this))
	{
		// FText::Format() is slow, so we cache this to save on performance
		CachedTooltip.SetCachedText(FText::Format(NSLOCTEXT("K2Node", "GetNumEnumEntries_Tooltip", "Returns {0}_MAX value"), FText::FromName(Enum->GetFName())), this);
	}
	return CachedTooltip;
}

FText UK2Node_GetNumEnumEntries::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (Enum == nullptr)
	{
		return NSLOCTEXT("K2Node", "GetNumEnumEntries_BadEnumTitle", "Get number of entries in (bad enum)");
	}
	else if (CachedNodeTitle.IsOutOfDate(this))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("EnumName"), FText::FromString(Enum->GetName()));
		CachedNodeTitle.SetCachedText(FText::Format(NSLOCTEXT("K2Node", "GetNumEnumEntries_Title", "Get number of entries in {EnumName}"), Args), this);
	}
	return CachedNodeTitle;
}

FSlateIcon UK2Node_GetNumEnumEntries::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("EditorStyle", "GraphEditor.Enum_16x");
	return Icon;
}

void UK2Node_GetNumEnumEntries::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	if(NULL == Enum)
	{
		CompilerContext.MessageLog.Error(*FString::Printf(*NSLOCTEXT("K2Node", "GetNumEnumEntries_Error", "@@ must have a valid enum defined").ToString()), this);
		return;
	}

	// Force the enum to load its values if it hasn't already
	if (Enum->HasAnyFlags(RF_NeedLoad))
	{
		Enum->GetLinker()->Preload(Enum);
	}

	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	check(NULL != Schema);

	//MAKE LITERAL
	const FName FunctionName = GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, MakeLiteralInt);
	UK2Node_CallFunction* MakeLiteralInt = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph); 
	MakeLiteralInt->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(FunctionName));
	MakeLiteralInt->AllocateDefaultPins();

	//OPUTPUT PIN
	UEdGraphPin* OrgReturnPin = FindPinChecked(Schema->PN_ReturnValue);
	UEdGraphPin* NewReturnPin = MakeLiteralInt->GetReturnValuePin();
	check(NULL != NewReturnPin);
	CompilerContext.MovePinLinksToIntermediate(*OrgReturnPin, *NewReturnPin);

	//INPUT PIN
	UEdGraphPin* InputPin = MakeLiteralInt->FindPinChecked(TEXT("Value"));
	check(EGPD_Input == InputPin->Direction);
	const FString DefaultValue = FString::FromInt(Enum->NumEnums() - 1);
	InputPin->DefaultValue = DefaultValue;

	BreakAllNodeLinks();
}

void UK2Node_GetNumEnumEntries::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	struct GetMenuActions_Utils
	{
		static void SetNodeEnum(UEdGraphNode* NewNode, UField const* /*EnumField*/, TWeakObjectPtr<UEnum> NonConstEnumPtr)
		{
			UK2Node_GetNumEnumEntries* EnumNode = CastChecked<UK2Node_GetNumEnumEntries>(NewNode);
			EnumNode->Enum = NonConstEnumPtr.Get();
		}
	};

	UClass* NodeClass = GetClass();
	ActionRegistrar.RegisterEnumActions( FBlueprintActionDatabaseRegistrar::FMakeEnumSpawnerDelegate::CreateLambda([NodeClass](const UEnum* InEnum)->UBlueprintNodeSpawner*
	{
		UBlueprintFieldNodeSpawner* NodeSpawner = UBlueprintFieldNodeSpawner::Create(NodeClass, InEnum);
		check(NodeSpawner != nullptr);
		TWeakObjectPtr<UEnum> NonConstEnumPtr = InEnum;
		NodeSpawner->SetNodeFieldDelegate = UBlueprintFieldNodeSpawner::FSetNodeFieldDelegate::CreateStatic(GetMenuActions_Utils::SetNodeEnum, NonConstEnumPtr);

		return NodeSpawner;
	}) );
}

FText UK2Node_GetNumEnumEntries::GetMenuCategory() const
{
	return FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::Enum);
}

