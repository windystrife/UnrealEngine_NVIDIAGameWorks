// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/TokenizedMessage.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "MessageFilter.h"
#include "Presentation/MessageLogListingViewModel.h"
#include "Framework/Commands/UICommandList.h"

/**
 * A message log listing, such as the Compiler Log, or the Map Check Log.
 * Holds the log lines, and any extra widgets necessary.
 */
class SMessageLogListing : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMessageLogListing)
		{}

	SLATE_END_ARGS()

	SMessageLogListing();

	~SMessageLogListing();

	void Construct( const FArguments& InArgs, const TSharedRef< class IMessageLogListing >& InModelView );

	/** Refreshes the log messages, reapplying any filtering */
	void RefreshVisibility();

	/** Used to execute the 'on clicked token' delegate */
	void BroadcastMessageTokenClicked( TSharedPtr< class FTokenizedMessage > Message, const TSharedRef<class IMessageToken>& Token );
	/** Used to execute a message's featured action (a token associated with the entire message) */
	void BroadcastMessageDoubleClicked(TSharedPtr< class FTokenizedMessage > Message);

	/** Gets a list of the selected messages */
	const TArray< TSharedRef< class FTokenizedMessage > > GetSelectedMessages() const;

	/** Set the message selection state */
	void SelectMessage( const TSharedRef< class FTokenizedMessage >& Message, bool bSelected ) const;

	/** Get the message selection state */
	bool IsMessageSelected( const TSharedRef< class FTokenizedMessage >& Message ) const;

	/** Scrolls the message into view */
	void ScrollToMessage( const TSharedRef< class FTokenizedMessage >& Message ) const;

	/** Clears the message selection */
	void ClearSelectedMessages() const;

	/** Inverts the message selection */
	void InvertSelectedMessages() const;

	/** Compiles the selected messages into a single string. */
	FText GetSelectedMessagesAsText() const;

	/** Compiles all the messages into a single string. */
	FText GetAllMessagesAsText() const;

	/** Gets the message log listing unique name */
	const FName& GetName() const { return MessageLogListingViewModel->GetName(); }

	/** Gets the message log listing label  */
	const FText& GetLabel() const { return MessageLogListingViewModel->GetLabel(); }

	/** Gets the set of message filters used when displaying messages */
	const TArray< TSharedRef< class FMessageFilter > >& GetMessageFilters() const { return MessageLogListingViewModel->GetMessageFilters(); }

	/** Called when data is changed changed/updated in the model */
	void OnChanged();

	/** Called whenever the viewmodel selection changes */
	void OnSelectionChanged();

	/**
	 * Copies the selected messages to the clipboard
	 */
	void CopySelectedToClipboard() const;

	/**
	 * Copies text (selected/all), optionally to the clipboard
	 */
	FText CopyText( bool bSelected, bool bClipboard ) const;

	/** @return	The UICommandList supported by the MessageLogView */
	const TSharedRef< const FUICommandList > GetCommandList() const;

	/**
	 * Called after a key is pressed when this widget has focus
	 *
	 * @param  InKeyEvent  Key event
	 *
	 * @return  Returns whether the event was handled, along with other possible actions
	 */
	FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;

	/** Delegate supplying a label for the page-flipper widget */
	FString GetPageText() const;

	/** Delegate to display the previous page */
	FReply OnClickedPrevPage();

	/** Delegate to display the next page */
	FReply OnClickedNextPage();

	/** Delegate to get the display visibility of the show filter combobox */
	EVisibility GetFilterMenuVisibility();

	/** Generates the widgets for show filtering */
	TSharedRef<ITableRow> MakeShowWidget(TSharedRef<FMessageFilter> Selection, const TSharedRef<STableViewBase>& OwnerTable);

	/** Generates the menu content for filtering log content */
	TSharedRef<SWidget> OnGetFilterMenuContent();

	/** Delegate for enabling & disabling the page widget depending on the number of pages */
	bool IsPageWidgetEnabled() const;

	/** Delegate for showing & hiding the page widget depending on whether this log uses pages or not */
	EVisibility GetPageWidgetVisibility() const;

	/** Delegate for enabling & disabling the clear button depending on the number of messages */
	bool IsClearWidgetEnabled() const;

	/** Delegate for showing & hiding the clear button depending on whether this log uses pages or not */
	EVisibility GetClearWidgetVisibility() const;

	/** Delegate to generate the label for the page combobox */
	FText OnGetPageMenuLabel() const;

	/** Delegate to generate the menu content for the page combobox */
	TSharedRef<SWidget> OnGetPageMenuContent() const;

	/** Delegate called when a page is selected from the page menu */
	void OnPageSelected(uint32 PageIndex);

	/** Delegate for Clear button */
	FReply OnClear();

private:
	/** Makes the message log line widgets for populating the listing */
	TSharedRef<ITableRow> MakeMessageLogListItemWidget( TSharedRef< class FTokenizedMessage > Message, const TSharedRef< STableViewBase >& OwnerTable );
	
	/** Called when map check message line selection has changed */
	void OnLineSelectionChanged( TSharedPtr< class FTokenizedMessage > Selection, ESelectInfo::Type SelectInfo );

private:
	/** The list of commands with bound delegates for the message log */
	const TSharedRef< FUICommandList > UICommandList;

	/** Reference to the ViewModel which holds state info and has access to data */
	TSharedPtr<class FMessageLogListingViewModel> MessageLogListingViewModel;

	/**	Whether the view is currently updating the viewmodel selection */
	bool bUpdatingSelection;

	/** The list view for showing all the message log lines */
	TSharedPtr< SListView< TSharedRef< class FTokenizedMessage > > > MessageListView;
};
