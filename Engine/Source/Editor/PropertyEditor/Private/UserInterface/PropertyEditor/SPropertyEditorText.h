// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "EditorStyleSet.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Presentation/PropertyEditor/PropertyEditor.h"
#include "UserInterface/PropertyEditor/PropertyEditorConstants.h"

class SEditableTextBox;
class SMultiLineEditableTextBox;

class SPropertyEditorText : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SPropertyEditorText )
		: _Font( FEditorStyle::GetFontStyle( PropertyEditorConstants::PropertyFontStyle ) ) 
		{}
		SLATE_ATTRIBUTE( FSlateFontInfo, Font )
	SLATE_END_ARGS()

	static bool Supports( const TSharedRef< class FPropertyEditor >& InPropertyEditor );

	void Construct( const FArguments& InArgs, const TSharedRef< class FPropertyEditor >& InPropertyEditor );

	void GetDesiredWidth( float& OutMinDesiredWidth, float& OutMaxDesiredWidth );

	bool SupportsKeyboardFocus() const override;

	FReply OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent ) override;

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

private:

	void OnTextCommitted( const FText& NewText, ETextCommit::Type CommitInfo );

	/** Called if the single line widget text changes */
	void OnSingleLineTextChanged( const FText& NewText );

	/** Called if the multi line widget text changes */
	void OnMultiLineTextChanged( const FText& NewText );

	/** @return True if the property can be edited */
	bool CanEdit() const;

	/** @return True if the property is Read Only */
	bool IsReadOnly() const;

private:

	TSharedPtr< class FPropertyEditor > PropertyEditor;

	TSharedPtr< class SWidget > PrimaryWidget;

	/** Widget used for the multiline version of the text property */
	TSharedPtr<SMultiLineEditableTextBox> MultiLineWidget;

	/** Widget used for the single line version of the text property */
	TSharedPtr<SEditableTextBox> SingleLineWidget;

	TOptional< float > PreviousHeight;

	/** Cached flag as we would like multi-line text widgets to be slightly larger */
	bool bIsMultiLine;
	
	/** True if property is an FName property which causes us to run extra size validation checks */
	bool bIsFNameProperty;
};
