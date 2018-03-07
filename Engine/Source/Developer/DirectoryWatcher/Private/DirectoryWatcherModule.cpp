// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DirectoryWatcherModule.h"
#include "DirectoryWatcherPrivate.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE( FDirectoryWatcherModule, DirectoryWatcher );
DEFINE_LOG_CATEGORY(LogDirectoryWatcher);

void FDirectoryWatcherModule::StartupModule()
{
	DirectoryWatcher = new FDirectoryWatcher();
}


void FDirectoryWatcherModule::ShutdownModule()
{
	if (DirectoryWatcher != NULL)
	{
		delete DirectoryWatcher;
		DirectoryWatcher = NULL;
	}
}

IDirectoryWatcher* FDirectoryWatcherModule::Get()
{
	return DirectoryWatcher;
}
