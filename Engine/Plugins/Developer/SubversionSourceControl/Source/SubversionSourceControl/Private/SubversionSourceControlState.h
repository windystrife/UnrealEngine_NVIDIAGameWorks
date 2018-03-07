// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlRevision.h"
#include "SubversionSourceControlRevision.h"
#include "ISourceControlState.h"

namespace EWorkingCopyState
{
	enum Type
	{
		Unknown,
		Pristine,
		Added,
		Deleted,
		Modified,
		Replaced,
		Conflicted,
		External,
		Ignored,
		Incomplete,
		Merged,
		NotControlled,
		Obstructed,
		Missing,
		NotAWorkingCopy,
	};
}

namespace ELockState
{
	enum Type
	{
		Unknown,
		NotLocked,
		Locked,
		LockedOther,
	};
}

class FSubversionSourceControlState : public ISourceControlState, public TSharedFromThis<FSubversionSourceControlState, ESPMode::ThreadSafe>
{
public:
	FSubversionSourceControlState( const FString& InLocalFilename )
		: LocalFilename(InLocalFilename)
		, PendingMergeBaseFileRevNumber( INVALID_REVISION )
		, bNewerVersionOnServer(false)
		, WorkingCopyState(EWorkingCopyState::Unknown)
		, LockState(ELockState::Unknown)
		, TimeStamp(0)
		, bCopied(false)
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

public:
	/** History of the item, if any */
	TArray< TSharedRef<FSubversionSourceControlRevision, ESPMode::ThreadSafe> > History;

	/** Filename on disk */
	FString LocalFilename;

	/** Revision number with which our local revision diverged from the remote revision */
	int PendingMergeBaseFileRevNumber;

	/** Whether a newer version exists on the server */
	bool bNewerVersionOnServer;

	/** State of the working copy */
	EWorkingCopyState::Type WorkingCopyState;

	/** Lock state */
	ELockState::Type LockState;

	/** Name of other user who has file locked */
	FString LockUser;

	/** The timestamp of the last update */
	FDateTime TimeStamp;

	/** Flagged as a copy/branch */
	bool bCopied;
};
