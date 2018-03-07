// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "EditorStyleSet.h"
#include "UserInterface/PropertyEditor/PropertyEditorConstants.h"
#include "Widgets/Input/SComboBox.h"

class SToolTip;

class SPropertyComboBox : public SComboBox< TSharedPtr<FString> >
{
public:

	SLATE_BEGIN_ARGS( SPropertyComboBox )
		: _Font( FEditorStyle::GetFontStyle( PropertyEditorConstants::PropertyFontStyle ) )
	{}
		SLATE_ATTRIBUTE( TArray< TSharedPtr< FString > >, ComboItemList )
		SLATE_ATTRIBUTE( TArray< bool >, RestrictedList )
		SLATE_ATTRIBUTE( FString, VisibleText )
		SLATE_ARGUMENT( TArray< TSharedPtr< SToolTip > >, RichToolTipList)
		SLATE_EVENT( FOnSelectionChanged, OnSelectionChanged )
		SLATE_EVENT( FOnComboBoxOpening, OnComboBoxOpening )
		SLATE_ARGUMENT( FSlateFontInfo, Font )
	SLATE_END_ARGS()

	
	void Construct( const FArguments& InArgs );

	~SPropertyComboBox();

	/**
	 *	Sets the currently selected item for the combo box.
	 *	@param InSelectedItem			The name of the item to select.
	 */
	void SetSelectedItem( const FString& InSelectedItem );

	/** 
	 *	Sets the item list for the combo box.
	 *	@param InItemList			The item list to use.
	 *	@param InTooltipList		The list of tooltips to use.
	 *	@param InRestrictedList		The list of restricted items.
	 */
	void SetItemList(TArray< TSharedPtr< FString > >& InItemList, TArray< TSharedPtr< SToolTip > >& InRichTooltips, TArray<bool>& InRestrictedList);


private:

	void OnSelectionChangedInternal( TSharedPtr<FString> InSelectedItem, ESelectInfo::Type SelectInfo );

	TSharedRef<SWidget> OnGenerateComboWidget( TSharedPtr<FString> InComboString );

	/** SWidget interface */
	FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;

private:

	/** List of items in our combo box. Only generated once as combo items dont change at runtime */
	TArray< TSharedPtr<FString> > ComboItemList;
	TArray< TSharedPtr< SToolTip > > RichToolTips;
	FOnSelectionChanged OnSelectionChanged;
	FSlateFontInfo Font;
	TArray< bool > RestrictedList;
};
