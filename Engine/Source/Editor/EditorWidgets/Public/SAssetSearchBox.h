// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Misc/Attribute.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"

class SMenuAnchor;

/**
 * A widget to provide a search box with a filtered dropdown menu.
 */

class EDITORWIDGETS_API SAssetSearchBox : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAssetSearchBox)
		: _SuggestionListPlacement( MenuPlacement_BelowAnchor )
		, _OnTextChanged()
		, _OnTextCommitted()
		, _InitialText()
		, _HintText()
		, _PossibleSuggestions(TArray<FString>())
		, _DelayChangeNotificationsWhileTyping( false )
		, _MustMatchPossibleSuggestions( false )
	{}

	/** Where to place the suggestion list */
		SLATE_ARGUMENT( EMenuPlacement, SuggestionListPlacement )

		/** Invoked whenever the text changes */
		SLATE_EVENT( FOnTextChanged, OnTextChanged )

		/** Invoked whenever the text is committed (e.g. user presses enter) */
		SLATE_EVENT( FOnTextCommitted, OnTextCommitted )

		/** Initial text to display for the search text */
		SLATE_ATTRIBUTE( FText, InitialText )

		/** Hint text to display for the search text when there is no value */
		SLATE_ATTRIBUTE( FText, HintText )

		/** All possible suggestions for the search text */
		SLATE_ATTRIBUTE( TArray<FString>, PossibleSuggestions )

		/** Whether the SearchBox should delay notifying listeners of text changed events until the user is done typing */
		SLATE_ATTRIBUTE( bool, DelayChangeNotificationsWhileTyping )

		/** Whether the SearchBox allows entries that don't match the possible suggestions */
		SLATE_ATTRIBUTE( bool, MustMatchPossibleSuggestions )

		/** Callback delegate to have first chance handling of the OnKeyDown event */
		SLATE_EVENT( FOnKeyDown, OnKeyDownHandler )

		SLATE_END_ARGS()

		/** Constructs this widget with InArgs */
		void Construct( const FArguments& InArgs );

	/** Sets the text string currently being edited */
	void SetText(const TAttribute< FText >& InNewText);

	/** Set or clear the current error reporting information for this search box */
	void SetError( const FText& InError );
	void SetError( const FString& InError );

	// SWidget implementation
	virtual FReply OnPreviewKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual bool SupportsKeyboardFocus() const override;
	virtual bool HasKeyboardFocus() const override;
	virtual FReply OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent ) override;

private:
	/** First chance handler for key down events to the editable text widget */
	FReply HandleKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);

	/** Handler for when text in the editable text box changed */
	void HandleTextChanged(const FText& NewText);

	/** Handler for when text in the editable text box changed */
	void HandleTextCommitted(const FText& NewText, ETextCommit::Type CommitType);

	/** Called by SListView when the selection changes in the suggestion list */
	void OnSelectionChanged( TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo );

	/** Makes the widget for a suggestion message in the list view */
	TSharedRef<ITableRow> MakeSuggestionListItemWidget(TSharedPtr<FString> Text, const TSharedRef<STableViewBase>& OwnerTable);

	/** Gets the text to highlight in the suggestion list */
	FText GetHighlightText() const;

	/** Updates and shows or hides the suggestion list */
	void UpdateSuggestionList();

	/** Sets the focus to the InputText box */
	void FocusEditBox();

	/** Returns the currently selected suggestion */
	TSharedPtr<FString> GetSelectedSuggestion();

private:
	/** The editable text field */
	TSharedPtr< SSearchBox > InputText;

	/** The the state of the text prior to being committed */
	FText PreCommittedText;

	/** Auto completion elements */
	TSharedPtr< SMenuAnchor > SuggestionBox;

	/** All suggestions stored in this widget for the list view */
	TArray< TSharedPtr<FString> > Suggestions;

	/** The list view for showing all suggestions */
	TSharedPtr< SListView< TSharedPtr<FString> > > SuggestionListView;

	/** Delegate for when text is changed in the edit box */
	FOnTextChanged OnTextChanged;

	/** Delegate for when text is changed in the edit box */
	FOnTextCommitted OnTextCommitted;

	/** Delegate for first chance handling for key down events */
	FOnKeyDown OnKeyDownHandler;

	/** All possible suggestions for the search text */
	TAttribute< TArray<FString> > PossibleSuggestions;

	/** Determines whether or not the committed text should match a suggestion */
	bool bMustMatchPossibleSuggestions;
};
