// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlRevision.h"
#include "ISourceControlLabel.h"

/** 
 * Abstraction of a Perforce label.
 */
class FPerforceSourceControlLabel : public ISourceControlLabel, public TSharedFromThis<FPerforceSourceControlLabel>
{
public:

	FPerforceSourceControlLabel( const FString& InName )
		: Name(InName)
	{
	}

	/** ISourceControlLabel implementation */
	virtual const FString& GetName() const override;
	virtual bool GetFileRevisions( const TArray<FString>& InFiles, TArray< TSharedRef<ISourceControlRevision, ESPMode::ThreadSafe> >& OutRevisions ) const override;
	virtual bool Sync( const TArray<FString>& InFilenames ) const override;

private:

	/** Label name */
	FString Name;
};
