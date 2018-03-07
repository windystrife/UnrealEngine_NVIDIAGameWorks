// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "SGraphPin.h"

class GRAPHEDITOR_API SGraphPinColor : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinColor) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

	/** Return the current color value stored in the pin */
	FLinearColor GetColor () const;

protected:
	//~ Begin SGraphPin Interface
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;
	//~ End SGraphPin Interface

	/** Called when clicking on the color button */
	FReply OnColorBoxClicked(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	/** Called when the color picker commits a color value */
	void OnColorCommitted(FLinearColor color);


private:
	/** Current selected color */
	FLinearColor SelectedColor;
	TSharedPtr<SWidget> DefaultValueWidget;
};
