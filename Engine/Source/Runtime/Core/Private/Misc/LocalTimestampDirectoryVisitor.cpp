// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/LocalTimestampDirectoryVisitor.h"
#include "Misc/Paths.h"

/* FLocalTimestampVisitor structors
 *****************************************************************************/

FLocalTimestampDirectoryVisitor::FLocalTimestampDirectoryVisitor( IPlatformFile& InFileInterface, const TArray<FString>& InDirectoriesToIgnore, const TArray<FString>& InDirectoriesToNotRecurse, bool bInCacheDirectories )
	: bCacheDirectories(bInCacheDirectories)
	, FileInterface(InFileInterface)
{
	// make sure the paths are standardized, since the Visitor will assume they are standard
	for (int32 DirIndex = 0; DirIndex < InDirectoriesToIgnore.Num(); DirIndex++)
	{
		FString DirToIgnore = InDirectoriesToIgnore[DirIndex];
		FPaths::MakeStandardFilename(DirToIgnore);
		DirectoriesToIgnore.Add(DirToIgnore);
	}

	for (int32 DirIndex = 0; DirIndex < InDirectoriesToNotRecurse.Num(); DirIndex++)
	{
		FString DirToNotRecurse = InDirectoriesToNotRecurse[DirIndex];
		FPaths::MakeStandardFilename(DirToNotRecurse);
		DirectoriesToNotRecurse.Add(DirToNotRecurse);
	}
}


/* FLocalTimestampVisitor interface
 *****************************************************************************/

bool FLocalTimestampDirectoryVisitor::Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
{
	// make sure all paths are "standardized" so the other end can match up with it's own standardized paths
	FString RelativeFilename = FilenameOrDirectory;
	FPaths::MakeStandardFilename(RelativeFilename);

	// cache files and optionally directories
	if (!bIsDirectory)
	{
		FileTimes.Add(RelativeFilename, FileInterface.GetTimeStamp(FilenameOrDirectory));
	}
	else if (bCacheDirectories)
	{
		// we use a timestamp of 0 to indicate a directory
		FileTimes.Add(RelativeFilename, 0);
	}

	// iterate over directories we care about
	if (bIsDirectory)
	{
		bool bShouldRecurse = true;
		// look in all the ignore directories looking for a match
		for (int32 DirIndex = 0; DirIndex < DirectoriesToIgnore.Num() && bShouldRecurse; DirIndex++)
		{
			if (RelativeFilename.StartsWith(DirectoriesToIgnore[DirIndex]))
			{
				bShouldRecurse = false;
			}
		}

		if (bShouldRecurse == true)
		{
			// If it is a directory that we should not recurse (ie we don't want to process subdirectories of it)
			// handle that case as well...
			for (int32 DirIndex = 0; DirIndex < DirectoriesToNotRecurse.Num() && bShouldRecurse; DirIndex++)
			{
				if (RelativeFilename.StartsWith(DirectoriesToNotRecurse[DirIndex]))
				{
					// Are we more than level deep in that directory?
					FString CheckFilename = RelativeFilename.Right(RelativeFilename.Len() - DirectoriesToNotRecurse[DirIndex].Len());
					if (CheckFilename.Len() > 1)
					{
						bShouldRecurse = false;
					}
				}
			}
		}

		// recurse if we should
		if (bShouldRecurse)
		{
			FileInterface.IterateDirectory(FilenameOrDirectory, *this);
		}
	}

	return true;
}
