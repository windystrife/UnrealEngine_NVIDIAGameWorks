// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WebBrowserModule.h"
#include "WebBrowserLog.h"
#include "WebBrowserSingleton.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#if WITH_CEF3
#	include "CEF3Utils.h"
#endif

DEFINE_LOG_CATEGORY(LogWebBrowser);

static FWebBrowserSingleton* WebBrowserSingleton = nullptr;

FWebBrowserInitSettings::FWebBrowserInitSettings()
	: ProductVersion(FString::Printf(TEXT("%s/%s UnrealEngine/%s"), FApp::GetProjectName(), FApp::GetBuildVersion(), *FEngineVersion::Current().ToString()))
{
}

class FWebBrowserModule : public IWebBrowserModule
{
private:
	// IModuleInterface Interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

public:
	virtual IWebBrowserSingleton* GetSingleton() override;
	virtual bool CustomInitialize(const FWebBrowserInitSettings& WebBrowserInitSettings) override;
};

IMPLEMENT_MODULE( FWebBrowserModule, WebBrowser );

void FWebBrowserModule::StartupModule()
{
#if WITH_CEF3
	CEF3Utils::LoadCEF3Modules();
#endif
}

void FWebBrowserModule::ShutdownModule()
{
	if (WebBrowserSingleton != nullptr)
	{
		delete WebBrowserSingleton;
		WebBrowserSingleton = nullptr;
	}

#if WITH_CEF3
	CEF3Utils::UnloadCEF3Modules();
#endif
}

bool FWebBrowserModule::CustomInitialize(const FWebBrowserInitSettings& WebBrowserInitSettings)
{
	if (WebBrowserSingleton == nullptr)
	{
		WebBrowserSingleton = new FWebBrowserSingleton(WebBrowserInitSettings);
		return true;
	}
	return false;
}

IWebBrowserSingleton* FWebBrowserModule::GetSingleton()
{
	if (WebBrowserSingleton == nullptr)
	{
		WebBrowserSingleton = new FWebBrowserSingleton(FWebBrowserInitSettings());
	}
	return WebBrowserSingleton;
}
