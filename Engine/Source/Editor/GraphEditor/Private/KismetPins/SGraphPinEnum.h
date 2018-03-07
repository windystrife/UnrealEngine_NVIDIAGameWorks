// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "SGraphPin.h"
#include "SGraphPinComboBox.h"

class GRAPHEDITOR_API SGraphPinEnum : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinEnum) {}
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

	/**
	 *	Function to get current string associated with the combo box selection
	 *
	 *	@return currently selected string
	 */
	FString OnGetText() const;

	/**
	 *	Function to generate the list of indexes from the enum object
	 *
	 *	@param OutComboBoxIndexes - Int array reference to store the list of indexes
	 */
	void GenerateComboBoxIndexes( TArray< TSharedPtr<int32> >& OutComboBoxIndexes );

	/**
	 *	Function to set the newly selected index
	 *
	 * @param NewSelection The newly selected item in the combo box
	 * @param SelectInfo Provides context on how the selection changed
	 */
	void ComboBoxSelectionChanged( TSharedPtr<int32> NewSelection, ESelectInfo::Type SelectInfo );

	/**
	 * Returns the friendly name of the enum at index EnumIndex
	 *
	 * @param EnumIndex	- The index of the enum to return the friendly name for
	 */
	FText OnGetFriendlyName(int32 EnumIndex);

	/**
	 * Returns the tooltip of the enum at index EnumIndex
	 *
	 * @param EnumIndex	- The index of the enum to return the tooltip for
	 */
	FText OnGetTooltip(int32 EnumIndex);

	TSharedPtr<class SPinComboBox>	ComboBox;
};
