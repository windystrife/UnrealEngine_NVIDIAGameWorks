// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "K2Node_AssignmentStatement.h"
#include "EdGraphSchema_K2.h"
#include "KismetCompiler.h"
#include "VariableSetHandler.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"

#define LOCTEXT_NAMESPACE "K2Node_AssignmentStatement"

//////////////////////////////////////////////////////////////////////////
// FKCHandler_AssignmentStatement

class FKCHandler_AssignmentStatement : public FKCHandler_VariableSet
{
public:
	FKCHandler_AssignmentStatement(FKismetCompilerContext& InCompilerContext)
		: FKCHandler_VariableSet(InCompilerContext)
	{
	}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UEdGraphPin* VariablePin = Node->FindPin(TEXT("Variable"));
		UEdGraphPin* ValuePin = Node->FindPin(TEXT("Value"));

		if ((VariablePin == NULL) || (ValuePin == NULL))
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("MissingPins_Error", "Missing pin(s) on @@; expected a pin named Variable and a pin named Value").ToString(), Node);
			return;
		}

		if (VariablePin->LinkedTo.Num() == 0)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("NoVarriableConnected_Error", "A variable needs to be connected to @@").ToString(), VariablePin);
		}

		ValidateAndRegisterNetIfLiteral(Context, ValuePin);
	}


	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UEdGraphPin* VariablePin = Node->FindPin(TEXT("Variable"));
		UEdGraphPin* ValuePin = Node->FindPin(TEXT("Value"));

		InnerAssignment(Context, Node, VariablePin, ValuePin);

		// Generate the output impulse from this node
		GenerateSimpleThenGoto(Context, *Node);
	}
};


FString UK2Node_AssignmentStatement::VariablePinName = FString(TEXT("Variable"));
FString UK2Node_AssignmentStatement::ValuePinName = FString(TEXT("Value"));


UK2Node_AssignmentStatement::UK2Node_AssignmentStatement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_AssignmentStatement::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	CreatePin(EGPD_Input, Schema->PC_Exec, FString(), nullptr, Schema->PN_Execute);
	CreatePin(EGPD_Output, Schema->PC_Exec, FString(), nullptr, Schema->PN_Then);

	UEdGraphPin* VariablePin = CreatePin(EGPD_Input, Schema->PC_Wildcard, FString(), nullptr, VariablePinName);
	UEdGraphPin* ValuePin = CreatePin(EGPD_Input, Schema->PC_Wildcard, FString(), nullptr, ValuePinName);

	Super::AllocateDefaultPins();
}

FText UK2Node_AssignmentStatement::GetTooltipText() const
{
	return LOCTEXT("AssignmentStatementTooltip", "Assigns Value to Variable");
}

FText UK2Node_AssignmentStatement::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Assign", "Assign");
}

bool UK2Node_AssignmentStatement::IsCompatibleWithGraph(UEdGraph const* TargetGraph) const
{
	bool bIsCompatible = Super::IsCompatibleWithGraph(TargetGraph);
	if (bIsCompatible)
	{
		EGraphType const GraphType = TargetGraph->GetSchema()->GetGraphType(TargetGraph);
		bIsCompatible = GraphType == GT_Macro;
	}

	return bIsCompatible;
}

bool UK2Node_AssignmentStatement::CanPasteHere(const UEdGraph* TargetGraph) const
{
	// These nodes can be pasted anywhere that UK2Node's are compatible with the graph
	// Avoiding the call to IsCompatibleWithGraph because these nodes should normally only
	// be placed in Macros, but it's nice to be able to paste Macro functionality anywhere.
	return Super::IsCompatibleWithGraph(TargetGraph);
}

void UK2Node_AssignmentStatement::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* VariablePin = FindPin(TEXT("Variable"));
	UEdGraphPin* ValuePin = FindPin(TEXT("Value"));

	if ((VariablePin->LinkedTo.Num() == 0) && (ValuePin->LinkedTo.Num() == 0))
	{
		// Restore the wildcard status
		VariablePin->PinType.PinCategory = Schema->PC_Wildcard;
		VariablePin->PinType.PinSubCategory = TEXT("");
		VariablePin->PinType.PinSubCategoryObject = NULL;
		ValuePin->PinType.PinCategory = Schema->PC_Wildcard;
		ValuePin->PinType.PinSubCategory = TEXT("");
		ValuePin->PinType.PinSubCategoryObject = NULL;
	}
	else if (Pin->LinkedTo.Num() > 0)
	{
		Pin->PinType = Pin->LinkedTo[0]->PinType;

		// Enforce the type on the other pin
		if (VariablePin == Pin)
		{
			ValuePin->PinType = VariablePin->PinType;
			UEdGraphSchema_K2::ValidateExistingConnections(ValuePin);
		}
		else
		{
			VariablePin->PinType = ValuePin->PinType;
			UEdGraphSchema_K2::ValidateExistingConnections(VariablePin);
		}
	}
}

void UK2Node_AssignmentStatement::PostReconstructNode()
{
	UEdGraphPin* VariablePin = FindPin(TEXT("Variable"));
	UEdGraphPin* ValuePin = FindPin(TEXT("Value"));

	PinConnectionListChanged(VariablePin);
	PinConnectionListChanged(ValuePin);

	Super::PostReconstructNode();
}

UEdGraphPin* UK2Node_AssignmentStatement::GetThenPin() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* Pin = FindPin(K2Schema->PN_Then);
	check(Pin != NULL);
	return Pin;
}

UEdGraphPin* UK2Node_AssignmentStatement::GetVariablePin() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* Pin = FindPin(VariablePinName);
	check(Pin != NULL);
	return Pin;
}

UEdGraphPin* UK2Node_AssignmentStatement::GetValuePin() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* Pin = FindPin(ValuePinName);
	check(Pin != NULL);
	return Pin;
}

FNodeHandlingFunctor* UK2Node_AssignmentStatement::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_AssignmentStatement(CompilerContext);
}

void UK2Node_AssignmentStatement::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
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

FText UK2Node_AssignmentStatement::GetMenuCategory() const
{
	return FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::Macro);
}

#undef LOCTEXT_NAMESPACE
