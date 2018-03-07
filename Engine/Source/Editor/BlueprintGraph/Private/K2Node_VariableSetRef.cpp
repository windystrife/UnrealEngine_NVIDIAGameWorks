// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "K2Node_VariableSetRef.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphUtilities.h"
#include "KismetCompiler.h"
#include "VariableSetHandler.h"
#include "BlueprintActionFilter.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"

static FString TargetVarPinName(TEXT("Target"));
static FString VarValuePinName(TEXT("Value"));

#define LOCTEXT_NAMESPACE "K2Node_VariableSetRef"

class FKCHandler_VariableSetRef : public FKCHandler_VariableSet
{
public:
	FKCHandler_VariableSetRef(FKismetCompilerContext& InCompilerContext)
		: FKCHandler_VariableSet(InCompilerContext)
	{
	}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_VariableSetRef* VarRefNode = CastChecked<UK2Node_VariableSetRef>(Node);
		UEdGraphPin* ValuePin = VarRefNode->GetValuePin();
		ValidateAndRegisterNetIfLiteral(Context, ValuePin);
	}

	void InnerAssignment(FKismetFunctionContext& Context, UEdGraphNode* Node, UEdGraphPin* VariablePin, UEdGraphPin* ValuePin)
	{
		FBPTerminal** VariableTerm = Context.NetMap.Find(VariablePin);
		if (VariableTerm == NULL)
		{
			VariableTerm = Context.NetMap.Find(FEdGraphUtilities::GetNetFromPin(VariablePin));
		}

		FBPTerminal** ValueTerm = Context.LiteralHackMap.Find(ValuePin);
		if (ValueTerm == NULL)
		{
			ValueTerm = Context.NetMap.Find(FEdGraphUtilities::GetNetFromPin(ValuePin));
		}

		if ((VariableTerm != NULL) && (ValueTerm != NULL))
		{
			FBlueprintCompiledStatement& Statement = Context.AppendStatementForNode(Node);

			Statement.Type = KCST_Assignment;
			Statement.LHS = *VariableTerm;
			Statement.RHS.Add(*ValueTerm);

			if (!(*VariableTerm)->IsTermWritable())
			{
				CompilerContext.MessageLog.Error(*LOCTEXT("WriteConst_Error", "Cannot write to const @@").ToString(), VariablePin);
			}
		}
		else
		{
			if (VariablePin != ValuePin)
			{
				CompilerContext.MessageLog.Error(*LOCTEXT("ResolveValueIntoVariablePin_Error", "Failed to resolve term @@ passed into @@").ToString(), ValuePin, VariablePin);
			}
			else
			{
				CompilerContext.MessageLog.Error(*LOCTEXT("ResolveTermPassed_Error", "Failed to resolve term passed into @@").ToString(), VariablePin);
			}
		}
	}

	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_VariableSetRef* VarRefNode = CastChecked<UK2Node_VariableSetRef>(Node);
		UEdGraphPin* VarTargetPin = VarRefNode->GetTargetPin();
		UEdGraphPin* ValuePin = VarRefNode->GetValuePin();

		InnerAssignment(Context, Node, VarTargetPin, ValuePin);

		// Generate the output impulse from this node
		GenerateSimpleThenGoto(Context, *Node);
	}
};

UK2Node_VariableSetRef::UK2Node_VariableSetRef(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_VariableSetRef::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	CreatePin(EGPD_Input, K2Schema->PC_Exec, FString(), nullptr, K2Schema->PN_Execute);
	CreatePin(EGPD_Output, K2Schema->PC_Exec, FString(), nullptr, K2Schema->PN_Then);

	CreatePin(EGPD_Input, K2Schema->PC_Wildcard, FString(), nullptr, TargetVarPinName, EPinContainerType::None, true);
	UEdGraphPin* ValuePin = CreatePin(EGPD_Input, K2Schema->PC_Wildcard, FString(), nullptr, VarValuePinName);
}

void UK2Node_VariableSetRef::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	AllocateDefaultPins();

 	// Coerce the type of the node from the old pin, if available
  	UEdGraphPin* OldTargetPin = NULL;
  	for( auto OldPinIt = OldPins.CreateIterator(); OldPinIt; ++OldPinIt )
  	{
  		UEdGraphPin* CurrPin = *OldPinIt;
  		if( CurrPin->PinName == TargetVarPinName )
  		{
  			OldTargetPin = CurrPin;
  			break;
  		}
  	}
  
  	if( OldTargetPin )
  	{
  		UEdGraphPin* NewTargetPin = GetTargetPin();
  		CoerceTypeFromPin(OldTargetPin);
  	}
	CachedNodeTitle.MarkDirty();

	RestoreSplitPins(OldPins);
}

FText UK2Node_VariableSetRef::GetTooltipText() const
{
	return NSLOCTEXT("K2Node", "SetValueOfRefVariable", "Set the value of the connected pass-by-ref variable");
}

FText UK2Node_VariableSetRef::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* TargetPin = GetTargetPin();
	if ((TargetPin == nullptr) || (TargetPin->PinType.PinCategory == Schema->PC_Wildcard))
	{
		return NSLOCTEXT("K2Node", "SetRefVarNodeTitle", "Set By-Ref Var");
	}
	else if (CachedNodeTitle.IsOutOfDate(this))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("PinType"), Schema->TypeToText(TargetPin->PinType));
		// FText::Format() is slow, so we cache this to save on performance
		CachedNodeTitle.SetCachedText(FText::Format(NSLOCTEXT("K2Node", "SetRefVarNodeTitle_Typed", "Set {PinType}"), Args), this);
	}
	return CachedNodeTitle;
}

bool UK2Node_VariableSetRef::IsActionFilteredOut(class FBlueprintActionFilter const& Filter)
{
	// Default to filtering this node out unless dragging off of a reference output pin
	bool bIsFilteredOut = false;
	FBlueprintActionContext const& FilterContext = Filter.Context;

	for (UEdGraphPin* Pin : FilterContext.Pins)
	{
		if(Pin->Direction == EGPD_Output && Pin->PinType.bIsReference == true)
		{
			bIsFilteredOut = false;
			break;
		}
	}
	return bIsFilteredOut;
}

void UK2Node_VariableSetRef::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

	UEdGraphPin* TargetPin = GetTargetPin();
	UEdGraphPin* ValuePin = GetValuePin();

	if( (Pin == TargetPin) || (Pin == ValuePin) )
	{
		UEdGraphPin* ConnectedToPin = (Pin->LinkedTo.Num() > 0) ? Pin->LinkedTo[0] : NULL;
		CoerceTypeFromPin(ConnectedToPin);
	}
}

void UK2Node_VariableSetRef::CoerceTypeFromPin(const UEdGraphPin* Pin)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* TargetPin = GetTargetPin();
	UEdGraphPin* ValuePin = GetValuePin();

	check(TargetPin && ValuePin);

	if( Pin )
	{
		check((Pin != TargetPin) || (Pin->PinType.bIsReference && !Pin->PinType.IsContainer()));

		TargetPin->PinType = Pin->PinType;
		TargetPin->PinType.bIsReference = true;

		ValuePin->PinType = Pin->PinType;
		ValuePin->PinType.bIsReference = false;
	}
	else
	{
		// Pin disconnected...revert to wildcard
		TargetPin->PinType.PinCategory = K2Schema->PC_Wildcard;
		TargetPin->PinType.PinSubCategory.Reset();
		TargetPin->PinType.PinSubCategoryObject = nullptr;
		TargetPin->BreakAllPinLinks();

		ValuePin->PinType.PinCategory = K2Schema->PC_Wildcard;
		ValuePin->PinType.PinSubCategory.Reset();
		ValuePin->PinType.PinSubCategoryObject = nullptr;
		ValuePin->BreakAllPinLinks();

		CachedNodeTitle.MarkDirty();
	}
}

UEdGraphPin* UK2Node_VariableSetRef::GetTargetPin() const
{
	return FindPin(TargetVarPinName);
}

UEdGraphPin* UK2Node_VariableSetRef::GetValuePin() const
{
	return FindPin(VarValuePinName);
}

FNodeHandlingFunctor* UK2Node_VariableSetRef::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_VariableSetRef(CompilerContext);
}

void UK2Node_VariableSetRef::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
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

FText UK2Node_VariableSetRef::GetMenuCategory() const
{
	return FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::Variables);
}


#undef LOCTEXT_NAMESPACE
