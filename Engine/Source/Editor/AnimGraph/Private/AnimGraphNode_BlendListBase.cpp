// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_BlendListBase.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_BlendListBase

UAnimGraphNode_BlendListBase::UAnimGraphNode_BlendListBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, RemovedPinArrayIndex(INDEX_NONE)
{
}

FLinearColor UAnimGraphNode_BlendListBase::GetNodeTitleColor() const
{
	return FLinearColor(0.2f, 0.8f, 0.2f);
}

void UAnimGraphNode_BlendListBase::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if ((PropertyName == TEXT("Node")))
	{
		ReconstructNode();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

FString UAnimGraphNode_BlendListBase::GetNodeCategory() const
{
	return TEXT("Blends");
}

void UAnimGraphNode_BlendListBase::RemovePinsFromOldPins(TArray<UEdGraphPin*>& OldPins, int32 RemovedArrayIndex)
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

void UAnimGraphNode_BlendListBase::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
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
