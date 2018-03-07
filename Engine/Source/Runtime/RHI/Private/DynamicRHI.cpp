// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DynamicRHI.cpp: Dynamically bound Render Hardware Interface implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"
#include "Misc/OutputDeviceRedirector.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "RHI.h"
#include "Modules/ModuleManager.h"
#include "GenericPlatform/GenericPlatformDriver.h"
#include "PipelineStateCache.h"

#ifndef PLATFORM_ALLOW_NULL_RHI
	#define PLATFORM_ALLOW_NULL_RHI		0
#endif
// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
#if WITH_NVVOLUMETRICLIGHTING
#include "NVVolumetricLightingRHI.h"
#endif
// NVCHANGE_END: Nvidia Volumetric Lighting
// Globals.
FDynamicRHI* GDynamicRHI = NULL;

static TAutoConsoleVariable<int32> CVarWarnOfBadDrivers(
	TEXT("r.WarnOfBadDrivers"),
	1,
	TEXT("On engine startup we can check the current GPU driver and warn the user about issues and suggest a specific version\n")
	TEXT("The test is fast so this should not cost any performance.\n")
	TEXT(" 0: off\n")
	TEXT(" 1: a message on startup might appear (default)\n")
	TEXT(" 2: Simulating the system has a blacklisted NVIDIA driver (UI should appear)\n")
	TEXT(" 3: Simulating the system has a blacklisted AMD driver (UI should appear)\n")
	TEXT(" 4: Simulating the system has a not blacklisted AMD driver (no UI should appear)\n")
	TEXT(" 5: Simulating the system has a Intel driver (no UI should appear)"),
	ECVF_RenderThreadSafe
	);


void InitNullRHI()
{
	// Use the null RHI if it was specified on the command line, or if a commandlet is running.
	IDynamicRHIModule* DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("NullDrv"));
	// Create the dynamic RHI.
	if ((DynamicRHIModule == 0) || !DynamicRHIModule->IsSupported())
	{
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("DynamicRHI", "NullDrvFailure", "NullDrv failure?"));
		FPlatformMisc::RequestExit(1);
	}

	GDynamicRHI = DynamicRHIModule->CreateRHI();
	GDynamicRHI->Init();
	GRHICommandList.GetImmediateCommandList().SetContext(GDynamicRHI->RHIGetDefaultContext());
	GRHICommandList.GetImmediateAsyncComputeCommandList().SetComputeContext(GDynamicRHI->RHIGetDefaultAsyncComputeContext());
	GUsingNullRHI = true;
	GRHISupportsTextureStreaming = false;
}

#if PLATFORM_WINDOWS
static void RHIDetectAndWarnOfBadDrivers()
{
	int32 CVarValue = CVarWarnOfBadDrivers.GetValueOnGameThread();

	if(!GIsRHIInitialized || !CVarValue || GRHIVendorId == 0)
	{
		return;
	}

	FGPUDriverInfo DriverInfo;

	// later we should make the globals use the struct directly
	DriverInfo.VendorId = GRHIVendorId;
	DriverInfo.DeviceDescription = GRHIAdapterName;
	DriverInfo.ProviderName = TEXT("Unknown");
	DriverInfo.InternalDriverVersion = GRHIAdapterInternalDriverVersion;
	DriverInfo.UserDriverVersion = GRHIAdapterUserDriverVersion;
	DriverInfo.DriverDate = GRHIAdapterDriverDate;


#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// for testing
	if(CVarValue == 2)
	{
		DriverInfo.SetNVIDIA();
		DriverInfo.DeviceDescription = TEXT("Test NVIDIA (bad)");
		DriverInfo.UserDriverVersion = TEXT("346.43");
		DriverInfo.InternalDriverVersion = TEXT("9.18.134.643");
		DriverInfo.DriverDate = TEXT("01-01-1900");
	}
	else if(CVarValue == 3)
	{
		DriverInfo.SetAMD();
		DriverInfo.DeviceDescription = TEXT("Test AMD (bad)");
		DriverInfo.UserDriverVersion = TEXT("Test Catalyst Version");
		DriverInfo.InternalDriverVersion = TEXT("13.152.1.1000");
		DriverInfo.DriverDate = TEXT("09-10-13");
	}
	else if(CVarValue == 4)
	{
		DriverInfo.SetAMD();
		DriverInfo.DeviceDescription = TEXT("Test AMD (good)");
		DriverInfo.UserDriverVersion = TEXT("Test Catalyst Version");
		DriverInfo.InternalDriverVersion = TEXT("15.30.1025.1001");
		DriverInfo.DriverDate = TEXT("01-01-16");
	}
	else if(CVarValue == 5)
	{
		DriverInfo.SetIntel();
		DriverInfo.DeviceDescription = TEXT("Test Intel (good)");
		DriverInfo.UserDriverVersion = TEXT("Test Intel Version");
		DriverInfo.InternalDriverVersion = TEXT("8.15.10.2302");
		DriverInfo.DriverDate = TEXT("01-01-15");
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	FGPUHardware DetectedGPUHardware(DriverInfo);

	if (DriverInfo.IsValid())
	{
		FBlackListEntry BlackListEntry = DetectedGPUHardware.FindDriverBlacklistEntry();

		if (BlackListEntry.IsValid())
		{
			bool bLatestBlacklisted = DetectedGPUHardware.IsLatestBlacklisted();

			// Note: we don't localize the vendor's name.
			FString VendorString = DriverInfo.ProviderName;
			if (DriverInfo.IsNVIDIA())
			{
				VendorString = TEXT("NVIDIA");
			}
			else if (DriverInfo.IsAMD())
			{
				VendorString = TEXT("AMD");
			}
			else if (DriverInfo.IsIntel())
			{
				VendorString = TEXT("Intel");
			}

			// format message box UI
			FFormatNamedArguments Args;
			Args.Add(TEXT("AdapterName"), FText::FromString(DriverInfo.DeviceDescription));
			Args.Add(TEXT("Vendor"), FText::FromString(VendorString));
			Args.Add(TEXT("RecommendedVer"), FText::FromString(DetectedGPUHardware.GetSuggestedDriverVersion()));
			Args.Add(TEXT("InstalledVer"), FText::FromString(DriverInfo.UserDriverVersion));

			// this message can be suppressed with r.WarnOfBadDrivers=0
			FText LocalizedMsg;
			if (bLatestBlacklisted)
			{
				LocalizedMsg = FText::Format(NSLOCTEXT("MessageDialog", "LatestVideoCardDriverIssueReport","The latest version of the {Vendor} graphics driver has known issues.\nPlease install the recommended driver version.\n\n{AdapterName}\nInstalled: {InstalledVer}\nRecommended: {RecommendedVer}"),Args);
			}
			else
			{
				LocalizedMsg = FText::Format(NSLOCTEXT("MessageDialog", "VideoCardDriverIssueReport","The installed version of the {Vendor} graphics driver has known issues.\nPlease update to the latest driver version.\n\n{AdapterName}\nInstalled: {InstalledVer}\nRecommended: {RecommendedVer}"),Args);
			}

			FPlatformMisc::MessageBoxExt(EAppMsgType::Ok,
				*LocalizedMsg.ToString(),
				*NSLOCTEXT("MessageDialog", "TitleVideoCardDriverIssue", "WARNING: Known issues with graphics driver").ToString());
		}
	}
}
#endif // PLATFORM_WINDOWS

void RHIInit(bool bHasEditorToken)
{
	if(!GDynamicRHI)
	{
		GRHICommandList.LatchBypass(); // read commandline for bypass flag

		if (!FApp::CanEverRender())
		{
			InitNullRHI();
		}
		else
		{
			LLM_SCOPE(ELLMTag::RHIMisc);

			GDynamicRHI = PlatformCreateDynamicRHI();
			if (GDynamicRHI)
			{
				GDynamicRHI->Init();
				GRHICommandList.GetImmediateCommandList().SetContext(GDynamicRHI->RHIGetDefaultContext());
				GRHICommandList.GetImmediateAsyncComputeCommandList().SetComputeContext(GDynamicRHI->RHIGetDefaultAsyncComputeContext());
			}
#if PLATFORM_ALLOW_NULL_RHI
			else
			{
				// If the platform supports doing so, fall back to the NULL RHI on failure
				InitNullRHI();
			}
#endif
		}

		check(GDynamicRHI);
	}

#if PLATFORM_WINDOWS
	RHIDetectAndWarnOfBadDrivers();
#endif
}

void RHIPostInit(const TArray<uint32>& InPixelFormatByteWidth)
{
	check(GDynamicRHI);
	GDynamicRHI->InitPixelFormatInfo(InPixelFormatByteWidth);
	GDynamicRHI->PostInit();
	// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
#if WITH_NVVOLUMETRICLIGHTING
	if (!GNVVolumetricLightingRHI)
	{
		GNVVolumetricLightingRHI = CreateNVVolumetricLightingRHI();
		if (GNVVolumetricLightingRHI)
		{
			if (!GNVVolumetricLightingRHI->Init())
			{
				delete GNVVolumetricLightingRHI;
				GNVVolumetricLightingRHI = NULL;
			}
		}
	}
#endif
	// NVCHANGE_END: Nvidia Volumetric Lighting
}

void RHIExit()
{
	// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
#if WITH_NVVOLUMETRICLIGHTING
	if (GNVVolumetricLightingRHI)
	{
		GNVVolumetricLightingRHI->Shutdown();
		delete GNVVolumetricLightingRHI;
		GNVVolumetricLightingRHI = NULL;
	}
#endif
	// NVCHANGE_END: Nvidia Volumetric Lighting
	if ( !GUsingNullRHI && GDynamicRHI != NULL )
	{
		// Clean up all cached pipelines
		ClearPipelineCache();

		// Destruct the dynamic RHI.
		GDynamicRHI->Shutdown();
		delete GDynamicRHI;
		GDynamicRHI = NULL;
	}
}


static void BaseRHISetGPUCaptureOptions(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() > 0)
	{
		const bool bEnabled = Args[0].ToBool();
		GDynamicRHI->EnableIdealGPUCaptureOptions(bEnabled);
	}
	else
	{
		UE_LOG(LogRHI, Display, TEXT("Usage: r.PS4.EnableCaptureMode 0 or r.PS4.EnableCaptureMode 1"));
	}
}

static FAutoConsoleCommandWithWorldAndArgs GBaseRHISetGPUCaptureOptions(
	TEXT("r.RHISetGPUCaptureOptions"),
	TEXT("Utility function to change multiple CVARs useful when profiling or debugging GPU rendering. Setting to 1 or 0 will guarantee all options are in the appropriate state.\n")
	TEXT("r.rhithread.enable, r.rhicmdbypass, r.showmaterialdrawevents, toggledrawevents\n")
	TEXT("Platform RHI's may implement more feature toggles."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BaseRHISetGPUCaptureOptions)
	);

void FDynamicRHI::EnableIdealGPUCaptureOptions(bool bEnabled)
{
	static IConsoleVariable* RHICmdBypassVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.rhicmdbypass"));
	static IConsoleVariable* ShowMaterialDrawEventVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ShowMaterialDrawEvents"));	
	static IConsoleObject* RHIThreadEnableObj = IConsoleManager::Get().FindConsoleObject(TEXT("r.RHIThread.Enable"));
	static IConsoleCommand* RHIThreadEnableCommand = RHIThreadEnableObj ? RHIThreadEnableObj->AsCommand() : nullptr;

	const bool bShouldEnableDrawEvents = bEnabled;
	const bool bShouldEnableMaterialDrawEvents = bEnabled;
	const bool bShouldEnableRHIThread = !bEnabled;
	const bool bShouldRHICmdBypass = bEnabled;	

	const bool bDrawEvents = GEmitDrawEvents != 0;
	const bool bMaterialDrawEvents = ShowMaterialDrawEventVar ? ShowMaterialDrawEventVar->GetInt() != 0 : false;
	const bool bRHIThread = IsRunningRHIInSeparateThread();
	const bool bRHIBypass = RHICmdBypassVar ? RHICmdBypassVar->GetInt() != 0 : false;

	UE_LOG(LogRHI, Display, TEXT("Setting GPU Capture Options: %i"), bEnabled ? 1 : 0);
	if (bShouldEnableDrawEvents != bDrawEvents)
	{
		UE_LOG(LogRHI, Display, TEXT("Toggling draw events: %i"), bShouldEnableDrawEvents ? 1 : 0);
		GEmitDrawEvents = bShouldEnableDrawEvents;
	}
	if (bShouldEnableMaterialDrawEvents != bMaterialDrawEvents && ShowMaterialDrawEventVar)
	{
		UE_LOG(LogRHI, Display, TEXT("Toggling showmaterialdrawevents: %i"), bShouldEnableDrawEvents ? 1 : 0);
		ShowMaterialDrawEventVar->Set(bShouldEnableDrawEvents ? 1 : 0);		
	}
	if (bRHIThread != bShouldEnableRHIThread && RHIThreadEnableCommand)
	{
		UE_LOG(LogRHI, Display, TEXT("Toggling rhi thread: %i"), bShouldEnableRHIThread ? 1 : 0);
		TArray<FString> Args;
		Args.Add(FString::Printf(TEXT("%i"), bShouldEnableRHIThread ? 1 : 0));
		RHIThreadEnableCommand->Execute(Args, nullptr, *GLog);
	}
	if (bRHIBypass != bShouldRHICmdBypass && RHICmdBypassVar)
	{
		UE_LOG(LogRHI, Display, TEXT("Toggling rhi bypass: %i"), bEnabled ? 1 : 0);
		RHICmdBypassVar->Set(bShouldRHICmdBypass ? 1 : 0, ECVF_SetByConsole);		
	}	
}
