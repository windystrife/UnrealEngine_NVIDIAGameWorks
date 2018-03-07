// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
 
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateColor.h"
#include "Layout/Margin.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Framework/SlateDelegates.h"
#include "Widgets/Input/SComboBox.h"

//#include "UnrealString.h"

/**
 * A combo box that shows FName content.
 */
class SNameComboBox : public SCompoundWidget
{
public:

	DECLARE_DELEGATE_RetVal_OneParam(FString, FGetNameComboLabel, TSharedPtr<FName>);
	typedef TSlateDelegates< TSharedPtr<FName> >::FOnSelectionChanged FOnNameSelectionChanged;

	SLATE_BEGIN_ARGS( SNameComboBox ) 
		: _ColorAndOpacity( FSlateColor::UseForeground() )
		, _ContentPadding(FMargin(4.0, 2.0))
		, _OnGetNameLabelForItem()
		{}

		/** Selection of FNames to pick from */
		SLATE_ARGUMENT( TArray< TSharedPtr<FName> >*, OptionsSource )

		/** Text color and opacity */
		SLATE_ATTRIBUTE( FSlateColor, ColorAndOpacity )

		/** Visual padding of the button content for the combobox */
		SLATE_ATTRIBUTE( FMargin, ContentPadding )

		/** Called when the FName is chosen. */
		SLATE_EVENT( FOnNameSelectionChanged, OnSelectionChanged)

		/** Called when the combo box is opened */
		SLATE_EVENT( FOnComboBoxOpening, OnComboBoxOpening )

		/** Called when combo box needs to establish selected item */
		SLATE_ARGUMENT( TSharedPtr<FName>, InitiallySelectedItem )

		/** [Optional] Called to get the label for the currently selected item */
		SLATE_EVENT( FGetNameComboLabel, OnGetNameLabelForItem ) 
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );

	/** Called to create a widget for each FName */
	TSharedRef<SWidget> MakeItemWidget( TSharedPtr<FName> StringItem );

	void SetSelectedItem (TSharedPtr<FName> NewSelection);

	/** Returns the currently selected FName */
	TSharedPtr<FName> GetSelectedItem()
	{
		return SelectedItem;
	}

	/** Request to reload the name options in the combobox from the OptionsSource attribute */
	void RefreshOptions();

	/** Clears the selected item in the name combo */
	void ClearSelection();

private:
	TSharedPtr<FName> OnGetSelection() const {return SelectedItem;}

	/** Called when selection changes in the combo pop-up */
	void OnSelectionChanged(TSharedPtr<FName> Selection, ESelectInfo::Type SelectInfo);

	/** Helper method to get the text for a given item in the combo box */
	FText GetSelectedNameLabel() const;

	FText GetItemNameLabel(TSharedPtr<FName> StringItem) const;

private:

	/** Called to get the text label for an item */
	FGetNameComboLabel GetTextLabelForItem;

	/** The FName item selected */
	TSharedPtr<FName> SelectedItem;

	/** Array of shared pointers to FNames so combo widget can work on them */
	TArray< TSharedPtr<FName> >		Names;

	/** The combo box */
	TSharedPtr< SComboBox< TSharedPtr<FName> > >	NameCombo;

	/** Forwarding Delegate */
	FOnNameSelectionChanged SelectionChanged;
};
