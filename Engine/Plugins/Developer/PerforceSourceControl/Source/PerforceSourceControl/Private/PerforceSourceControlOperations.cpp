// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PerforceSourceControlOperations.h"
#include "PerforceSourceControlPrivate.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "SourceControlOperations.h"
#include "PerforceSourceControlRevision.h"
#include "PerforceSourceControlCommand.h"
#include "PerforceConnection.h"
#include "PerforceSourceControlModule.h"
#include "SPerforceSourceControlSettings.h"

#define LOCTEXT_NAMESPACE "PerforceSourceControl"

/**
 * Helper struct for RemoveRedundantErrors()
 */
struct FRemoveRedundantErrors
{
	FRemoveRedundantErrors(const FString& InFilter)
		: Filter(InFilter)
	{
	}

	bool operator()(const FText& Text) const
	{
		if(Text.ToString().Contains(Filter))
		{
			return true;
		}

		return false;
	}

	/** The filter string we try to identify in the reported error */
	FString Filter;
};

/** 
 * Remove redundant errors (that contain a particular string) and also
 * update the commands success status if all errors were removed.
 */
static void RemoveRedundantErrors(FPerforceSourceControlCommand& InCommand, const FString& InFilter)
{
	bool bFoundRedundantError = false;
	for(auto Iter(InCommand.ErrorMessages.CreateConstIterator()); Iter; Iter++)
	{
		// Perforce reports files that are already synced as errors, so copy any errors
		// we get to the info list in this case
		if(Iter->ToString().Contains(InFilter))
		{
			InCommand.InfoMessages.Add(*Iter);
			bFoundRedundantError = true;
		}
	}

	InCommand.ErrorMessages.RemoveAll( FRemoveRedundantErrors(InFilter) );

	// if we have no error messages now, assume success!
	if(bFoundRedundantError && InCommand.ErrorMessages.Num() == 0 && !InCommand.bCommandSuccessful)
	{
		InCommand.bCommandSuccessful = true;
	}
}

/** Simple parsing of a record set into strings, one string per record */
static void ParseRecordSet(const FP4RecordSet& InRecords, TArray<FText>& OutResults)
{
	const FString Delimiter = FString(TEXT(" "));

	for (int32 RecordIndex = 0; RecordIndex < InRecords.Num(); ++RecordIndex)
	{
		const FP4Record& ClientRecord = InRecords[RecordIndex];
		for(FP4Record::TConstIterator It = ClientRecord.CreateConstIterator(); It; ++It)
		{
			OutResults.Add(FText::FromString(It.Key() + Delimiter + It.Value()));
		}
	}
}

/** Simple parsing of a record set to update state */
static void ParseRecordSetForState(const FP4RecordSet& InRecords, TMap<FString, EPerforceState::Type>& OutResults)
{
	// Iterate over each record found as a result of the command, parsing it for relevant information
	for (int32 Index = 0; Index < InRecords.Num(); ++Index)
	{
		const FP4Record& ClientRecord = InRecords[Index];
		FString FileName = ClientRecord(TEXT("clientFile"));
		FString Action = ClientRecord(TEXT("action"));

		check(FileName.Len());
		FString FullPath(FileName);
		FPaths::NormalizeFilename(FullPath);

		if(Action.Len() > 0)
		{
			if(Action == TEXT("add"))
			{
				OutResults.Add(FullPath, EPerforceState::OpenForAdd);
			}
			else if(Action == TEXT("edit"))
			{
				OutResults.Add(FullPath, EPerforceState::CheckedOut);
			}
			else if(Action == TEXT("delete"))
			{
				OutResults.Add(FullPath, EPerforceState::MarkedForDelete);
			}
			else if(Action == TEXT("abandoned"))
			{
				OutResults.Add(FullPath, EPerforceState::NotInDepot);
			}
			else if(Action == TEXT("reverted"))
			{
				FString OldAction = ClientRecord(TEXT("oldAction"));
				if(OldAction == TEXT("add"))
				{
					OutResults.Add(FullPath, EPerforceState::NotInDepot);
				}
				else if(OldAction == TEXT("edit"))
				{
					OutResults.Add(FullPath, EPerforceState::ReadOnly);
				}
				else if(OldAction == TEXT("delete"))
				{
					OutResults.Add(FullPath, EPerforceState::ReadOnly);
				}
			}
			else if(Action == TEXT("branch"))
			{
				OutResults.Add(FullPath, EPerforceState::Branched);
			}
		}
	}
}

static bool UpdateCachedStates(const TMap<FString, EPerforceState::Type>& InResults)
{
	FPerforceSourceControlModule& PerforceSourceControl = FPerforceSourceControlModule::Get();
	for(TMap<FString, EPerforceState::Type>::TConstIterator It(InResults); It; ++It)
	{
		TSharedRef<FPerforceSourceControlState, ESPMode::ThreadSafe> State = PerforceSourceControl.GetProvider().GetStateInternal(It.Key());
		State->SetState(It.Value());
		State->TimeStamp = FDateTime::Now();
	}

	return InResults.Num() > 0;
}

static bool CheckWorkspaceRecordSet(const FP4RecordSet& InRecords, TArray<FText>& OutErrorMessages, FText& OutNotificationText)
{
	FString ApplicationPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::ProjectDir()).ToLower();
	ApplicationPath = ApplicationPath.Replace(TEXT("\\"), TEXT("/"));

	for(const auto& Record : InRecords)
	{
		FString Root = Record(TEXT("Root"));	
		
		// A workspace root could be "null" which allows the user to map depot locations to different drives.
		// Allow these workspaces since we already allow workspaces mapped to drive letters.
		const bool bIsNullClientRootPath = (Root == TEXT("null"));
			
		// Sanitize root name
		Root = Root.Replace(TEXT("\\"), TEXT("/"));
		if (!Root.EndsWith(TEXT("/")))
		{
			Root += TEXT("/");
		}
			
		if (bIsNullClientRootPath || ApplicationPath.Contains(Root))
		{
			return true;
		}
		else
		{
			const FString Client = Record(TEXT("Client"));	
			OutNotificationText = FText::Format(LOCTEXT("WorkspaceError", "Workspace '{0}' does not map into this project's directory."), FText::FromString(Client));
			OutErrorMessages.Add(OutNotificationText);
			OutErrorMessages.Add(LOCTEXT("WorkspaceHelp", "You should set your workspace up to map to a directory at or above the project's directory."));
		}
	}

	return false;
}

static void AppendChangelistParameter(TArray<FString>& InOutParams)
{
	FPerforceSourceControlModule& PerforceSourceControl = FModuleManager::GetModuleChecked<FPerforceSourceControlModule>("PerforceSourceControl");
	FPerforceSourceControlSettings& Settings = PerforceSourceControl.AccessSettings();

	const FString& ChangelistNumber = Settings.GetChangelistNumber();
	if ( !ChangelistNumber.IsEmpty() )
	{
		InOutParams.Add(TEXT("-c"));
		InOutParams.Add(ChangelistNumber);
	}
}

FName FPerforceConnectWorker::GetName() const
{
	return "Connect";
}

bool FPerforceConnectWorker::Execute(FPerforceSourceControlCommand& InCommand)
{
	FScopedPerforceConnection ScopedConnection(InCommand);
	if (!InCommand.IsCanceled() && ScopedConnection.IsValid())
	{
		FPerforceConnection& Connection = ScopedConnection.GetConnection();
		TArray<FString> Parameters;
		FP4RecordSet Records;
		Parameters.Add(TEXT("-o"));
		Parameters.Add(InCommand.ConnectionInfo.Workspace);
		InCommand.bCommandSuccessful = Connection.RunCommand(TEXT("client"), Parameters, Records, InCommand.ErrorMessages, FOnIsCancelled::CreateRaw(&InCommand, &FPerforceSourceControlCommand::IsCanceled), InCommand.bConnectionDropped);
		
		// If there are error messages, user name is most likely invalid. Otherwise, make sure workspace actually
		// exists on server by checking if we have it's update date.
		InCommand.bCommandSuccessful &= InCommand.ErrorMessages.Num() == 0 && Records.Num() > 0 && Records[0].Contains(TEXT("Update"));
		if (!InCommand.bCommandSuccessful && InCommand.ErrorMessages.Num() == 0)
		{
			InCommand.ErrorMessages.Add(LOCTEXT("InvalidWorkspace", "Invalid workspace."));
		}

		// check if we can actually work with this workspace
		if(InCommand.bCommandSuccessful)
		{
			FText Notification;
			InCommand.bCommandSuccessful = CheckWorkspaceRecordSet(Records, InCommand.ErrorMessages, Notification);
			if(!InCommand.bCommandSuccessful)
			{
				check(InCommand.Operation->GetName() == GetName());
				TSharedRef<FConnect, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FConnect>(InCommand.Operation);
				Operation->SetErrorText(Notification);
			}
		}

		if(InCommand.bCommandSuccessful)
		{
			ParseRecordSet(Records, InCommand.InfoMessages);
		}
	}
	return InCommand.bCommandSuccessful;
}

bool FPerforceConnectWorker::UpdateStates() const
{
	return true;
}

FName FPerforceCheckOutWorker::GetName() const
{
	return "CheckOut";
}

bool FPerforceCheckOutWorker::Execute(FPerforceSourceControlCommand& InCommand)
{	
	FScopedPerforceConnection ScopedConnection(InCommand);
	if (!InCommand.IsCanceled() && ScopedConnection.IsValid())
	{
		FPerforceConnection& Connection = ScopedConnection.GetConnection();
		TArray<FString> Parameters;

		AppendChangelistParameter(Parameters);

		Parameters.Append(InCommand.Files);
		FP4RecordSet Records;
		InCommand.bCommandSuccessful = Connection.RunCommand(TEXT("edit"), Parameters, Records, InCommand.ErrorMessages, FOnIsCancelled::CreateRaw(&InCommand, &FPerforceSourceControlCommand::IsCanceled), InCommand.bConnectionDropped);
		ParseRecordSetForState(Records, OutResults);
	}

	return InCommand.bCommandSuccessful;
}

bool FPerforceCheckOutWorker::UpdateStates() const
{
	return UpdateCachedStates(OutResults);
}

FName FPerforceCheckInWorker::GetName() const
{
	return "CheckIn";
}

static FText ParseSubmitResults(const FP4RecordSet& InRecords)
{
	// Iterate over each record found as a result of the command, parsing it for relevant information
	for (int32 Index = 0; Index < InRecords.Num(); ++Index)
	{
		const FP4Record& ClientRecord = InRecords[Index];
		const FString SubmittedChange = ClientRecord(TEXT("submittedChange"));
		if(SubmittedChange.Len() > 0)
		{
			return FText::Format(LOCTEXT("SubmitMessage", "Submitted changelist {0}"), FText::FromString(SubmittedChange));
		}
	}

	return LOCTEXT("SubmitMessageUnknown", "Submitted changelist");
}

bool FPerforceCheckInWorker::Execute(FPerforceSourceControlCommand& InCommand)
{
	FScopedPerforceConnection ScopedConnection(InCommand);
	if (!InCommand.IsCanceled() && ScopedConnection.IsValid())
	{	
		FPerforceConnection& Connection = ScopedConnection.GetConnection();

		check(InCommand.Operation->GetName() == GetName());
		TSharedRef<FCheckIn, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FCheckIn>(InCommand.Operation);

		int32 ChangeList = Connection.CreatePendingChangelist(Operation->GetDescription(), FOnIsCancelled::CreateRaw(&InCommand, &FPerforceSourceControlCommand::IsCanceled), InCommand.ErrorMessages);
		if (ChangeList > 0)
		{
			// Batch reopen into multiple commands, to avoid command line limits
			const int32 BatchedCount = 100;
			InCommand.bCommandSuccessful = true;
			for (int32 StartingIndex = 0; StartingIndex < InCommand.Files.Num() && InCommand.bCommandSuccessful; StartingIndex += BatchedCount)
			{
				FP4RecordSet Records;
				TArray< FString > ReopenParams;
						
				//Add changelist information to params
				ReopenParams.Insert(TEXT("-c"), 0);
				ReopenParams.Insert(FString::Printf(TEXT("%d"), ChangeList), 1);
				int32 NextIndex = FMath::Min(StartingIndex + BatchedCount, InCommand.Files.Num());

				for (int32 FileIndex = StartingIndex; FileIndex < NextIndex; FileIndex++)
				{
					ReopenParams.Add(InCommand.Files[FileIndex]);
				}

				InCommand.bCommandSuccessful = Connection.RunCommand(TEXT("reopen"), ReopenParams, Records, InCommand.ErrorMessages, FOnIsCancelled::CreateRaw(&InCommand, &FPerforceSourceControlCommand::IsCanceled), InCommand.bConnectionDropped);
			}

			if (InCommand.bCommandSuccessful)
			{
				// Only submit if reopen was successful
				TArray<FString> SubmitParams;
				FP4RecordSet Records;

				SubmitParams.Insert(TEXT("-c"), 0);
				SubmitParams.Insert(FString::Printf(TEXT("%d"), ChangeList), 1);

				InCommand.bCommandSuccessful = Connection.RunCommand(TEXT("submit"), SubmitParams, Records, InCommand.ErrorMessages, FOnIsCancelled::CreateRaw(&InCommand, &FPerforceSourceControlCommand::IsCanceled), InCommand.bConnectionDropped);

				if (InCommand.ErrorMessages.Num() > 0)
				{
					InCommand.bCommandSuccessful = false;
				}

				if (InCommand.bCommandSuccessful)
				{
					// Remove any deleted files from status cache
					FPerforceSourceControlModule& PerforceSourceControl = FModuleManager::GetModuleChecked<FPerforceSourceControlModule>("PerforceSourceControl");
					FPerforceSourceControlProvider& Provider = PerforceSourceControl.GetProvider();

					TArray<TSharedRef<ISourceControlState, ESPMode::ThreadSafe>> States;
					Provider.GetState(InCommand.Files, States, EStateCacheUsage::Use);
					for (const auto& State : States)
					{
						if (State->IsDeleted())
						{
							Provider.RemoveFileFromCache(State->GetFilename());
						}
					}

					StaticCastSharedRef<FCheckIn>(InCommand.Operation)->SetSuccessMessage(ParseSubmitResults(Records));

					for(auto Iter(InCommand.Files.CreateIterator()); Iter; Iter++)
					{
						OutResults.Add(*Iter, EPerforceState::ReadOnly);
					}
				}
			}
		}
		else
		{
			// Failed to create the changelist
			InCommand.bCommandSuccessful = false;
		}
	}

	return InCommand.bCommandSuccessful;
}

bool FPerforceCheckInWorker::UpdateStates() const
{
	return UpdateCachedStates(OutResults);
}

FName FPerforceMarkForAddWorker::GetName() const
{
	return "MarkForAdd";
}

bool FPerforceMarkForAddWorker::Execute(FPerforceSourceControlCommand& InCommand)
{
	// Perforce will allow you to mark files for add that don't currently exist on disk
	// This goes against the workflow of our other SCC providers (such as SVN and Git), 
	// so we manually check that the files exist before allowing this command to continue
	// This keeps the behavior consistent between SCC providers
	bool bHasMissingFiles = false;
	for(const FString& FileToAdd : InCommand.Files)
	{
		if(!IFileManager::Get().FileExists(*FileToAdd))
		{
			bHasMissingFiles = true;
			InCommand.ErrorMessages.Add(FText::Format(LOCTEXT("Error_FailedToMarkFileForAdd_FileMissing", "Failed mark the file '{0}' for add. The file doesn't exist on disk."), FText::FromString(FileToAdd)));
		}
	}
	if(bHasMissingFiles)
	{
		InCommand.bCommandSuccessful = false;
		return false;
	}

	FScopedPerforceConnection ScopedConnection(InCommand);
	if (!InCommand.IsCanceled() && ScopedConnection.IsValid())
	{
		FPerforceConnection& Connection = ScopedConnection.GetConnection();
		TArray<FString> Parameters;
		FP4RecordSet Records;

		AppendChangelistParameter(Parameters);
		Parameters.Append(InCommand.Files);

		InCommand.bCommandSuccessful = Connection.RunCommand(TEXT("add"), Parameters, Records, InCommand.ErrorMessages, FOnIsCancelled::CreateRaw(&InCommand, &FPerforceSourceControlCommand::IsCanceled), InCommand.bConnectionDropped);
		ParseRecordSetForState(Records, OutResults);
	}
	return InCommand.bCommandSuccessful;
}

bool FPerforceMarkForAddWorker::UpdateStates() const
{
	return UpdateCachedStates(OutResults);
}

FName FPerforceDeleteWorker::GetName() const
{
	return "Delete";
}

bool FPerforceDeleteWorker::Execute(FPerforceSourceControlCommand& InCommand)
{
	FScopedPerforceConnection ScopedConnection(InCommand);
	if (!InCommand.IsCanceled() && ScopedConnection.IsValid())
	{
		FPerforceConnection& Connection = ScopedConnection.GetConnection();
		TArray<FString> Parameters;

		AppendChangelistParameter(Parameters);
		Parameters.Append(InCommand.Files);

		FP4RecordSet Records;
		InCommand.bCommandSuccessful = Connection.RunCommand(TEXT("delete"), Parameters, Records, InCommand.ErrorMessages, FOnIsCancelled::CreateRaw(&InCommand, &FPerforceSourceControlCommand::IsCanceled), InCommand.bConnectionDropped);
		ParseRecordSetForState(Records, OutResults);
	}
	return InCommand.bCommandSuccessful;
}

bool FPerforceDeleteWorker::UpdateStates() const
{
	return UpdateCachedStates(OutResults);
}

FName FPerforceRevertWorker::GetName() const
{
	return "Revert";
}

bool FPerforceRevertWorker::Execute(FPerforceSourceControlCommand& InCommand)
{
	FScopedPerforceConnection ScopedConnection(InCommand);
	if (!InCommand.IsCanceled() && ScopedConnection.IsValid())
	{
		FPerforceConnection& Connection = ScopedConnection.GetConnection();
		TArray<FString> Parameters;

		AppendChangelistParameter(Parameters);
		Parameters.Append(InCommand.Files);

		FP4RecordSet Records;
		InCommand.bCommandSuccessful = Connection.RunCommand(TEXT("revert"), Parameters, Records, InCommand.ErrorMessages, FOnIsCancelled::CreateRaw(&InCommand, &FPerforceSourceControlCommand::IsCanceled), InCommand.bConnectionDropped);
		ParseRecordSetForState(Records, OutResults);
	}
	return InCommand.bCommandSuccessful;
}

bool FPerforceRevertWorker::UpdateStates() const
{
	return UpdateCachedStates(OutResults);
}

FName FPerforceSyncWorker::GetName() const
{
	return "Sync";
}

static void ParseSyncResults(const FP4RecordSet& InRecords, TMap<FString, EPerforceState::Type>& OutResults)
{
	// Iterate over each record found as a result of the command, parsing it for relevant information
	for (int32 Index = 0; Index < InRecords.Num(); ++Index)
	{
		const FP4Record& ClientRecord = InRecords[Index];
		FString FileName = ClientRecord(TEXT("clientFile"));
		FString Action = ClientRecord(TEXT("action"));

		check(FileName.Len());
		FString FullPath(FileName);
		FPaths::NormalizeFilename(FullPath);

		if(Action.Len() > 0)
		{
			if(Action == TEXT("updated"))
			{
				OutResults.Add(FullPath, EPerforceState::ReadOnly);
			}
		}
	}
}

bool FPerforceSyncWorker::Execute(FPerforceSourceControlCommand& InCommand)
{
	FScopedPerforceConnection ScopedConnection(InCommand);
	if (!InCommand.IsCanceled() && ScopedConnection.IsValid())
	{
		FPerforceConnection& Connection = ScopedConnection.GetConnection();
		TArray<FString> Parameters;
		Parameters.Append(InCommand.Files);

		// check for directories and add '...'
		for(auto& FileName : Parameters)
		{
			if(FileName.EndsWith(TEXT("/")))
			{
				FileName += TEXT("...");
			}
		}

		FP4RecordSet Records;
		InCommand.bCommandSuccessful = Connection.RunCommand(TEXT("sync"), Parameters, Records, InCommand.ErrorMessages, FOnIsCancelled::CreateRaw(&InCommand, &FPerforceSourceControlCommand::IsCanceled), InCommand.bConnectionDropped);
		ParseSyncResults(Records, OutResults);

		RemoveRedundantErrors(InCommand, TEXT("file(s) up-to-date"));
	}
	return InCommand.bCommandSuccessful;
}

bool FPerforceSyncWorker::UpdateStates() const
{
	return UpdateCachedStates(OutResults);
}

static void ParseUpdateStatusResults(const FP4RecordSet& InRecords, const TArray<FText>& ErrorMessages, TArray<FPerforceSourceControlState>& OutStates)
{
	// Iterate over each record found as a result of the command, parsing it for relevant information
	for (int32 Index = 0; Index < InRecords.Num(); ++Index)
	{
		const FP4Record& ClientRecord = InRecords[Index];
		FString FileName = ClientRecord(TEXT("clientFile"));
		FString DepotFileName = ClientRecord(TEXT("depotFile"));
		FString HeadRev  = ClientRecord(TEXT("headRev"));
		FString HaveRev  = ClientRecord(TEXT("haveRev"));
		FString OtherOpen = ClientRecord(TEXT("otherOpen"));
		FString OpenType = ClientRecord(TEXT("type"));
		FString HeadAction = ClientRecord(TEXT("headAction"));
		FString Action = ClientRecord(TEXT("action"));
		FString HeadType = ClientRecord(TEXT("headType"));
		const bool bUnresolved = ClientRecord.Contains(TEXT("unresolved"));

		FString FullPath(FileName);
		FPaths::NormalizeFilename(FullPath);
		OutStates.Add(FPerforceSourceControlState(FullPath));
		FPerforceSourceControlState& State = OutStates.Last();
		State.DepotFilename = DepotFileName;

		State.State = EPerforceState::ReadOnly;
		if (Action.Len() > 0 && Action == TEXT("add")) 
		{
			State.State = EPerforceState::OpenForAdd;
		}
		else if (Action.Len() > 0 && Action == TEXT("delete")) 
		{
			State.State = EPerforceState::MarkedForDelete;
		}
		else if (OpenType.Len() > 0)
		{
			if(Action.Len() > 0 && Action == TEXT("branch")) 
			{
				State.State = EPerforceState::Branched;
			}
			else
			{
				State.State = EPerforceState::CheckedOut;
			}
		}
		else if (OtherOpen.Len() > 0)
		{
			// OtherOpen just reports the number of developers that have a file open, now add a string for every entry
			int32 OtherOpenNum = FCString::Atoi(*OtherOpen);
			for ( int32 OpenIdx = 0; OpenIdx < OtherOpenNum; ++OpenIdx )
			{
				const FString OtherOpenRecordKey = FString::Printf(TEXT("otherOpen%d"), OpenIdx);
				const FString OtherOpenRecordValue = ClientRecord(OtherOpenRecordKey);

				State.OtherUserCheckedOut += OtherOpenRecordValue;
				if(OpenIdx < OtherOpenNum - 1)
				{
					State.OtherUserCheckedOut += TEXT(", ");
				}
			}

			State.State = EPerforceState::CheckedOutOther;
		}
		//file has been previously deleted, ok to add again
		else if (HeadAction.Len() > 0 && HeadAction == TEXT("delete")) 
		{
			State.State = EPerforceState::NotInDepot;
		}
		
		if (HeadRev.Len() > 0 && HaveRev.Len() > 0)
		{
			TTypeFromString<int>::FromString(State.DepotRevNumber, *HeadRev);
			TTypeFromString<int>::FromString(State.LocalRevNumber, *HaveRev);
			if( bUnresolved )
			{
				int32 ResolveActionNumber = 0;
				for (;;)
				{
					// Extract the revision number
					FString VarName = FString::Printf(TEXT("resolveAction%d"), ResolveActionNumber);
					if (!ClientRecord.Contains(*VarName))
					{
						// No more revisions
						ensureMsgf( ResolveActionNumber > 0, TEXT("Resolve is pending but no resolve actions for file %s"), *FileName );
						break;
					}

					VarName = FString::Printf(TEXT("resolveBaseFile%d"), ResolveActionNumber);
					FString ResolveBaseFile = ClientRecord(VarName);
					VarName = FString::Printf(TEXT("resolveFromFile%d"), ResolveActionNumber);
					FString ResolveFromFile = ClientRecord(VarName);
					if(!ensureMsgf( ResolveFromFile == ResolveBaseFile, TEXT("Text cannot resolve %s with %s, we do not support cross file merging"), *ResolveBaseFile, *ResolveFromFile ) )
					{
						break;
					}

					VarName = FString::Printf(TEXT("resolveBaseRev%d"), ResolveActionNumber);
					FString ResolveBaseRev = ClientRecord(VarName);

					TTypeFromString<int>::FromString(State.PendingResolveRevNumber, *ResolveBaseRev);
					
					++ResolveActionNumber;
				}
			}
		}

		// Check binary status
		State.bBinary = false;
		if (HeadType.Len() > 0 && HeadType.Contains(TEXT("binary")))
		{
			State.bBinary = true;
		}

		// Check exclusive checkout flag
		State.bExclusiveCheckout = false;
		if (HeadType.Len() > 0 && HeadType.Contains(TEXT("+l")))
		{
			State.bExclusiveCheckout = true;
		}
	}

	// also see if we can glean anything from the error messages
	for (int32 Index = 0; Index < ErrorMessages.Num(); ++Index)
	{
		const FText& Error = ErrorMessages[Index];

		//@todo P4 could be returning localized error messages
		int32 NoSuchFilePos = Error.ToString().Find(TEXT(" - no such file(s).\n"), ESearchCase::IgnoreCase, ESearchDir::FromStart);
		if(NoSuchFilePos != INDEX_NONE)
		{
			// found an error about a file that is not in the depot
			FString FullPath(Error.ToString().Left(NoSuchFilePos));
			FPaths::NormalizeFilename(FullPath);
			OutStates.Add(FPerforceSourceControlState(FullPath));
			FPerforceSourceControlState& State = OutStates.Last();
			State.State = EPerforceState::NotInDepot;
		}

		//@todo P4 could be returning localized error messages
		int32 NotUnderClientRootPos = Error.ToString().Find(TEXT("' is not under client's root"), ESearchCase::IgnoreCase, ESearchDir::FromStart);
		if(NotUnderClientRootPos != INDEX_NONE)
		{
			// found an error about a file that is not under the client root
			static const FString Prefix(TEXT("Path \'"));
			FString FullPath(Error.ToString().Mid(Prefix.Len(), NotUnderClientRootPos - Prefix.Len()));
			FPaths::NormalizeFilename(FullPath);
			OutStates.Add(FPerforceSourceControlState(FullPath));
			FPerforceSourceControlState& State = OutStates.Last();
			State.State = EPerforceState::NotUnderClientRoot;	
		}
	}
}

static void ParseOpenedResults(const FP4RecordSet& InRecords, const FString& ClientName, const FString& ClientRoot, TMap<FString, EPerforceState::Type>& OutResults)
{
	// Iterate over each record found as a result of the command, parsing it for relevant information
	for (int32 Index = 0; Index < InRecords.Num(); ++Index)
	{
		const FP4Record& ClientRecord = InRecords[Index];
		FString ClientFileName = ClientRecord(TEXT("clientFile"));
		FString Action = ClientRecord(TEXT("action"));

		check(ClientFileName.Len() > 0);

		// Convert the depot file name to a local file name
		FString FullPath = ClientFileName;
		const FString PathRoot = FString::Printf(TEXT("//%s"), *ClientName);

		if (FullPath.StartsWith(PathRoot))
		{
			const bool bIsNullClientRootPath = (ClientRoot == TEXT("null"));
			if ( bIsNullClientRootPath )
			{
				// Null clients use the pattern in PathRoot: //Workspace/FileName
				// Here we chop off the '//Workspace/' to return the workspace filename
				FullPath = FullPath.RightChop(PathRoot.Len() + 1);
			}
			else
			{
				// This is a normal workspace where we can simply replace the pathroot with the client root to form the filename
				FullPath = FullPath.Replace(*PathRoot, *ClientRoot);
			}
		}
		else
		{
			// This file is not in the workspace, ignore it
			continue;
		}

		if (Action.Len() > 0)
		{
			if(Action == TEXT("add"))
			{
				OutResults.Add(FullPath, EPerforceState::OpenForAdd);
			}
			else if(Action == TEXT("edit"))
			{
				OutResults.Add(FullPath, EPerforceState::CheckedOut);
			}
			else if(Action == TEXT("delete"))
			{
				OutResults.Add(FullPath, EPerforceState::MarkedForDelete);
			}
		}
	}
}

static const FString& FindWorkspaceFile(const TArray<FPerforceSourceControlState>& InStates, const FString& InDepotFile)
{
	for(auto It(InStates.CreateConstIterator()); It; It++)
	{
		if(It->DepotFilename == InDepotFile)
		{
			return It->LocalFilename;
		}
	}

	return InDepotFile;
}

static void ParseHistoryResults(const FP4RecordSet& InRecords, const TArray<FPerforceSourceControlState>& InStates, FPerforceUpdateStatusWorker::FHistoryMap& OutHistory)
{
	if (InRecords.Num() > 0)
	{
		// Iterate over each record, extracting the relevant information for each
		for (int32 RecordIndex = 0; RecordIndex < InRecords.Num(); ++RecordIndex)
		{
			const FP4Record& ClientRecord = InRecords[RecordIndex];

			// Extract the file name 
			check(ClientRecord.Contains(TEXT("depotFile")));
			FString DepotFileName = ClientRecord(TEXT("depotFile"));
			FString LocalFileName = FindWorkspaceFile(InStates, DepotFileName);

			TArray< TSharedRef<FPerforceSourceControlRevision, ESPMode::ThreadSafe> > Revisions;
			int32 RevisionNumbers = 0;
			for (;;)
			{
				// Extract the revision number
				FString VarName = FString::Printf(TEXT("rev%d"), RevisionNumbers);
				if (!ClientRecord.Contains(*VarName))
				{
					// No more revisions
					break;
				}
				FString RevisionNumber = ClientRecord(*VarName);

				// Extract the user name
				VarName = FString::Printf(TEXT("user%d"), RevisionNumbers);
				check(ClientRecord.Contains(*VarName));
				FString UserName = ClientRecord(*VarName);

				// Extract the date
				VarName = FString::Printf(TEXT("time%d"), RevisionNumbers);
				check(ClientRecord.Contains(*VarName));
				FString Date = ClientRecord(*VarName);

				// Extract the changelist number
				VarName = FString::Printf(TEXT("change%d"), RevisionNumbers);
				check(ClientRecord.Contains(*VarName));
				FString ChangelistNumber = ClientRecord(*VarName);

				// Extract the description
				VarName = FString::Printf(TEXT("desc%d"), RevisionNumbers);
				check(ClientRecord.Contains(*VarName));
				FString Description = ClientRecord(*VarName);

				// Extract the action
				VarName = FString::Printf(TEXT("action%d"), RevisionNumbers);
				check(ClientRecord.Contains(*VarName));
				FString Action = ClientRecord(*VarName);

				FString FileSize(TEXT("0"));

				// Extract the file size
				if(Action.ToLower() != TEXT("delete") && Action.ToLower() != TEXT("move/delete")) //delete actions don't have a fileSize from PV4
				{
					VarName = FString::Printf(TEXT("fileSize%d"), RevisionNumbers);
					check(ClientRecord.Contains(*VarName));
					FileSize = ClientRecord(*VarName);
				}
		
				// Extract the clientspec/workspace
				VarName = FString::Printf(TEXT("client%d"), RevisionNumbers);
				check(ClientRecord.Contains(*VarName));
				FString ClientSpec = ClientRecord(*VarName);

				// check for branch
				TSharedPtr<FPerforceSourceControlRevision, ESPMode::ThreadSafe> BranchSource;
				VarName = FString::Printf(TEXT("how%d,0"), RevisionNumbers);
				if(ClientRecord.Contains(*VarName))
				{
					BranchSource = MakeShareable( new FPerforceSourceControlRevision() );

					VarName = FString::Printf(TEXT("file%d,0"), RevisionNumbers);
					FString BranchSourceFileName = ClientRecord(*VarName);
					BranchSource->FileName = FindWorkspaceFile(InStates, BranchSourceFileName);

					VarName = FString::Printf(TEXT("erev%d,0"), RevisionNumbers);
					FString BranchSourceRevision = ClientRecord(*VarName);
					BranchSource->RevisionNumber = FCString::Atoi(*BranchSourceRevision);
				}

				TSharedRef<FPerforceSourceControlRevision, ESPMode::ThreadSafe> Revision = MakeShareable( new FPerforceSourceControlRevision() );
				Revision->FileName = LocalFileName;
				Revision->RevisionNumber = FCString::Atoi(*RevisionNumber);
				Revision->Revision = RevisionNumber;
				Revision->ChangelistNumber = FCString::Atoi(*ChangelistNumber);
				Revision->Description = Description;
				Revision->UserName = UserName;
				Revision->ClientSpec = ClientSpec;
				Revision->Action = Action;
				Revision->BranchSource = BranchSource;
				Revision->Date = FDateTime(1970, 1, 1, 0, 0, 0, 0) + FTimespan::FromSeconds(FCString::Atoi(*Date));
				Revision->FileSize = FCString::Atoi(*FileSize);

				Revisions.Add(Revision);

				RevisionNumbers++;
			}

			if(Revisions.Num() > 0)
			{
				OutHistory.Add(LocalFileName, Revisions);
			}
		}
	}
}

static void ParseDiffResults(const FP4RecordSet& InRecords, TArray<FString>& OutModifiedFiles)
{
	if (InRecords.Num() > 0)
	{
		// Iterate over each record found as a result of the command, parsing it for relevant information
		for (int32 Index = 0; Index < InRecords.Num(); ++Index)
		{
			const FP4Record& ClientRecord = InRecords[Index];
			FString FileName = ClientRecord(TEXT("clientFile"));
			FPaths::NormalizeFilename(FileName);
			OutModifiedFiles.Add(FileName);	
		}
	}
}


FName FPerforceUpdateStatusWorker::GetName() const
{
	return "UpdateStatus";
}

bool FPerforceUpdateStatusWorker::Execute(FPerforceSourceControlCommand& InCommand)
{
#if USE_P4_API
	FScopedPerforceConnection ScopedConnection(InCommand);
	if (!InCommand.IsCanceled() && ScopedConnection.IsValid())
	{
		FPerforceConnection& Connection = ScopedConnection.GetConnection();
		if(InCommand.Files.Num())
		{
			// See http://www.perforce.com/perforce/doc.current/manuals/cmdref/p4_fstat.html
			// for full reference info on fstat command parameters...

			TArray<FString> Parameters;

			// We want to include integration record information:
			Parameters.Add(TEXT("-Or"));

			// Mandatory parameters (the list of files to stat):
			for (FString& File : InCommand.Files)
			{
				if (IFileManager::Get().DirectoryExists(*File))
				{
					// If the file is a directory, do a recursive fstat on the contents
					File /= TEXT("...");
				}

				Parameters.Add(File);
			}

			FP4RecordSet Records;
			InCommand.bCommandSuccessful = Connection.RunCommand(TEXT("fstat"), Parameters, Records, InCommand.ErrorMessages, FOnIsCancelled::CreateRaw(&InCommand, &FPerforceSourceControlCommand::IsCanceled), InCommand.bConnectionDropped);
			ParseUpdateStatusResults(Records, InCommand.ErrorMessages, OutStates);
			RemoveRedundantErrors(InCommand, TEXT(" - no such file(s)."));
			RemoveRedundantErrors(InCommand, TEXT("' is not under client's root '"));
		}
		else
		{
			InCommand.bCommandSuccessful = true;
		}

		// update using any special hints passed in via the operation
		check(InCommand.Operation->GetName() == GetName());
		TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FUpdateStatus>(InCommand.Operation);

		if(Operation->ShouldUpdateHistory())
		{
			TArray<FString> Parameters;
			FP4RecordSet Records;
			// disregard non-contributory integrations
			Parameters.Add(TEXT("-s"));
			//include branching history
			Parameters.Add(TEXT("-i"));
			//include truncated change list descriptions
			Parameters.Add(TEXT("-L"));
			//include time stamps
			Parameters.Add(TEXT("-t"));
			//limit to last 100 changes
			Parameters.Add(TEXT("-m 100"));
			Parameters.Append(InCommand.Files);
			InCommand.bCommandSuccessful &= Connection.RunCommand(TEXT("filelog"), Parameters, Records, InCommand.ErrorMessages, FOnIsCancelled::CreateRaw(&InCommand, &FPerforceSourceControlCommand::IsCanceled), InCommand.bConnectionDropped);
			ParseHistoryResults(Records, OutStates, OutHistory);
			RemoveRedundantErrors(InCommand, TEXT(" - no such file(s)."));
			RemoveRedundantErrors(InCommand, TEXT(" - file(s) not on client"));
			RemoveRedundantErrors(InCommand, TEXT("' is not under client's root '"));
		}

		if(Operation->ShouldGetOpenedOnly())
		{
			const FString ContentFolder = FPaths::ConvertRelativePathToFull(FPaths::RootDir());
			const FString FileQuery = FString::Printf(TEXT("%s..."), *ContentFolder);
			TArray<FString> Parameters = InCommand.Files;
			Parameters.Add(FileQuery);
			FP4RecordSet Records;
			InCommand.bCommandSuccessful &= Connection.RunCommand(TEXT("opened"), Parameters, Records, InCommand.ErrorMessages, FOnIsCancelled::CreateRaw(&InCommand, &FPerforceSourceControlCommand::IsCanceled), InCommand.bConnectionDropped);
			ParseOpenedResults(Records, ANSI_TO_TCHAR(Connection.P4Client.GetClient().Text()), Connection.ClientRoot, OutStateMap);
			RemoveRedundantErrors(InCommand, TEXT(" - no such file(s)."));
			RemoveRedundantErrors(InCommand, TEXT("' is not under client's root '"));
		}

		if(Operation->ShouldUpdateModifiedState())
		{
			TArray<FString> Parameters;
			FP4RecordSet Records;
			// Query for open files different than the versions stored in Perforce
			Parameters.Add(TEXT("-sa"));
			Parameters.Append(InCommand.Files);
			InCommand.bCommandSuccessful &= Connection.RunCommand(TEXT("diff"), Parameters, Records, InCommand.ErrorMessages, FOnIsCancelled::CreateRaw(&InCommand, &FPerforceSourceControlCommand::IsCanceled), InCommand.bConnectionDropped);

			// Parse the results and store them in the command
			ParseDiffResults(Records, OutModifiedFiles);
			RemoveRedundantErrors(InCommand, TEXT(" - no such file(s)."));
			RemoveRedundantErrors(InCommand, TEXT(" - file(s) not opened for edit"));
			RemoveRedundantErrors(InCommand, TEXT("' is not under client's root '"));
		}
	}
#endif

	return InCommand.bCommandSuccessful;
}

bool FPerforceUpdateStatusWorker::UpdateStates() const
{
	bool bUpdated = false;

	FPerforceSourceControlModule& PerforceSourceControl = FPerforceSourceControlModule::Get();
	const FDateTime Now = FDateTime::Now();

	// first update cached state from 'fstat' call
	for(int StatusIndex = 0; StatusIndex < OutStates.Num(); StatusIndex++)
	{
		const FPerforceSourceControlState& Status = OutStates[StatusIndex];
		TSharedRef<FPerforceSourceControlState, ESPMode::ThreadSafe> State = PerforceSourceControl.GetProvider().GetStateInternal(Status.LocalFilename);
		// Update every member except History and Timestamp. History will be updated below from the OutHistory map.
		// Timestamp is used to throttle status requests, so update it to current time:
		auto History = MoveTemp(State->History);
		*State = Status; 
		State->History = MoveTemp(History);
		State->TimeStamp = Now;
		bUpdated = true;
	}

	// next update state from 'opened' call
	bUpdated |= UpdateCachedStates(OutStateMap);

	// add history, if any
	for(FHistoryMap::TConstIterator It(OutHistory); It; ++It)
	{
		TSharedRef<FPerforceSourceControlState, ESPMode::ThreadSafe> State = PerforceSourceControl.GetProvider().GetStateInternal(It.Key());
		const TArray< TSharedRef<FPerforceSourceControlRevision, ESPMode::ThreadSafe> >& History = It.Value();
		State->History = History;
		State->TimeStamp = Now;
		bUpdated = true;
	}

	// add modified state
	for(int ModifiedIndex = 0; ModifiedIndex < OutModifiedFiles.Num(); ModifiedIndex++)
	{
		const FString& FileName = OutModifiedFiles[ModifiedIndex];
		TSharedRef<FPerforceSourceControlState, ESPMode::ThreadSafe> State = PerforceSourceControl.GetProvider().GetStateInternal(FileName);
		State->bModifed = true;
		State->TimeStamp = Now;
		bUpdated = true;
	}

	return bUpdated;
}

FName FPerforceGetWorkspacesWorker::GetName() const
{
	return "GetWorkspaces";
}

bool FPerforceGetWorkspacesWorker::Execute(FPerforceSourceControlCommand& InCommand)
{
	FScopedPerforceConnection ScopedConnection(InCommand);
	if (!InCommand.IsCanceled() && ScopedConnection.IsValid())
	{
		FPerforceConnection& Connection = ScopedConnection.GetConnection();
		TArray<FString> ClientSpecList;
		InCommand.bCommandSuccessful = Connection.GetWorkspaceList(InCommand.ConnectionInfo, FOnIsCancelled::CreateRaw(&InCommand, &FPerforceSourceControlCommand::IsCanceled), ClientSpecList, InCommand.ErrorMessages);

		check(InCommand.Operation->GetName() == GetName());
		TSharedRef<FGetWorkspaces, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FGetWorkspaces>(InCommand.Operation);
		Operation->Results = ClientSpecList;
	}
	return InCommand.bCommandSuccessful;
}

bool FPerforceGetWorkspacesWorker::UpdateStates() const
{
	return false;
}


FName FPerforceCopyWorker::GetName() const
{
	return "Copy";
}

bool FPerforceCopyWorker::Execute(class FPerforceSourceControlCommand& InCommand)
{
	FScopedPerforceConnection ScopedConnection(InCommand);
	if (!InCommand.IsCanceled() && ScopedConnection.IsValid())
	{
		FPerforceConnection& Connection = ScopedConnection.GetConnection();

		check(InCommand.Operation->GetName() == GetName());
		TSharedRef<FCopy, ESPMode::ThreadSafe> Operation = StaticCastSharedRef<FCopy>(InCommand.Operation);

		FString DestinationPath = FPaths::ConvertRelativePathToFull(Operation->GetDestination());

		TArray<FString> Parameters;

		AppendChangelistParameter(Parameters);

		Parameters.Append(InCommand.Files);
		Parameters.Add(DestinationPath);
		
		FP4RecordSet Records;
		InCommand.bCommandSuccessful = Connection.RunCommand(TEXT("integrate"), Parameters, Records, InCommand.ErrorMessages, FOnIsCancelled::CreateRaw(&InCommand, &FPerforceSourceControlCommand::IsCanceled), InCommand.bConnectionDropped);

		// We now need to do a p4 resolve.
		// This is because when we copy a file in the Editor, we first make the copy on disk before attempting to branch. This causes a conflict in P4's eyes.
		// We must do this to prevent the asset registry from picking up what it thinks is a newly-added file (which would be created by the p4 integrate command)
		// and then the package system getting very confused about where to save the now-duplicated assets.
		if(InCommand.bCommandSuccessful)
		{
			TArray<FString> ResolveParameters;
			ResolveParameters.Add(TEXT("-ay"));	// 'accept yours'
			ResolveParameters.Add(DestinationPath);
			InCommand.bCommandSuccessful = Connection.RunCommand(TEXT("resolve"), ResolveParameters, Records, InCommand.ErrorMessages, FOnIsCancelled::CreateRaw(&InCommand, &FPerforceSourceControlCommand::IsCanceled), InCommand.bConnectionDropped);
		}
	}
	return InCommand.bCommandSuccessful;
}

bool FPerforceCopyWorker::UpdateStates() const
{
	return UpdateCachedStates(OutResults);
}

// IPerforceSourceControlWorker interface
FName FPerforceResolveWorker::GetName() const 
{
	return "Resolve";
}

bool FPerforceResolveWorker::Execute(class FPerforceSourceControlCommand& InCommand) 
{
	FScopedPerforceConnection ScopedConnection(InCommand);
	if (!InCommand.IsCanceled() && ScopedConnection.IsValid())
	{
		FPerforceConnection& Connection = ScopedConnection.GetConnection();

		TArray<FString> Parameters;

		Parameters.Add("-ay");
		Parameters.Append(InCommand.Files);
		AppendChangelistParameter(Parameters);

		FP4RecordSet Records;
		InCommand.bCommandSuccessful = Connection.RunCommand(TEXT("resolve"), Parameters, Records, InCommand.ErrorMessages, FOnIsCancelled::CreateRaw(&InCommand, &FPerforceSourceControlCommand::IsCanceled), InCommand.bConnectionDropped);
		if( InCommand.bCommandSuccessful )
		{
			UpdatedFiles = InCommand.Files;
		}
	}

	return InCommand.bCommandSuccessful;
}

bool FPerforceResolveWorker::UpdateStates() const 
{
	FPerforceSourceControlModule& PerforceSourceControl = FPerforceSourceControlModule::Get();

	for( const auto& Filename : UpdatedFiles )
	{
		auto State = PerforceSourceControl.GetProvider().GetStateInternal( Filename );
		State->LocalRevNumber = State->DepotRevNumber;
		State->PendingResolveRevNumber = FPerforceSourceControlState::INVALID_REVISION;
	}

	return UpdatedFiles.Num() > 0;
}

#undef LOCTEXT_NAMESPACE
