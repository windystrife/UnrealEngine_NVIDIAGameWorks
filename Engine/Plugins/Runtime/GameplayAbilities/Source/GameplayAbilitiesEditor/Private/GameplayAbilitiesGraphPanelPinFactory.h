// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "AttributeSet.h"
#include "EdGraphUtilities.h"
#include "EdGraphSchema_K2.h"
#include "SGraphPin.h"
#include "SGameplayAttributeGraphPin.h"

class FGameplayAbilitiesGraphPanelPinFactory: public FGraphPanelPinFactory
{
	virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* InPin) const override
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		if (InPin->PinType.PinCategory == K2Schema->PC_Struct && InPin->PinType.PinSubCategoryObject == FGameplayAttribute::StaticStruct())
		{
			return SNew(SGameplayAttributeGraphPin, InPin);
		}
		return NULL;
	}
};
