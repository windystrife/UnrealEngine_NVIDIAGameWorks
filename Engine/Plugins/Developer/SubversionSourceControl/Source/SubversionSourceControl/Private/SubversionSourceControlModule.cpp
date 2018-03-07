// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SubversionSourceControlModule.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "SubversionSourceControlOperations.h"
#include "Features/IModularFeatures.h"

#define LOCTEXT_NAMESPACE "SubversionSourceControl"

template<typename Type>
static TSharedRef<ISubversionSourceControlWorker, ESPMode::ThreadSafe> CreateWorker()
{
	return MakeShareable( new Type() );
}

void FSubversionSourceControlModule::StartupModule()
{
	// Register our operations
	SubversionSourceControlProvider.RegisterWorker( "Connect", FGetSubversionSourceControlWorker::CreateStatic( &CreateWorker<FSubversionConnectWorker> ) );
	SubversionSourceControlProvider.RegisterWorker( "CheckOut", FGetSubversionSourceControlWorker::CreateStatic( &CreateWorker<FSubversionCheckOutWorker> ) );
	SubversionSourceControlProvider.RegisterWorker( "UpdateStatus", FGetSubversionSourceControlWorker::CreateStatic( &CreateWorker<FSubversionUpdateStatusWorker> ) );
	SubversionSourceControlProvider.RegisterWorker( "MarkForAdd", FGetSubversionSourceControlWorker::CreateStatic( &CreateWorker<FSubversionMarkForAddWorker> ) );
	SubversionSourceControlProvider.RegisterWorker( "Delete", FGetSubversionSourceControlWorker::CreateStatic( &CreateWorker<FSubversionDeleteWorker> ) );
	SubversionSourceControlProvider.RegisterWorker( "Revert", FGetSubversionSourceControlWorker::CreateStatic( &CreateWorker<FSubversionRevertWorker> ) );
	SubversionSourceControlProvider.RegisterWorker( "Sync", FGetSubversionSourceControlWorker::CreateStatic( &CreateWorker<FSubversionSyncWorker> ) );
	SubversionSourceControlProvider.RegisterWorker( "CheckIn", FGetSubversionSourceControlWorker::CreateStatic( &CreateWorker<FSubversionCheckInWorker> ) );
	SubversionSourceControlProvider.RegisterWorker( "Copy", FGetSubversionSourceControlWorker::CreateStatic( &CreateWorker<FSubversionCopyWorker> ) );
	SubversionSourceControlProvider.RegisterWorker( "Resolve", FGetSubversionSourceControlWorker::CreateStatic( &CreateWorker<FSubversionResolveWorker> ) );

	// load our settings
	SubversionSourceControlSettings.LoadSettings();

	// Bind our source control provider to the editor
	IModularFeatures::Get().RegisterModularFeature( "SourceControl", &SubversionSourceControlProvider );
}

void FSubversionSourceControlModule::ShutdownModule()
{
	// shut down the provider, as this module is going away
	SubversionSourceControlProvider.Close();

	// unbind provider from editor
	IModularFeatures::Get().UnregisterModularFeature("SourceControl", &SubversionSourceControlProvider);
}

FSubversionSourceControlSettings& FSubversionSourceControlModule::AccessSettings()
{
	return SubversionSourceControlSettings;
}

void FSubversionSourceControlModule::SaveSettings()
{
	if (FApp::IsUnattended() || IsRunningCommandlet())
	{
		return;
	}

	SubversionSourceControlSettings.SaveSettings();
}

IMPLEMENT_MODULE(FSubversionSourceControlModule, SubversionSourceControl);

#undef LOCTEXT_NAMESPACE
