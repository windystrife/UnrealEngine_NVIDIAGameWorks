// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateColor.h"
#include "SGraphPin.h"

class SGraphPinMaterialInput : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinMaterialInput) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:
	//~ Begin SGraphPin Interface
	virtual FSlateColor GetPinColor() const override;
	//~ End SGraphPin Interface

};
