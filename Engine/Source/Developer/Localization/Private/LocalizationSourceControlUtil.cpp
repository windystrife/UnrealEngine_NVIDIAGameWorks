// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LocalizationSourceControlUtil.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogLocalizationSourceControl, Log, All);

#define LOCTEXT_NAMESPACE "LocalizationSourceControl"

FLocalizationSCC::FLocalizationSCC()
{
	ISourceControlModule::Get().GetProvider().Init();
}

FLocalizationSCC::~FLocalizationSCC()
{
	if (CheckedOutFiles.Num() > 0)
	{
		UE_LOG(LogLocalizationSourceControl, Log, TEXT("Source Control wrapper shutting down with checked out files."));
	}

	ISourceControlModule::Get().GetProvider().Close();
}

bool FLocalizationSCC::CheckOutFile(const FString& InFile, FText& OutError)
{
	if (InFile.IsEmpty())
	{
		OutError = LOCTEXT("InvalidFileSpecified", "Could not checkout file at invalid path.");
		return false;
	}

	if (InFile.StartsWith(TEXT("\\\\")))
	{
		// We can't check out a UNC path, but don't say we failed
		return true;
	}

	FText SCCError;
	if (!IsReady(SCCError))
	{
		OutError = SCCError;
		return false;
	}

	FString AbsoluteFilename = FPaths::ConvertRelativePathToFull(InFile);

	if (CheckedOutFiles.Contains(AbsoluteFilename))
	{
		return true;
	}

	bool bSuccessfullyCheckedOut = false;
	TArray<FString> FilesToBeCheckedOut;
	FilesToBeCheckedOut.Add(AbsoluteFilename);

	FFormatNamedArguments Args;
	Args.Add(TEXT("Filepath"), FText::FromString(InFile));

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(AbsoluteFilename, EStateCacheUsage::ForceUpdate);

	if (SourceControlState.IsValid() && SourceControlState->IsDeleted())
	{
		// If it's deleted, we need to revert that first
		SourceControlProvider.Execute(ISourceControlOperation::Create<FRevert>(), FilesToBeCheckedOut);
		SourceControlState = SourceControlProvider.GetState(AbsoluteFilename, EStateCacheUsage::ForceUpdate);
	}

	if (SourceControlState.IsValid())
	{
		FString Other;
		if (SourceControlState->IsAdded() ||
			SourceControlState->IsCheckedOut())
		{
			// Already checked out or opened for add
			bSuccessfullyCheckedOut = true;
		}
		else if (SourceControlState->CanCheckout())
		{
			bSuccessfullyCheckedOut = (SourceControlProvider.Execute(ISourceControlOperation::Create<FCheckOut>(), FilesToBeCheckedOut) == ECommandResult::Succeeded);
			if (!bSuccessfullyCheckedOut)
			{
				OutError = FText::Format(LOCTEXT("FailedToCheckOutFile", "Failed to check out file '{Filepath}'."), Args);
			}
		}
		else if (!SourceControlState->IsSourceControlled() && SourceControlState->CanAdd())
		{
			bSuccessfullyCheckedOut = (SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), FilesToBeCheckedOut) == ECommandResult::Succeeded);
			if (!bSuccessfullyCheckedOut)
			{
				OutError = FText::Format(LOCTEXT("FailedToAddFileToSourceControl", "Failed to add file '{Filepath}' to source control."), Args);
			}
		}
		else if (!SourceControlState->IsCurrent())
		{
			OutError = FText::Format(LOCTEXT("FileIsNotAtHeadRevision", "File '{Filepath}' is not at head revision."), Args);
		}
		else if (SourceControlState->IsCheckedOutOther(&(Other)))
		{
			Args.Add(TEXT("Username"), FText::FromString(Other));
			OutError = FText::Format(LOCTEXT("FileIsAlreadyCheckedOutByAnotherUser", "File '{Filepath}' is checked out by another ('{Username}')."), Args);
		}
		else
		{
			// Improper or invalid SCC state
			OutError = FText::Format(LOCTEXT("CouldNotGetStateOfFile", "Could not determine source control state of file '{Filepath}'."), Args);
		}
	}
	else
	{
		// Improper or invalid SCC state
		OutError = FText::Format(LOCTEXT("CouldNotGetStateOfFile", "Could not determine source control state of file '{Filepath}'."), Args);
	}

	if (bSuccessfullyCheckedOut)
	{
		CheckedOutFiles.AddUnique(AbsoluteFilename);
	}

	return bSuccessfullyCheckedOut;
}

bool FLocalizationSCC::CheckinFiles(const FText& InChangeDescription, FText& OutError)
{
	if (CheckedOutFiles.Num() == 0)
	{
		return true;
	}

	FText SCCError;
	if (!IsReady(SCCError))
	{
		OutError = SCCError;
		return false;
	}

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	SourceControlHelpers::RevertUnchangedFiles(SourceControlProvider, CheckedOutFiles);

	// remove unchanged files
	for (int32 VerifyIndex = CheckedOutFiles.Num() - 1; VerifyIndex >= 0; --VerifyIndex)
	{
		FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(CheckedOutFiles[VerifyIndex], EStateCacheUsage::ForceUpdate);
		if (SourceControlState.IsValid() && !SourceControlState->IsCheckedOut() && !SourceControlState->IsAdded())
		{
			CheckedOutFiles.RemoveAt(VerifyIndex);
		}
	}

	if (CheckedOutFiles.Num() > 0)
	{
		TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOperation = ISourceControlOperation::Create<FCheckIn>();
		CheckInOperation->SetDescription(InChangeDescription);
		if (!SourceControlProvider.Execute(CheckInOperation, CheckedOutFiles))
		{
			OutError = LOCTEXT("FailedToCheckInFiles", "The checked out localization files could not be checked in.");
			return false;
		}
		CheckedOutFiles.Empty();
	}
	return true;
}

bool FLocalizationSCC::CleanUp(FText& OutError)
{
	if (CheckedOutFiles.Num() == 0)
	{
		return true;
	}

	bool bCleanupSuccess = true;
	FString AccumulatedErrorsStr;
	const int32 FileCount = CheckedOutFiles.Num();
	int32 FileIdx = 0;
	for (int32 i = 0; i < FileCount; ++i)
	{
		FText ErrorText;
		bool bReverted = RevertFile(CheckedOutFiles[FileIdx], ErrorText);
		bCleanupSuccess &= bReverted;

		if (!bReverted)
		{
			if (!AccumulatedErrorsStr.IsEmpty())
			{
				AccumulatedErrorsStr += TEXT(", ");
			}
			AccumulatedErrorsStr += FString::Printf(TEXT("%s : %s"), *CheckedOutFiles[FileIdx], *ErrorText.ToString());
			++FileIdx;
		}
	}

	if (!bCleanupSuccess)
	{
		OutError = FText::Format(LOCTEXT("CouldNotCompleteSourceControlCleanup", "Could not complete Source Control cleanup.  {FailureReason}"), FText::FromString(AccumulatedErrorsStr));
	}

	return bCleanupSuccess;
}

bool FLocalizationSCC::IsReady(FText& OutError)
{
	if (!ISourceControlModule::Get().IsEnabled())
	{
		OutError = LOCTEXT("SourceControlNotEnabled", "Source control is not enabled.");
		return false;
	}

	if (!ISourceControlModule::Get().GetProvider().IsAvailable())
	{
		OutError = LOCTEXT("SourceControlNotAvailable", "Source control server is currently not available.");
		return false;
	}
	return true;
}

bool FLocalizationSCC::RevertFile(const FString& InFile, FText& OutError)
{
	if (InFile.IsEmpty() || InFile.StartsWith(TEXT("\\\\")))
	{
		OutError = LOCTEXT("CouldNotRevertFile", "Could not revert file.");
		return false;
	}

	FText SCCError;
	if (!IsReady(SCCError))
	{
		OutError = SCCError;
		return false;
	}

	FString AbsoluteFilename = FPaths::ConvertRelativePathToFull(InFile);
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(AbsoluteFilename, EStateCacheUsage::ForceUpdate);

	bool bSuccessfullyReverted = false;

	TArray<FString> FilesToRevert;
	FilesToRevert.Add(AbsoluteFilename);

	if (SourceControlState.IsValid() && !SourceControlState->IsCheckedOut() && !SourceControlState->IsAdded())
	{
		bSuccessfullyReverted = true;
	}
	else
	{
		bSuccessfullyReverted = (SourceControlProvider.Execute(ISourceControlOperation::Create<FRevert>(), FilesToRevert) == ECommandResult::Succeeded);
	}

	if (!bSuccessfullyReverted)
	{
		OutError = LOCTEXT("CouldNotRevertFile", "Could not revert file.");
	}
	else
	{
		CheckedOutFiles.Remove(AbsoluteFilename);
	}
	return bSuccessfullyReverted;
}

void FLocFileSCCNotifies::PreFileWrite(const FString& InFilename)
{
	if (SourceControlInfo.IsValid() && FPaths::FileExists(InFilename))
	{
		// File already exists, so check it out before writing to it
		FText ErrorMsg;
		if (!SourceControlInfo->CheckOutFile(InFilename, ErrorMsg))
		{
			UE_LOG(LogLocalizationSourceControl, Error, TEXT("Failed to check out file '%s'. %s"), *InFilename, *ErrorMsg.ToString());
		}
	}
}

void FLocFileSCCNotifies::PostFileWrite(const FString& InFilename)
{
	if (SourceControlInfo.IsValid())
	{
		// If the file didn't exist before then this will add it, otherwise it will do nothing
		FText ErrorMsg;
		if (!SourceControlInfo->CheckOutFile(InFilename, ErrorMsg))
		{
			UE_LOG(LogLocalizationSourceControl, Error, TEXT("Failed to check out file '%s'. %s"), *InFilename, *ErrorMsg.ToString());
		}
	}
}

#undef LOCTEXT_NAMESPACE
