// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SubversionSourceControlProvider.h"
#include "HAL/PlatformProcess.h"
#include "Misc/MessageDialog.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/QueuedThreadPool.h"
#include "Modules/ModuleManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "ISourceControlModule.h"
#include "SubversionSourceControlCommand.h"
#include "SubversionSourceControlModule.h"
#include "ISourceControlLabel.h"
#include "SubversionSourceControlLabel.h"
#include "SubversionSourceControlUtils.h"
#include "SSubversionSourceControlSettings.h"
#include "Logging/MessageLog.h"
#include "XmlFile.h"
#include "ScopedSourceControlProgress.h"

#define LOCTEXT_NAMESPACE "SubversionSourceControl"

static FName ProviderName("Subversion");

void FSubversionSourceControlProvider::Init(bool bForceConnection)
{
	ParseCommandLineSettings(bForceConnection);
}

void FSubversionSourceControlProvider::Close()
{
	// clear the cache
	StateCache.Empty();
}

TSharedRef<FSubversionSourceControlState, ESPMode::ThreadSafe> FSubversionSourceControlProvider::GetStateInternal(const FString& Filename)
{
	TSharedRef<FSubversionSourceControlState, ESPMode::ThreadSafe>* State = StateCache.Find(Filename);
	if(State != NULL)
	{
		// found cached item
		return (*State);
	}
	else
	{
		// cache an unknown state for this item
		TSharedRef<FSubversionSourceControlState, ESPMode::ThreadSafe> NewState = MakeShareable( new FSubversionSourceControlState(Filename) );
		StateCache.Add(Filename, NewState);
		return NewState;
	}
}

bool FSubversionSourceControlProvider::RemoveFileFromCache(const FString& Filename)
{
	return StateCache.Remove(Filename) > 0;
}

FText FSubversionSourceControlProvider::GetStatusText() const
{
	FFormatNamedArguments Args;
	Args.Add( TEXT("IsEnabled"), IsEnabled() ? LOCTEXT("Yes", "Yes") : LOCTEXT("No", "No") );
	Args.Add( TEXT("RepositoryName"), FText::FromString( GetRepositoryName() ) );
	Args.Add( TEXT("UserName"), FText::FromString( GetUserName() ) );

	return FText::Format( NSLOCTEXT("Status", "Provider: Subversion\nEnabledLabel", "Enabled: {IsEnabled}\nRepository: {RepositoryName}\nUser name: {UserName}"), Args );
}

bool FSubversionSourceControlProvider::IsEnabled() const
{
	return true;
}

bool FSubversionSourceControlProvider::IsAvailable() const
{
	// we are always able to work without a server, we just cant sync/commit etc.
	return !bWorkingOffline;
}

const FName& FSubversionSourceControlProvider::GetName(void) const
{
	return ProviderName;
}

ECommandResult::Type FSubversionSourceControlProvider::GetState( const TArray<FString>& InFiles, TArray< TSharedRef<ISourceControlState, ESPMode::ThreadSafe> >& OutState, EStateCacheUsage::Type InStateCacheUsage )
{
	if(!IsEnabled())
	{
		return ECommandResult::Failed;
	}

	TArray<FString> AbsoluteFiles = SourceControlHelpers::AbsoluteFilenames(InFiles);

	if(InStateCacheUsage == EStateCacheUsage::ForceUpdate)
	{
		Execute(ISourceControlOperation::Create<FUpdateStatus>(), AbsoluteFiles);
	}

	for( TArray<FString>::TConstIterator It(AbsoluteFiles); It; It++)
	{
		TSharedRef<FSubversionSourceControlState, ESPMode::ThreadSafe>* State = StateCache.Find(*It);
		if(State != NULL)
		{
			// found cached item for this file, return that
			OutState.Add(*State);
		}
		else
		{
			// cache an unknown state for this item & return that
			TSharedRef<FSubversionSourceControlState, ESPMode::ThreadSafe> NewState = MakeShareable( new FSubversionSourceControlState(*It) );
			StateCache.Add(*It, NewState);
			OutState.Add(NewState);
		}
	}

	return ECommandResult::Succeeded;
}

TArray<FSourceControlStateRef> FSubversionSourceControlProvider::GetCachedStateByPredicate(TFunctionRef<bool(const FSourceControlStateRef&)> Predicate) const
{
	TArray<FSourceControlStateRef> Result;
	for (const auto& CacheItem : StateCache)
	{
		FSourceControlStateRef State = CacheItem.Value;
		if (Predicate(State))
		{
			Result.Add(State);
		}
	}
	return Result;
}

FDelegateHandle FSubversionSourceControlProvider::RegisterSourceControlStateChanged_Handle( const FSourceControlStateChanged::FDelegate& SourceControlStateChanged )
{
	return OnSourceControlStateChanged.Add( SourceControlStateChanged );
}

void FSubversionSourceControlProvider::UnregisterSourceControlStateChanged_Handle( FDelegateHandle Handle )
{
	OnSourceControlStateChanged.Remove( Handle );
}

ECommandResult::Type FSubversionSourceControlProvider::Execute( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation, const TArray<FString>& InFiles, EConcurrency::Type InConcurrency, const FSourceControlOperationComplete& InOperationCompleteDelegate )
{
	if(!IsEnabled())
	{
		return ECommandResult::Failed;
	}

	if(!SubversionSourceControlUtils::CheckFilenames(InFiles))
	{
		return ECommandResult::Failed;
	}

	TArray<FString> AbsoluteFiles = SourceControlHelpers::AbsoluteFilenames(InFiles);

	// Query to see if the we allow this operation
	TSharedPtr<ISubversionSourceControlWorker, ESPMode::ThreadSafe> Worker = CreateWorker(InOperation->GetName());
	if(!Worker.IsValid())
	{
		// this operation is unsupported by this source control provider
		FFormatNamedArguments Arguments;
		Arguments.Add( TEXT("OperationName"), FText::FromName(InOperation->GetName()) );
		Arguments.Add( TEXT("ProviderName"), FText::FromName(GetName()) );
		FMessageLog("SourceControl").Error(FText::Format(LOCTEXT("UnsupportedOperation", "Operation '{OperationName}' not supported by source control provider '{ProviderName}'"), Arguments));
		return ECommandResult::Failed;
	}

	// fire off operation
	if(InConcurrency == EConcurrency::Synchronous)
	{
		FSubversionSourceControlCommand* Command = new FSubversionSourceControlCommand(InOperation, Worker.ToSharedRef());
		Command->bAutoDelete = false;
		Command->Files = AbsoluteFiles;
		Command->OperationCompleteDelegate = InOperationCompleteDelegate;
		return ExecuteSynchronousCommand(*Command, InOperation->GetInProgressString(), true);
	}
	else
	{
		FSubversionSourceControlCommand* Command = new FSubversionSourceControlCommand(InOperation, Worker.ToSharedRef());
		Command->bAutoDelete = true;
		Command->Files = AbsoluteFiles;
		Command->OperationCompleteDelegate = InOperationCompleteDelegate;
		return IssueCommand(*Command, false);
	}
}

bool FSubversionSourceControlProvider::CanCancelOperation( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation ) const
{
	return false;
}

void FSubversionSourceControlProvider::CancelOperation( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation )
{
}

bool FSubversionSourceControlProvider::UsesLocalReadOnlyState() const
{
	return false;
}

bool FSubversionSourceControlProvider::UsesChangelists() const
{
	return false;
}

bool FSubversionSourceControlProvider::UsesCheckout() const
{
	return true;
}

TSharedPtr<ISubversionSourceControlWorker, ESPMode::ThreadSafe> FSubversionSourceControlProvider::CreateWorker(const FName& InOperationName) const
{
	const FGetSubversionSourceControlWorker* Operation = WorkersMap.Find(InOperationName);
	if(Operation != NULL)
	{
		return Operation->Execute();
	}
		
	return NULL;
}

void FSubversionSourceControlProvider::RegisterWorker( const FName& InName, const FGetSubversionSourceControlWorker& InDelegate )
{
	WorkersMap.Add( InName, InDelegate );
}

/**
 * Loads user/SCC information from the command line or INI file.
 */
void FSubversionSourceControlProvider::ParseCommandLineSettings(bool bForceConnection)
{
	ISourceControlModule& SourceControlModule = FModuleManager::LoadModuleChecked<ISourceControlModule>( "SourceControl" );
	FSubversionSourceControlModule& SubversionSourceControl = FModuleManager::GetModuleChecked<FSubversionSourceControlModule>( "SubversionSourceControl" );

	bool bFoundCmdLineSettings = false;

	// Check command line for any overridden settings
	FString RepositoryName = SubversionSourceControl.AccessSettings().GetRepository();
	FString UserName = SubversionSourceControl.AccessSettings().GetUserName();
	FString Password;
	bFoundCmdLineSettings = FParse::Value(FCommandLine::Get(), TEXT("SVNRepo="), RepositoryName);
	bFoundCmdLineSettings |= FParse::Value(FCommandLine::Get(), TEXT("SVNUser="), UserName);
	bFoundCmdLineSettings |= FParse::Value(FCommandLine::Get(), TEXT("SVNPass="), Password);

	// Command line settings get written to the global settings.
	// These wont get saved if the build is unattended.
	if(bFoundCmdLineSettings)
	{
		SubversionSourceControl.AccessSettings().SetRepository(RepositoryName);
		SubversionSourceControl.AccessSettings().SetUserName(UserName);
	}

	if(bForceConnection && TestConnection(RepositoryName, UserName, Password))
	{
		bWorkingOffline = false;
	}

	//Save off settings so this doesn't happen every time (wont save if unattended)
	SubversionSourceControl.SaveSettings();
}

const FString& FSubversionSourceControlProvider::GetRepositoryName() const
{
	FSubversionSourceControlModule& SubversionSourceControl = FModuleManager::GetModuleChecked<FSubversionSourceControlModule>( "SubversionSourceControl" );
	return SubversionSourceControl.AccessSettings().GetRepository();
}

const FString& FSubversionSourceControlProvider::GetUserName() const
{
	FSubversionSourceControlModule& SubversionSourceControl = FModuleManager::GetModuleChecked<FSubversionSourceControlModule>( "SubversionSourceControl" );
	return SubversionSourceControl.AccessSettings().GetUserName();
}

const FString& FSubversionSourceControlProvider::GetWorkingCopyRoot() const
{
	return WorkingCopyRoot;
}

void FSubversionSourceControlProvider::SetWorkingCopyRoot(const FString& InWorkingCopyRoot)
{
	WorkingCopyRoot = InWorkingCopyRoot;
}

const FString& FSubversionSourceControlProvider::GetRepositoryRoot() const
{
	return RepositoryRoot;
}

void FSubversionSourceControlProvider::SetRepositoryRoot(const FString& InRepositoryRoot)
{
	RepositoryRoot = InRepositoryRoot;
}

bool FSubversionSourceControlProvider::TestConnection(const FString& RepositoryName, const FString& UserName, const FString& Password)
{
	FMessageLog SourceControlLog("SourceControl");

	// run a command on the server to see check connection.
	// If our credentials have not been cached then this will fail.
	TArray<FString> Files;
	Files.Add(FPaths::ProjectDir());
	TArray<class FXmlFile> ResultsXml;
	TArray<FString> Errors;

	bool bResult = SubversionSourceControlUtils::RunCommand(TEXT("info"), Files, TArray<FString>(), ResultsXml, Errors, UserName, Password);
	if(bResult)
	{
		SubversionSourceControlUtils::ParseInfoResults(ResultsXml, WorkingCopyRoot, RepositoryRoot);
	}

	// output any errors/results
	for(TArray<FString>::TConstIterator It = Errors.CreateConstIterator(); It; It++)
	{
		SourceControlLog.Warning(FText::FromString(*It));
	}

	if(bResult)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add( TEXT("RepositoryName"), FText::FromString(RepositoryName) );
		SourceControlLog.Warning(FText::Format(LOCTEXT("ConnectionSuccess", "Successfully connected to repository {RepositoryName}"), Arguments));
	}
	else
	{
		FFormatNamedArguments Arguments;
		Arguments.Add( TEXT("RepositoryName"), FText::FromString(RepositoryName) );
		SourceControlLog.Warning(FText::Format(LOCTEXT("ConnectionFailed", "Failed to connect to repository {RepositoryName}"), Arguments));
	}

	return bResult;
}

void FSubversionSourceControlProvider::OutputCommandMessages(const FSubversionSourceControlCommand& InCommand) const
{
	FMessageLog SourceControlLog("SourceControl");

	for (int32 ErrorIndex = 0; ErrorIndex < InCommand.ErrorMessages.Num(); ++ErrorIndex)
	{
		SourceControlLog.Error(FText::FromString(InCommand.ErrorMessages[ErrorIndex]));
	}

	for (int32 InfoIndex = 0; InfoIndex < InCommand.InfoMessages.Num(); ++InfoIndex)
	{
		SourceControlLog.Info(FText::FromString(InCommand.InfoMessages[InfoIndex]));
	}
}

void FSubversionSourceControlProvider::Tick()
{	
	bool bStatesUpdated = false;
	for (int32 CommandIndex = 0; CommandIndex < CommandQueue.Num(); ++CommandIndex)
	{
		FSubversionSourceControlCommand& Command = *CommandQueue[CommandIndex];
		if (Command.bExecuteProcessed)
		{
			// Remove command from the queue
			CommandQueue.RemoveAt(CommandIndex);

			// let command update the states of any files
			bStatesUpdated |= Command.Worker->UpdateStates();

			// update connection state
			UpdateConnectionState(Command);

			// dump any messages to output log
			OutputCommandMessages(Command);

			// run the completion delegate if we have one bound
			Command.OperationCompleteDelegate.ExecuteIfBound(Command.Operation, Command.bCommandSuccessful ? ECommandResult::Succeeded : ECommandResult::Failed);

			//commands that are left in the array during a tick need to be deleted
			if(Command.bAutoDelete)
			{
				// Only delete commands that are not running 'synchronously'
				delete &Command;
			}
			
			// only do one command per tick loop, as we dont want concurrent modification 
			// of the command queue (which can happen in the completion delegate)
			break;
		}
	}

	if(bStatesUpdated)
	{
		OnSourceControlStateChanged.Broadcast();
	}
}

static void ParseListResults(const TArray<FXmlFile>& ResultsXml, TArray< TSharedRef<FSubversionSourceControlLabel> >& OutLabels)
{
	static const FString Lists(TEXT("lists"));
	static const FString List(TEXT("list"));
	static const FString Path(TEXT("path"));
	static const FString Entry(TEXT("entry"));
	static const FString Kind(TEXT("kind"));
	static const FString Dir(TEXT("dir"));
	static const FString Name(TEXT("name"));
	static const FString Commit(TEXT("commit"));
	static const FString Revision(TEXT("revision"));

	for(auto ResultIt(ResultsXml.CreateConstIterator()); ResultIt; ResultIt++)
	{
		const FXmlNode* ListsNode = ResultIt->GetRootNode();
		if(ListsNode != NULL && ListsNode->GetTag() == Lists)
		{ 
			const TArray<FXmlNode*> ListsChildren = ListsNode->GetChildrenNodes();
			for(auto ListIter(ListsChildren.CreateConstIterator()); ListIter; ListIter++)
			{
				const FXmlNode* ListNode = *ListIter;
				if(ListNode == NULL || ListNode->GetTag() != List)
				{
					continue;
				}

				FString DirectoryPath = ListNode->GetAttribute(Path);

				const TArray<FXmlNode*> ListChildren = ListNode->GetChildrenNodes();
				for(auto EntryIter(ListChildren.CreateConstIterator()); EntryIter; EntryIter++)
				{
					const FXmlNode* EntryNode = *EntryIter;
					if(EntryNode == NULL || EntryNode->GetTag() != Entry)
					{
						continue;
					}

					if(EntryNode->GetAttribute(Kind) == Dir)
					{
						// find a name & revision for this directory
						const FXmlNode* NameNode = EntryNode->FindChildNode(Name);
						if(NameNode != NULL)
						{
							FString LabelName = NameNode->GetContent();
							if (LabelName.Len() > 0)
							{
								const FXmlNode* CommitNode = EntryNode->FindChildNode(Commit);
								if(CommitNode != NULL)
								{
									FString RevisionString = CommitNode->GetAttribute(Revision);
									if(RevisionString.Len() > 0)
									{
										int RevisionNum = FCString::Atoi(*RevisionString);
										OutLabels.Add(MakeShareable(new FSubversionSourceControlLabel(LabelName, DirectoryPath / LabelName, RevisionNum)));
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

TArray< TSharedRef<ISourceControlLabel> > FSubversionSourceControlProvider::GetLabels( const FString& InMatchingSpec ) const
{
	TArray< TSharedRef<ISourceControlLabel> > Labels;

	// look for each directory that matches the spec in the repository
	FSubversionSourceControlModule& SubversionSourceControl = FModuleManager::LoadModuleChecked<FSubversionSourceControlModule>( "SubversionSourceControl" );

	TArray<FXmlFile> ResultsXml;
	TArray<FString> Files;
	TArray<FString> ErrorMessages;
	Files.Add(GetRepositoryName() / SubversionSourceControl.AccessSettings().GetLabelsRoot());

	if(SubversionSourceControlUtils::RunCommand(TEXT("list"), Files, TArray<FString>(), ResultsXml, ErrorMessages, GetUserName()))
	{
		TArray< TSharedRef<FSubversionSourceControlLabel> > AllLabels;
		ParseListResults(ResultsXml, AllLabels);
		
		for(auto Iter(AllLabels.CreateConstIterator()); Iter; Iter++)
		{
			if((*Iter)->GetName().Contains(InMatchingSpec))
			{
				Labels.Add(*Iter);
			}
		}
	}
	else
	{
		// output errors if any
		for (int32 ErrorIndex = 0; ErrorIndex < ErrorMessages.Num(); ++ErrorIndex)
		{
			FMessageLog("SourceControl").Warning(FText::FromString(ErrorMessages[ErrorIndex]));
		}
	}

	return Labels;
}

#if SOURCE_CONTROL_WITH_SLATE
TSharedRef<class SWidget> FSubversionSourceControlProvider::MakeSettingsWidget() const
{
	return SNew(SSubversionSourceControlSettings);
}
#endif

ECommandResult::Type FSubversionSourceControlProvider::ExecuteSynchronousCommand(FSubversionSourceControlCommand& InCommand, const FText& Task, bool bSuppressResponseMsg)
{
	ECommandResult::Type Result = ECommandResult::Failed;

	// Display the progress dialog if a string was provided
	{
		FScopedSourceControlProgress Progress(Task);

		// Perform the command asynchronously
		IssueCommand( InCommand, false );

		while(!InCommand.bExecuteProcessed)
		{
			// Tick the command queue and update progress.
			Tick();
			
			Progress.Tick();

			// Sleep for a bit so we don't busy-wait so much.
			FPlatformProcess::Sleep(0.01f);
		}
	
		// always do one more Tick() to make sure the command queue is cleaned up.
		Tick();

		if(InCommand.bCommandSuccessful)
		{
			Result = ECommandResult::Succeeded;
		}
	}
	

	// If the command failed, inform the user that they need to try again
	if ( Result != ECommandResult::Succeeded && !bSuppressResponseMsg )
	{
		FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("Subversion_ServerUnresponsive", "Subversion repository is unresponsive. Please check your connection and try again.") );
	}

	// Delete the command now
	check(!InCommand.bAutoDelete);

	// ensure commands that are not auto deleted do not end up in the command queue
	if ( CommandQueue.Contains( &InCommand ) ) 
	{
		CommandQueue.Remove( &InCommand );
	}

	delete &InCommand;

	return Result;
}

ECommandResult::Type FSubversionSourceControlProvider::IssueCommand(FSubversionSourceControlCommand& InCommand, const bool bSynchronous)
{
	if ( !bSynchronous && GThreadPool != NULL )
	{
		// Queue this to our worker thread(s) for resolving
		GThreadPool->AddQueuedWork(&InCommand);
		CommandQueue.Add(&InCommand);
		return ECommandResult::Succeeded;
	}
	else
	{
		InCommand.bCommandSuccessful = InCommand.DoWork();
		
		UpdateConnectionState(InCommand);

		InCommand.Worker->UpdateStates();

		OutputCommandMessages(InCommand);

		// Callback now if present. When asynchronous, this callback gets called from Tick().
		ECommandResult::Type Result = InCommand.bCommandSuccessful ? ECommandResult::Succeeded : ECommandResult::Failed;
		InCommand.OperationCompleteDelegate.ExecuteIfBound(InCommand.Operation, Result);

		return Result;
	}
}

void FSubversionSourceControlProvider::UpdateConnectionState(FSubversionSourceControlCommand& InCommand)
{
	if(InCommand.Operation->GetName() == "Connect" && !InCommand.bCommandSuccessful)
	{
		bWorkingOffline = true;
	}
	else if(InCommand.bCommandSuccessful)
	{
		bWorkingOffline = false;
	}
}

#undef LOCTEXT_NAMESPACE
