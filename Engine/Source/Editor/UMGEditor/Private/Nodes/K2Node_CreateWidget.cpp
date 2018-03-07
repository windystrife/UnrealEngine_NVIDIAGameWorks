// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Nodes/K2Node_CreateWidget.h"

#if WITH_EDITOR
	#include "GameFramework/PlayerController.h"
#endif // WITH_EDITOR
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "Blueprint/UserWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "KismetCompilerMisc.h"
#include "KismetCompiler.h"
#include "EditorCategoryUtils.h"


#define LOCTEXT_NAMESPACE "UMG"

struct FK2Node_CreateWidgetHelper
{
	static FString OwningPlayerPinName;
};

FString FK2Node_CreateWidgetHelper::OwningPlayerPinName(TEXT("OwningPlayer"));

UK2Node_CreateWidget::UK2Node_CreateWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeTooltip = LOCTEXT("NodeTooltip", "Creates a new widget");
}

void UK2Node_CreateWidget::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	// OwningPlayer pin
	UEdGraphPin* OwningPlayerPin = CreatePin(EGPD_Input, K2Schema->PC_Object, FString(), APlayerController::StaticClass(), FK2Node_CreateWidgetHelper::OwningPlayerPinName);
	SetPinToolTip(*OwningPlayerPin, LOCTEXT("OwningPlayerPinDescription", "The player that 'owns' the widget."));
}

FLinearColor UK2Node_CreateWidget::GetNodeTitleColor() const
{
	return Super::GetNodeTitleColor();
}

FText UK2Node_CreateWidget::GetBaseNodeTitle() const
{
	return LOCTEXT("CreateWidget_BaseTitle", "Create Widget");
}

FText UK2Node_CreateWidget::GetNodeTitleFormat() const
{
	return LOCTEXT("CreateWidget", "Create {ClassName} Widget");
}

UClass* UK2Node_CreateWidget::GetClassPinBaseClass() const
{
	return UUserWidget::StaticClass();
}

FText UK2Node_CreateWidget::GetMenuCategory() const
{
	return FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::UserInterface);
}

FName UK2Node_CreateWidget::GetCornerIcon() const
{
	return TEXT("Graph.Replication.ClientEvent");
}

UEdGraphPin* UK2Node_CreateWidget::GetOwningPlayerPin() const
{
	UEdGraphPin* Pin = FindPin(FK2Node_CreateWidgetHelper::OwningPlayerPinName);
	check(Pin == NULL || Pin->Direction == EGPD_Input);
	return Pin;
}

bool UK2Node_CreateWidget::IsSpawnVarPin(UEdGraphPin* Pin) const
{
	return( Super::IsSpawnVarPin(Pin) &&
		Pin->PinName != FK2Node_CreateWidgetHelper::OwningPlayerPinName );
}

void UK2Node_CreateWidget::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	static FName Create_FunctionName = GET_FUNCTION_NAME_CHECKED(UWidgetBlueprintLibrary, Create);
	static FString WorldContextObject_ParamName = FString(TEXT("WorldContextObject"));
	static FString WidgetType_ParamName = FString(TEXT("WidgetType"));
	static FString OwningPlayer_ParamName = FString(TEXT("OwningPlayer"));

	UK2Node_CreateWidget* CreateWidgetNode = this;
	UEdGraphPin* SpawnNodeExec = CreateWidgetNode->GetExecPin();
	UEdGraphPin* SpawnWorldContextPin = CreateWidgetNode->GetWorldContextPin();
	UEdGraphPin* SpawnOwningPlayerPin = CreateWidgetNode->GetOwningPlayerPin();
	UEdGraphPin* SpawnClassPin = CreateWidgetNode->GetClassPin();
	UEdGraphPin* SpawnNodeThen = CreateWidgetNode->GetThenPin();
	UEdGraphPin* SpawnNodeResult = CreateWidgetNode->GetResultPin();

	UClass* SpawnClass = ( SpawnClassPin != NULL ) ? Cast<UClass>(SpawnClassPin->DefaultObject) : NULL;
	if ( !SpawnClassPin || ((0 == SpawnClassPin->LinkedTo.Num()) && (NULL == SpawnClass)))
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("CreateWidgetNodeMissingClass_Error", "Spawn node @@ must have a class specified.").ToString(), CreateWidgetNode);
		// we break exec links so this is the only error we get, don't want the CreateWidget node being considered and giving 'unexpected node' type warnings
		CreateWidgetNode->BreakAllNodeLinks();
		return;
	}

	//////////////////////////////////////////////////////////////////////////
	// create 'UWidgetBlueprintLibrary::Create' call node
	UK2Node_CallFunction* CallCreateNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(CreateWidgetNode, SourceGraph);
	CallCreateNode->FunctionReference.SetExternalMember(Create_FunctionName, UWidgetBlueprintLibrary::StaticClass());
	CallCreateNode->AllocateDefaultPins();

	UEdGraphPin* CallCreateExec = CallCreateNode->GetExecPin();
	UEdGraphPin* CallCreateWorldContextPin = CallCreateNode->FindPinChecked(WorldContextObject_ParamName);
	UEdGraphPin* CallCreateWidgetTypePin = CallCreateNode->FindPinChecked(WidgetType_ParamName);
	UEdGraphPin* CallCreateOwningPlayerPin = CallCreateNode->FindPinChecked(OwningPlayer_ParamName);
	UEdGraphPin* CallCreateResult = CallCreateNode->GetReturnValuePin();

	// Move 'exec' connection from create widget node to 'UWidgetBlueprintLibrary::Create'
	CompilerContext.MovePinLinksToIntermediate(*SpawnNodeExec, *CallCreateExec);

	if ( SpawnClassPin->LinkedTo.Num() > 0 )
	{
		// Copy the 'blueprint' connection from the spawn node to 'UWidgetBlueprintLibrary::Create'
		CompilerContext.MovePinLinksToIntermediate(*SpawnClassPin, *CallCreateWidgetTypePin);
	}
	else
	{
		// Copy blueprint literal onto 'UWidgetBlueprintLibrary::Create' call 
		CallCreateWidgetTypePin->DefaultObject = SpawnClass;
	}

	// Copy the world context connection from the spawn node to 'UWidgetBlueprintLibrary::Create' if necessary
	if ( SpawnWorldContextPin )
	{
		CompilerContext.MovePinLinksToIntermediate(*SpawnWorldContextPin, *CallCreateWorldContextPin);
	}

	// Copy the 'Owning Player' connection from the spawn node to 'UWidgetBlueprintLibrary::Create'
	CompilerContext.MovePinLinksToIntermediate(*SpawnOwningPlayerPin, *CallCreateOwningPlayerPin);

	// Move result connection from spawn node to 'UWidgetBlueprintLibrary::Create'
	CallCreateResult->PinType = SpawnNodeResult->PinType; // Copy type so it uses the right actor subclass
	CompilerContext.MovePinLinksToIntermediate(*SpawnNodeResult, *CallCreateResult);

	//////////////////////////////////////////////////////////////////////////
	// create 'set var' nodes

	// Get 'result' pin from 'begin spawn', this is the actual actor we want to set properties on
	UEdGraphPin* LastThen = FKismetCompilerUtilities::GenerateAssignmentNodes(CompilerContext, SourceGraph, CallCreateNode, CreateWidgetNode, CallCreateResult, GetClassToSpawn());

	// Move 'then' connection from create widget node to the last 'then'
	CompilerContext.MovePinLinksToIntermediate(*SpawnNodeThen, *LastThen);

	// Break any links to the expanded node
	CreateWidgetNode->BreakAllNodeLinks();
}

#undef LOCTEXT_NAMESPACE
