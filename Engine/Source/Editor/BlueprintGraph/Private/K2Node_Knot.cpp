// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "K2Node_Knot.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"

#define LOCTEXT_NAMESPACE "K2Node_Knot"

/////////////////////////////////////////////////////
// UK2Node_Knot

UK2Node_Knot::UK2Node_Knot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCanRenameNode = true;
}

void UK2Node_Knot::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	const FString InputPinName(TEXT("InputPin"));
	const FString OutputPinName(TEXT("OutputPin"));

	UEdGraphPin* MyInputPin = CreatePin(EGPD_Input, Schema->PC_Wildcard, FString(), nullptr, InputPinName);
	MyInputPin->bDefaultValueIsIgnored = true;

	UEdGraphPin* MyOutputPin = CreatePin(EGPD_Output, Schema->PC_Wildcard, FString(), nullptr, OutputPinName);
}

FText UK2Node_Knot::GetTooltipText() const
{
	//@TODO: Should pull the tooltip from the source pin
	return LOCTEXT("KnotTooltip", "Reroute Node (reroutes wires)");
}

FText UK2Node_Knot::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType == ENodeTitleType::EditableTitle)
	{
		return FText::FromString(NodeComment);
	}
	else if (TitleType == ENodeTitleType::MenuTitle)
	{
		return LOCTEXT("KnotListTitle", "Add Reroute Node...");
	}
	else
	{
		return LOCTEXT("KnotTitle", "Reroute Node");
	}
}

void UK2Node_Knot::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* MyInputPin = GetInputPin();
	UEdGraphPin* MyOutputPin = GetOutputPin();

	K2Schema->CombineTwoPinNetsAndRemoveOldPins(MyInputPin, MyOutputPin);
}

bool UK2Node_Knot::IsNodeSafeToIgnore() const
{
	return true;
}

bool UK2Node_Knot::CanSplitPin(const UEdGraphPin* Pin) const
{
	return false;
}

void UK2Node_Knot::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	PropagatePinType();
}

void UK2Node_Knot::PostReconstructNode()
{
	PropagatePinType();
	Super::PostReconstructNode();
}

void UK2Node_Knot::PropagatePinType()
{
	UEdGraphPin* MyInputPin  = GetInputPin();
	UEdGraphPin* MyOutputPin = GetOutputPin();

	for (UEdGraphPin* Inputs : MyInputPin->LinkedTo)
	{
		if (Inputs->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
		{
			PropagatePinTypeFromInput();
			return;
		}
	}

	for (UEdGraphPin* Outputs : MyOutputPin->LinkedTo)
	{
		if (Outputs->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
		{
			PropagatePinTypeFromOutput();
			return;
		}
	}

	// if all inputs/outputs are wildcards, still favor the inputs first (propagate array/reference/etc. state)
	if (MyInputPin->LinkedTo.Num() > 0)
	{
		// If we can't mirror from output type, we should at least get the type information from the input connection chain
		PropagatePinTypeFromInput();
	}
	else if (MyOutputPin->LinkedTo.Num() > 0)
	{
		// Try to mirror from output first to make sure we get appropriate member references
		PropagatePinTypeFromOutput();
	}
	else
	{
		// Revert to wildcard
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

		MyInputPin->BreakAllPinLinks();
		MyInputPin->PinType.ResetToDefaults();
		MyInputPin->PinType.PinCategory = K2Schema->PC_Wildcard;

		MyOutputPin->BreakAllPinLinks();
		MyOutputPin->PinType.ResetToDefaults();
		MyOutputPin->PinType.PinCategory = K2Schema->PC_Wildcard;
	}
}

void UK2Node_Knot::PropagatePinTypeFromInput()
{
	if (bRecursionGuard)
	{
		return;
	}
	// Set the type of the pin based on input connections.
	// We have to move up the chain of linked reroute nodes until we reach a node
	// with type information before percolating that information down.
	UEdGraphPin* MyInputPin = GetInputPin();
	UEdGraphPin* MyOutputPin = GetOutputPin();

	TGuardValue<bool> RecursionGuard(bRecursionGuard, true);

	for (UEdGraphPin* InPin : MyInputPin->LinkedTo)
	{
		if (UK2Node_Knot* KnotNode = Cast<UK2Node_Knot>(InPin->GetOwningNode()))
		{
			KnotNode->PropagatePinTypeFromInput();
		}
	}

	UEdGraphPin* TypeSource = MyInputPin->LinkedTo.Num() ? MyInputPin->LinkedTo[0] : nullptr;
	if (TypeSource)
	{
		MyInputPin->PinType = TypeSource->PinType;
		MyOutputPin->PinType = TypeSource->PinType;

		for (UEdGraphPin* InPin : MyInputPin->LinkedTo)
		{
			if (UK2Node* OwningNode = Cast<UK2Node>(InPin->GetOwningNode()))
			{
				if (!OwningNode->IsA<UK2Node_Knot>())
				{
					OwningNode->PinConnectionListChanged(InPin);
				}
			}
		}
	}
	else
	{
		// TODO?
	}
}

void UK2Node_Knot::PropagatePinTypeFromOutput()
{
	if (bRecursionGuard)
	{
		return;
	}
	// Set the type of the pin based on the output connection, and then percolate
	// that type information up until we no longer reach another Reroute node
	UEdGraphPin* MyInputPin = GetInputPin();
	UEdGraphPin* MyOutputPin = GetOutputPin();

	TGuardValue<bool> RecursionGuard(bRecursionGuard, true);

	for (UEdGraphPin* InPin : MyOutputPin->LinkedTo)
	{
		if (UK2Node_Knot* KnotNode = Cast<UK2Node_Knot>(InPin->GetOwningNode()))
		{
			KnotNode->PropagatePinTypeFromOutput();
		}
	}

	UEdGraphPin* TypeSource = MyOutputPin->LinkedTo.Num() ? MyOutputPin->LinkedTo[0] : nullptr;
	if (TypeSource)
	{
		MyInputPin->PinType = TypeSource->PinType;
		MyOutputPin->PinType = TypeSource->PinType;

		for (UEdGraphPin* InPin : MyInputPin->LinkedTo)
		{
			if (UK2Node* OwningNode = Cast<UK2Node>(InPin->GetOwningNode()))
			{
				if (UK2Node_Knot* KnotNode = Cast<UK2Node_Knot>(OwningNode))
				{
					KnotNode->PropagatePinTypeFromOutput();
				}
				else
				{
					OwningNode->PinConnectionListChanged(InPin);
				}
			}
		}
	}
	else 
	{
		// TODO?
	}
}

void UK2Node_Knot::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
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

bool UK2Node_Knot::ShouldOverridePinNames() const
{
	return true;
}

FText UK2Node_Knot::GetPinNameOverride(const UEdGraphPin& Pin) const
{
	// Keep the pin size tiny
	return FText::GetEmpty();
}

void UK2Node_Knot::OnRenameNode(const FString& NewName)
{
	NodeComment = NewName;
}

TSharedPtr<class INameValidatorInterface> UK2Node_Knot::MakeNameValidator() const
{
	// Comments can be duplicated, etc...
	return MakeShareable(new FDummyNameValidator(EValidatorResult::Ok));
}

UEdGraphPin* UK2Node_Knot::GetPassThroughPin(const UEdGraphPin* FromPin) const
{
	if(FromPin && Pins.Contains(FromPin))
	{
		return FromPin == Pins[0] ? Pins[1] : Pins[0];
	}

	return nullptr;
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
