// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class ISourceControlRevision;
class ISourceControlState;

typedef TSharedRef<class ISourceControlState, ESPMode::ThreadSafe> FSourceControlStateRef;
typedef TSharedPtr<class ISourceControlState, ESPMode::ThreadSafe> FSourceControlStatePtr;

/**
 * An abstraction of the state of a file under source control
 */
class ISourceControlState : public TSharedFromThis<ISourceControlState, ESPMode::ThreadSafe>
{
public:
	enum { INVALID_REVISION = -1 };

	/**
	 * Virtual destructor
	 */
	virtual ~ISourceControlState() {}

	/** 
	 * Get the size of the history. 
	 * If an FUpdateStatus operation has been called with the ShouldUpdateHistory() set, there 
	 * should be history present if the file has been committed to source control.
	 * @returns the number of items in the history
	 */
	virtual int32 GetHistorySize() const = 0;

	/**
	 * Get an item from the history
	 * @param	HistoryIndex	the index of the history item
	 * @returns a history item or NULL if none exist
	 */
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> GetHistoryItem( int32 HistoryIndex ) const = 0;

	/**
	 * Find an item from the history with the specified revision number.
	 * @param	RevisionNumber	the revision number to look for
	 * @returns a history item or NULL if the item could not be found
	 */
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FindHistoryRevision( int32 RevisionNumber ) const = 0;
	
	/**
	 * Find an item from the history with the specified revision.
	 * @param	InRevision	the revision identifier to look for
	 * @returns a history item or NULL if the item could not be found
	 */
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FindHistoryRevision( const FString& InRevision ) const = 0;

	/**
	 * Get the revision that we should use as a base when performing a three wage merge, does not refresh source control state
	 * @returns a revision identifier or NULL if none exist
	 */
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> GetBaseRevForMerge() const = 0;

	/**
	 * Get the name of the icon graphic we should use to display the state in a UI.
	 * @returns the name of the icon to display
	 */
	virtual FName GetIconName() const = 0;
		
	/**
	 * Get the name of the small icon graphic we should use to display the state in a UI.
	 * @returns the name of the icon to display
	 */	
	virtual FName GetSmallIconName() const = 0;

	/**
	 * Get a text representation of the state
	 * @returns	the text to display for this state
	 */
	virtual FText GetDisplayName() const = 0;

	/**
	 * Get a tooltip to describe this state
	 * @returns	the text to display for this states tooltip
	 */
	virtual FText GetDisplayTooltip() const = 0;

	/**
	 * Get the local filename that this state represents
	 * @returns	the filename
	 */
	virtual const FString& GetFilename() const = 0;

	/**
	 * Get the timestamp of the last update that was made to this state.
	 * @returns	the timestamp of the last update
	 */
	virtual const FDateTime& GetTimeStamp() const = 0;

	/** Get whether this file can be checked in. */
	virtual bool CanCheckIn() const = 0;

	/** Get whether this file can be checked out */
	virtual bool CanCheckout() const = 0;

	/** Get whether this file is checked out */
	virtual bool IsCheckedOut() const = 0;

	/** Get whether this file is checked out by someone else */
	virtual bool IsCheckedOutOther(FString* Who = NULL) const = 0;

	/** Get whether this file is up-to-date with the version in source control */
	virtual bool IsCurrent() const = 0;

	/** Get whether this file is under source control */
	virtual bool IsSourceControlled() const = 0;

	/** Get whether this file is marked for add */
	virtual bool IsAdded() const = 0;

	/** Get whether this file is marked for delete */
	virtual bool IsDeleted() const = 0;

	/** Get whether this file is ignored by source control */
	virtual bool IsIgnored() const = 0;

	/** Get whether source control allows this file to be edited */
	virtual bool CanEdit() const = 0;

	/** Get whether source control allows this file to be deleted. */
	virtual bool CanDelete() const = 0;

	/** Get whether we know anything about this files source control state */
	virtual bool IsUnknown() const = 0;

	/** Get whether this file is modified compared to the version we have from source control */
	virtual bool IsModified() const = 0;

	/** 
	 * Get whether this file can be added to source control (i.e. is part of the directory 
	 * structure currently under source control) 
	 */
	virtual bool CanAdd() const = 0;

	/** Get whether this file is in a conflicted state */
	virtual bool IsConflicted() const = 0;
};

