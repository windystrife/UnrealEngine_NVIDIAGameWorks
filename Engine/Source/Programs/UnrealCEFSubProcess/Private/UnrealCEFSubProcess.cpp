// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//
// UnrealCEFSubProcess.cpp : Defines the entry point for the browser sub process
//

#include "UnrealCEFSubProcess.h"

#include "RequiredProgramMainCPPInclude.h"

#include "CEF3Utils.h"
#include "UnrealCEFSubProcessApp.h"

//#define DEBUG_USING_CONSOLE	0

IMPLEMENT_APPLICATION(UnrealCEFSubProcess, "UnrealCEFSubProcess")

#if WITH_CEF3
int32 RunCEFSubProcess(const CefMainArgs& MainArgs)
{
	CEF3Utils::LoadCEF3Modules();

	// Create an App object for handling various render process events, such as message passing
    CefRefPtr<CefApp> App(new FUnrealCEFSubProcessApp);

	// Execute the sub-process logic. This will block until the sub-process should exit.
	int32 Result = CefExecuteProcess(MainArgs, App, nullptr);
	CEF3Utils::UnloadCEF3Modules();
	return Result;
}
#endif
