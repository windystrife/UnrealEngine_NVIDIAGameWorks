// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlRevision.h"

class FSubversionSourceControlRevision : public ISourceControlRevision, public TSharedFromThis<FSubversionSourceControlRevision, ESPMode::ThreadSafe>
{
public:
	FSubversionSourceControlRevision()
		: RevisionNumber(0)
		, Date(0)
	{
	}

	/** ISourceControlRevision interface */
	virtual bool Get( FString& InOutFilename ) const override;
	virtual bool GetAnnotated( TArray<FAnnotationLine>& OutLines ) const override;
	virtual bool GetAnnotated( FString& InOutFilename ) const override;
	virtual const FString& GetFilename() const override;
	virtual int32 GetRevisionNumber() const override;
	virtual const FString& GetRevision() const override;
	virtual const FString& GetDescription() const override;
	virtual const FString& GetUserName() const override;
	virtual const FString& GetClientSpec() const override;
	virtual const FString& GetAction() const override;
	virtual TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> GetBranchSource() const override;
	virtual const FDateTime& GetDate() const override;
	virtual int32 GetCheckInIdentifier() const override;
	virtual int32 GetFileSize() const override;

public:

	/** The filename this revision refers to */
	FString Filename;

	/** The revision number */
	int32 RevisionNumber;

	/** The revision to display to users */
	FString Revision;

	/** The description of this revision */
	FString Description;

	/** The user that made the change */
	FString UserName;

	/** The action (add, edit etc.) performed at this revision */
	FString Action;

	/** Source of branch, if any */
	TSharedPtr<FSubversionSourceControlRevision, ESPMode::ThreadSafe> BranchSource;

	/** The date this revision was made */
	FDateTime Date;

	/** The repo URL */
	FString RepoFilename;
};
