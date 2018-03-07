// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "RHI.h"
#include "ModuleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MessageDialog.h"

FDynamicRHI* PlatformCreateDynamicRHI()
{
	FDynamicRHI* DynamicRHI = nullptr;

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	if (!FPlatformMisc::IsDebuggerPresent())
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("AttachDebugger")))
		{
			// Wait to attach debugger
			do
			{
				FPlatformProcess::Sleep(0);
			}
			while (!FPlatformMisc::IsDebuggerPresent());
		}
	}
#endif

	// command line overrides
	bool bForceSM5  = FParse::Param(FCommandLine::Get(),TEXT("sm5"));
	bool bForceSM4  = FParse::Param(FCommandLine::Get(), TEXT("sm4"));
	bool bForceVulkan = FParse::Param(FCommandLine::Get(), TEXT("vulkan"));
	bool bForceOpenGL = FWindowsPlatformMisc::VerifyWindowsVersion(6, 0) == false || FParse::Param(FCommandLine::Get(), TEXT("opengl")) || FParse::Param(FCommandLine::Get(), TEXT("opengl3")) || FParse::Param(FCommandLine::Get(), TEXT("opengl4"));
	bool bForceD3D10 = FParse::Param(FCommandLine::Get(),TEXT("d3d10")) || FParse::Param(FCommandLine::Get(),TEXT("dx10")) || (bForceSM4 && !bForceVulkan && !bForceOpenGL);
	bool bForceD3D11 = FParse::Param(FCommandLine::Get(),TEXT("d3d11")) || FParse::Param(FCommandLine::Get(),TEXT("dx11")) || (bForceSM5 && !bForceVulkan && !bForceOpenGL);
	bool bForceD3D12 = FParse::Param(FCommandLine::Get(), TEXT("d3d12")) || FParse::Param(FCommandLine::Get(), TEXT("dx12"));
	ERHIFeatureLevel::Type RequestedFeatureLevel = ERHIFeatureLevel::Num;
	int32 Sum = ((bForceD3D12 ? 1 : 0) + (bForceD3D11 ? 1 : 0) + (bForceD3D10 ? 1 : 0) + (bForceOpenGL ? 1 : 0) + (bForceVulkan ? 1 : 0));

	if (bForceSM5 && bForceSM4)
	{
		UE_LOG(LogRHI, Fatal, TEXT("-sm4 and -sm5 are mutually exclusive options, but more than one was specified on the command-line."));
	}

	if (Sum > 1)
	{
		UE_LOG(LogRHI, Fatal,TEXT("-d3d12, -d3d11, -d3d10, -vulkan, and -opengl[3|4] are mutually exclusive options, but more than one was specified on the command-line."));
	}
	else if (Sum == 0)
	{
		// Check the list of targeted shader platforms and decide an RHI based off them
		TArray<FString> TargetedShaderFormats;
		GConfig->GetArray(TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings"), TEXT("TargetedRHIs"), TargetedShaderFormats, GEngineIni);
		if (TargetedShaderFormats.Num() > 0)
		{
			// Pick the first one
			FName ShaderFormatName(*TargetedShaderFormats[0]);
			EShaderPlatform TargetedPlatform = ShaderFormatToLegacyShaderPlatform(ShaderFormatName);
			bForceVulkan = IsVulkanPlatform(TargetedPlatform);
			bForceD3D11 = IsD3DPlatform(TargetedPlatform, false);
			bForceOpenGL = IsOpenGLPlatform(TargetedPlatform);
			RequestedFeatureLevel = GetMaxSupportedFeatureLevel(TargetedPlatform);
		}
	}
	else
	{
		if (bForceSM5)
		{
			RequestedFeatureLevel = ERHIFeatureLevel::SM5;
		}

		if (bForceSM4)
		{
			RequestedFeatureLevel = ERHIFeatureLevel::SM4;
		}
	}

	// Load the dynamic RHI module.
	IDynamicRHIModule* DynamicRHIModule = NULL;

#if defined(SWITCHRHI)
	const bool bForceSwitch = FParse::Param(FCommandLine::Get(), TEXT("switch"));
	// Load the dynamic RHI module.
	if (bForceSwitch)
	{
		#define A(x) #x
		#define B(x) A(x)
		#define SWITCH_RHI_STR B(SWITCHRHI)
		DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT( SWITCH_RHI_STR ));
		if (!DynamicRHIModule->IsSupported())
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("SwitchDynamicRHI", "UnsupportedRHI", "The chosen RHI is not supported"));
			FPlatformMisc::RequestExit(1);
			DynamicRHIModule = NULL;
		}
	}
	else
#endif

	if (bForceOpenGL)
	{
		DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("OpenGLDrv"));

		if (!DynamicRHIModule->IsSupported())
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "RequiredOpenGL", "OpenGL 3.2 is required to run the engine."));
			FPlatformMisc::RequestExit(1);
			DynamicRHIModule = NULL;
		}
	}
	else if (bForceVulkan)
	{
		DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("VulkanRHI"));
		if (!DynamicRHIModule->IsSupported())
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "RequiredVulkan", "Vulkan Driver is required to run the engine."));
			FPlatformMisc::RequestExit(1);
			DynamicRHIModule = NULL;
		}
	}
	else if (bForceD3D12)
	{
		DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("D3D12RHI"));

		if (!DynamicRHIModule->IsSupported())
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "RequiredDX12", "DX12 is not supported on your system. Try running without the -dx12 or -d3d12 command line argument."));
			FPlatformMisc::RequestExit(1);
			DynamicRHIModule = NULL;
		}
		else if (FPlatformProcess::IsApplicationRunning(TEXT("fraps.exe")))
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "UseExpressionEncoder", "Fraps has been known to crash D3D12. Please use Microsoft Expression Encoder instead for capturing."));
		}
	}
	else
	{
		DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("D3D11RHI"));

		if (!DynamicRHIModule->IsSupported())
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "RequiredDX11Feature", "DX11 feature level 10.0 is required to run the engine."));
			FPlatformMisc::RequestExit(1);
			DynamicRHIModule = NULL;
		}
		else if (FPlatformProcess::IsApplicationRunning(TEXT("fraps.exe")))
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "UseExpressionEncoderDX11", "Fraps has been known to crash D3D11. Please use Microsoft Expression Encoder instead for capturing."));
		}
	}

	if (DynamicRHIModule)
	{
		// Create the dynamic RHI.
		DynamicRHI = DynamicRHIModule->CreateRHI(RequestedFeatureLevel);
	}

	return DynamicRHI;
}
