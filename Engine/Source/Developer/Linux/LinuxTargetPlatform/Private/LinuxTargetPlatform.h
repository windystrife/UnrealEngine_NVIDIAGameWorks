// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LinuxTargetPlatform.h: Declares the FLinuxTargetPlatform class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/TargetDeviceId.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Interfaces/ITargetPlatform.h"
#include "Common/TargetPlatformBase.h"

#if WITH_ENGINE
#include "Sound/SoundWave.h"
#include "StaticMeshResources.h"
#endif // WITH_ENGINE
#include "Interfaces/IProjectManager.h"
#include "InstalledPlatformInfo.h"
#include "LinuxTargetDevice.h"
#include "Linux/LinuxPlatformProperties.h"

#define LOCTEXT_NAMESPACE "TLinuxTargetPlatform"

class UTextureLODSettings;

/**
 * Template for Linux target platforms
 */
template<bool HAS_EDITOR_DATA, bool IS_DEDICATED_SERVER, bool IS_CLIENT_ONLY>
class TLinuxTargetPlatform
	: public TTargetPlatformBase<FLinuxPlatformProperties<HAS_EDITOR_DATA, IS_DEDICATED_SERVER, IS_CLIENT_ONLY> >
{
public:
	
	typedef FLinuxPlatformProperties<HAS_EDITOR_DATA, IS_DEDICATED_SERVER, IS_CLIENT_ONLY> TProperties;
	typedef TTargetPlatformBase<TProperties> TSuper;

	/**
	 * Default constructor.
	 */
	TLinuxTargetPlatform( )
#if WITH_ENGINE
		: bChangingDeviceConfig(false)
#endif // WITH_ENGINE
	{		
#if PLATFORM_LINUX
		// only add local device if actually running on Linux
		LocalDevice = MakeShareable(new FLinuxTargetDevice(*this, FPlatformProcess::ComputerName(), nullptr));
#endif
	
#if WITH_ENGINE
		FConfigCacheIni::LoadLocalIniFile(EngineSettings, TEXT("Engine"), true, *this->PlatformName());
		TextureLODSettings = nullptr;
		StaticMeshLODSettings.Initialize(EngineSettings);

		InitDevicesFromConfig();
#endif // WITH_ENGINE
	}


public:

	//~ Begin ITargetPlatform Interface

	virtual void EnableDeviceCheck(bool OnOff) override {}

	virtual bool AddDevice(const FString& DeviceName, bool bDefault) override
	{
		FLinuxTargetDevicePtr& Device = Devices.FindOrAdd(DeviceName);

		if (Device.IsValid())
		{
			// do not allow duplicates
			return false;
		}

		FTargetDeviceId UATFriendlyId(TEXT("Linux"), DeviceName);
		Device = MakeShareable(new FLinuxTargetDevice(*this, DeviceName, 
#if WITH_ENGINE
			[&]() { SaveDevicesToConfig(); }));
		SaveDevicesToConfig();	// this will do the right thing even if AddDevice() was called from InitDevicesFromConfig
#else
			nullptr));
#endif // WITH_ENGINE	

		DeviceDiscoveredEvent.Broadcast(Device.ToSharedRef());
		return true;
	}

	virtual void GetAllDevices( TArray<ITargetDevicePtr>& OutDevices ) const override
	{
		// TODO: ping all the machines in a local segment and/or try to connect to port 22 of those that respond
		OutDevices.Reset();
		if (LocalDevice.IsValid())
		{
			OutDevices.Add(LocalDevice);
		}

		for (const auto & DeviceIter : Devices)
		{
			OutDevices.Add(DeviceIter.Value);
		}
	}

	virtual ECompressionFlags GetBaseCompressionMethod( ) const override
	{
		return COMPRESS_ZLIB;
	}

	virtual bool GenerateStreamingInstallManifest(const TMultiMap<FString, int32>& ChunkMap, const TSet<int32>& ChunkIDsInUse) const override
	{
		return true;
	}

	virtual ITargetDevicePtr GetDefaultDevice( ) const override
	{
		if (LocalDevice.IsValid())
		{
			return LocalDevice;
		}

		return nullptr;
	}

	virtual ITargetDevicePtr GetDevice( const FTargetDeviceId& DeviceId ) override
	{
		if (LocalDevice.IsValid() && (DeviceId == LocalDevice->GetId()))
		{
			return LocalDevice;
		}

		for (const auto & DeviceIter : Devices)
		{
			if (DeviceId == DeviceIter.Value->GetId())
			{
				return DeviceIter.Value;
			}
		}

		return nullptr;
	}

	virtual bool IsRunningPlatform( ) const override
	{
		// Must be Linux platform as editor for this to be considered a running platform
		return PLATFORM_LINUX && !UE_SERVER && !UE_GAME && WITH_EDITOR && HAS_EDITOR_DATA;
	}

	virtual bool SupportsFeature(ETargetPlatformFeatures Feature) const override
	{
		if (Feature == ETargetPlatformFeatures::UserCredentials || Feature == ETargetPlatformFeatures::Packaging)
		{
			return true;
		}

		return TTargetPlatformBase<FLinuxPlatformProperties<HAS_EDITOR_DATA, IS_DEDICATED_SERVER, IS_CLIENT_ONLY>>::SupportsFeature(Feature);
	}

	virtual bool IsSdkInstalled(bool bProjectHasCode, FString& OutDocumentationPath) const override
	{
		if (!PLATFORM_LINUX)
		{
			// check for LINUX_MULTIARCH_ROOT or for legacy LINUX_ROOT when targeting Linux from Win/Mac
			TCHAR ToolchainRoot[32768] = { 0 };
			FPlatformMisc::GetEnvironmentVariable(TEXT("LINUX_MULTIARCH_ROOT"), ToolchainRoot, ARRAY_COUNT(ToolchainRoot));
			// proceed with any value for MULTIARCH root, because checking exact architecture is not possible at this point
			FString ToolchainMultiarchRoot = ToolchainRoot;
			if (ToolchainMultiarchRoot.Len() > 0 && FPaths::DirectoryExists(ToolchainMultiarchRoot))
			{
				return true;
			}
			
			// else check for legacy LINUX_ROOT
			ToolchainRoot[ 0 ] = 0;
			FPlatformMisc::GetEnvironmentVariable(TEXT("LINUX_ROOT"), ToolchainRoot, ARRAY_COUNT(ToolchainRoot));
			FString ToolchainCompiler = ToolchainRoot;
			if (PLATFORM_WINDOWS)
			{
				ToolchainCompiler += "/bin/clang++.exe";
			}
			else if (PLATFORM_MAC)
			{
				ToolchainCompiler += "/bin/clang++";
			}
			else
			{
				checkf(false, TEXT("Unable to target Linux on an unknown platform."));
				return false;
			}

			return FPaths::FileExists(ToolchainCompiler);
		}

		return true;
	}

	virtual int32 CheckRequirements(const FString& ProjectPath, bool bProjectHasCode, FString& OutTutorialPath, FString& OutDocumentationPath, FText& CustomizedLogMessage) const override
	{
		int32 ReadyToBuild = TSuper::CheckRequirements(ProjectPath, bProjectHasCode, OutTutorialPath, OutDocumentationPath, CustomizedLogMessage);

		// do not support code/plugins in Installed builds if the required libs aren't bundled (on Windows/Mac)
		if (!PLATFORM_LINUX && !FInstalledPlatformInfo::Get().IsValidPlatform(TSuper::GetPlatformInfo().BinaryFolderName, EProjectType::Code))
		{
			if (bProjectHasCode)
			{
				ReadyToBuild |= ETargetPlatformReadyStatus::CodeUnsupported;
			}

			if (IProjectManager::Get().IsNonDefaultPluginEnabled())
			{
				ReadyToBuild |= ETargetPlatformReadyStatus::PluginsUnsupported;
			}
		}

		return ReadyToBuild;
	}


#if WITH_ENGINE
	virtual void GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const override
	{
		// no shaders needed for dedicated server target
		if (!IS_DEDICATED_SERVER)
		{
			static FName NAME_GLSL_150(TEXT("GLSL_150"));
			static FName NAME_GLSL_430(TEXT("GLSL_430"));
			static FName NAME_VULKAN_SM4(TEXT("SF_VULKAN_SM4"));
			static FName NAME_VULKAN_SM5(TEXT("SF_VULKAN_SM5"));
			static FName NAME_VULKAN_ES31(TEXT("SF_VULKAN_ES31"));

			OutFormats.AddUnique(NAME_GLSL_150);
			OutFormats.AddUnique(NAME_GLSL_430);
			OutFormats.AddUnique(NAME_VULKAN_SM4);
			OutFormats.AddUnique(NAME_VULKAN_SM5);
			OutFormats.AddUnique(NAME_VULKAN_ES31);
		}
	}

	virtual void GetAllTargetedShaderFormats( TArray<FName>& OutFormats ) const override
	{
		// Get the Target RHIs for this platform, we do not always want all those that are supported. (reload in case user changed in the editor)
		TArray<FString>TargetedShaderFormats;
		GConfig->GetArray(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), TEXT("TargetedRHIs"), TargetedShaderFormats, GEngineIni);

		// Gather the list of Target RHIs and filter out any that may be invalid.
		TArray<FName> PossibleShaderFormats;
		GetAllPossibleShaderFormats(PossibleShaderFormats);

		for (int32 ShaderFormatIdx = TargetedShaderFormats.Num() - 1; ShaderFormatIdx >= 0; ShaderFormatIdx--)
		{
			FString ShaderFormat = TargetedShaderFormats[ShaderFormatIdx];
			if (PossibleShaderFormats.Contains(FName(*ShaderFormat)) == false)
			{
				TargetedShaderFormats.RemoveAt(ShaderFormatIdx);
			}
		}

		for(const FString& ShaderFormat : TargetedShaderFormats)
		{
			OutFormats.AddUnique(FName(*ShaderFormat));
		}
	}
	virtual const class FStaticMeshLODSettings& GetStaticMeshLODSettings( ) const override
	{
		return StaticMeshLODSettings;
	}


	virtual void GetTextureFormats( const UTexture* InTexture, TArray<FName>& OutFormats ) const override
	{
		if (!IS_DEDICATED_SERVER)
		{
			// just use the standard texture format name for this texture
			FName TextureFormatName = GetDefaultTextureFormatName(this, InTexture, EngineSettings, false);
			OutFormats.Add(TextureFormatName);
		}
	}


	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override
	{
		if (!IS_DEDICATED_SERVER)
		{
			// just use the standard texture format name for this texture
			GetAllDefaultTextureFormats(this, OutFormats, false);
		}
	}

	virtual const UTextureLODSettings& GetTextureLODSettings() const override
	{
		return *TextureLODSettings;
	}

	virtual void RegisterTextureLODSettings(const UTextureLODSettings* InTextureLODSettings) override
	{
		TextureLODSettings = InTextureLODSettings;
	}

	virtual FName GetWaveFormat( const class USoundWave* Wave ) const override
	{
		static FName NAME_OGG(TEXT("OGG"));
		return NAME_OGG;
	}

	virtual void GetAllWaveFormats(TArray<FName>& OutFormats) const override
	{
		static FName NAME_OGG(TEXT("OGG"));
		OutFormats.Add(NAME_OGG);
	}
#endif //WITH_ENGINE

	virtual bool SupportsVariants() const override
	{
		return true;
	}

	virtual FText GetVariantDisplayName() const override
	{
		if (IS_DEDICATED_SERVER)
		{
			return LOCTEXT("LinuxServerVariantTitle", "Dedicated Server");
		}

		if (HAS_EDITOR_DATA)
		{
			return LOCTEXT("LinuxClientEditorDataVariantTitle", "Client with Editor Data");
		}

		if (IS_CLIENT_ONLY)
		{
			return LOCTEXT("LinuxClientOnlyVariantTitle", "Client only");
		}

		return LOCTEXT("LinuxClientVariantTitle", "Client");
	}

	virtual FText GetVariantTitle() const override
	{
		return LOCTEXT("LinuxVariantTitle", "Build Type");
	}

	virtual float GetVariantPriority() const override
	{
		return TProperties::GetVariantPriority();
	}

	DECLARE_DERIVED_EVENT(TLinuxTargetPlatform, ITargetPlatform::FOnTargetDeviceDiscovered, FOnTargetDeviceDiscovered);
	virtual FOnTargetDeviceDiscovered& OnDeviceDiscovered( ) override
	{
		return DeviceDiscoveredEvent;
	}
	
	DECLARE_DERIVED_EVENT(TLinuxTargetPlatform, ITargetPlatform::FOnTargetDeviceLost, FOnTargetDeviceLost);
	virtual FOnTargetDeviceLost& OnDeviceLost( ) override
	{
		return DeviceLostEvent;
	}

	//~ End ITargetPlatform Interface

private:

#if WITH_ENGINE
	/** Whether we're in process of changing device config - if yes, we will prevent recurrent calls. */
	bool bChangingDeviceConfig;

	void InitDevicesFromConfig()
	{
		if (bChangingDeviceConfig)
		{
			return;
		}
		bChangingDeviceConfig = true;

		int NumDevices = 0;	
		for (;; ++NumDevices)
		{
			FString DeviceName, DeviceUser, DevicePass;

			FString DeviceBaseKey(FString::Printf(TEXT("LinuxTargetPlatfrom_%s_Device_%d"), *TSuper::PlatformName(), NumDevices));
			FString DeviceNameKey = DeviceBaseKey + TEXT("_Name");
			if (!GConfig->GetString(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), *DeviceNameKey, DeviceName, GEngineIni))
			{
				// no such device
				break;
			}

			if (!AddDevice(DeviceName, false))
			{
				break;
			}

			// set credentials, if any
			FString DeviceUserKey = DeviceBaseKey + TEXT("_User");
			if (GConfig->GetString(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), *DeviceUserKey, DeviceUser, GEngineIni))
			{
				FString DevicePassKey = DeviceBaseKey + TEXT("_Pass");
				if (GConfig->GetString(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), *DevicePassKey, DevicePass, GEngineIni))
				{
					for (const auto & DeviceIter : Devices)
					{
						ITargetDevicePtr Device = DeviceIter.Value;
						if (Device.IsValid() && Device->GetId().GetDeviceName() == DeviceName)
						{
							Device->SetUserCredentials(DeviceUser, DevicePass);
						}
					}
				}
			}
		}
		
		bChangingDeviceConfig = false;
	}

	void SaveDevicesToConfig()
	{
		if (bChangingDeviceConfig)
		{
			return;
		}
		bChangingDeviceConfig = true;

		int DeviceIndex = 0;
		for (const auto & DeviceIter : Devices)
		{
			ITargetDevicePtr Device = DeviceIter.Value;

			FString DeviceBaseKey(FString::Printf(TEXT("LinuxTargetPlatfrom_%s_Device_%d"), *TSuper::PlatformName(), DeviceIndex));
			FString DeviceNameKey = DeviceBaseKey + TEXT("_Name");

			if (Device.IsValid())
			{
				FString DeviceName = Device->GetId().GetDeviceName();
				// do not save a local device on Linux or it will be duplicated
				if (PLATFORM_LINUX && DeviceName == FPlatformProcess::ComputerName())
				{
					continue;
				}

				GConfig->SetString(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), *DeviceNameKey, *DeviceName, GEngineIni);

				FString DeviceUser, DevicePass;
				if (Device->GetUserCredentials(DeviceUser, DevicePass))
				{
					FString DeviceUserKey = DeviceBaseKey + TEXT("_User");
					FString DevicePassKey = DeviceBaseKey + TEXT("_Pass");

					GConfig->SetString(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), *DeviceUserKey, *DeviceUser, GEngineIni);
					GConfig->SetString(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), *DevicePassKey, *DevicePass, GEngineIni);
				}

				++DeviceIndex;	// needs to be incremented here since we cannot allow gaps
			}
		}

		bChangingDeviceConfig = false;
	}
#endif // WITH_ENGINE

	// Holds the local device.
	FLinuxTargetDevicePtr LocalDevice;
	// Holds a map of valid devices.
	TMap<FString, FLinuxTargetDevicePtr> Devices;


#if WITH_ENGINE
	// Holds the Engine INI settings for quick use.
	FConfigFile EngineSettings;

	// Holds the texture LOD settings.
	const UTextureLODSettings* TextureLODSettings;

	// Holds static mesh LOD settings.
	FStaticMeshLODSettings StaticMeshLODSettings;

#endif // WITH_ENGINE

private:

	// Holds an event delegate that is executed when a new target device has been discovered.
	FOnTargetDeviceDiscovered DeviceDiscoveredEvent;

	// Holds an event delegate that is executed when a target device has been lost, i.e. disconnected or timed out.
	FOnTargetDeviceLost DeviceLostEvent;
};

#undef LOCTEXT_NAMESPACE
