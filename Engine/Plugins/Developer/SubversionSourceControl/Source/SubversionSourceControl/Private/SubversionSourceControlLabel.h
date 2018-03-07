// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlRevision.h"
#include "ISourceControlLabel.h"

/** 
 * Abstraction of a 'Subversion label'.
 * Some things to note:
 * Subversion doesn't have 'label' functionality like Perforce, although it is flexible enough to allow
 * us to emulate it.
 * We assume that a standard SVN repository layout is used, i.e.:
 *
 * repo/
 * repo/branches/
 * repo/trunk/
 * repo/tags/
 *
 * The tags directory (which can be user specified in the UE SVN settings) is the one we are interested in.
 * This implementation assumes that each subdirectory (e.g. repo/tags/LabelName) in the tags dir is an 
 * analogue of a Perforce label. That is, the revision of the folder specifies a tagged revision of the 
 * repository. For the moment, our 'labels' don't filter the parts of the repo that are under them, so 
 * they effectively just act as metadata on a revision number across the whole repo.
 */
class FSubversionSourceControlLabel : public ISourceControlLabel, public TSharedFromThis<FSubversionSourceControlLabel>
{
public:

	FSubversionSourceControlLabel( const FString& InName, const FString& InDirectory, int32 InRevision )
		: Name(InName)
		, Directory(InDirectory)
		, Revision(InRevision)
	{
	}

	/** ISourceControlLabel implementation */
	virtual const FString& GetName() const override;
	virtual bool GetFileRevisions( const TArray<FString>& InFiles, TArray< TSharedRef<ISourceControlRevision, ESPMode::ThreadSafe> >& OutRevisions ) const override;
	virtual bool Sync( const TArray<FString>& InFilenames ) const override;

private:

	/** Label name */
	FString Name;

	/** Label directory in the repository */
	FString Directory;

	/** Repository revision this label was created at */
	int32 Revision;
};
