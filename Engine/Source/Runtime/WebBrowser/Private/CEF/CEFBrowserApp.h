// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_CEF3

#if PLATFORM_WINDOWS
#include "WindowsHWrapper.h"
#include "AllowWindowsPlatformTypes.h"
#include "AllowWindowsPlatformAtomics.h"
#endif
#pragma push_macro("OVERRIDE")
#undef OVERRIDE // cef headers provide their own OVERRIDE macro
THIRD_PARTY_INCLUDES_START
	#include "include/cef_app.h"
THIRD_PARTY_INCLUDES_END
#pragma pop_macro("OVERRIDE")
#if PLATFORM_WINDOWS
#include "HideWindowsPlatformAtomics.h"
#include "HideWindowsPlatformTypes.h"
#endif

/**
 * Implements CEF App and other Process level interfaces
 */
class FCEFBrowserApp : public CefApp,
	public CefBrowserProcessHandler
{
public:

	/**
	 * Default Constructor
	 */
	FCEFBrowserApp();

	/** A delegate this is invoked when an existing browser requests creation of a new browser window. */
	DECLARE_DELEGATE_OneParam(FOnRenderProcessThreadCreated, CefRefPtr<CefListValue> /*ExtraInfo*/);
	virtual FOnRenderProcessThreadCreated& OnRenderProcessThreadCreated()
	{
		return RenderProcessThreadCreatedDelegate;
	}

private:
	// CefApp methods.
	virtual CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }
	virtual void OnBeforeCommandLineProcessing(const CefString& ProcessType, CefRefPtr< CefCommandLine > CommandLine) override;
	// CefBrowserProcessHandler methods:
	virtual void OnBeforeChildProcessLaunch(CefRefPtr<CefCommandLine> CommandLine) override;
	virtual void OnRenderProcessThreadCreated(CefRefPtr<CefListValue> ExtraInfo) override;

	FOnRenderProcessThreadCreated RenderProcessThreadCreatedDelegate;

	// Include the default reference counting implementation.
	IMPLEMENT_REFCOUNTING(FCEFBrowserApp);
};
#endif
