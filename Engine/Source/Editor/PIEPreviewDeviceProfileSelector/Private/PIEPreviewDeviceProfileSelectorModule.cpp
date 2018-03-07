// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PIEPreviewDeviceProfileSelectorModule.h"
#include "FileHelper.h"
#include "JsonObject.h"
#include "JsonReader.h"
#include "JsonSerializer.h"
#include "PIEPreviewDeviceSpecification.h"
#include "JsonObjectConverter.h"
#include "MaterialShaderQualitySettings.h"
#include "RHI.h"
#include "TabManager.h"
#include "CoreGlobals.h"
#include "ModuleManager.h"
#include "ConfigCacheIni.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPIEPreviewDevice, Log, All); 
DEFINE_LOG_CATEGORY(LogPIEPreviewDevice);
IMPLEMENT_MODULE(FPIEPreviewDeviceProfileSelectorModule, PIEPreviewDeviceProfileSelector);

void FPIEPreviewDeviceProfileSelectorModule::StartupModule()
{
}

void FPIEPreviewDeviceProfileSelectorModule::ShutdownModule()
{
}

FString const FPIEPreviewDeviceProfileSelectorModule::GetRuntimeDeviceProfileName()
{
	if (!bInitialized)
	{
		InitPreviewDevice();
	}

	return DeviceProfile;
}

void FPIEPreviewDeviceProfileSelectorModule::InitPreviewDevice()
{
	bInitialized = true;

	if (ReadDeviceSpecification())
	{
		ERHIFeatureLevel::Type PreviewFeatureLevel = ERHIFeatureLevel::Num;
		switch (DeviceSpecs->DevicePlatform)
		{
			case EPIEPreviewDeviceType::Android:
			{
				IDeviceProfileSelectorModule* AndroidDeviceProfileSelector = FModuleManager::LoadModulePtr<IDeviceProfileSelectorModule>("AndroidDeviceProfileSelector");
				if (AndroidDeviceProfileSelector)
				{
					FPIEAndroidDeviceProperties& AndroidProperties = DeviceSpecs->AndroidProperties;

					TMap<FString, FString> DeviceParameters;
					DeviceParameters.Add("GPUFamily", AndroidProperties.GPUFamily);
					DeviceParameters.Add("GLVersion", AndroidProperties.GLVersion);
					DeviceParameters.Add("VulkanVersion", AndroidProperties.VulkanVersion);
					DeviceParameters.Add("AndroidVersion", AndroidProperties.AndroidVersion);
					DeviceParameters.Add("DeviceMake", AndroidProperties.DeviceMake);
					DeviceParameters.Add("DeviceModel", AndroidProperties.DeviceModel);
					DeviceParameters.Add("UsingHoudini", AndroidProperties.UsingHoudini ? "true" : "false");

					FString PIEProfileName = AndroidDeviceProfileSelector->GetDeviceProfileName(DeviceParameters);
					if (!PIEProfileName.IsEmpty())
					{
						DeviceProfile = PIEProfileName;
					}
				}
				break;
			}
			case EPIEPreviewDeviceType::IOS:
			{
				FPIEIOSDeviceProperties& IOSProperties = DeviceSpecs->IOSProperties;
				DeviceProfile = IOSProperties.DeviceModel;
				break;
			}
		}
		RHISetMobilePreviewFeatureLevel(GetPreviewDeviceFeatureLevel());
	}
}

static void ApplyRHIOverrides(FPIERHIOverrideState* RHIOverrideState)
{
	check(RHIOverrideState);
	GMaxTextureDimensions.SetPreviewOverride(RHIOverrideState->MaxTextureDimensions);
	GMaxShadowDepthBufferSizeX.SetPreviewOverride(RHIOverrideState->MaxShadowDepthBufferSizeX);
	GMaxShadowDepthBufferSizeY.SetPreviewOverride(RHIOverrideState->MaxShadowDepthBufferSizeY);
	GMaxCubeTextureDimensions.SetPreviewOverride(RHIOverrideState->MaxCubeTextureDimensions);
	GRHISupportsInstancing.SetPreviewOverride(RHIOverrideState->SupportsInstancing);
	GSupportsMultipleRenderTargets.SetPreviewOverride(RHIOverrideState->SupportsMultipleRenderTargets);
	GSupportsRenderTargetFormat_PF_FloatRGBA.SetPreviewOverride(RHIOverrideState->SupportsRenderTargetFormat_PF_FloatRGBA);
	GSupportsRenderTargetFormat_PF_G8.SetPreviewOverride(RHIOverrideState->SupportsRenderTargetFormat_PF_G8);
}

void FPIEPreviewDeviceProfileSelectorModule::ApplyPreviewDeviceState()
{
	if (!DeviceSpecs.IsValid())
	{
		return;
	}

	EShaderPlatform PreviewPlatform = SP_NumPlatforms;
	ERHIFeatureLevel::Type PreviewFeatureLevel = GetPreviewDeviceFeatureLevel();
	FPIERHIOverrideState* RHIOverrideState = nullptr;
	switch (DeviceSpecs->DevicePlatform)
	{
		case EPIEPreviewDeviceType::Android:
		{
			if (PreviewFeatureLevel == ERHIFeatureLevel::ES2)
			{
				PreviewPlatform = SP_OPENGL_ES2_ANDROID;
				RHIOverrideState = &DeviceSpecs->AndroidProperties.GLES2RHIState;
			}
			else
			{
				PreviewPlatform = SP_OPENGL_ES3_1_ANDROID;
				RHIOverrideState = &DeviceSpecs->AndroidProperties.GLES31RHIState;
			}
		}
		break;
		case EPIEPreviewDeviceType::IOS:
		{
			if (PreviewFeatureLevel == ERHIFeatureLevel::ES2)
			{
				PreviewPlatform = SP_OPENGL_ES2_IOS;
				RHIOverrideState = &DeviceSpecs->IOSProperties.GLES2RHIState;
			}
			else
			{
				PreviewPlatform = SP_METAL_MACES3_1;
				RHIOverrideState = &DeviceSpecs->IOSProperties.MetalRHIState;
			}
		}
		break;
	}

	if(PreviewPlatform != SP_NumPlatforms)
	{
		UMaterialShaderQualitySettings* MaterialShaderQualitySettings = UMaterialShaderQualitySettings::Get();
		FName QualityPreviewShaderPlatform = LegacyShaderPlatformToShaderFormat(PreviewPlatform);
		MaterialShaderQualitySettings->GetShaderPlatformQualitySettings(QualityPreviewShaderPlatform);
		MaterialShaderQualitySettings->SetPreviewPlatform(QualityPreviewShaderPlatform);
	}

	ApplyRHIOverrides(RHIOverrideState);

	// TODO: Localization
	FString AppTitle = FGlobalTabmanager::Get()->GetApplicationTitle().ToString() + "Previewing: "+ PreviewDevice;
	FGlobalTabmanager::Get()->SetApplicationTitle(FText::FromString(AppTitle));
}

const FPIEPreviewDeviceContainer& FPIEPreviewDeviceProfileSelectorModule::GetPreviewDeviceContainer()
{
	if (!EnumeratedDevices.GetRootCategory().IsValid())
	{
		EnumeratedDevices.EnumerateDeviceSpecifications(GetDeviceSpecificationContentDir());
	}
	return EnumeratedDevices;
}

FString FPIEPreviewDeviceProfileSelectorModule::GetDeviceSpecificationContentDir()
{
	return FPaths::EngineContentDir() / TEXT("Editor") / TEXT("PIEPreviewDeviceSpecs");
}

FString FPIEPreviewDeviceProfileSelectorModule::FindDeviceSpecificationFilePath(const FString& SearchDevice)
{
	const FPIEPreviewDeviceContainer& PIEPreviewDeviceContainer = GetPreviewDeviceContainer();
	FString FoundPath;

	int32 FoundIndex;
	if (PIEPreviewDeviceContainer.GetDeviceSpecifications().Find(SearchDevice, FoundIndex))
	{
		TSharedPtr<FPIEPreviewDeviceContainerCategory> SubCategory = PIEPreviewDeviceContainer.FindDeviceContainingCategory(FoundIndex);
		if(SubCategory.IsValid())
		{
			FoundPath = SubCategory->GetSubDirectoryPath() / SearchDevice + ".json";
		}
	}
	return FoundPath;
}

bool FPIEPreviewDeviceProfileSelectorModule::ReadDeviceSpecification()
{
	DeviceSpecs = nullptr;

	if (!FParse::Value(FCommandLine::Get(), GetPreviewDeviceCommandSwitch(), PreviewDevice))
	{
		return false;
	}

	const FString Filename = FindDeviceSpecificationFilePath(PreviewDevice);

	FString Json;
	if (FFileHelper::LoadFileToString(Json, *Filename))
	{
		TSharedPtr<FJsonObject> RootObject;
		TSharedRef<TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(Json);
		if (FJsonSerializer::Deserialize(JsonReader, RootObject) && RootObject.IsValid())
		{
			// We need to initialize FPIEPreviewDeviceSpecifications early as device profiles need to be evaluated before ProcessNewlyLoadedUObjects can be called.
			CreatePackage(nullptr, TEXT("/Script/PIEPreviewDeviceProfileSelector"));

			DeviceSpecs = MakeShareable(new FPIEPreviewDeviceSpecifications());

			if (!FJsonObjectConverter::JsonAttributesToUStruct(RootObject->Values, FPIEPreviewDeviceSpecifications::StaticStruct(), DeviceSpecs.Get(), 0, 0))
			{
				DeviceSpecs = nullptr;
			}
		}
	}
	bool bValidDeviceSpec = DeviceSpecs.IsValid();
	if (!bValidDeviceSpec)
	{
		UE_LOG(LogPIEPreviewDevice, Warning, TEXT("Could not load device specifications for preview target device '%s'"), *PreviewDevice);
	}
	return bValidDeviceSpec;
}

ERHIFeatureLevel::Type FPIEPreviewDeviceProfileSelectorModule::GetPreviewDeviceFeatureLevel() const
{
	check(DeviceSpecs.IsValid());

	switch (DeviceSpecs->DevicePlatform)
	{
		case EPIEPreviewDeviceType::Android:
		{
			FString SubVersion;
			// Check for ES3.1+ support from GLVersion, TODO: check other ES31 feature level constraints, see android's PlatformInitOpenGL
			const bool bDeviceSupportsES31 = DeviceSpecs->AndroidProperties.GLVersion.Split(TEXT("OpenGL ES 3."), nullptr, &SubVersion) && FCString::Atoi(*SubVersion) >= 1;

			// check the project's gles support:
			bool bProjectBuiltForES2 = false, bProjectBuiltForES31 = false;
			GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bBuildForES31"), bProjectBuiltForES31, GEngineIni);
			GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bBuildForES2"), bProjectBuiltForES2, GEngineIni);

			// Android Preview Device is currently expected to work on gles.
			check(bProjectBuiltForES2 || bProjectBuiltForES31);

			// Projects without ES2 support can only expect to run on ES31 devices.
			check(bProjectBuiltForES2 || bDeviceSupportsES31);

			// ES3.1+ devices fallback to ES2 if the project itself doesn't support ES3.1
			return bDeviceSupportsES31 && bProjectBuiltForES31 ? ERHIFeatureLevel::ES3_1 : ERHIFeatureLevel::ES2;
		}
		case EPIEPreviewDeviceType::IOS:
		{
			bool bProjectBuiltForES2 = false, bProjectBuiltForMetal = false, bProjectBuiltForMRTMetal = false;
			GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetal"), bProjectBuiltForMetal, GEngineIni);
			GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsOpenGLES2"), bProjectBuiltForES2, GEngineIni);
			GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetalMRT"), bProjectBuiltForMRTMetal, GEngineIni);
			
			const bool bDeviceSupportsMetal = DeviceSpecs->IOSProperties.MetalRHIState.MaxTextureDimensions > 0;

			// not supporting preview for MRT metal 
			check(!bProjectBuiltForMRTMetal);

			// atleast one of these should be valid!
			check(bProjectBuiltForES2 || bProjectBuiltForMetal);

			// if device doesnt support metal the project must have ES2 enabled.
			check(bProjectBuiltForES2 || (bProjectBuiltForMetal && bDeviceSupportsMetal));

			return (bDeviceSupportsMetal && bProjectBuiltForMetal) ? ERHIFeatureLevel::ES3_1 : ERHIFeatureLevel::ES2;
		}
	}

	checkNoEntry();
	return  ERHIFeatureLevel::Num;
}
