// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "K2Node_ForEachElementInEnum.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_AssignmentStatement.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_TemporaryVariable.h"
#include "KismetCompiler.h"
#include "Kismet/KismetNodeHelperLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "K2Node_CastByteToEnum.h"
#include "K2Node_GetNumEnumEntries.h"
#include "BlueprintFieldNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"

#define LOCTEXT_NAMESPACE "K2Node"

struct FForExpandNodeHelper
{
	UEdGraphPin* StartLoopExecInPin;
	UEdGraphPin* InsideLoopExecOutPin;
	UEdGraphPin* LoopCompleteOutExecPin;

	UEdGraphPin* ArrayIndexOutPin;
	UEdGraphPin* LoopCounterOutPin;
	// for(LoopCounter = 0; LoopCounter < LoopCounterLimit; ++LoopCounter)
	UEdGraphPin* LoopCounterLimitInPin;

	FForExpandNodeHelper()
		: StartLoopExecInPin(nullptr)
		, InsideLoopExecOutPin(nullptr)
		, LoopCompleteOutExecPin(nullptr)
		, ArrayIndexOutPin(nullptr)
		, LoopCounterOutPin(nullptr)
		, LoopCounterLimitInPin(nullptr)
	{ }

	bool BuildLoop(UK2Node* Node, FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UEnum* Enum)
	{
		const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
		check(Node && SourceGraph && Schema);

		bool bResult = true;

		// Create int Loop Counter
		UK2Node_TemporaryVariable* LoopCounterNode = CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(Node, SourceGraph);
		LoopCounterNode->VariableType.PinCategory = Schema->PC_Int;
		LoopCounterNode->AllocateDefaultPins();
		LoopCounterOutPin = LoopCounterNode->GetVariablePin();
		check(LoopCounterOutPin);

		// Initialize loop counter
		UK2Node_AssignmentStatement* LoopCounterInitialize = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(Node, SourceGraph);
		LoopCounterInitialize->AllocateDefaultPins();
		LoopCounterInitialize->GetValuePin()->DefaultValue = TEXT("0");
		bResult &= Schema->TryCreateConnection(LoopCounterOutPin, LoopCounterInitialize->GetVariablePin());
		StartLoopExecInPin = LoopCounterInitialize->GetExecPin();
		check(StartLoopExecInPin);

		// Create int Array Index
		UK2Node_TemporaryVariable* ArrayIndexNode = CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(Node, SourceGraph);
		ArrayIndexNode->VariableType.PinCategory = Schema->PC_Int;
		ArrayIndexNode->AllocateDefaultPins();
		ArrayIndexOutPin = ArrayIndexNode->GetVariablePin();
		check(ArrayIndexOutPin);

		// Initialize array index
		UK2Node_AssignmentStatement* ArrayIndexInitialize = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(Node, SourceGraph);
		ArrayIndexInitialize->AllocateDefaultPins();
		ArrayIndexInitialize->GetValuePin()->DefaultValue = TEXT("0");
		bResult &= Schema->TryCreateConnection(ArrayIndexOutPin, ArrayIndexInitialize->GetVariablePin());
		bResult &= Schema->TryCreateConnection(LoopCounterInitialize->GetThenPin(), ArrayIndexInitialize->GetExecPin());

		// Do loop branch
		UK2Node_IfThenElse* Branch = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(Node, SourceGraph);
		Branch->AllocateDefaultPins();
		bResult &= Schema->TryCreateConnection(ArrayIndexInitialize->GetThenPin(), Branch->GetExecPin());
		LoopCompleteOutExecPin = Branch->GetElsePin();
		check(LoopCompleteOutExecPin);

		// Do loop condition
		UK2Node_CallFunction* Condition = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(Node, SourceGraph); 
		Condition->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Less_IntInt)));
		Condition->AllocateDefaultPins();
		bResult &= Schema->TryCreateConnection(Condition->GetReturnValuePin(), Branch->GetConditionPin());
		bResult &= Schema->TryCreateConnection(Condition->FindPinChecked(TEXT("A")), LoopCounterOutPin);
		LoopCounterLimitInPin = Condition->FindPinChecked(TEXT("B"));
		check(LoopCounterLimitInPin);

		// Convert the Enum index to a value
		UK2Node_CallFunction* GetEnumeratorValueFromIndexCall = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(Node, SourceGraph); 
		GetEnumeratorValueFromIndexCall->SetFromFunction(UKismetNodeHelperLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetNodeHelperLibrary, GetEnumeratorValueFromIndex)));
		GetEnumeratorValueFromIndexCall->AllocateDefaultPins();
		Schema->TrySetDefaultObject(*GetEnumeratorValueFromIndexCall->FindPinChecked(TEXT("Enum")), Enum);
		bResult &= Schema->TryCreateConnection(GetEnumeratorValueFromIndexCall->FindPinChecked(TEXT("EnumeratorIndex")), LoopCounterOutPin);

		// Array Index assigned
		UK2Node_AssignmentStatement* ArrayIndexAssign = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(Node, SourceGraph);
		ArrayIndexAssign->AllocateDefaultPins();
		bResult &= Schema->TryCreateConnection(Branch->GetThenPin(), ArrayIndexAssign->GetExecPin());
		bResult &= Schema->TryCreateConnection(ArrayIndexAssign->GetVariablePin(), ArrayIndexOutPin);
		bResult &= Schema->TryCreateConnection(ArrayIndexAssign->GetValuePin(), GetEnumeratorValueFromIndexCall->GetReturnValuePin());

		// body sequence
		UK2Node_ExecutionSequence* Sequence = CompilerContext.SpawnIntermediateNode<UK2Node_ExecutionSequence>(Node, SourceGraph);
		Sequence->AllocateDefaultPins();
		bResult &= Schema->TryCreateConnection(ArrayIndexAssign->GetThenPin(), Sequence->GetExecPin());
		InsideLoopExecOutPin = Sequence->GetThenPinGivenIndex(0);
		check(InsideLoopExecOutPin);

		// Loop Counter increment
		UK2Node_CallFunction* Increment = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(Node, SourceGraph); 
		Increment->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Add_IntInt)));
		Increment->AllocateDefaultPins();
		bResult &= Schema->TryCreateConnection(Increment->FindPinChecked(TEXT("A")), LoopCounterOutPin);
		Increment->FindPinChecked(TEXT("B"))->DefaultValue = TEXT("1");

		// Loop Counter assigned
		UK2Node_AssignmentStatement* LoopCounterAssign = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(Node, SourceGraph);
		LoopCounterAssign->AllocateDefaultPins();
		bResult &= Schema->TryCreateConnection(LoopCounterAssign->GetExecPin(), Sequence->GetThenPinGivenIndex(1));
		bResult &= Schema->TryCreateConnection(LoopCounterAssign->GetVariablePin(), LoopCounterOutPin);
		bResult &= Schema->TryCreateConnection(LoopCounterAssign->GetValuePin(), Increment->GetReturnValuePin());
		bResult &= Schema->TryCreateConnection(LoopCounterAssign->GetThenPin(), Branch->GetExecPin());

		return bResult;
	}
};

UK2Node_ForEachElementInEnum::UK2Node_ForEachElementInEnum(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

const FString UK2Node_ForEachElementInEnum::InsideLoopPinName(TEXT("LoopBody"));
const FString UK2Node_ForEachElementInEnum::EnumOuputPinName(TEXT("EnumValue"));

void UK2Node_ForEachElementInEnum::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	check(K2Schema);

	CreatePin(EGPD_Input, K2Schema->PC_Exec, FString(), nullptr, K2Schema->PN_Execute);

	if (Enum)
	{
		CreatePin(EGPD_Output, K2Schema->PC_Exec, FString(), nullptr, InsideLoopPinName);
		CreatePin(EGPD_Output, K2Schema->PC_Byte, FString(), Enum, EnumOuputPinName);
	}

	if (UEdGraphPin* CompletedPin = CreatePin(EGPD_Output, K2Schema->PC_Exec, FString(), nullptr, K2Schema->PN_Then))
	{
		CompletedPin->PinFriendlyName = LOCTEXT("Completed", "Completed");
	}
}

void UK2Node_ForEachElementInEnum::ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	if (!Enum)
	{
		MessageLog.Error(*NSLOCTEXT("K2Node", "ForEachElementInEnum_NoEnumError", "No Enum in @@").ToString(), this);
	}
}

FText UK2Node_ForEachElementInEnum::GetTooltipText() const
{
	return GetNodeTitle(ENodeTitleType::FullTitle);
}

FText UK2Node_ForEachElementInEnum::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (Enum == nullptr)
	{
		return LOCTEXT("ForEachElementInUnknownEnum_Title", "ForEach UNKNOWN");
	}
	else if (CachedNodeTitle.IsOutOfDate(this))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("EnumName"), FText::FromName(Enum->GetFName()));
		// FText::Format() is slow, so we cache this to save on performance
		CachedNodeTitle.SetCachedText(FText::Format(LOCTEXT("ForEachElementInEnum_Title", "ForEach {EnumName}"), Args), this);
	}
	return CachedNodeTitle;
}

FSlateIcon UK2Node_ForEachElementInEnum::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("EditorStyle", "GraphEditor.Macro.Loop_16x");
	return Icon;
}

void UK2Node_ForEachElementInEnum::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	if (!Enum)
	{
		ValidateNodeDuringCompilation(CompilerContext.MessageLog);
		return;
	}

	FForExpandNodeHelper ForLoop;
	if (!ForLoop.BuildLoop(this, CompilerContext, SourceGraph, Enum))
	{
		CompilerContext.MessageLog.Error(*NSLOCTEXT("K2Node", "ForEachElementInEnum_ForError", "For Expand error in @@").ToString(), this);
	}

	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();

	CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *ForLoop.StartLoopExecInPin);
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(Schema->PN_Then), *ForLoop.LoopCompleteOutExecPin);
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(InsideLoopPinName), *ForLoop.InsideLoopExecOutPin);

	UK2Node_GetNumEnumEntries* GetNumEnumEntries = CompilerContext.SpawnIntermediateNode<UK2Node_GetNumEnumEntries>(this, SourceGraph);
	GetNumEnumEntries->Enum = Enum;
	GetNumEnumEntries->AllocateDefaultPins();
	bool bResult = Schema->TryCreateConnection(GetNumEnumEntries->FindPinChecked(Schema->PN_ReturnValue), ForLoop.LoopCounterLimitInPin);

	UK2Node_CallFunction* Conv_Func = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	FName Conv_Func_Name = GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Conv_IntToByte);
	Conv_Func->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(Conv_Func_Name));
	Conv_Func->AllocateDefaultPins();
	bResult &= Schema->TryCreateConnection(Conv_Func->FindPinChecked(TEXT("InInt")), ForLoop.ArrayIndexOutPin);

	UK2Node_CastByteToEnum* CastByteToEnum = CompilerContext.SpawnIntermediateNode<UK2Node_CastByteToEnum>(this, SourceGraph);
	CastByteToEnum->Enum = Enum;
	CastByteToEnum->bSafe = true;
	CastByteToEnum->AllocateDefaultPins();
	bResult &= Schema->TryCreateConnection(Conv_Func->FindPinChecked(Schema->PN_ReturnValue), CastByteToEnum->FindPinChecked(UK2Node_CastByteToEnum::ByteInputPinName));
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(EnumOuputPinName), *CastByteToEnum->FindPinChecked(Schema->PN_ReturnValue));

	if (!bResult)
	{
		CompilerContext.MessageLog.Error(*NSLOCTEXT("K2Node", "ForEachElementInEnum_ExpandError", "Expand error in @@").ToString(), this);
	}

	BreakAllNodeLinks();
}

void UK2Node_ForEachElementInEnum::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	struct GetMenuActions_Utils
	{
		static void SetNodeEnum(UEdGraphNode* NewNode, UField const* /*EnumField*/, TWeakObjectPtr<UEnum> NonConstEnumPtr)
		{
			UK2Node_ForEachElementInEnum* EnumNode = CastChecked<UK2Node_ForEachElementInEnum>(NewNode);
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

FText UK2Node_ForEachElementInEnum::GetMenuCategory() const
{
	return FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::Enum);
}

#undef LOCTEXT_NAMESPACE
