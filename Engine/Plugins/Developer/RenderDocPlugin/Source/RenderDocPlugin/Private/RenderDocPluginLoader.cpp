// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "RenderDocPluginLoader.h"
#include "RenderDocPluginModule.h"
#include "RenderDocPluginSettings.h"

#include "Internationalization.h"

#include "Developer/DesktopPlatform/public/DesktopPlatformModule.h"
#include "AllowWindowsPlatformTypes.h"
#include "ConfigCacheIni.h"
#include "RHI.h"

#define LOCTEXT_NAMESPACE "RenderDocPlugin" 

static TAutoConsoleVariable<int32> CVarRenderDocEnableCrashHandler(
	TEXT("renderdoc.EnableCrashHandler"),
	0,
	TEXT("0 - Crash handling is completely delegated to the engine. ")
	TEXT("1 - The RenderDoc crash handler will be used (Only use this if you know the problem is with RenderDoc and you want to notify the RenderDoc developers!)."));

static TAutoConsoleVariable<FString> CVarRenderDocBinaryPath(
	TEXT("renderdoc.BinaryPath"),
	TEXT(""),
	TEXT("Path to the main RenderDoc executable to use."));

static void* LoadAndCheckRenderDocLibrary(FRenderDocPluginLoader::RENDERDOC_API_CONTEXT*& RenderDocAPI, const FString& RenderdocPath)
{
	check(nullptr == RenderDocAPI);

	if (RenderdocPath.IsEmpty())
	{
		return(nullptr);
	}

	FString PathToRenderDocDLL = FPaths::Combine(*RenderdocPath, *FString("renderdoc.dll"));
	if (!FPaths::FileExists(PathToRenderDocDLL))
	{
		UE_LOG(RenderDocPlugin, Warning, TEXT("unable to locate RenderDoc library at: %s"), *PathToRenderDocDLL);
		return(nullptr);
	}

	UE_LOG(RenderDocPlugin, Log, TEXT("a RenderDoc library has been located at: %s"), *PathToRenderDocDLL);

	void* RenderDocDLL = FPlatformProcess::GetDllHandle(*PathToRenderDocDLL);
	if (RenderDocDLL == nullptr)
	{
		UE_LOG(RenderDocPlugin, Warning, TEXT("unable to dynamically load RenderDoc library"));
		return(nullptr);
	}

	pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)FPlatformProcess::GetDllExport(RenderDocDLL, TEXT("RENDERDOC_GetAPI"));
	if (RENDERDOC_GetAPI == nullptr)
	{
		UE_LOG(RenderDocPlugin, Warning, TEXT("unable to obtain 'RENDERDOC_GetAPI' function from 'renderdoc.dll'. You are likely using an incompatible version of RenderDoc."), *PathToRenderDocDLL);
		FPlatformProcess::FreeDllHandle(RenderDocDLL);
		return(nullptr);
	}

	// Version checking and reporting
	if (0 == RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_0_0, (void**)&RenderDocAPI))
	{
		UE_LOG(RenderDocPlugin, Warning, TEXT("unable to initialize RenderDoc library due to API incompatibility (plugin requires eRENDERDOC_API_Version_1_0_0)."), *PathToRenderDocDLL);
		FPlatformProcess::FreeDllHandle(RenderDocDLL);
		return(nullptr);
	}

	//Unregister crash handler unless the user has enabled it. This is to avoid sending unneccesary crash reports to Baldur.
	if (!CVarRenderDocEnableCrashHandler.GetValueOnAnyThread())
	{
		RenderDocAPI->UnloadCrashHandler();
	}

	int MajorVersion(0), MinorVersion(0), PatchVersion(0);
	RenderDocAPI->GetAPIVersion(&MajorVersion, &MinorVersion, &PatchVersion);
	UE_LOG(RenderDocPlugin, Log, TEXT("RenderDoc library has been loaded (RenderDoc API v%i.%i.%i)."), MajorVersion, MinorVersion, PatchVersion);

	return(RenderDocDLL);
}

void FRenderDocPluginLoader::Initialize()
{
	RenderDocDLL = nullptr;
	RenderDocAPI = nullptr;

	if (GUsingNullRHI)
	{
		// THIS WILL NEVER TRIGGER because of a sort of chicken-and-egg problem: RenderDoc Loader is a PostConfigInit
		// plugin, and GUsingNullRHI is only initialized properly between PostConfigInit and PreLoadingScreen phases.
		// (nevertheless, keep this comment around for future iterations of UE4)
		UE_LOG(RenderDocPlugin, Warning, TEXT("this plugin will not be loaded because a null RHI (Cook Server, perhaps) is being used."));
		return;
	}
	
	// Look for a renderdoc.dll somewhere in the system:
	UE_LOG(RenderDocPlugin, Log, TEXT("locating RenderDoc library (renderdoc.dll)..."));

	// 1) Check the Game configuration files. Since we are so early in the loading phase, we first need to load the cvars since they're not loaded at this point:
	ApplyCVarSettingsFromIni(TEXT("/Script/RenderDocPlugin.RenderDocPluginSettings"), *GEngineIni, ECVF_SetByProjectSetting);
	RenderDocDLL = LoadAndCheckRenderDocLibrary(RenderDocAPI, CVarRenderDocBinaryPath.GetValueOnAnyThread());

	// 2) Check for a RenderDoc system installation in the registry:
	if (RenderDocDLL == nullptr)
	{
		FString RenderdocPath;
		FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Classes\\RenderDoc.RDCCapture.1\\DefaultIcon\\"), TEXT(""), RenderdocPath);
		RenderdocPath = FPaths::GetPath(RenderdocPath);
		RenderDocDLL = LoadAndCheckRenderDocLibrary(RenderDocAPI, RenderdocPath);
		if (RenderDocDLL)
		{
			CVarRenderDocBinaryPath.AsVariable()->Set(*RenderdocPath, ECVF_SetByProjectSetting);
		}
	}

	// 3) Check for a RenderDoc custom installation by prompting the user:
	if (RenderDocDLL == nullptr)
	{
		//Renderdoc does not seem to be installed, but it might be built from source or downloaded by archive, 
		//so prompt the user to navigate to the main exe file
		UE_LOG(RenderDocPlugin, Log, TEXT("RenderDoc library not found; provide a custom installation location..."));
		FString RenderdocPath;
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		if (DesktopPlatform)
		{
			FString Filter = TEXT("Renderdoc executable|renderdocui.exe");
			TArray<FString> OutFiles;
			if (DesktopPlatform->OpenFileDialog(nullptr, TEXT("Locate main Renderdoc executable..."), TEXT(""), TEXT(""), Filter, EFileDialogFlags::None, OutFiles))
			{
				RenderdocPath = OutFiles[0];
			}
		}
		RenderdocPath = FPaths::GetPath(RenderdocPath);
		RenderDocDLL = LoadAndCheckRenderDocLibrary(RenderDocAPI, RenderdocPath);
		if (RenderDocDLL)
		{
			CVarRenderDocBinaryPath.AsVariable()->Set(*RenderdocPath, ECVF_SetByProjectSetting);
		}
	}

	// 4) All bets are off; aborting...
	if (RenderDocDLL == nullptr)
	{
		UE_LOG(RenderDocPlugin, Error, TEXT("unable to initialize the plugin because no RenderDoc libray has been located."));
		return;
	}

	UE_LOG(RenderDocPlugin, Log, TEXT("plugin has been loaded successfully."));
}

void FRenderDocPluginLoader::Release()
{
	if (GUsingNullRHI)
	{
		return;
	}

	if (RenderDocDLL)
	{
		FPlatformProcess::FreeDllHandle(RenderDocDLL);
		RenderDocDLL = nullptr;
	}

	UE_LOG(RenderDocPlugin, Log, TEXT("plugin has been unloaded."));
}

void* FRenderDocPluginLoader::GetRenderDocLibrary()
{
	void* RenderDocDLL = nullptr;
	FString PathToRenderDocDLL = FPaths::Combine(*CVarRenderDocBinaryPath.GetValueOnAnyThread(), *FString("renderdoc.dll"));
	RenderDocDLL = FPlatformProcess::GetDllHandle(*PathToRenderDocDLL);

	return RenderDocDLL;
}

#undef LOCTEXT_NAMESPACE 
#include "HideWindowsPlatformTypes.h"
