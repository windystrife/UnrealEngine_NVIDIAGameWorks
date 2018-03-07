// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "SGraphPin.h"

class GRAPHEDITOR_API SGraphPinVector2D : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinVector2D) {}
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

	// Enum values to identify text boxes.
	enum ETextBoxIndex
	{
		TextBox_X,
		TextBox_Y
	};

	/*
	 *	Function to get current value in text box 0
	 *
	 *	@return current string value
	 */
	FString GetCurrentValue_X() const;

	/*
	 *	Function to get current value in text box 1
	 *
	 *	@return current string value
	 */
	FString GetCurrentValue_Y() const;

	/*
	 *	Function to getch current value based on text box index value
	 *
	 *	@param: Text box index
	 *
	 *	@return current string value
	 */
	FString GetValue( ETextBoxIndex Index ) const;

	/*
	 *	Function to store value when text box 0 value in modified
	 *
	 *	@param 0: Updated Float Value
	 */
	void OnChangedValueTextBox_X(float NewValue, ETextCommit::Type CommitInfo);

	/*
	 *	Function to store value when text box 1 value in modified
	 *
	 *	@param 0: Updated Float Value
	 */
	void OnChangedValueTextBox_Y(float NewValue, ETextCommit::Type CommitInfo);
};
