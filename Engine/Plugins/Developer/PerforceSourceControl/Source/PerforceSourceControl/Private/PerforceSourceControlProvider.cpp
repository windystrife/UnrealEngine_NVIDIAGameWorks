// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PerforceSourceControlProvider.h"
#include "PerforceSourceControlPrivate.h"
#include "HAL/PlatformProcess.h"
#include "Misc/MessageDialog.h"
#include "Misc/CommandLine.h"
#include "Misc/QueuedThreadPool.h"
#include "Modules/ModuleManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "ISourceControlModule.h"
#include "PerforceConnectionInfo.h"
#include "PerforceSourceControlCommand.h"
#include "ISourceControlLabel.h"
#include "PerforceSourceControlLabel.h"
#include "PerforceConnection.h"
#include "PerforceSourceControlModule.h"
#include "SPerforceSourceControlSettings.h"
#include "Logging/MessageLog.h"
#include "ScopedSourceControlProgress.h"

static FName ProviderName("Perforce");

#define LOCTEXT_NAMESPACE "PerforceSourceControl"

/** Init of connection with source control server */
void FPerforceSourceControlProvider::Init(bool bForceConnection)
{
	ParseCommandLineSettings(bForceConnection);
}

/** API Specific close the connection with source control server*/
void FPerforceSourceControlProvider::Close()
{
	if ( PersistentConnection )
	{
		PersistentConnection->Disconnect();
		delete PersistentConnection;
		PersistentConnection = NULL;
	}

	// clear the cache
	StateCache.Empty();

	bServerAvailable = false;
}

TSharedRef<FPerforceSourceControlState, ESPMode::ThreadSafe> FPerforceSourceControlProvider::GetStateInternal(const FString& Filename)
{
	TSharedRef<FPerforceSourceControlState, ESPMode::ThreadSafe>* State = StateCache.Find(Filename);
	if(State != NULL)
	{
		// found cached item
		return (*State);
	}
	else
	{
		// cache an unknown state for this item
		TSharedRef<FPerforceSourceControlState, ESPMode::ThreadSafe> NewState = MakeShareable( new FPerforceSourceControlState(Filename) );
		StateCache.Add(Filename, NewState);
		return NewState;
	}
}

FText FPerforceSourceControlProvider::GetStatusText() const
{
	FPerforceSourceControlModule& PerforceSourceControl = FModuleManager::LoadModuleChecked<FPerforceSourceControlModule>( "PerforceSourceControl" );
	const FPerforceSourceControlSettings& Settings = PerforceSourceControl.AccessSettings();

	FFormatNamedArguments Args;
	Args.Add( TEXT("IsEnabled"), IsEnabled() ? LOCTEXT("Yes", "Yes") : LOCTEXT("No", "No") );
	Args.Add( TEXT("IsConnected"), (IsEnabled() && IsAvailable()) ? LOCTEXT("Yes", "Yes") : LOCTEXT("No", "No") );
	Args.Add( TEXT("PortNumber"), FText::FromString( Settings.GetPort() ) );
	Args.Add( TEXT("UserName"), FText::FromString( Settings.GetUserName() ) );
	Args.Add( TEXT("ClientSpecName"), FText::FromString( Settings.GetWorkspace() ) );

	return FText::Format( LOCTEXT("PerforceStatusText", "Enabled: {IsEnabled}\nConnected: {IsConnected}\n\nPort: {PortNumber}\nUser name: {UserName}\nClient name: {ClientSpecName}"), Args );
}

bool FPerforceSourceControlProvider::IsEnabled() const
{
	return true;
}

bool FPerforceSourceControlProvider::IsAvailable() const
{
	return bServerAvailable;
}

bool FPerforceSourceControlProvider::EstablishPersistentConnection()
{
	FPerforceSourceControlModule& PerforceSourceControl = FModuleManager::LoadModuleChecked<FPerforceSourceControlModule>( "PerforceSourceControl" );
	FPerforceConnectionInfo ConnectionInfo = PerforceSourceControl.AccessSettings().GetConnectionInfo();

	bool bIsValidConnection = false;
	if ( !PersistentConnection )
	{
		PersistentConnection = new FPerforceConnection(ConnectionInfo);
	}

	bIsValidConnection = PersistentConnection->IsValidConnection();
	if ( !bIsValidConnection )
	{
		delete PersistentConnection;
		PersistentConnection = new FPerforceConnection(ConnectionInfo);
		bIsValidConnection = PersistentConnection->IsValidConnection();
	}

	bServerAvailable = bIsValidConnection;
	return bIsValidConnection;
}

/**
 * Loads user/SCC information from the command line or INI file.
 */
void FPerforceSourceControlProvider::ParseCommandLineSettings(bool bForceConnection)
{
	ISourceControlModule& SourceControlModule = FModuleManager::LoadModuleChecked<ISourceControlModule>( "SourceControl" );
	FPerforceSourceControlModule& PerforceSourceControl = FModuleManager::GetModuleChecked<FPerforceSourceControlModule>( "PerforceSourceControl" );

	bool bFoundCmdLineSettings = false;

	// Check command line for any overridden settings
	FPerforceSourceControlSettings& P4Settings = PerforceSourceControl.AccessSettings();

	FString PortName = P4Settings.GetPort();
	FString UserName = P4Settings.GetUserName();
	FString ClientSpecName = P4Settings.GetWorkspace();
	FString HostOverrideName = P4Settings.GetHostOverride();
	FString Changelist = P4Settings.GetChangelistNumber();
	bFoundCmdLineSettings = FParse::Value(FCommandLine::Get(), TEXT("P4Port="), PortName);
	bFoundCmdLineSettings |= FParse::Value(FCommandLine::Get(), TEXT("P4User="), UserName);
	bFoundCmdLineSettings |= FParse::Value(FCommandLine::Get(), TEXT("P4Client="), ClientSpecName);
	bFoundCmdLineSettings |= FParse::Value(FCommandLine::Get(), TEXT("P4Host="), HostOverrideName);
	bFoundCmdLineSettings |= FParse::Value(FCommandLine::Get(), TEXT("P4Passwd="), Ticket);
	bFoundCmdLineSettings |= FParse::Value(FCommandLine::Get(), TEXT("P4Changelist="), Changelist);
	if(bFoundCmdLineSettings)
	{
		P4Settings.SetPort(PortName);
		P4Settings.SetUserName(UserName);
		P4Settings.SetWorkspace(ClientSpecName);
		P4Settings.SetHostOverride(HostOverrideName);
		P4Settings.SetChangelistNumber(Changelist);
	}
	
	if (bForceConnection)
	{
		FPerforceConnectionInfo ConnectionInfo = P4Settings.GetConnectionInfo();
		if(FPerforceConnection::EnsureValidConnection(PortName, UserName, ClientSpecName, ConnectionInfo))
		{
			P4Settings.SetPort(PortName);
			P4Settings.SetUserName(UserName);
			P4Settings.SetWorkspace(ClientSpecName);
			P4Settings.SetHostOverride(HostOverrideName);
		}

		bServerAvailable = true;
	}

	//Save off settings so this doesn't happen every time
	PerforceSourceControl.SaveSettings();
}


void FPerforceSourceControlProvider::GetWorkspaceList(const FPerforceConnectionInfo& InConnectionInfo, TArray<FString>& OutWorkspaceList, TArray<FText>& OutErrorMessages)
{
	//attempt to ask perforce for a list of client specs that belong to this user
	FPerforceConnection Connection(InConnectionInfo);
	Connection.GetWorkspaceList(InConnectionInfo, FOnIsCancelled(), OutWorkspaceList, OutErrorMessages);
}

const FString& FPerforceSourceControlProvider::GetTicket() const 
{ 
	return Ticket; 
}

const FName& FPerforceSourceControlProvider::GetName() const
{
	return ProviderName;
}

ECommandResult::Type FPerforceSourceControlProvider::GetState( const TArray<FString>& InFiles, TArray< TSharedRef<ISourceControlState, ESPMode::ThreadSafe> >& OutState, EStateCacheUsage::Type InStateCacheUsage )
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
		TSharedRef<FPerforceSourceControlState, ESPMode::ThreadSafe>* State = StateCache.Find(*It);
		if(State != NULL)
		{
			// found cached item for this file, return that
			OutState.Add(*State);
		}
		else
		{
			// cache an unknown state for this item & return that
			TSharedRef<FPerforceSourceControlState, ESPMode::ThreadSafe> NewState = MakeShareable( new FPerforceSourceControlState(*It) );
			StateCache.Add(*It, NewState);
			OutState.Add(NewState);
		}
	}

	return ECommandResult::Succeeded;
}

bool FPerforceSourceControlProvider::RemoveFileFromCache(const FString& Filename)
{
	return StateCache.Remove(Filename) > 0;
}

TArray<FSourceControlStateRef> FPerforceSourceControlProvider::GetCachedStateByPredicate(TFunctionRef<bool(const FSourceControlStateRef&)> Predicate) const
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

FDelegateHandle FPerforceSourceControlProvider::RegisterSourceControlStateChanged_Handle( const FSourceControlStateChanged::FDelegate& SourceControlStateChanged )
{
	return OnSourceControlStateChanged.Add( SourceControlStateChanged );
}

void FPerforceSourceControlProvider::UnregisterSourceControlStateChanged_Handle( FDelegateHandle Handle )
{
	OnSourceControlStateChanged.Remove( Handle );
}

ECommandResult::Type FPerforceSourceControlProvider::Execute( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation, const TArray<FString>& InFiles, EConcurrency::Type InConcurrency, const FSourceControlOperationComplete& InOperationCompleteDelegate )
{
	if(!IsEnabled())
	{
		return ECommandResult::Failed;
	}

	TArray<FString> AbsoluteFiles = SourceControlHelpers::AbsoluteFilenames(InFiles);

	// Query to see if the we allow this operation
	TSharedPtr<IPerforceSourceControlWorker, ESPMode::ThreadSafe> Worker = CreateWorker(InOperation->GetName());
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
		FPerforceSourceControlCommand* Command = new FPerforceSourceControlCommand(InOperation, Worker.ToSharedRef());
		Command->bAutoDelete = false;
		Command->Files = AbsoluteFiles;
		Command->OperationCompleteDelegate = InOperationCompleteDelegate;
		return ExecuteSynchronousCommand(*Command, InOperation->GetInProgressString(), true);
	}
	else
	{
		FPerforceSourceControlCommand* Command = new FPerforceSourceControlCommand(InOperation, Worker.ToSharedRef());
		Command->bAutoDelete = true;
		Command->Files = AbsoluteFiles;
		Command->OperationCompleteDelegate = InOperationCompleteDelegate;
		return IssueCommand(*Command, false);
	}
}

bool FPerforceSourceControlProvider::CanCancelOperation( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation ) const
{
	for (int32 CommandIndex = 0; CommandIndex < CommandQueue.Num(); ++CommandIndex)
	{
		FPerforceSourceControlCommand& Command = *CommandQueue[CommandIndex];
		if (Command.Operation == InOperation)
		{
			check(Command.bAutoDelete);
			return true;
		}
	}

	// operation was not in progress!
	return false;
}

void FPerforceSourceControlProvider::CancelOperation( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation )
{
	for (int32 CommandIndex = 0; CommandIndex < CommandQueue.Num(); ++CommandIndex)
	{
		FPerforceSourceControlCommand& Command = *CommandQueue[CommandIndex];
		if (Command.Operation == InOperation)
		{
			check(Command.bAutoDelete);
			Command.Cancel();
			return;
		}
	}
}

bool FPerforceSourceControlProvider::UsesLocalReadOnlyState() const
{
	return true;
}

bool FPerforceSourceControlProvider::UsesChangelists() const
{
	return true;
}

bool FPerforceSourceControlProvider::UsesCheckout() const
{
	return true;
}

void FPerforceSourceControlProvider::OutputCommandMessages(const FPerforceSourceControlCommand& InCommand) const
{
	FMessageLog SourceControlLog("SourceControl");

	for (int32 ErrorIndex = 0; ErrorIndex < InCommand.ErrorMessages.Num(); ++ErrorIndex)
	{
		SourceControlLog.Error(InCommand.ErrorMessages[ErrorIndex]);
	}

	for (int32 InfoIndex = 0; InfoIndex < InCommand.InfoMessages.Num(); ++InfoIndex)
	{
		SourceControlLog.Info(InCommand.InfoMessages[InfoIndex]);
	}
}

void FPerforceSourceControlProvider::Tick()
{	
	bool bStatesUpdated = false;
	for (int32 CommandIndex = 0; CommandIndex < CommandQueue.Num(); ++CommandIndex)
	{
		FPerforceSourceControlCommand& Command = *CommandQueue[CommandIndex];
		if (Command.bExecuteProcessed)
		{
			// Remove command from the queue
			CommandQueue.RemoveAt(CommandIndex);

			// update connection state
			bServerAvailable = !Command.bConnectionDropped || Command.bCancelled;

			// let command update the states of any files
			bStatesUpdated |= Command.Worker->UpdateStates();

			// dump any messages to output log
			OutputCommandMessages(Command);

			// If the command was cancelled while trying to connect, the operation complete delegate will already
			// have been called. Otherwise, now we have to call it.
			if (!Command.bCancelledWhileTryingToConnect)
			{
				// run the completion delegate if we have one bound
				ECommandResult::Type Result = ECommandResult::Failed;
				if (Command.bCancelled)
				{
					Result = ECommandResult::Cancelled;
				}
				else if (Command.bCommandSuccessful)
				{
					Result = ECommandResult::Succeeded;
				}
				Command.OperationCompleteDelegate.ExecuteIfBound(Command.Operation, Result);
			}

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
		// If a cancel is detected before the server has connected, abort immediately.
		else if (Command.bCancelled && !Command.bConnectionWasSuccessful)
		{
			// Mark command as having been cancelled while trying to connect
			Command.CancelWhileTryingToConnect();

			// If this was a synchronous command, set it free so that it will be deleted automatically
			// when its (still running) thread finally finishes
			Command.bAutoDelete = true;

			// run the completion delegate if we have one bound
			Command.OperationCompleteDelegate.ExecuteIfBound(Command.Operation, ECommandResult::Cancelled);

			break;
		}
	}

	if(bStatesUpdated)
	{
		OnSourceControlStateChanged.Broadcast();
	}
}

static void ParseGetLabelsResults(const FP4RecordSet& InRecords, TArray< TSharedRef<ISourceControlLabel> >& OutLabels)
{
	// Iterate over each record found as a result of the command, parsing it for relevant information
	for (int32 Index = 0; Index < InRecords.Num(); ++Index)
	{
		const FP4Record& ClientRecord = InRecords[Index];
		FString LabelName = ClientRecord(TEXT("label"));
		if(LabelName.Len() > 0)
		{
			OutLabels.Add(MakeShareable( new FPerforceSourceControlLabel(LabelName) ) );
		}
	}
}

TArray< TSharedRef<ISourceControlLabel> > FPerforceSourceControlProvider::GetLabels( const FString& InMatchingSpec ) const
{
	TArray< TSharedRef<ISourceControlLabel> > Labels;

	FPerforceSourceControlModule& PerforceSourceControl = FModuleManager::LoadModuleChecked<FPerforceSourceControlModule>("PerforceSourceControl");
	FScopedPerforceConnection ScopedConnection(EConcurrency::Synchronous, PerforceSourceControl.AccessSettings().GetConnectionInfo());
	if(ScopedConnection.IsValid())
	{
		FPerforceConnection& Connection = ScopedConnection.GetConnection();
		FP4RecordSet Records;
		TArray<FString> Parameters;
		TArray<FText> ErrorMessages;
		Parameters.Add(TEXT("-E"));
		Parameters.Add(InMatchingSpec);
		bool bConnectionDropped = false;
		if(Connection.RunCommand(TEXT("labels"), Parameters, Records, ErrorMessages, FOnIsCancelled(), bConnectionDropped))
		{
			ParseGetLabelsResults(Records, Labels);
		}
		else
		{
			// output errors if any
			for (int32 ErrorIndex = 0; ErrorIndex < ErrorMessages.Num(); ++ErrorIndex)
			{
				FMessageLog("SourceControl").Warning(ErrorMessages[ErrorIndex]);
			}
		}
	}

	return Labels;
}

#if SOURCE_CONTROL_WITH_SLATE
TSharedRef<class SWidget> FPerforceSourceControlProvider::MakeSettingsWidget() const
{
	return SNew(SPerforceSourceControlSettings);
}
#endif

TSharedPtr<IPerforceSourceControlWorker, ESPMode::ThreadSafe> FPerforceSourceControlProvider::CreateWorker(const FName& InOperationName) const
{
	const FGetPerforceSourceControlWorker* Operation = WorkersMap.Find(InOperationName);
	if(Operation != NULL)
	{
		return Operation->Execute();
	}
		
	return NULL;
}

void FPerforceSourceControlProvider::RegisterWorker( const FName& InName, const FGetPerforceSourceControlWorker& InDelegate )
{
	WorkersMap.Add( InName, InDelegate );
}

ECommandResult::Type FPerforceSourceControlProvider::ExecuteSynchronousCommand(FPerforceSourceControlCommand& InCommand, const FText& Task, bool bSuppressResponseMsg)
{
	ECommandResult::Type Result = ECommandResult::Failed;

	struct Local
	{
		static void CancelCommand(FPerforceSourceControlCommand* InControlCommand)
		{
			InControlCommand->Cancel();
		}
	};

	// Display the progress dialog
	FScopedSourceControlProgress Progress(Task, FSimpleDelegate::CreateStatic(&Local::CancelCommand, &InCommand));

	// Perform the command asynchronously
	IssueCommand( InCommand, false );

	// Wait until the command has been processed
	while (!InCommand.bCancelledWhileTryingToConnect && CommandQueue.Contains(&InCommand))
	{
		// Tick the command queue and update progress.
		Tick();

		Progress.Tick();

		// Sleep for a bit so we don't busy-wait so much.
		FPlatformProcess::Sleep(0.01f);
	}

	if (InCommand.bCancelled)
	{
		Result = ECommandResult::Cancelled;
	}
	else if (InCommand.bCommandSuccessful)
	{
		Result = ECommandResult::Succeeded;
	}

	// If the command failed, inform the user that they need to try again
	if ( !InCommand.bCancelled && Result != ECommandResult::Succeeded && !bSuppressResponseMsg )
	{
		FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("Perforce_ServerUnresponsive", "Perforce server is unresponsive. Please check your connection and try again.") );
	}

	// Delete the command now if not marked as auto-delete
	if (!InCommand.bAutoDelete)
	{
		delete &InCommand;
	}

	return Result;
}

ECommandResult::Type FPerforceSourceControlProvider::IssueCommand(FPerforceSourceControlCommand& InCommand, const bool bSynchronous)
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

		InCommand.Worker->UpdateStates();

		OutputCommandMessages(InCommand);

		// Callback now if present. When asynchronous, this callback gets called from Tick().
		ECommandResult::Type Result = InCommand.bCommandSuccessful ? ECommandResult::Succeeded : ECommandResult::Failed;
		InCommand.OperationCompleteDelegate.ExecuteIfBound(InCommand.Operation, Result);

		return Result;
	}
}

#undef LOCTEXT_NAMESPACE
