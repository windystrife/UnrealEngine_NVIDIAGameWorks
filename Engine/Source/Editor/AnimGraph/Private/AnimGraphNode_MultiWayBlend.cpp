// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_MultiWayBlend.h"
#include "BlueprintEditorUtils.h"
#include "GraphEditorActions.h"
#include "ScopedTransaction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"


/////////////////////////////////////////////////////
// UAnimGraphNode_MultiWayBlend

#define LOCTEXT_NAMESPACE "AnimGraphNode_MultiWayBlend"

UAnimGraphNode_MultiWayBlend::UAnimGraphNode_MultiWayBlend(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FString UAnimGraphNode_MultiWayBlend::GetNodeCategory() const
{
	return TEXT("Blends");
}

FLinearColor UAnimGraphNode_MultiWayBlend::GetNodeTitleColor() const
{
	return FLinearColor(0.75f, 0.75f, 0.75f);
}

FText UAnimGraphNode_MultiWayBlend::GetTooltipText() const
{
	return LOCTEXT("MultiWayBlendTooltip", "Blend multiple poses together by Alpha");
}

FText UAnimGraphNode_MultiWayBlend::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Blend", "Blend Multi");
}

void UAnimGraphNode_MultiWayBlend::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	if (!Context.bIsDebugging)
	{
		Context.MenuBuilder->BeginSection("AnimGraphBlendMulti", LOCTEXT("BlendMultiHeader", "BlendMulti"));
		{
			if (Context.Pin != NULL)
			{
				// we only do this for normal BlendMulti/BlendMulti by enum, BlendMulti by Bool doesn't support add/remove pins
				if (Context.Pin->Direction == EGPD_Input)
				{
					Context.MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().RemoveBlendListPin);
				}
			}
			else
			{
				Context.MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().AddBlendListPin);
			}
		}
		Context.MenuBuilder->EndSection();
	}
}

void UAnimGraphNode_MultiWayBlend::AddPinToBlendNode()
{
	FScopedTransaction Transaction(LOCTEXT("AddBlendMultiPin", "AddBlendMultiPin"));
	Modify();

	Node.AddPose();
	ReconstructNode();

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
}

void UAnimGraphNode_MultiWayBlend::RemovePinFromBlendNode(UEdGraphPin* Pin)
{
	FScopedTransaction Transaction(LOCTEXT("RemoveBlendMultiPin", "RemoveBlendMultiPin"));
	Modify();

	UProperty* AssociatedProperty;
	int32 ArrayIndex;
	GetPinAssociatedProperty(GetFNodeType(), Pin, /*out*/ AssociatedProperty, /*out*/ ArrayIndex);

	if (ArrayIndex != INDEX_NONE)
	{
		//@TODO: ANIMREFACTOR: Need to handle moving pins below up correctly
		// setting up removed pins info
		RemovedPinArrayIndex = ArrayIndex;
		Node.RemovePose(ArrayIndex);
		// removes the selected pin and related properties in reconstructNode()
		// @TODO: Considering passing "RemovedPinArrayIndex" to ReconstructNode as the argument
		ReconstructNode();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
	}
}

void UAnimGraphNode_MultiWayBlend::PostPlacedNewNode()
{
	// Make sure we start out with two inputs
	Node.AddPose();
	Node.AddPose();
	ReconstructNode();
}

void UAnimGraphNode_MultiWayBlend::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	Super::ReallocatePinsDuringReconstruction(OldPins);

	// Delete Pins by removed pin info 
	if (RemovedPinArrayIndex != INDEX_NONE)
	{
		RemovePinsFromOldPins(OldPins, RemovedPinArrayIndex);
		// Clears removed pin info to avoid to remove multiple times
		// @TODO : Considering receiving RemovedPinArrayIndex as an argument of ReconstructNode()
		RemovedPinArrayIndex = INDEX_NONE;
	}
}

void UAnimGraphNode_MultiWayBlend::RemovePinsFromOldPins(TArray<UEdGraphPin*>& OldPins, int32 RemovedArrayIndex)
{
	TArray<FString> RemovedPropertyNames;
	TArray<FString> NewPinNames;

	// Store new pin names to compare with old pin names
	for (int32 NewPinIndx = 0; NewPinIndx < Pins.Num(); NewPinIndx++)
	{
		NewPinNames.Add(Pins[NewPinIndx]->PinName);
	}

	// don't know which pins are removed yet so find removed pins comparing NewPins and OldPins
	for (int32 OldPinIdx = 0; OldPinIdx < OldPins.Num(); OldPinIdx++)
	{
		FString& OldPinName = OldPins[OldPinIdx]->PinName;
		if (!NewPinNames.Contains(OldPinName))
		{
			int32 UnderscoreIndex = OldPinName.Find(TEXT("_"));
			if (UnderscoreIndex != INDEX_NONE)
			{
				FString PropertyName = OldPinName.Left(UnderscoreIndex);
				RemovedPropertyNames.Add(PropertyName);
			}
		}
	}

	for (int32 PinIdx = 0; PinIdx < OldPins.Num(); PinIdx++)
	{
		// Separate the pin name into property name and index
		FString PropertyName;
		int32 ArrayIndex = -1;
		FString& OldPinName = OldPins[PinIdx]->PinName;

		int32 UnderscoreIndex = OldPinName.Find(TEXT("_"));
		if (UnderscoreIndex != INDEX_NONE)
		{
			PropertyName = OldPinName.Left(UnderscoreIndex);
			ArrayIndex = FCString::Atoi(*(OldPinName.Mid(UnderscoreIndex + 1)));

			if (RemovedPropertyNames.Contains(PropertyName))
			{
				// if array index is matched, removes pins 
				// and if array index is greater than removed index, decrease index
				if (ArrayIndex == RemovedArrayIndex)
				{
					OldPins[PinIdx]->MarkPendingKill();
					OldPins.RemoveAt(PinIdx);
					--PinIdx;
				}
				else
					if (ArrayIndex > RemovedArrayIndex)
					{
						OldPinName = FString::Printf(TEXT("%s_%d"), *PropertyName, ArrayIndex - 1);
					}
			}
		}
	}
}
#undef LOCTEXT_NAMESPACE

