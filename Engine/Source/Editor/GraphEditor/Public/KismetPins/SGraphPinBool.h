// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "SGraphPin.h"

class GRAPHEDITOR_API SGraphPinBool : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinBool) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:
	//~ Begin SGraphPin Interface
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;
	//~ End SGraphPin Interface

	/** Determine if the check box should be checked or not */
	ECheckBoxState IsDefaultValueChecked() const;

	/** Called when check box is changed */
	void OnDefaultValueCheckBoxChanged( ECheckBoxState InIsChecked );
};
