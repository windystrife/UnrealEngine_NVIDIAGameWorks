// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "SGraphPin.h"

/** A graph pin widget for displaying niagara numeric inputs and outputs. */
class SNiagaraGraphPinNumeric : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SNiagaraGraphPinNumeric) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);
};