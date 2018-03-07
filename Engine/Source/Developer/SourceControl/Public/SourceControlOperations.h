// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlOperation.h"

#define LOCTEXT_NAMESPACE "SourceControl"

/**
 * Operation used to connect (or test a connection) to source control
 */
class FConnect : public ISourceControlOperation
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override 
	{ 
		return "Connect"; 
	}

	virtual FText GetInProgressString() const override
	{ 
		return LOCTEXT("SourceControl_Connecting", "Connecting to source control..."); 
	}

	const FString& GetPassword() const
	{
		return Password;
	}

	void SetPassword(const FString& InPassword)
	{
		Password = InPassword;
	}

	const FText& GetErrorText() const
	{
		return OutErrorText;
	}

	void SetErrorText(const FText& InErrorText)
	{
		OutErrorText = InErrorText;
	}

protected:
	/** Password we use for this operation */
	FString Password;

	/** Error text for easy diagnosis */
	FText OutErrorText;
};

/**
 * Operation used to check files into source control
 */
class FCheckIn : public ISourceControlOperation
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "CheckIn";
	}

	virtual FText GetInProgressString() const override
	{ 
		return LOCTEXT("SourceControl_CheckIn", "Checking file(s) into Source Control..."); 
	}

	void SetDescription( const FText& InDescription )
	{
		Description = InDescription;
	}

	const FText& GetDescription() const
	{
		return Description;
	}

	void SetSuccessMessage( const FText& InSuccessMessage )
	{
		SuccessMessage = InSuccessMessage;
	}

	const FText& GetSuccessMessage() const
	{
		return SuccessMessage;
	}

protected:
	/** Description of the checkin */
	FText Description;

	/** A short message listing changelist/revision we submitted, if successful */
	FText SuccessMessage;
};

/**
 * Operation used to check files out of source control
 */
class FCheckOut : public ISourceControlOperation
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "CheckOut";
	}

	virtual FText GetInProgressString() const override
	{ 
		return LOCTEXT("SourceControl_CheckOut", "Checking file(s) out of Source Control..."); 
	}
};

/**
 * Operation used to mark files for add in source control
 */
class FMarkForAdd : public ISourceControlOperation
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "MarkForAdd";
	}

	virtual FText GetInProgressString() const override
	{ 
		return LOCTEXT("SourceControl_Add", "Adding file(s) to Source Control..."); 
	}
};

/**
 * Operation used to mark files for delete in source control
 */
class FDelete : public ISourceControlOperation
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "Delete";
	}

	virtual FText GetInProgressString() const override
	{ 
		return LOCTEXT("SourceControl_Delete", "Deleting file(s) from Source Control..."); 
	}
};

/**
 * Operation used to revert changes made back to the state they are in source control
 */
class FRevert : public ISourceControlOperation
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "Revert";
	}

	virtual FText GetInProgressString() const override
	{ 
		return LOCTEXT("SourceControl_Revert", "Reverting file(s) in Source Control..."); 
	}	
};

/**
 * Operation used to sync files to the state they are in source control
 */
class FSync : public ISourceControlOperation
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "Sync";
	}

	virtual FText GetInProgressString() const override
	{ 
		return LOCTEXT("SourceControl_Sync", "Syncing file(s) from source control..."); 
	}	

	void SetRevision( int32 InRevisionNumber )
	{
		RevisionNumber = InRevisionNumber;
	}

protected:
	/** Revision to sync to */
	int32 RevisionNumber;
};

/**
 * Operation used to update the source control status of files
 */
class FUpdateStatus : public ISourceControlOperation
{
public:
	FUpdateStatus()
		: bUpdateHistory(false)
		, bGetOpenedOnly(false)
		, bUpdateModifiedState(false)
	{
	}

	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "UpdateStatus";
	}

	virtual FText GetInProgressString() const override
	{ 
		return LOCTEXT("SourceControl_Update", "Updating file(s) source control status..."); 
	}	

	void SetUpdateHistory( bool bInUpdateHistory )
	{
		bUpdateHistory = bInUpdateHistory;
	}

	void SetGetOpenedOnly( bool bInGetOpenedOnly )
	{
		bGetOpenedOnly = bInGetOpenedOnly;
	}

	void SetUpdateModifiedState( bool bInUpdateModifiedState )
	{
		bUpdateModifiedState = bInUpdateModifiedState;
	}

	void SetCheckingAllFiles( bool bInCheckingAllFiles )
	{
		bCheckingAllFiles = bInCheckingAllFiles;
	}

	bool ShouldUpdateHistory() const
	{
		return bUpdateHistory;
	}

	bool ShouldGetOpenedOnly() const
	{
		return bGetOpenedOnly;
	}

	bool ShouldUpdateModifiedState() const
	{
		return bUpdateModifiedState;
	}

	bool ShouldCheckAllFiles() const
	{
		return bCheckingAllFiles;
	}

protected:
	/** Whether to update history */
	bool bUpdateHistory;

	/** Whether to just get files that are opened/edited */
	bool bGetOpenedOnly;

	/** Whether to update the modified state - expensive */
	bool bUpdateModifiedState;

	/** Hint that we are intending on checking all files in the project - some providers can optimize for this */
	bool bCheckingAllFiles;
};

/**
 * Operation used to copy a file or directory from one location to another
 */
class FCopy : public ISourceControlOperation
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		return "Copy";
	}

	virtual FText GetInProgressString() const override
	{ 
		return LOCTEXT("SourceControl_Copy", "Copying file(s) in Source Control..."); 
	}	

	void SetDestination(const FString& InDestination)
	{
		Destination = InDestination;
	}

	const FString& GetDestination() const
	{
		return Destination;
	}

protected:
	/** Destination path of the copy operation */
	FString Destination;
};

/**
 * Operation used to resolve a file that is in a conflicted state.
 */
class FResolve : public ISourceControlOperation
{
public:
	// ISourceControlOperation interface
	virtual FName GetName() const override
	{
		  return "Resolve";
	}

	virtual FText GetInProgressString() const override
	{
		return LOCTEXT("SourceControl_Resolve", "Resolving file(s) in Source Control...");
	}
};

#undef LOCTEXT_NAMESPACE
