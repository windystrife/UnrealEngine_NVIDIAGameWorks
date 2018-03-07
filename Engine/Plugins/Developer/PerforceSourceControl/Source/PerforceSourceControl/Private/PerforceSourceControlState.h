// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlState.h"
#include "ISourceControlRevision.h"

class FPerforceSourceControlRevision;

namespace EPerforceState
{
	enum Type
	{
		/** Don't know or don't care. */
		DontCare		= 0,

		/** File is checked out to current user. */
		CheckedOut		= 1,

		/** File is not checked out (but IS controlled by the source control system). */
		ReadOnly		= 2,

		/** File is new and not in the depot - needs to be added. */
		NotInDepot		= 4,

		/** File is checked out by another user and cannot be checked out locally. */
		CheckedOutOther	= 5,

		/** Certain packages are best ignored by the SCC system (MyLevel, Transient, etc). */
		Ignore			= 6,

		/** File is marked for add */
		OpenForAdd		= 7,

		/** File is marked for delete */
		MarkedForDelete	= 8,

		/** Not under client root */
		NotUnderClientRoot	= 9,

		/** Opened for branch */
		Branched = 10,
	};
}

class FPerforceSourceControlState : public ISourceControlState, public TSharedFromThis<FPerforceSourceControlState, ESPMode::ThreadSafe>
{
public:
	FPerforceSourceControlState( const FString& InLocalFilename, EPerforceState::Type InState = EPerforceState::DontCare )
		: LocalFilename(InLocalFilename)
		, State(InState)
		, DepotRevNumber(INVALID_REVISION)
		, LocalRevNumber(INVALID_REVISION)
		, PendingResolveRevNumber(INVALID_REVISION)
		, bModifed(false)
		, TimeStamp(0)
	{
	}

	/** ISourceControlState interface */
	virtual int32 GetHistorySize() const override;
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> GetHistoryItem( int32 HistoryIndex ) const override;
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FindHistoryRevision( int32 RevisionNumber ) const override;
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FindHistoryRevision( const FString& InRevision ) const override;
	virtual TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> GetBaseRevForMerge() const override;
	virtual FName GetIconName() const override;
	virtual FName GetSmallIconName() const override;
	virtual FText GetDisplayName() const override;
	virtual FText GetDisplayTooltip() const override;
	virtual const FString& GetFilename() const override;
	virtual const FDateTime& GetTimeStamp() const override;
	virtual bool CanCheckIn() const override;
	virtual bool CanCheckout() const override;
	virtual bool IsCheckedOut() const override;
	virtual bool IsCheckedOutOther(FString* Who = NULL) const override;
	virtual bool IsCurrent() const override;
	virtual bool IsSourceControlled() const override;
	virtual bool IsAdded() const override;
	virtual bool IsDeleted() const override;
	virtual bool IsIgnored() const override;
	virtual bool CanEdit() const override;
	virtual bool IsUnknown() const override;
	virtual bool IsModified() const override;
	virtual bool CanAdd() const override;
	virtual bool CanDelete() const override;
	virtual bool IsConflicted() const override;

	/** Get the state of a file */
	EPerforceState::Type GetState() const
	{
		return State;
	}

	/** Set the state of the file */
	void SetState( EPerforceState::Type InState )
	{
		State = InState;
	}

public:
	/** History of the item, if any */
	TArray< TSharedRef<FPerforceSourceControlRevision, ESPMode::ThreadSafe> > History;

	/** Filename on disk */
	FString LocalFilename;

	/** Filename in the Perforce depot */
	FString DepotFilename;

	/** If another user has this file checked out, this contains their name(s). Multiple users are comma-delimited */
	FString OtherUserCheckedOut;

	/** Status of the file */
	EPerforceState::Type State;

	/** Latest revision number of the file in the depot */
	int DepotRevNumber;

	/** Latest rev number at which a file was synced to before being edited */
	int LocalRevNumber;

	/** Pending rev number with which a file must be resolved, INVALID_REVISION if no resolve pending */
	int PendingResolveRevNumber;

	/** Modified from depot version */
	bool bModifed;

	/** Whether the file is a binary file or not */
	bool bBinary;

	/** Whether the file is a marked for exclusive checkout or not */
	bool bExclusiveCheckout;

	/** The timestamp of the last update */
	FDateTime TimeStamp;
};
