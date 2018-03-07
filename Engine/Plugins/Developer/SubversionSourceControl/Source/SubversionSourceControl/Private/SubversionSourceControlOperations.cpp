// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SubversionSourceControlOperations.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "SourceControlOperations.h"
#include "ISourceControlModule.h"
#include "SubversionSourceControlCommand.h"
#include "SubversionSourceControlModule.h"
#include "XmlFile.h"

#define LOCTEXT_NAMESPACE "SubversionSourceControl"

FName FSubversionConnectWorker::GetName() const
{
	return "Connect";
}

bool FSubversionConnectWorker::Execute(FSubversionSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());
	TSharedRef<FConnect, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FConnect>(InCommand.Operation);
	FString Password = InCommand.Password;
	// see if we are getting a password passed in from the calling code
	if(Operation->GetPassword().Len() > 0)
	{
		Password = Operation->GetPassword();
	}

	{
		TArray<FXmlFile> ResultsXml;
		TArray<FString> Parameters;
		FString GameRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		// We need to manually quote the filename because we're not passing the file argument via RunCommand's InFiles parameter
		SubversionSourceControlUtils::QuoteFilename(GameRoot);
		Parameters.Add(GameRoot);
	
		InCommand.bCommandSuccessful = SubversionSourceControlUtils::RunCommand(TEXT("info"), TArray<FString>(), Parameters, ResultsXml, InCommand.ErrorMessages, InCommand.UserName, Password);	
		if(InCommand.bCommandSuccessful)
		{
			SubversionSourceControlUtils::ParseInfoResults(ResultsXml, WorkingCopyRoot, RepositoryRoot);
		}
	}

	if(InCommand.bCommandSuccessful)
	{
		TArray<FXmlFile> ResultsXml;
		TArray<FString> Files;
		FString GameRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		Files.Add(GameRoot);

		TArray<FString> StatusParameters;
		StatusParameters.Add(TEXT("--show-updates"));
		StatusParameters.Add(TEXT("--verbose"));

		InCommand.bCommandSuccessful = SubversionSourceControlUtils::RunCommand(TEXT("status"), Files, StatusParameters, ResultsXml, InCommand.ErrorMessages, InCommand.UserName, Password);

		if(InCommand.bCommandSuccessful)
		{
			// Check to see if this was a working copy - if not deny connection as we wont be able to work with it.
			TArray<FSubversionSourceControlState> States;
			SubversionSourceControlUtils::ParseStatusResults(ResultsXml, InCommand.ErrorMessages, InCommand.UserName, InCommand.WorkingCopyRoot, States);
			if(InCommand.ErrorMessages.Num() > 0)
			{
				for (int32 MessageIndex = 0; MessageIndex < InCommand.ErrorMessages.Num(); ++MessageIndex)
				{
					const FString& Error = InCommand.ErrorMessages[MessageIndex];
					int32 Pattern = Error.Find(TEXT("' is not a working copy"), ESearchCase::IgnoreCase, ESearchDir::FromStart);
					if(Pattern != INDEX_NONE)
					{
						StaticCastSharedRef<FConnect>(InCommand.Operation)->SetErrorText(LOCTEXT("NotAWorkingCopyError", "Project is not part of an SVN working copy."));
						InCommand.ErrorMessages.Add(LOCTEXT("NotAWorkingCopyErrorHelp", "You should check out a working copy into your project directory.").ToString());
						InCommand.bCommandSuccessful = false;
						break;
					}
				}
			}
		}	
	}

	return InCommand.bCommandSuccessful;
}

bool FSubversionConnectWorker::UpdateStates() const
{
	FSubversionSourceControlModule& SubversionSourceControl = FModuleManager::GetModuleChecked<FSubversionSourceControlModule>( "SubversionSourceControl" );
	FSubversionSourceControlProvider& Provider = SubversionSourceControl.GetProvider();
	Provider.SetWorkingCopyRoot(WorkingCopyRoot);
	Provider.SetRepositoryRoot(RepositoryRoot);
	return true;
}

FName FSubversionCheckOutWorker::GetName() const
{
	return "CheckOut";
}

bool FSubversionCheckOutWorker::Execute(FSubversionSourceControlCommand& InCommand)
{
	// @todo: we only need to lock binary files to simulate a 'checkout' state, so split files up according to type

	InCommand.bCommandSuccessful = SubversionSourceControlUtils::RunCommand(TEXT("lock"), InCommand.Files, TArray<FString>(), InCommand.InfoMessages, InCommand.ErrorMessages, InCommand.UserName);

	if(InCommand.bCommandSuccessful)
	{
		// annoyingly, we need remove any read-only flags here (for cross-working with Perforce)
		for(auto Iter(InCommand.Files.CreateConstIterator()); Iter; Iter++)
		{
			FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(**Iter, false);
		}
	}

	// now update the status of our files
	{
		TArray<FXmlFile> ResultsXml;
		TArray<FString> StatusParameters;
		StatusParameters.Add(TEXT("--show-updates"));
		StatusParameters.Add(TEXT("--verbose"));

		InCommand.bCommandSuccessful = SubversionSourceControlUtils::RunCommand(TEXT("status"), InCommand.Files, StatusParameters, ResultsXml, InCommand.ErrorMessages, InCommand.UserName);
		SubversionSourceControlUtils::ParseStatusResults(ResultsXml, InCommand.ErrorMessages, InCommand.UserName, InCommand.WorkingCopyRoot, OutStates);
	}

	return InCommand.bCommandSuccessful;
}

bool FSubversionCheckOutWorker::UpdateStates() const
{
	return SubversionSourceControlUtils::UpdateCachedStates(OutStates);
}

FName FSubversionCheckInWorker::GetName() const
{
	return "CheckIn";
}

/** Helper function for AddDirectoriesToCommit() - determines whether a directory is currently marked for add */
static bool IsDirectoryAdded(const FSubversionSourceControlCommand& InCommand, const FString& InDirectory)
{
	TArray<FXmlFile> ResultsXml;
	TArray<FString> ErrorMessages;
	TArray<FString> StatusParameters;
	StatusParameters.Add(TEXT("--verbose"));
	StatusParameters.Add(TEXT("--show-updates"));

	TArray<FString> Files;
	Files.Add(InDirectory);

	if(SubversionSourceControlUtils::RunCommand(TEXT("status"), Files, StatusParameters, ResultsXml, ErrorMessages, InCommand.UserName))
	{
		TArray<FSubversionSourceControlState> OutStates;
		SubversionSourceControlUtils::ParseStatusResults(ResultsXml, ErrorMessages, InCommand.UserName, InCommand.WorkingCopyRoot, OutStates);
		
		for(auto It(OutStates.CreateConstIterator()); It; It++)
		{
			const FSubversionSourceControlState& State = *It;
			if(State.GetFilename() == InDirectory)
			{
				return State.IsAdded();
			}
		}
	}

	return false;
}

/** 
 * Helper function for FCheckInWorker::Execute.
 * Makes sure directories are committed with files that are also marked for add.
 * If we don't do this, the commit will fail.
 */
static void AddDirectoriesToCommit(const FSubversionSourceControlCommand& InCommand, TArray<FString>& InOutFiles)
{
	// because of the use of "--parents" when we mark for add, we can just traverse up 
	// the directory tree until we meet a directory that isn't already marked for add

	TArray<FString> Directories;

	FString WorkingCopyRoot = InCommand.WorkingCopyRoot;
	FPaths::NormalizeDirectoryName(WorkingCopyRoot);

	for(const auto& Filename : InOutFiles)
	{
		FString Directory = FPaths::GetPath(Filename);
		FPaths::NormalizeDirectoryName(Directory);

		// Stop once we leave our working copy, or find a directory that isn't marked for add
		while(Directory.StartsWith(WorkingCopyRoot))
		{
			// Stop if we've already processed this directory, or if this directory isn't marked for add
			if(Directories.Contains(Directory) || !IsDirectoryAdded(InCommand, Directory))
			{
				break;
			}

			Directories.Add(Directory);

			// Chop off the end of this directory to move up to our parent directory
			// This is safe to do since we called NormalizeDirectoryName on it, and we're testing to make sure we stay within the working copy
			int32 ChopPoint = INDEX_NONE;
			if(Directory.FindLastChar('/', ChopPoint))
			{
				Directory = Directory.Left(ChopPoint);
			}
			else
			{
				// No more path to process
				break;
			}
		}
	}

	InOutFiles.Append(MoveTemp(Directories));
}

static FText ParseCommitResults(const TArray<FString>& InResults)
{
	for(const auto& Result : InResults)
	{
		// @todo: We could potentially parse the recent history for the last commit by this user here. This is the simpler option.
		const FString ExpectedText(TEXT("Committed revision"));
		if(Result.Contains(ExpectedText))
		{
			const FString RevisionNumberString = Result.RightChop(ExpectedText.Len());
			return FText::Format(LOCTEXT("CommitMessage", "Submitted revision {0}."), FText::AsNumber(FCString::Atoi(*RevisionNumberString)));
		}
	}

	return LOCTEXT("CommitMessageUnknown", "Submitted revision.");
}

/** Helper function for ReleaseAnyLocksForCopies */
static bool FindSourceRepoFileForCopy(const FString& InOrigFile, const FString& InUserName, FString& OutSourceFile)
{
	// we are copied, so we need to find our original file here. To do this we need to get the files recent history
	TArray<FXmlFile> ResultsXml;
	TArray<FString> Parameters;
	TArray<FString> ErrorMessages;

	//limit to last 100 changes
	Parameters.Add(TEXT("--limit 10"));
	// output all properties
	Parameters.Add(TEXT("--with-all-revprops"));
	// we want all the output!
	Parameters.Add(TEXT("--verbose"));

	TArray<FString> Files;
	Files.Add(InOrigFile);

	SubversionSourceControlUtils::FHistoryOutput History;

	if(SubversionSourceControlUtils::RunCommand(TEXT("log"), Files, Parameters, ResultsXml, ErrorMessages, InUserName))
	{
		SubversionSourceControlUtils::ParseLogResults(InOrigFile, ResultsXml, InUserName, History);

		// find first history item that doesnt match the filename
		for(const auto& HistoryEntry : History)
		{
			if(HistoryEntry.Key == InOrigFile)
			{
				if(HistoryEntry.Value.Num() > 0)
				{
					OutSourceFile = HistoryEntry.Value[0]->RepoFilename;
					return true;				
				}
			}
		}
	}

	return false;
}


/** 
 * Helper function that releases file locks on source files we have performed copies on 
 * If we do not do this then commits will fail complaining about the source file of the copy operation
 * being 'locked in another working copy'.
 */
static void ReleaseAnyLocksForCopies(const TArray<FString>& InFilesToCommit, const FString& InWorkingCopyRoot, const FString& InRepoRoot, const FString& InUserName)
{
	// first get the status of the files
	TArray<FSubversionSourceControlState> States;
	{
		TArray<FXmlFile> ResultsXml;
		TArray<FString> ErrorMessages;
		
		TArray<FString> StatusParameters;
		StatusParameters.Add(TEXT("--verbose"));
		StatusParameters.Add(TEXT("--show-updates"));

		SubversionSourceControlUtils::RunCommand(TEXT("status"), InFilesToCommit, StatusParameters, ResultsXml, ErrorMessages, InUserName);
		SubversionSourceControlUtils::ParseStatusResults(ResultsXml, ErrorMessages, InUserName, InWorkingCopyRoot, States);
	}

	// now unlock any that need it
	{
		TArray<FString> FilesToUnlock;
		for(const auto& State : States)
		{
			if(State.bCopied)
			{
				FString SourceRepoFileForCopy;
				if(FindSourceRepoFileForCopy(State.GetFilename(), InUserName, SourceRepoFileForCopy))
				{
					SourceRepoFileForCopy = InRepoRoot / SourceRepoFileForCopy;
					FilesToUnlock.Add(SourceRepoFileForCopy);
				}
			}
		}

		if(FilesToUnlock.Num() > 0)
		{
			UE_LOG(LogSourceControl, Log, TEXT("Unlocking %d files that were copied before commit"), FilesToUnlock.Num());

			TArray<FString> Results;
			TArray<FString> ErrorMessages;

			SubversionSourceControlUtils::RunCommand(TEXT("unlock"), FilesToUnlock, TArray<FString>(), Results, ErrorMessages, InUserName);
		}
	}
}

bool FSubversionCheckInWorker::Execute(FSubversionSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());
	TSharedRef<FCheckIn, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FCheckIn>(InCommand.Operation);
	{
		// make a temp file to place our message in
		FSVNScopedTempFile DescriptionFile(Operation->GetDescription());
		if(DescriptionFile.GetFilename().Len() > 0)
		{
			TArray<FString> Parameters;
			FString DescriptionFilename = DescriptionFile.GetFilename();
			// We need to manually quote the filename because we're not passing the file argument via RunCommand's InFiles parameter
			SubversionSourceControlUtils::QuoteFilename(DescriptionFilename);
			Parameters.Add(FString(TEXT("--file ")) + DescriptionFilename);
			Parameters.Add(TEXT("--encoding utf-8"));

			// we need commit directories that are marked for add here if we are committing any child files that are also marked for add
			TArray<FString> FilesToCommit = InCommand.Files;
			AddDirectoriesToCommit(InCommand, FilesToCommit);

			// we need another temp file to add our file list to (as this must be an atomic operation we cant risk overflowing command-line limits)
			FString Targets;
			for(auto It(FilesToCommit.CreateConstIterator()); It; It++)
			{
				Targets += *It + LINE_TERMINATOR;
			}

			FSVNScopedTempFile TargetsFile(Targets);
			if(TargetsFile.GetFilename().Len() > 0)
			{
				// we first need to release locks if we have any copy (branch) operations in our changes
				ReleaseAnyLocksForCopies(InCommand.Files, InCommand.WorkingCopyRoot, InCommand.RepositoryRoot, InCommand.UserName);

				FString TargetsFilename = TargetsFile.GetFilename();
				// We need to manually quote the filename because we're not passing the file argument via RunCommand's InFiles parameter
				SubversionSourceControlUtils::QuoteFilename(TargetsFilename);
				Parameters.Add(FString(TEXT("--targets ")) + TargetsFilename);

				InCommand.bCommandSuccessful = SubversionSourceControlUtils::RunAtomicCommand(TEXT("commit"), TArray<FString>(), Parameters, InCommand.InfoMessages, InCommand.ErrorMessages, InCommand.UserName);
				if(InCommand.bCommandSuccessful)
				{
					// Remove any deleted files from status cache
					FSubversionSourceControlModule& SubversionSourceControl = FModuleManager::GetModuleChecked<FSubversionSourceControlModule>("SubversionSourceControl");
					FSubversionSourceControlProvider& Provider = SubversionSourceControl.GetProvider();

					TArray<TSharedRef<ISourceControlState, ESPMode::ThreadSafe>> States;
					Provider.GetState(InCommand.Files, States, EStateCacheUsage::Use);
					for (const auto& State : States)
					{
						if (State->IsDeleted())
						{
							Provider.RemoveFileFromCache(State->GetFilename());
						}
					}

					StaticCastSharedRef<FCheckIn>(InCommand.Operation)->SetSuccessMessage(ParseCommitResults(InCommand.InfoMessages));
				}
			}
		}
	}

	// now update the status of our files
	{
		TArray<FXmlFile> ResultsXml;
		TArray<FString> StatusParameters;
		StatusParameters.Add(TEXT("--verbose"));
		StatusParameters.Add(TEXT("--show-updates"));

		InCommand.bCommandSuccessful &= SubversionSourceControlUtils::RunCommand(TEXT("status"), InCommand.Files, StatusParameters, ResultsXml, InCommand.ErrorMessages, InCommand.UserName);
		SubversionSourceControlUtils::ParseStatusResults(ResultsXml, InCommand.ErrorMessages, InCommand.UserName, InCommand.WorkingCopyRoot, OutStates);
	}

	return InCommand.bCommandSuccessful;
}

bool FSubversionCheckInWorker::UpdateStates() const
{
	return SubversionSourceControlUtils::UpdateCachedStates(OutStates);
}

FName FSubversionMarkForAddWorker::GetName() const
{
	return "MarkForAdd";
}

bool FSubversionMarkForAddWorker::Execute(FSubversionSourceControlCommand& InCommand)
{
	TArray<FString> Parameters;
	// Make sure we add files if we encounter one that has already been added.
	Parameters.Add(TEXT("--force"));
	// Add nonexistent/non-versioned parent directories too
	Parameters.Add(TEXT("--parents"));

	InCommand.bCommandSuccessful = SubversionSourceControlUtils::RunCommand(TEXT("add"), InCommand.Files, Parameters, InCommand.InfoMessages, InCommand.ErrorMessages, InCommand.UserName);

	// now update the status of our files
	{
		TArray<FXmlFile> ResultsXml;
		TArray<FString> StatusParameters;
		StatusParameters.Add(TEXT("--verbose"));

		InCommand.bCommandSuccessful &= SubversionSourceControlUtils::RunCommand(TEXT("status"), InCommand.Files, StatusParameters, ResultsXml, InCommand.ErrorMessages, InCommand.UserName);
		SubversionSourceControlUtils::ParseStatusResults(ResultsXml, InCommand.ErrorMessages, InCommand.UserName, InCommand.WorkingCopyRoot, OutStates);
	}

	return InCommand.bCommandSuccessful;
}

bool FSubversionMarkForAddWorker::UpdateStates() const
{
	return SubversionSourceControlUtils::UpdateCachedStates(OutStates);
}

FName FSubversionDeleteWorker::GetName() const
{
	return "Delete";
}

bool FSubversionDeleteWorker::Execute(FSubversionSourceControlCommand& InCommand)
{
	TArray<FString> Parameters;

	InCommand.bCommandSuccessful = SubversionSourceControlUtils::RunCommand(TEXT("delete"), InCommand.Files, Parameters, InCommand.InfoMessages, InCommand.ErrorMessages, InCommand.UserName);

	// now update the status of our files
	{
		TArray<FXmlFile> ResultsXml;
		TArray<FString> StatusParameters;
		StatusParameters.Add(TEXT("--verbose"));

		InCommand.bCommandSuccessful &= SubversionSourceControlUtils::RunCommand(TEXT("status"), InCommand.Files, StatusParameters, ResultsXml, InCommand.ErrorMessages, InCommand.UserName);
		SubversionSourceControlUtils::ParseStatusResults(ResultsXml, InCommand.ErrorMessages, InCommand.UserName, InCommand.WorkingCopyRoot, OutStates);
	}

	return InCommand.bCommandSuccessful;
}

bool FSubversionDeleteWorker::UpdateStates() const
{
	return SubversionSourceControlUtils::UpdateCachedStates(OutStates);
}

FName FSubversionRevertWorker::GetName() const
{
	return "Revert";
}

bool FSubversionRevertWorker::Execute(FSubversionSourceControlCommand& InCommand)
{
	// revert any changes
	{
		TArray<FString> Parameters;
		InCommand.bCommandSuccessful = SubversionSourceControlUtils::RunCommand(TEXT("revert"), InCommand.Files, Parameters, InCommand.InfoMessages, InCommand.ErrorMessages, InCommand.UserName);
	}

	// unlock any files
	{
		TArray<FString> Parameters;
		InCommand.bCommandSuccessful &= SubversionSourceControlUtils::RunCommand(TEXT("unlock"), InCommand.Files, Parameters, InCommand.InfoMessages, InCommand.ErrorMessages, InCommand.UserName);
	}

	// now update the status of our files
	{
		TArray<FXmlFile> ResultsXml;
		TArray<FString> StatusParameters;
		StatusParameters.Add(TEXT("--verbose"));
		StatusParameters.Add(TEXT("--show-updates"));

		InCommand.bCommandSuccessful &= SubversionSourceControlUtils::RunCommand(TEXT("status"), InCommand.Files, StatusParameters, ResultsXml, InCommand.ErrorMessages, InCommand.UserName);
		SubversionSourceControlUtils::ParseStatusResults(ResultsXml, InCommand.ErrorMessages, InCommand.UserName, InCommand.WorkingCopyRoot, OutStates);
	}

	return InCommand.bCommandSuccessful;
}

bool FSubversionRevertWorker::UpdateStates() const
{
	return SubversionSourceControlUtils::UpdateCachedStates(OutStates);
}

FName FSubversionSyncWorker::GetName() const
{
	return "Sync";
}

bool FSubversionSyncWorker::Execute(FSubversionSourceControlCommand& InCommand)
{
	InCommand.bCommandSuccessful = SubversionSourceControlUtils::RunCommand(TEXT("update"), InCommand.Files, TArray<FString>(), InCommand.InfoMessages, InCommand.ErrorMessages, InCommand.UserName);

	// now update the status of our files
	{
		TArray<FXmlFile> ResultsXml;
		TArray<FString> StatusParameters;
		StatusParameters.Add(TEXT("--verbose"));
		StatusParameters.Add(TEXT("--show-updates"));

		InCommand.bCommandSuccessful &= SubversionSourceControlUtils::RunCommand(TEXT("status"), InCommand.Files, StatusParameters, ResultsXml, InCommand.ErrorMessages, InCommand.UserName);
		SubversionSourceControlUtils::ParseStatusResults(ResultsXml, InCommand.ErrorMessages, InCommand.UserName, InCommand.WorkingCopyRoot, OutStates);
	}

	return InCommand.bCommandSuccessful;
}

bool FSubversionSyncWorker::UpdateStates() const
{
	return SubversionSourceControlUtils::UpdateCachedStates(OutStates);
}

FName FSubversionUpdateStatusWorker::GetName() const
{
	return "UpdateStatus";
}

bool FSubversionUpdateStatusWorker::Execute(FSubversionSourceControlCommand& InCommand)
{
	// update using any special hints passed in via the operation
	check(InCommand.Operation->GetName() == GetName());
	TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FUpdateStatus>(InCommand.Operation);

	if(InCommand.Files.Num() > 0)
	{
		TArray<FXmlFile> ResultsXml;
		TArray<FString> Parameters;
		Parameters.Add(TEXT("--show-updates"));
		Parameters.Add(TEXT("--verbose"));

		TArray<FString> Files;
		if(Operation->ShouldCheckAllFiles() && InCommand.Files.Num() > 1)
		{
			// Prime the resultant states here depending on whether the files are under the 
			// working copy or not.
			// This works because these states will be processed first when they come to be updated on
			// the main thread, before being updated with any later on in the array by any that were 
			// returned from the svn status command.

			Files.Add(InCommand.WorkingCopyRoot);

			for(auto Iter(InCommand.Files.CreateConstIterator()); Iter; ++Iter)
			{
				FSubversionSourceControlState State(*Iter);

				if(State.GetFilename().StartsWith(InCommand.WorkingCopyRoot))
				{
					State.WorkingCopyState = EWorkingCopyState::NotControlled;
				}
				else
				{
					State.WorkingCopyState = EWorkingCopyState::NotAWorkingCopy;
				}

				OutStates.Add(State);
			}
		}
		else
		{
			Files.Append(InCommand.Files);
		}

		InCommand.bCommandSuccessful = SubversionSourceControlUtils::RunCommand(TEXT("status"), Files, Parameters, ResultsXml, InCommand.ErrorMessages, InCommand.UserName);
		SubversionSourceControlUtils::ParseStatusResults(ResultsXml, InCommand.ErrorMessages, InCommand.UserName, InCommand.WorkingCopyRoot, OutStates);
		SubversionSourceControlUtils::RemoveRedundantErrors(InCommand, TEXT("' is not a working copy"));
	}
	else
	{
		InCommand.bCommandSuccessful = true;
	}

	if(Operation->ShouldUpdateHistory())
	{
		for(auto Iter(InCommand.Files.CreateConstIterator()); Iter; Iter++)
		{
			TArray<FXmlFile> ResultsXml;
			TArray<FString> Parameters;

			//limit to last 100 changes
			Parameters.Add(TEXT("--limit 100"));
			// output all properties
			Parameters.Add(TEXT("--with-all-revprops"));
			// we want all the output!
			Parameters.Add(TEXT("--verbose"));

			TArray<FString> Files;
			Files.Add(*Iter);

			InCommand.bCommandSuccessful &= SubversionSourceControlUtils::RunCommand(TEXT("log"), Files, Parameters, ResultsXml, InCommand.ErrorMessages, InCommand.UserName);
			SubversionSourceControlUtils::ParseLogResults(Iter->TrimQuotes(), ResultsXml, InCommand.UserName, OutHistory);
		}
	}

	if(Operation->ShouldGetOpenedOnly())
	{
		TArray<FXmlFile> ResultsXml;
		TArray<FString> Parameters;
		Parameters.Add(TEXT("--show-updates"));
		Parameters.Add(TEXT("--verbose"));

		TArray<FString> Files;
		Files.Add(FPaths::RootDir());

		InCommand.bCommandSuccessful &= SubversionSourceControlUtils::RunCommand(TEXT("status"), Files, Parameters, ResultsXml, InCommand.ErrorMessages, InCommand.UserName);
		SubversionSourceControlUtils::ParseStatusResults(ResultsXml, InCommand.ErrorMessages, InCommand.UserName, InCommand.WorkingCopyRoot, OutStates);
	}

	// NOTE: we dont use the ShouldUpdateModifiedState() hint here as a normal svn status will tell us this information

	return InCommand.bCommandSuccessful;
}

bool FSubversionUpdateStatusWorker::UpdateStates() const
{
	bool bUpdated = false;

	FSubversionSourceControlModule& SubversionSourceControl = FModuleManager::GetModuleChecked<FSubversionSourceControlModule>( "SubversionSourceControl" );
	FSubversionSourceControlProvider& Provider = SubversionSourceControl.GetProvider();

	bUpdated |= SubversionSourceControlUtils::UpdateCachedStates(OutStates);

	// add history, if any
	for(SubversionSourceControlUtils::FHistoryOutput::TConstIterator It(OutHistory); It; ++It)
	{
		TSharedRef<FSubversionSourceControlState, ESPMode::ThreadSafe> State = Provider.GetStateInternal(It.Key());
		State->History = It.Value();
		State->TimeStamp = FDateTime::Now();
		bUpdated = true;
	}

	return bUpdated;
}

FName FSubversionCopyWorker::GetName() const
{
	return "Copy";
}

bool FSubversionCopyWorker::Execute(FSubversionSourceControlCommand& InCommand)
{
	check(InCommand.Operation->GetName() == GetName());
	TSharedRef<FCopy, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FCopy>(InCommand.Operation);

	FString Destination = FPaths::ConvertRelativePathToFull(Operation->GetDestination());

	// perform svn revert if the dest file already exists in the working copy (this is usually the case
	// as files that are copied in the editor are already marked for add when the package is created
	// in the new location)
	{
		TArray<FString> Files;
		Files.Add(Destination);

		InCommand.bCommandSuccessful = SubversionSourceControlUtils::RunCommand(TEXT("revert"), Files, TArray<FString>(), InCommand.InfoMessages, InCommand.ErrorMessages, InCommand.UserName);
	}

	// Now we need to move the file out of directory, copy then restore over the top, as SVN wont allow us 
	// to 'svn copy' over an existing file, even if it is not already added to the working copy.
	// This will be OK as far as the asset registry/directory watcher goes as it will just see the file being modified several times
	const FString TempFileName = FPaths::CreateTempFilename(*FPaths::ProjectLogDir(), TEXT("SVN-CopyTemp"), TEXT(".uasset"));
	const bool bReplace = true;
	const bool bEvenIfReadOnly = true;
	
	if (InCommand.bCommandSuccessful)
	{
		InCommand.bCommandSuccessful = IFileManager::Get().Move(*TempFileName, *Destination, bReplace, bEvenIfReadOnly);
	}

	// copy from source files to destination parameter
	if(InCommand.bCommandSuccessful)
	{
		TArray<FString> Files;
		Files.Append(InCommand.Files);
		Files.Add(Destination);

		TArray<FString> Parameters;
		// Add nonexistent/non-versioned parent directories too
		Parameters.Add(TEXT("--parents"));

		InCommand.bCommandSuccessful = SubversionSourceControlUtils::RunCommand(TEXT("copy"), Files, Parameters, InCommand.InfoMessages, InCommand.ErrorMessages, InCommand.UserName);
	}

	// now move the file back
	if (InCommand.bCommandSuccessful)
	{
		InCommand.bCommandSuccessful = IFileManager::Get().Move(*Destination, *TempFileName, bReplace, bEvenIfReadOnly);
	}

	// now update the status of our files
	{
		TArray<FXmlFile> ResultsXml;
		TArray<FString> StatusParameters;
		StatusParameters.Add(TEXT("--verbose"));
		StatusParameters.Add(TEXT("--show-updates"));

		// Get status of both source and destination files
		TArray<FString> StatusFiles;
		StatusFiles.Append(InCommand.Files);
		StatusFiles.Add(Destination);

		InCommand.bCommandSuccessful &= SubversionSourceControlUtils::RunCommand(TEXT("status"), StatusFiles, StatusParameters, ResultsXml, InCommand.ErrorMessages, InCommand.UserName);
		SubversionSourceControlUtils::ParseStatusResults(ResultsXml, InCommand.ErrorMessages, InCommand.UserName, InCommand.WorkingCopyRoot, OutStates);
	}

	return InCommand.bCommandSuccessful;
}

bool FSubversionCopyWorker::UpdateStates() const
{
	return SubversionSourceControlUtils::UpdateCachedStates(OutStates);
}

FName FSubversionResolveWorker::GetName() const
{
	return "Resolve";
}

bool FSubversionResolveWorker::Execute( class FSubversionSourceControlCommand& InCommand )
{
	// mark the conflicting files as resolved:
	{
		TArray<FString> Results;
		TArray<FString> ResolveParameters;
		ResolveParameters.Add(TEXT("--accept mine-full"));
		InCommand.bCommandSuccessful = SubversionSourceControlUtils::RunCommand(TEXT("resolve"), InCommand.Files, ResolveParameters, Results, InCommand.ErrorMessages, InCommand.UserName);
	}

	// now update the status of our files
	{
		TArray<FXmlFile> ResultsXml;
		TArray<FString> StatusParameters;
		StatusParameters.Add(TEXT("--verbose"));
		StatusParameters.Add(TEXT("--show-updates"));

		InCommand.bCommandSuccessful &= SubversionSourceControlUtils::RunCommand(TEXT("status"), InCommand.Files, StatusParameters, ResultsXml, InCommand.ErrorMessages, InCommand.UserName);
		SubversionSourceControlUtils::ParseStatusResults(ResultsXml, InCommand.ErrorMessages, InCommand.UserName, InCommand.WorkingCopyRoot, OutStates);
	}

	return InCommand.bCommandSuccessful;
}

bool FSubversionResolveWorker::UpdateStates() const
{
	return SubversionSourceControlUtils::UpdateCachedStates(OutStates);
}

#undef LOCTEXT_NAMESPACE
