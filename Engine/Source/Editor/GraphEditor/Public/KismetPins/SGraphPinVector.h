// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "SGraphPin.h"

class GRAPHEDITOR_API SGraphPinVector : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinVector) {}
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
		TextBox_0,
		TextBox_1,
		TextBox_2,
	};

	/*
	 *	Function to get current value in text box 0
	 *
	 *	@return current string value
	 */
	FString GetCurrentValue_0() const;

	/*
	 *	Function to get current value in text box 1
	 *
	 *	@return current string value
	 */
	FString GetCurrentValue_1() const;

	/*
	 *	Function to get current value in text box 2
	 *
	 *	@return current string value
	 */
	FString GetCurrentValue_2() const;

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
	void OnChangedValueTextBox_0(float NewValue, ETextCommit::Type CommitInfo);

	/*
	 *	Function to store value when text box 1 value in modified
	 *
	 *	@param 0: Updated Float Value
	 */
	void OnChangedValueTextBox_1(float NewValue, ETextCommit::Type CommitInfo);

	/*
	 *	Function to store value when text box 2 value in modified
	 *
	 *	@param 0: Updated Float Value
	 */
	void OnChangedValueTextBox_2(float NewValue, ETextCommit::Type CommitInfo);

private:
	/** Flag is true if the widget is used to represent a rotator; false otherwise */
	bool bIsRotator;
};
