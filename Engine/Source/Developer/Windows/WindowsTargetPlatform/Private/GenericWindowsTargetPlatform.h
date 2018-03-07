// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/TargetPlatformBase.h"
#include "Misc/ConfigCacheIni.h"
#include "LocalPcTargetDevice.h"

#if WITH_ENGINE
	#include "Sound/SoundWave.h"
	#include "TextureResource.h"
	#include "StaticMeshResources.h"
#endif // WITH_ENGINE

#define LOCTEXT_NAMESPACE "TGenericWindowsTargetPlatform"

/**
 * Template for Windows target platforms
 */
template<bool HAS_EDITOR_DATA, bool IS_DEDICATED_SERVER, bool IS_CLIENT_ONLY>
class TGenericWindowsTargetPlatform
	: public TTargetPlatformBase<FWindowsPlatformProperties<HAS_EDITOR_DATA, IS_DEDICATED_SERVER, IS_CLIENT_ONLY> >
{
public:
	typedef FWindowsPlatformProperties<HAS_EDITOR_DATA, IS_DEDICATED_SERVER, IS_CLIENT_ONLY> TProperties;
	typedef TTargetPlatformBase<TProperties> TSuper;

	/**
	 * Default constructor.
	 */
	TGenericWindowsTargetPlatform( )
	{
#if PLATFORM_WINDOWS
		// only add local device if actually running on Windows
		LocalDevice = MakeShareable(new TLocalPcTargetDevice<PLATFORM_64BITS>(*this));
#endif
		
		#if WITH_ENGINE
			FConfigCacheIni::LoadLocalIniFile(EngineSettings, TEXT("Engine"), true, *this->PlatformName());
			TextureLODSettings = nullptr; // These are registered by the device profile system.
			StaticMeshLODSettings.Initialize(EngineSettings);


			// Get the Target RHIs for this platform, we do not always want all those that are supported.
			TArray<FName> TargetedShaderFormats;
			GetAllTargetedShaderFormats(TargetedShaderFormats);

			// If we're targeting only DX11 we can use DX11 texture formats. Otherwise we'd have to compress fallbacks and increase the size of cooked content significantly.
			static FName NAME_PCD3D_SM5(TEXT("PCD3D_SM5"));
			bSupportDX11TextureFormats = TargetedShaderFormats.Num() == 1
				&& TargetedShaderFormats[0] == NAME_PCD3D_SM5;
		#endif
	}

public:

	//~ Begin ITargetPlatform Interface

	virtual void EnableDeviceCheck(bool OnOff) override {}

	virtual void GetAllDevices( TArray<ITargetDevicePtr>& OutDevices ) const override
	{
		OutDevices.Reset();
		if (LocalDevice.IsValid())
		{
			OutDevices.Add(LocalDevice);
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

	virtual ITargetDevicePtr GetDevice( const FTargetDeviceId& DeviceId )
	{
		if (LocalDevice.IsValid() && (DeviceId == LocalDevice->GetId()))
		{
			return LocalDevice;
		}

		return nullptr;
	}

	virtual bool IsRunningPlatform( ) const override
	{
		// Must be Windows platform as editor for this to be considered a running platform
		return PLATFORM_WINDOWS && !UE_SERVER && !UE_GAME && WITH_EDITOR && HAS_EDITOR_DATA;
	}

	virtual bool SupportsFeature( ETargetPlatformFeatures Feature ) const override
	{
		// we currently do not have a build target for WindowsServer
		if (Feature == ETargetPlatformFeatures::Packaging)
		{
			return (HAS_EDITOR_DATA || !IS_DEDICATED_SERVER);
		}

		if ( Feature == ETargetPlatformFeatures::ShouldSplitPaksIntoSmallerSizes )
		{
			return IS_CLIENT_ONLY;
		}

		return TSuper::SupportsFeature(Feature);
	}

	virtual void GetBuildProjectSettingKeys(FString& OutSection, TArray<FString>& InBoolKeys, TArray<FString>& InIntKeys, TArray<FString>& InStringKeys) const override
	{
		OutSection = TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings");
		InStringKeys.Add(TEXT("MinimumOSVersion"));
	}

#if WITH_ENGINE
	virtual void GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const override
	{
		// no shaders needed for dedicated server target
		if (!IS_DEDICATED_SERVER)
		{
			static FName NAME_PCD3D_SM5(TEXT("PCD3D_SM5"));
			static FName NAME_PCD3D_SM4(TEXT("PCD3D_SM4"));
			static FName NAME_GLSL_150(TEXT("GLSL_150"));
			static FName NAME_GLSL_430(TEXT("GLSL_430"));
			static FName NAME_VULKAN_ES31(TEXT("SF_VULKAN_ES31"));
			static FName NAME_OPENGL_150_ES2(TEXT("GLSL_150_ES2"));
			static FName NAME_OPENGL_150_ES3_1(TEXT("GLSL_150_ES31"));
			static FName NAME_VULKAN_SM5(TEXT("SF_VULKAN_SM5"));

			OutFormats.AddUnique(NAME_PCD3D_SM5);
			OutFormats.AddUnique(NAME_PCD3D_SM4);
			OutFormats.AddUnique(NAME_GLSL_150);
			OutFormats.AddUnique(NAME_GLSL_430);
			OutFormats.AddUnique(NAME_VULKAN_ES31);
			OutFormats.AddUnique(NAME_OPENGL_150_ES2);
			OutFormats.AddUnique(NAME_OPENGL_150_ES3_1);
			OutFormats.AddUnique(NAME_VULKAN_SM5);
		}
	}

	virtual void GetAllTargetedShaderFormats( TArray<FName>& OutFormats ) const override
	{
		// Get the Target RHIs for this platform, we do not always want all those that are supported. (reload in case user changed in the editor)
		TArray<FString>TargetedShaderFormats;
		GConfig->GetArray(TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings"), TEXT("TargetedRHIs"), TargetedShaderFormats, GEngineIni);

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
			FName TextureFormatName = GetDefaultTextureFormatName(this, InTexture, EngineSettings, bSupportDX11TextureFormats);
			OutFormats.Add(TextureFormatName);
		}
	}

	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override
	{
		if (!IS_DEDICATED_SERVER)
		{
			GetAllDefaultTextureFormats(this, OutFormats, bSupportDX11TextureFormats);
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
		static FName NAME_OPUS(TEXT("OPUS"));

		if (Wave->IsStreaming())
		{
			return NAME_OPUS;
		}

		return NAME_OGG;
	}

	virtual void GetAllWaveFormats(TArray<FName>& OutFormats) const override
	{
		static FName NAME_OGG(TEXT("OGG"));
		static FName NAME_OPUS(TEXT("OPUS"));
		OutFormats.Add(NAME_OGG);
		OutFormats.Add(NAME_OPUS);
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
			return LOCTEXT("WindowsServerVariantTitle", "Dedicated Server");
		}

		if (HAS_EDITOR_DATA)
		{
			return LOCTEXT("WindowsClientEditorDataVariantTitle", "Client with Editor Data");
		}

		if (IS_CLIENT_ONLY)
		{
			return LOCTEXT("WindowsClientOnlyVariantTitle", "Client only");
		}

		return LOCTEXT("WindowsClientVariantTitle", "Client");
	}

	virtual FText GetVariantTitle() const override
	{
		return LOCTEXT("WindowsVariantTitle", "Build Type");
	}

	virtual float GetVariantPriority() const override
	{
		return TProperties::GetVariantPriority();
	}

	DECLARE_DERIVED_EVENT(TGenericWindowsTargetPlatform, ITargetPlatform::FOnTargetDeviceDiscovered, FOnTargetDeviceDiscovered);
	virtual FOnTargetDeviceDiscovered& OnDeviceDiscovered( ) override
	{
		return DeviceDiscoveredEvent;
	}

	DECLARE_DERIVED_EVENT(TGenericWindowsTargetPlatform, ITargetPlatform::FOnTargetDeviceLost, FOnTargetDeviceLost);
	virtual FOnTargetDeviceLost& OnDeviceLost( ) override
	{
		return DeviceLostEvent;
	}

	//~ End ITargetPlatform Interface

private:

	// Holds the local device.
	ITargetDevicePtr LocalDevice;

#if WITH_ENGINE
	// Holds the Engine INI settings for quick use.
	FConfigFile EngineSettings;

	// Holds the texture LOD settings.
	const UTextureLODSettings* TextureLODSettings;

	// Holds static mesh LOD settings.
	FStaticMeshLODSettings StaticMeshLODSettings;

	// True if the project supports non-DX11 texture formats.
	bool bSupportDX11TextureFormats;
#endif // WITH_ENGINE

private:

	// Holds an event delegate that is executed when a new target device has been discovered.
	FOnTargetDeviceDiscovered DeviceDiscoveredEvent;

	// Holds an event delegate that is executed when a target device has been lost, i.e. disconnected or timed out.
	FOnTargetDeviceLost DeviceLostEvent;
};

#undef LOCTEXT_NAMESPACE
