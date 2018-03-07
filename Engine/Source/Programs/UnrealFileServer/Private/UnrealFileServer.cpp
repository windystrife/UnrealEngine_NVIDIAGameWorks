// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UnrealFileServer.h"

#include "DirectoryWatcherModule.h"
#include "RequiredProgramMainCPPInclude.h"
#include "INetworkFileServer.h"
#include "INetworkFileSystemModule.h"
#include "IDirectoryWatcher.h"
#include "SocketSubsystem.h"


IMPLEMENT_APPLICATION(UnrealFileServer, "UnrealFileServer");


/**
 * Application entry point
 *
 * @param	ArgC	Command-line argument count
 * @param	ArgV	Argument strings
 */
INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	// start up the main loop
	GEngineLoop.PreInit(ArgC, ArgV);

	check(GConfig && GConfig->IsReadyForUse());

#if PLATFORM_WINDOWS
	// hide the console window, if desired
	if (FParse::Param(FCommandLine::Get(), TEXT("HIDDEN")))
	{
		ShowWindow(GetConsoleWindow(), SW_HIDE);
	}
#endif

	// start the listening thread
	INetworkFileServer* NetworkFileServer = FModuleManager::LoadModuleChecked<INetworkFileSystemModule>("NetworkFileSystem")
		.CreateNetworkFileServer(false);

	// loop while the server does the rest
	double LastTime = FPlatformTime::Seconds();
	
	while (!GIsRequestingExit)
	{
		// let some time pass
		FPlatformProcess::Sleep(1.0f);

		double Now = FPlatformTime::Seconds();
		float DeltaSeconds = LastTime - Now;
		LastTime = Now;

		// @todo: Put me into an FTicker that is created when the DW module is loaded
		FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::Get().LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
		DirectoryWatcherModule.Get()->Tick(Now - LastTime);

		GLog->FlushThreadedLogs();

		LastTime = Now;
	}

	// shutdown the server
	NetworkFileServer->Shutdown();
	delete NetworkFileServer;

	// Shutdown sockets layer
	ISocketSubsystem::ShutdownAllSystems();

	return 0;
}
