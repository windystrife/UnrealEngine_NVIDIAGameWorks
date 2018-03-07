// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MessageLogInitializationOptions.h"
#include "Logging/TokenizedMessage.h"
#include "IMessageLogListing.h"
#include "Model/MessageLogListingModel.h"
#include "MessageFilter.h"

/**
 * The non-UI solution specific presentation logic for a collection of messages for a particular system.
 */
class FMessageLogListingViewModel
	: public TSharedFromThis<FMessageLogListingViewModel>
	, public IMessageLogListing
{
public:

	/**  
	 *	Factory method which creates a new FMessageLogListingViewModel object
	 *
	 *	@param	InMessageLogListingModel		The MessageLogListing data to view
	 *	@param	InLogLabel						The label that will be displayed in the UI for this log listing
	 *	@param	InMessageLogListingModel		If true, a filters list will be displayed for this log listing
	 */
	static TSharedRef< FMessageLogListingViewModel > Create( const TSharedRef< FMessageLogListingModel >& InMessageLogListingModel, const FText& InLogLabel, const FMessageLogInitializationOptions& InitializationOptions = FMessageLogInitializationOptions() )
	{
		TSharedRef< FMessageLogListingViewModel > NewLogListingView( new FMessageLogListingViewModel( InMessageLogListingModel, InLogLabel, InitializationOptions ) );
		NewLogListingView->Initialize();

		return NewLogListingView;
	}

public:

	~FMessageLogListingViewModel();

	/** Begin IMessageLogListing interface */
	virtual void AddMessage( const TSharedRef< class FTokenizedMessage >& NewMessage, bool bMirrorToOutputLog ) override;
	virtual void AddMessages( const TArray< TSharedRef< class FTokenizedMessage > >& NewMessages, bool bMirrorToOutputLog ) override;
	virtual void ClearMessages() override;
	virtual TSharedPtr<FTokenizedMessage> GetMessageFromData(const struct FTokenizedMiscData& MessageData) const override;
	virtual const TArray< TSharedRef<class FTokenizedMessage> >& GetSelectedMessages() const override;
	virtual void SelectMessages( const TArray< TSharedRef<class FTokenizedMessage> >& InSelectedMessages ) override;
	virtual const TArray< TSharedRef<class FTokenizedMessage> >& GetFilteredMessages() const override;
	virtual void SelectMessage(const TSharedRef<class FTokenizedMessage>& Message, bool bSelected) override;
	virtual bool IsMessageSelected(const TSharedRef<class FTokenizedMessage>& Message) const override;
	virtual void ClearSelectedMessages() override;
	virtual void InvertSelectedMessages() override;
	virtual FText GetSelectedMessagesAsText() const override;
	virtual FText GetAllMessagesAsText() const override;
	virtual const FName& GetName() const override;
	virtual void SetLabel( const FText& InLogLabel ) override;
	virtual const FText& GetLabel() const override;
	virtual const TArray< TSharedRef< class FMessageFilter> >& GetMessageFilters() const override;
	virtual void ExecuteToken( const TSharedRef<class IMessageToken>& Token ) const override;
	virtual void NewPage( const FText& Title ) override;
	virtual void NotifyIfAnyMessages( const FText& Message, EMessageSeverity::Type SeverityFilter = EMessageSeverity::Info, bool bForce = false ) override;
	virtual void Open() override;
	virtual int32 NumMessages( EMessageSeverity::Type SeverityFilter ) override;

	DECLARE_DERIVED_EVENT(FMessageLogListingViewModel, IMessageLogListing::IMessageTokenClickedEvent, IMessageTokenClickedEvent)
	virtual IMessageLogListing::IMessageTokenClickedEvent& OnMessageTokenClicked() override { return TokenClickedEvent; }

	DECLARE_DERIVED_EVENT(FMessageLogListingViewModel, IMessageLogListing::FChangedEvent, FChangedEvent)
	virtual IMessageLogListing::FChangedEvent& OnDataChanged() override { return ChangedEvent; }

	DECLARE_DERIVED_EVENT(FMessageLogListingViewModel, IMessageLogListing::FOnSelectionChangedEvent, FOnSelectionChangedEvent)
	virtual IMessageLogListing::FOnSelectionChangedEvent& OnSelectionChanged() override { return SelectionChangedEvent; }

	DECLARE_DERIVED_EVENT(FMessageLogListingViewModel, IMessageLogListing::FOnPageSelectionChangedEvent, FOnPageSelectionChangedEvent )
	virtual IMessageLogListing::FOnPageSelectionChangedEvent& OnPageSelectionChanged() override { return PageSelectionChangedEvent; }
	/** End IMessageLogListing interface */

	/** Initializes the FMessageLogListingViewModel for use */
	virtual void Initialize();

	/** Handles updating the viewmodel when one of its filters changes */
	void OnFilterChanged();

	/** Called when data is changed changed/updated in the model */
	virtual void OnChanged();

	/** Returns the filtered message list */
	const TArray< TSharedRef< FTokenizedMessage > >& GetFilteredMessages() { return FilteredMessages; }

	/** Obtains a const iterator to the filtered messages */
	MessageContainer::TConstIterator GetFilteredMessageIterator() const;

	/** Obtains a const iterator to the selected filtered messages list */
	MessageContainer::TConstIterator GetSelectedMessageIterator() const;

	/** Returns the message at the specified index in the filtered list */
	const TSharedPtr<FTokenizedMessage> GetMessageAtIndex( const int32 MessageIndex ) const;

	/** Set whether we should show filters or not */
	void SetShowFilters(bool bInShowFilters);

	/** Get whether we should show filters or not */
	bool GetShowFilters() const;

	/** Set whether we should show pages or not */
	void SetShowPages(bool bInShowPages);

	/** Get whether we should show pages or not */
	bool GetShowPages() const;

	/** Set whether we should show allow the user to clear the log. */
	void SetAllowClear(bool bInAllowClear);

	/** Get whether we should show allow the user to clear the log. */
	bool GetAllowClear() const;

	/** Set whether we should discard duplicates or not */
	void SetDiscardDuplicates(bool bInDiscardDuplicates);

	/** Get whether we should discard duplicates or not */
	bool GetDiscardDuplicates() const;

	/** Set the maximum page count this log can hold */
	void SetMaxPageCount(uint32 InMaxPageCount);

	/** Get the maximum page count this log can hold */
	uint32 GetMaxPageCount() const;
	
	/** Get the number of pages we can view */
	uint32 GetPageCount() const;

	/** Get the current page index we are viewing */
	uint32 GetCurrentPageIndex() const;

	/** Set the current page index we are viewing */
	void SetCurrentPageIndex( uint32 InCurrentPageIndex );

	/** Go to the page at the index after the current */
	void NextPage();

	/** Go to the page at the index before the current */
	void PrevPage();

	/**
	 * Get the title of the page at the specified index
	 * @param	PageIndex	The index of the page
	 */
	const FText& GetPageTitle( const uint32 PageIndex ) const;

	/** Gets the number of messages in the current log page */
	uint32 NumMessages() const;

	/** Get whether to show this log in the main log window */
	bool ShouldShowInLogWindow() const { return bShowInLogWindow; }

private:
	FMessageLogListingViewModel( TSharedPtr< FMessageLogListingModel > InMessageLogListingModel, const FText& InLogLabel, const FMessageLogInitializationOptions& InitializationOptions )
		: bShowFilters( InitializationOptions.bShowFilters )
		, bShowPages( InitializationOptions.bShowPages )
		, bAllowClear( InitializationOptions.bAllowClear )
		, bDiscardDuplicates( InitializationOptions.bDiscardDuplicates )
		, MaxPageCount( InitializationOptions.MaxPageCount )
		, bShowInLogWindow( InitializationOptions.bShowInLogWindow )
		, CurrentPageIndex( 0 )
		, bIsRefreshing( false )
		, LogLabel( InLogLabel )
		, MessageLogListingModel( InMessageLogListingModel )
	{}

	/** Rebuilds the list of filtered messages */
	void RefreshFilteredMessages();

	/** Helper function for opening this message log from a notification */
	void OpenMessageLog();

	/** Helper function to check if this log contains messages above a certain severity */
	int32 NumMessagesPresent( uint32 PageIndex, EMessageSeverity::Type InSeverity ) const;

	/** Helper function to check the worst severity contained in this log page */
	EMessageSeverity::Type HighestSeverityPresent( uint32 PageIndex ) const;

private:

	/** Whether filters should be shown for this listing */
	bool bShowFilters;

	/** Whether pages should be used/shown for this listing */
	bool bShowPages;

	/** Whether we allow the user to clear the log. */
	bool bAllowClear;

	/** Whether to check for duplicate messages & discard them */
	bool bDiscardDuplicates;

	/** The limit on the number of displayed pages for this listing */
	uint32 MaxPageCount;

	/** Whether to show this log in the main log window */
	bool bShowInLogWindow;

	/** The currently displayed page index */
	uint32 CurrentPageIndex;

	/** Tracks if the viewmodel is in the middle of refreshing */
	bool bIsRefreshing;

	/** Label of the listing, displayed to users */
	FText LogLabel;

	/* The model we are getting display info from */
	TSharedPtr< FMessageLogListingModel > MessageLogListingModel;

	/** The same list of messages in the model after filtering is applied */
	MessageContainer FilteredMessages;

	/** The list of selected messages */
	MessageContainer SelectedFilteredMessages;

	/** The array of message filters used on this listing */
	TArray< TSharedRef< FMessageFilter > > MessageFilters;

	/** Delegate to call when a token is clicked */
	IMessageTokenClickedEvent TokenClickedEvent;

	/** Delegate to call when model data is changed */
	FChangedEvent ChangedEvent;

	/** Delegate to call when selection state is changed */
	FOnSelectionChangedEvent SelectionChangedEvent;

	/** Delegate to call when page selection state is changed */
	FOnPageSelectionChangedEvent PageSelectionChangedEvent;
};
