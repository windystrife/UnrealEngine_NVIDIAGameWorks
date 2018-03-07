// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PerforceSourceControlCommand.h"
#include "Modules/ModuleManager.h"
#include "PerforceSourceControlModule.h"

FPerforceSourceControlCommand::FPerforceSourceControlCommand(const TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe>& InOperation, const TSharedRef<class IPerforceSourceControlWorker, ESPMode::ThreadSafe>& InWorker, const FSourceControlOperationComplete& InOperationCompleteDelegate )
	: Operation(InOperation)
	, Worker(InWorker)
	, OperationCompleteDelegate(InOperationCompleteDelegate)
	, bExecuteProcessed(0)
	, bCancelled(0)
	, bConnectionWasSuccessful(0)
	, bCancelledWhileTryingToConnect(0)
	, bCommandSuccessful(false)
	, bConnectionDropped(false)
	, bAutoDelete(true)
	, Concurrency(EConcurrency::Synchronous)
{
	// grab the providers settings here, so we don't access them once the worker thread is launched
	check(IsInGameThread());
	FPerforceSourceControlModule& PerforceSourceControl = FModuleManager::LoadModuleChecked<FPerforceSourceControlModule>( "PerforceSourceControl" );
	ConnectionInfo = PerforceSourceControl.AccessSettings().GetConnectionInfo();
}

bool FPerforceSourceControlCommand::DoWork()
{
	bCommandSuccessful = Worker->Execute(*this);
	FPlatformAtomics::InterlockedExchange(&bExecuteProcessed, 1);

	return bCommandSuccessful;
}

void FPerforceSourceControlCommand::Abandon()
{
	FPlatformAtomics::InterlockedExchange(&bExecuteProcessed, 1);
}

void FPerforceSourceControlCommand::DoThreadedWork()
{
	Concurrency = EConcurrency::Asynchronous;
	DoWork();
}

void FPerforceSourceControlCommand::Cancel()
{
	FPlatformAtomics::InterlockedExchange(&bCancelled, 1);
}

void FPerforceSourceControlCommand::MarkConnectionAsSuccessful()
{
	FPlatformAtomics::InterlockedExchange(&bConnectionWasSuccessful, 1);
}

void FPerforceSourceControlCommand::CancelWhileTryingToConnect()
{
	FPlatformAtomics::InterlockedExchange(&bCancelledWhileTryingToConnect, 1);
}

bool FPerforceSourceControlCommand::IsCanceled() const
{
	return bCancelled != 0;
}

bool FPerforceSourceControlCommand::WasConnectionSuccessful() const
{
	return bConnectionWasSuccessful != 0;
}

bool FPerforceSourceControlCommand::WasCanceledWhileTryingToConnect() const
{
	return bCancelledWhileTryingToConnect != 0;
}
