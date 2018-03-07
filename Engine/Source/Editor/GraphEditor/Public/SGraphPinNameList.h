// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "SGraphPin.h"
#include "SNameComboBox.h"

class GRAPHEDITOR_API SGraphPinNameList : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinNameList) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj, const TArray<TSharedPtr<FName>>& InNameList);

protected:

	/**
	 *	Function to create class specific widget.
	 *
	 *	@return Reference to the newly created widget object
	 */
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;

	/**
	 *	Function to set the newly selected index
	 *
	 * @param NewSelection The newly selected item in the combo box
	 * @param SelectInfo Provides context on how the selection changed
	 */
	void ComboBoxSelectionChanged(TSharedPtr<FName> StringItem, ESelectInfo::Type SelectInfo);

	TSharedPtr<class SNameComboBox>	ComboBox;

	/** The actual list of FName values to choose from */
	TArray<TSharedPtr<FName>> NameList;
};
