// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CEF/CEFBrowserApp.h"

#if WITH_CEF3

FCEFBrowserApp::FCEFBrowserApp()
{
}

void FCEFBrowserApp::OnBeforeChildProcessLaunch(CefRefPtr<CefCommandLine> CommandLine)
{
}

void FCEFBrowserApp::OnBeforeCommandLineProcessing(const CefString& ProcessType, CefRefPtr< CefCommandLine > CommandLine)
{
	CommandLine->AppendSwitch("disable-gpu");
	CommandLine->AppendSwitch("disable-gpu-compositing");
#if !PLATFORM_MAC
	CommandLine->AppendSwitch("enable-begin-frame-scheduling");
#endif
}

void FCEFBrowserApp::OnRenderProcessThreadCreated(CefRefPtr<CefListValue> ExtraInfo)
{
	RenderProcessThreadCreatedDelegate.ExecuteIfBound(ExtraInfo);
}

#endif
