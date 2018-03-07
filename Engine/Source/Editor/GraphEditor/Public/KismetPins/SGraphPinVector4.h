// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "SGraphPin.h"

class GRAPHEDITOR_API SGraphPinVector4 : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinVector4) {}
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
		TextBox_3,
	};

	FString GetCurrentValue_0() const;
	FString GetCurrentValue_1() const;
	FString GetCurrentValue_2() const;
	FString GetCurrentValue_3() const;

	/*
	*	Function to getch current value based on text box index value
	*
	*	@param: Text box index
	*
	*	@return current string value
	*/
	FString GetValue(ETextBoxIndex Index) const;

	/*
	*	Function to store value when text box value in modified
	*
	*	@param 0: Updated Float Value
	*/
	void OnChangedValueTextBox_0(float NewValue, ETextCommit::Type CommitInfo);
	void OnChangedValueTextBox_1(float NewValue, ETextCommit::Type CommitInfo);
	void OnChangedValueTextBox_2(float NewValue, ETextCommit::Type CommitInfo);
	void OnChangedValueTextBox_3(float NewValue, ETextCommit::Type CommitInfo);

	static FVector4 ParseValue(FString Value);
private:
	/** Flag is true if the widget is used to represent a rotator; false otherwise */
	bool bIsRotator;
};
