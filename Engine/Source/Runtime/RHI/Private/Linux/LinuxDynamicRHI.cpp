// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"
#include "RHI.h"
#include "Modules/ModuleManager.h"
#include "Misc/ConfigCacheIni.h"

FDynamicRHI* PlatformCreateDynamicRHI()
{
	const bool bForceVulkan = FParse::Param(FCommandLine::Get(), TEXT("vulkan"));
	FDynamicRHI* DynamicRHI = nullptr;
	IDynamicRHIModule* DynamicRHIModule = nullptr;
	if(bForceVulkan)
	{
		DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("VulkanRHI"));
		if (!DynamicRHIModule->IsSupported())
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("LinuxDynamicRHI", "RequiredVulkan", "Vulkan Driver is required to run the engine."));
			FPlatformMisc::RequestExit(1);
			DynamicRHIModule = nullptr;
		}
	}
	else
	{
		// Load the dynamic RHI module.
		DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("OpenGLDrv"));
		if (!DynamicRHIModule->IsSupported())
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("LinuxDynamicRHI", "RequiredOpenGL", "OpenGL 3.2 is required to run the engine."));
			FPlatformMisc::RequestExit(1);
			DynamicRHIModule = nullptr;
		}
	}

	// default to SM4 for safety's sake
	ERHIFeatureLevel::Type RequestedFeatureLevel = ERHIFeatureLevel::SM4;

	// Check the list of targeted shader platforms and decide an RHI based off them
	TArray<FString> TargetedShaderFormats;
	GConfig->GetArray(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), TEXT("TargetedRHIs"), TargetedShaderFormats, GEngineIni);
	if (TargetedShaderFormats.Num() > 0)
	{
		// Pick the first one
		FName ShaderFormatName(*TargetedShaderFormats[0]);
		EShaderPlatform TargetedPlatform = ShaderFormatToLegacyShaderPlatform(ShaderFormatName);
		RequestedFeatureLevel = GetMaxSupportedFeatureLevel(TargetedPlatform);
	}

	// Create the dynamic RHI.
	if (DynamicRHIModule)
	{
		DynamicRHI = DynamicRHIModule->CreateRHI(RequestedFeatureLevel);
	}

	return DynamicRHI;
}
