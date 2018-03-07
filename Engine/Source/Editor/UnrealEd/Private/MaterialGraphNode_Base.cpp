// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MaterialGraphNode_Base.cpp
=============================================================================*/

#include "MaterialGraph/MaterialGraphNode_Base.h"
#include "EdGraph/EdGraphSchema.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphSchema.h"

/////////////////////////////////////////////////////
// UMaterialGraphNode_Base

UMaterialGraphNode_Base::UMaterialGraphNode_Base(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UEdGraphPin* UMaterialGraphNode_Base::GetInputPin(int32 InputIndex) const
{
	for (int32 PinIndex = 0, FoundInputs = 0; PinIndex < Pins.Num(); PinIndex++)
	{
		if (Pins[PinIndex]->Direction == EGPD_Input)
		{
			if (InputIndex == FoundInputs)
			{
				return Pins[PinIndex];
			}
			else
			{
				FoundInputs++;
			}
		}
	}

	return NULL;
}

void UMaterialGraphNode_Base::GetInputPins(TArray<class UEdGraphPin*>& OutInputPins) const
{
	OutInputPins.Empty();

	for (int32 PinIndex = 0; PinIndex < Pins.Num(); PinIndex++)
	{
		if (Pins[PinIndex]->Direction == EGPD_Input)
		{
			OutInputPins.Add(Pins[PinIndex]);
		}
	}
}

UEdGraphPin* UMaterialGraphNode_Base::GetOutputPin(int32 OutputIndex) const
{
	for (int32 PinIndex = 0, FoundOutputs = 0; PinIndex < Pins.Num(); PinIndex++)
	{
		if (Pins[PinIndex]->Direction == EGPD_Output)
		{
			if (OutputIndex == FoundOutputs)
			{
				return Pins[PinIndex];
			}
			else
			{
				FoundOutputs++;
			}
		}
	}

	return NULL;
}

void UMaterialGraphNode_Base::GetOutputPins(TArray<class UEdGraphPin*>& OutOutputPins) const
{
	OutOutputPins.Empty();

	for (int32 PinIndex = 0; PinIndex < Pins.Num(); PinIndex++)
	{
		if (Pins[PinIndex]->Direction == EGPD_Output)
		{
			OutOutputPins.Add(Pins[PinIndex]);
		}
	}
}

void UMaterialGraphNode_Base::ReplaceNode(UMaterialGraphNode_Base* OldNode)
{
	check(OldNode);
	check(OldNode != this);

	// Get Pins from node passed in
	TArray<UEdGraphPin*> OldInputPins;
	TArray<UEdGraphPin*> OldOutputPins;
	OldNode->GetInputPins(OldInputPins);
	OldNode->GetOutputPins(OldOutputPins);

	// Get our Input and Output pins
	TArray<UEdGraphPin*> NewInputPins;
	TArray<UEdGraphPin*> NewOutputPins;
	GetInputPins(NewInputPins);
	GetOutputPins(NewOutputPins);

	// Copy Inputs from old node
	for (int32 PinIndex = 0; PinIndex < OldInputPins.Num(); PinIndex++)
	{
		if (PinIndex < NewInputPins.Num())
		{
			ModifyAndCopyPersistentPinData(*NewInputPins[PinIndex], *OldInputPins[PinIndex]);
		}
	}

	// Copy Outputs from old node
	for (int32 PinIndex = 0; PinIndex < OldOutputPins.Num(); PinIndex++)
	{
		// If we can't find an equivalent output in this node, just use the first
		// The user will have to fix up any issues from the mismatch
		int32 FoundPinIndex = 0;

		// Try to find an equivalent output in this node
		for (int32 NewPinIndex = 0; NewPinIndex < NewOutputPins.Num(); NewPinIndex++)
		{
			if (OldOutputPins[PinIndex]->PinType == NewOutputPins[NewPinIndex]->PinType)
			{
				FoundPinIndex = NewPinIndex;
				break;
			}
		}
		
		if (FoundPinIndex < NewOutputPins.Num())
		{
			ModifyAndCopyPersistentPinData(*NewOutputPins[FoundPinIndex], *OldOutputPins[PinIndex]);
		}
	}

	// Break the original pin links
	for (int32 OldPinIndex = 0; OldPinIndex < OldNode->Pins.Num(); ++OldPinIndex)
	{
		UEdGraphPin* OldPin = OldNode->Pins[OldPinIndex];
		OldPin->Modify();
		OldPin->BreakAllPinLinks();
	}
}

void UMaterialGraphNode_Base::InsertNewNode(UEdGraphPin* FromPin, UEdGraphPin* NewLinkPin, TSet<UEdGraphNode*>& OutNodeList)
{
	const UMaterialGraphSchema* Schema = CastChecked<UMaterialGraphSchema>(GetSchema());

	// The pin we are creating from already has a connection that needs to be broken. We want to "insert" the new node in between, so that the output of the new node is hooked up too
	UEdGraphPin* OldLinkedPin = FromPin->LinkedTo[0];
	check(OldLinkedPin);

	FromPin->BreakAllPinLinks();

	// Hook up the old linked pin to the first valid output pin on the new node
	for (int32 OutpinPinIdx=0; OutpinPinIdx<Pins.Num(); OutpinPinIdx++)
	{
		UEdGraphPin* OutputPin = Pins[OutpinPinIdx];
		check(OutputPin);
		if (ECanCreateConnectionResponse::CONNECT_RESPONSE_MAKE == Schema->CanCreateConnection(OldLinkedPin, OutputPin).Response)
		{
			if (Schema->TryCreateConnection(OldLinkedPin, OutputPin))
			{
				OutNodeList.Add(OldLinkedPin->GetOwningNode());
				OutNodeList.Add(this);
			}
			break;
		}
	}

	if (Schema->TryCreateConnection(FromPin, NewLinkPin))
	{
		OutNodeList.Add(FromPin->GetOwningNode());
		OutNodeList.Add(this);
	}
}

void UMaterialGraphNode_Base::AllocateDefaultPins()
{
	check(Pins.Num() == 0);

	CreateInputPins();
	CreateOutputPins();
}

void UMaterialGraphNode_Base::ReconstructNode()
{
	Modify();

	// Break any links to 'orphan' pins
	for (int32 PinIndex = 0; PinIndex < Pins.Num(); ++PinIndex)
	{
		UEdGraphPin* Pin = Pins[PinIndex];
		TArray<class UEdGraphPin*>& LinkedToRef = Pin->LinkedTo;
		for (int32 LinkIdx=0; LinkIdx < LinkedToRef.Num(); LinkIdx++)
		{
			UEdGraphPin* OtherPin = LinkedToRef[LinkIdx];
			// If we are linked to a pin that its owner doesn't know about, break that link
			if (!OtherPin->GetOwningNode()->Pins.Contains(OtherPin))
			{
				Pin->LinkedTo.Remove(OtherPin);
			}
		}
	}

	// Store the old Input and Output pins
	TArray<UEdGraphPin*> OldInputPins;
	TArray<UEdGraphPin*> OldOutputPins;
	GetInputPins(OldInputPins);
	GetOutputPins(OldOutputPins);

	// Move the existing pins to a saved array
	TArray<UEdGraphPin*> OldPins(Pins);
	Pins.Reset();

	// Recreate the new pins
	AllocateDefaultPins();

	// Get new Input and Output pins
	TArray<UEdGraphPin*> NewInputPins;
	TArray<UEdGraphPin*> NewOutputPins;
	GetInputPins(NewInputPins);
	GetOutputPins(NewOutputPins);

	for (int32 PinIndex = 0; PinIndex < OldInputPins.Num(); PinIndex++)
	{
		if (PinIndex < NewInputPins.Num())
		{
			NewInputPins[PinIndex]->MovePersistentDataFromOldPin(*OldInputPins[PinIndex]);
		}
	}

	for (int32 PinIndex = 0; PinIndex < OldOutputPins.Num(); PinIndex++)
	{
		if (PinIndex < NewOutputPins.Num())
		{
			NewOutputPins[PinIndex]->MovePersistentDataFromOldPin(*OldOutputPins[PinIndex]);
		}
	}

	// Throw away the original pins
	for (UEdGraphPin* OldPin : OldPins)
	{
		OldPin->Modify();
		UEdGraphNode::DestroyPin(OldPin);
	}

	GetGraph()->NotifyGraphChanged();
}

void UMaterialGraphNode_Base::RemovePinAt(const int32 PinIndex, const EEdGraphPinDirection PinDirection)
{
	Super::RemovePinAt(PinIndex, PinDirection);

	UMaterialGraph* MaterialGraph = CastChecked<UMaterialGraph>(GetGraph());
	MaterialGraph->LinkMaterialExpressionsFromGraph();
}

void UMaterialGraphNode_Base::AutowireNewNode(UEdGraphPin* FromPin)
{
	if (FromPin != NULL)
	{
		const UMaterialGraphSchema* Schema = CastChecked<UMaterialGraphSchema>(GetSchema());

		TSet<UEdGraphNode*> NodeList;

		// auto-connect from dragged pin to first compatible pin on the new node
		for (int32 i=0; i<Pins.Num(); i++)
		{
			UEdGraphPin* Pin = Pins[i];
			check(Pin);
			FPinConnectionResponse Response = Schema->CanCreateConnection(FromPin, Pin);
			if (ECanCreateConnectionResponse::CONNECT_RESPONSE_MAKE == Response.Response)
			{
				if (Schema->TryCreateConnection(FromPin, Pin))
				{
					NodeList.Add(FromPin->GetOwningNode());
					NodeList.Add(this);
				}
				break;
			}
			else if(ECanCreateConnectionResponse::CONNECT_RESPONSE_BREAK_OTHERS_A == Response.Response)
			{
				InsertNewNode(FromPin, Pin, NodeList);
				break;
			}
		}

		// Send all nodes that received a new pin connection a notification
		for (auto It = NodeList.CreateConstIterator(); It; ++It)
		{
			UEdGraphNode* Node = (*It);
			Node->NodeConnectionListChanged();
		}
	}
}

bool UMaterialGraphNode_Base::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const
{
	return Schema->IsA(UMaterialGraphSchema::StaticClass());
}

void UMaterialGraphNode_Base::ModifyAndCopyPersistentPinData(UEdGraphPin& TargetPin, const UEdGraphPin& SourcePin) const
{
	if (SourcePin.LinkedTo.Num() > 0)
	{
		TargetPin.Modify();

		for (int32 LinkIndex = 0; LinkIndex < SourcePin.LinkedTo.Num(); ++LinkIndex)
		{
			UEdGraphPin* OtherPin = SourcePin.LinkedTo[LinkIndex];
			OtherPin->Modify();
		}
	}

	TargetPin.CopyPersistentDataFromOldPin(SourcePin);
}

FString UMaterialGraphNode_Base::GetDocumentationLink() const
{
	return TEXT("Shared/GraphNodes/Material");
}

uint32 UMaterialGraphNode_Base::GetInputType(const UEdGraphPin* InputPin) const
{
	return MCT_Unknown;
}
