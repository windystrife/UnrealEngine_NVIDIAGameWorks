// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "RHI.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "Misc/MessageDialog.h"
#include "ModuleManager.h"

int32 GAppleMetalEnabled = 1;
static FAutoConsoleVariableRef CVarMacMetalEnabled(
	TEXT("rhi.Apple.UseMetal"),
	GAppleMetalEnabled,
	TEXT("If set to true uses Metal when available rather than OpenGL as the graphics API. (Default: True)"));

int32 GAppleOpenGLDisabled = 0;
static FAutoConsoleVariableRef CVarMacOpenGLDisabled(
	TEXT("rhi.Apple.OpenGLDisabled"),
	GAppleOpenGLDisabled,
	TEXT("If set, OpenGL RHI will not be used if Metal is not available. Instead, a dialog box explaining that the hardware requirements are not met will appear. (Default: False)"));

FDynamicRHI* PlatformCreateDynamicRHI()
{
	SCOPED_AUTORELEASE_POOL;

	FDynamicRHI* DynamicRHI = NULL;
	IDynamicRHIModule* DynamicRHIModule = NULL;

	bool const bIsMetalSupported = FPlatformMisc::HasPlatformFeature(TEXT("Metal"));
	bool const bAllowMetal = (GAppleMetalEnabled && bIsMetalSupported);
	bool const bAllowOpenGL = !GAppleOpenGLDisabled && !PLATFORM_MAC;

#if PLATFORM_MAC
	bool bForceMetal = bAllowMetal && (FParse::Param(FCommandLine::Get(),TEXT("metal")) || FParse::Param(FCommandLine::Get(),TEXT("metalsm5")));
	bool bForceOpenGL = false; // OpenGL is no longer supported on Mac
#else
	bool bForceMetal = bAllowMetal && (FParse::Param(FCommandLine::Get(),TEXT("metal")) || FParse::Param(FCommandLine::Get(),TEXT("metalmrt")));
	bool bForceOpenGL = bAllowOpenGL && FParse::Param(FCommandLine::Get(),TEXT("es2"));
#endif

	ERHIFeatureLevel::Type RequestedFeatureLevel = ERHIFeatureLevel::Num;
	int32 Sum = ((bForceMetal ? 1 : 0) + (bForceOpenGL ? 1 : 0));
	if (Sum > 1)
	{
		UE_LOG(LogRHI, Fatal,TEXT("-metal, -metalsm5, and -opengl are mutually exclusive options, but more than one was specified on the command-line."));
	}
	else if (Sum == 0)
	{
		// Check the list of targeted shader platforms and decide an RHI based off them
		TArray<FString> TargetedShaderFormats;
#if PLATFORM_MAC
		GConfig->GetArray(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("TargetedRHIs"), TargetedShaderFormats, GEngineIni);
#else
		bool bSupportsMetalMRT = false;
		GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetalMRT"), bSupportsMetalMRT, GEngineIni);
		if (bSupportsMetalMRT)
		{
			TargetedShaderFormats.Add(LegacyShaderPlatformToShaderFormat(SP_METAL_MRT).ToString());
		}
		
		bool bSupportsMetal = false;
		GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetal"), bSupportsMetal, GEngineIni);
		if (bSupportsMetal)
		{
			TargetedShaderFormats.Add(LegacyShaderPlatformToShaderFormat(SP_METAL).ToString());
		}
	
		bool bSupportsOpenGLES2 = false;
		GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsOpenGLES2"), bSupportsOpenGLES2, GEngineIni);
		if (bSupportsOpenGLES2)
		{
			TargetedShaderFormats.Add(LegacyShaderPlatformToShaderFormat(SP_OPENGL_ES2_IOS).ToString());
		}
#endif
		// Metal is not always available, so don't assume that we can use the first platform
		for (FString Name : TargetedShaderFormats)
		{
			FName ShaderFormatName(*Name);
			EShaderPlatform TargetedPlatform = ShaderFormatToLegacyShaderPlatform(ShaderFormatName);
			
			// Instead use the first platform that *could* work
			if (bAllowMetal == true || !IsMetalPlatform(TargetedPlatform))
			{
				bForceMetal = IsMetalPlatform(TargetedPlatform);
				bForceOpenGL = IsOpenGLPlatform(TargetedPlatform) && !PLATFORM_MAC;
				RequestedFeatureLevel = GetMaxSupportedFeatureLevel(TargetedPlatform);
				break;
			}
		}
	}

	// Load the dynamic RHI module.
	if (bForceMetal)
	{
		DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("MetalRHI"));
		
		if (Sum == 1)
		{
#if PLATFORM_MAC
			if (FParse::Param(FCommandLine::Get(),TEXT("metal")))
			{
				RequestedFeatureLevel = ERHIFeatureLevel::SM4;
			}
			else if (FParse::Param(FCommandLine::Get(),TEXT("metalsm5")) || FParse::Param(FCommandLine::Get(),TEXT("metalmrt")))
			{
				RequestedFeatureLevel = ERHIFeatureLevel::SM5;
			}
#else
			if (FParse::Param(FCommandLine::Get(),TEXT("metal")))
			{
				RequestedFeatureLevel = ERHIFeatureLevel::ES3_1;
			}
			else if (FParse::Param(FCommandLine::Get(),TEXT("metalmrt")))
			{
				RequestedFeatureLevel = ERHIFeatureLevel::SM5;
			}
#endif
		}
	}
	else if (bForceOpenGL)
	{
		DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("OpenGLDrv"));
		if (Sum == 1)
		{
#if PLATFORM_MAC
			RequestedFeatureLevel = ERHIFeatureLevel::SM4;
#else
			RequestedFeatureLevel = ERHIFeatureLevel::ES2;
#endif
		}
	}
	else
	{
		FText Title = NSLOCTEXT("AppleDynamicRHI", "OpenGLNotSupportedTitle","OpenGL Not Supported");
#if PLATFORM_MAC
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("MacPlatformCreateDynamicRHI", "OpenGLNotSupported.", "You must have a Metal compatible graphics card and be running Mac OS X 10.11.6 or later to launch this process."), &Title);
#else
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("AppleDynamicRHI", "OpenGLNotSupported.", "You must have a Metal compatible iOS or tvOS device with iOS 8 or later to launch this app."), &Title);
#endif
		FPlatformMisc::RequestExit(true);
	}
	
	// Create the dynamic RHI.
	DynamicRHI = DynamicRHIModule->CreateRHI(RequestedFeatureLevel);
	return DynamicRHI;
}
