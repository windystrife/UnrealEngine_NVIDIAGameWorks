// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
 
#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Margin.h"
#include "Styling/SlateColor.h"
#include "Fonts/SlateFontInfo.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Framework/SlateDelegates.h"
#include "Widgets/Input/SComboBox.h"

/**
 * A combo box that shows text content.
 */
class SLATE_API STextComboBox : public SCompoundWidget
{
public:

	DECLARE_DELEGATE_RetVal_OneParam( FString, FGetTextComboLabel, TSharedPtr<FString> );
	typedef TSlateDelegates< TSharedPtr<FString> >::FOnSelectionChanged FOnTextSelectionChanged;

	SLATE_BEGIN_ARGS( STextComboBox )
		: _ComboBoxStyle(&FCoreStyle::Get().GetWidgetStyle< FComboBoxStyle >("ComboBox"))
		, _ButtonStyle(nullptr)
		, _ColorAndOpacity( FSlateColor::UseForeground() )
		, _ContentPadding(FMargin(4.0, 2.0))
		, _OnGetTextLabelForItem()
		{}

		SLATE_STYLE_ARGUMENT(FComboBoxStyle, ComboBoxStyle)

		/** The visual style of the button part of the combo box (overrides ComboBoxStyle) */
		SLATE_STYLE_ARGUMENT(FButtonStyle, ButtonStyle)

		/** Selection of strings to pick from */
		SLATE_ARGUMENT( TArray< TSharedPtr<FString> >*, OptionsSource )

		/** Text color and opacity */
		SLATE_ATTRIBUTE( FSlateColor, ColorAndOpacity )

		/** Sets the font used to draw the text */
		SLATE_ATTRIBUTE(FSlateFontInfo, Font)

		/** Visual padding of the button content for the combobox */
		SLATE_ATTRIBUTE( FMargin, ContentPadding )

		/** Called when the text is chosen. */
		SLATE_EVENT( FOnTextSelectionChanged, OnSelectionChanged)

		/** Called when the combo box is opened */
		SLATE_EVENT( FOnComboBoxOpening, OnComboBoxOpening )

		/** Called when combo box needs to establish selected item */
		SLATE_ARGUMENT( TSharedPtr<FString>, InitiallySelectedItem )

		/** [Optional] Called to get the label for the currently selected item */
		SLATE_EVENT( FGetTextComboLabel, OnGetTextLabelForItem ) 
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );

	/** Called to create a widget for each string */
	TSharedRef<SWidget> MakeItemWidget( TSharedPtr<FString> StringItem );

	void SetSelectedItem (TSharedPtr<FString> NewSelection);

	/** Returns the currently selected text string */
	TSharedPtr<FString> GetSelectedItem()
	{
		return SelectedItem;
	}

	/** Request to reload the text options in the combobox from the OptionsSource attribute */
	void RefreshOptions();

	/** Clears the selected item in the text combo */
	void ClearSelection();

private:
	TSharedPtr<FString> OnGetSelection() const {return SelectedItem;}

	/** Called when selection changes in the combo pop-up */
	void OnSelectionChanged (TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);

	/** Helper method to get the text for a given item in the combo box */
	FText GetSelectedTextLabel() const;

	FText GetItemTextLabel(TSharedPtr<FString> StringItem) const;

private:

	/** Called to get the text label for an item */
	FGetTextComboLabel GetTextLabelForItem;

	/** The text item selected */
	TSharedPtr<FString> SelectedItem;

	/** Array of shared pointers to strings so combo widget can work on them */
	TArray< TSharedPtr<FString> >		Strings;

	/** The combo box */
	TSharedPtr< SComboBox< TSharedPtr<FString> > >	StringCombo;

	/** Forwarding Delegate */
	FOnTextSelectionChanged SelectionChanged;

	/** Sets the font used to draw the text */
	TAttribute< FSlateFontInfo > Font;
};
