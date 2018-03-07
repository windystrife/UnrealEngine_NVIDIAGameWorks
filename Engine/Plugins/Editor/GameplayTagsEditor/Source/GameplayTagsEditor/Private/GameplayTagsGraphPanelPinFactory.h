// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "EdGraphUtilities.h"
#include "GameplayTagContainer.h"
#include "EdGraphSchema_K2.h"
#include "SGraphPin.h"
#include "SGameplayTagGraphPin.h"
#include "SGameplayTagContainerGraphPin.h"
#include "SGameplayTagQueryGraphPin.h"

class FGameplayTagsGraphPanelPinFactory: public FGraphPanelPinFactory
{
	virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* InPin) const override
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		if (InPin->PinType.PinCategory == K2Schema->PC_Struct && InPin->PinType.PinSubCategoryObject == FGameplayTag::StaticStruct())
		{
			return SNew(SGameplayTagGraphPin, InPin);
		}
		if (InPin->PinType.PinCategory == K2Schema->PC_Struct && InPin->PinType.PinSubCategoryObject == FGameplayTagContainer::StaticStruct())
		{
			return SNew(SGameplayTagContainerGraphPin, InPin);
		}
		if (InPin->PinType.PinCategory == K2Schema->PC_String && InPin->PinType.PinSubCategory == TEXT("LiteralGameplayTagContainer"))
		{
			return SNew(SGameplayTagContainerGraphPin, InPin);
		}
		if (InPin->PinType.PinCategory == K2Schema->PC_Struct && InPin->PinType.PinSubCategoryObject == FGameplayTagQuery::StaticStruct())
		{
			return SNew(SGameplayTagQueryGraphPin, InPin);
		}

		return nullptr;
	}
};
