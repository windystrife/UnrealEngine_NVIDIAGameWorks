// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "SGraphPin.h"

class SGraphPinKey : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinKey) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:

	/**
	 *	Function to create class specific widget.
	 *
	 *	@return Reference to the newly created widget object
	 */
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;

private:

	/** Gets the current Key being edited. */
	TOptional<FKey> GetCurrentKey() const;

	/** Updates the pin default when a new key is selected. */
	void OnKeyChanged(TSharedPtr<FKey> SelectedKey);

	FKey SelectedKey;
};
