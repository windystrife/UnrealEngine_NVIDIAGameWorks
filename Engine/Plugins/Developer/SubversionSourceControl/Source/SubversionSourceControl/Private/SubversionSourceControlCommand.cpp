// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SubversionSourceControlCommand.h"
#include "Modules/ModuleManager.h"
#include "SubversionSourceControlModule.h"
#include "SSubversionSourceControlSettings.h"

FSubversionSourceControlCommand::FSubversionSourceControlCommand(const TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe>& InOperation, const TSharedRef<class ISubversionSourceControlWorker, ESPMode::ThreadSafe>& InWorker, const FSourceControlOperationComplete& InOperationCompleteDelegate )
	: Operation(InOperation)
	, Worker(InWorker)
	, OperationCompleteDelegate(InOperationCompleteDelegate)
	, bExecuteProcessed(0)
	, bCommandSuccessful(false)
	, bAutoDelete(true)
	, Concurrency(EConcurrency::Synchronous)
{
	// grab the providers settings here, so we don't access them once the worker thread is launched
	check(IsInGameThread());
	FSubversionSourceControlModule& SubversionSourceControl = FModuleManager::LoadModuleChecked<FSubversionSourceControlModule>( "SubversionSourceControl" );
	FSubversionSourceControlProvider& Provider = SubversionSourceControl.GetProvider();
	RepositoryName = Provider.GetRepositoryName();
	UserName = Provider.GetUserName();
	WorkingCopyRoot = Provider.GetWorkingCopyRoot();
	RepositoryRoot = Provider.GetRepositoryRoot();

	// password needs to be gotten straight from the input UI, its not stored anywhere else
	if(SSubversionSourceControlSettings::GetPassword().Len() > 0)
	{
		Password = SSubversionSourceControlSettings::GetPassword();
	}
}

bool FSubversionSourceControlCommand::DoWork()
{
	bCommandSuccessful = Worker->Execute(*this);
	FPlatformAtomics::InterlockedExchange(&bExecuteProcessed, 1);

	return bCommandSuccessful;
}

void FSubversionSourceControlCommand::Abandon()
{
	FPlatformAtomics::InterlockedExchange(&bExecuteProcessed, 1);
}

void FSubversionSourceControlCommand::DoThreadedWork()
{
	Concurrency = EConcurrency::Asynchronous;
	DoWork();
}
