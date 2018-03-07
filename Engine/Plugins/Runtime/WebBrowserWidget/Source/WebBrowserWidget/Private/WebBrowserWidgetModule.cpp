// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WebBrowserWidgetModule.h"
#include "Modules/ModuleManager.h"

//////////////////////////////////////////////////////////////////////////
// FWebBrowserWidgetModule

class FWebBrowserWidgetModule : public IWebBrowserWidgetModule
{
public:
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};

//////////////////////////////////////////////////////////////////////////

IMPLEMENT_MODULE(FWebBrowserWidgetModule, WebBrowserWidget);
