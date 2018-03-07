// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HTML5TargetPlatform.h: Declares the FHTML5TargetPlatform class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"
#include "HTML5/HTML5PlatformProperties.h"
#include "Interfaces/ITargetPlatform.h"
#include "Common/TargetPlatformBase.h"
#include "Developer/HTML5/HTML5TargetPlatform/Private/HTML5TargetDevice.h"

#if WITH_ENGINE
#include "StaticMeshResources.h"
#endif // WITH_ENGINE

class UTextureLODSettings;

/**
 * Implements the HTML5 target platform.
 */
class FHTML5TargetPlatform
	: public TTargetPlatformBase<FHTML5PlatformProperties>
{
public:

	/**
	 * Default constructor.
	 */
	FHTML5TargetPlatform( );

	void RefreshHTML5Setup();

	//~ Begin ITargetPlatform Interface

	virtual void EnableDeviceCheck(bool OnOff) override {}

	virtual void GetAllDevices( TArray<ITargetDevicePtr>& OutDevices ) const override;

	virtual ECompressionFlags GetBaseCompressionMethod( ) const override;

	virtual bool GenerateStreamingInstallManifest(const TMultiMap<FString, int32>& ChunkMap, const TSet<int32>& ChunkIDsInUse) const override
	{
		return true;
	}

	virtual ITargetDevicePtr GetDefaultDevice( ) const override;

	virtual ITargetDevicePtr GetDevice( const FTargetDeviceId& DeviceId ) override;

	virtual bool IsRunningPlatform( ) const override;

	virtual bool SupportsFeature( ETargetPlatformFeatures Feature ) const override
	{
		switch (Feature)
		{
		case ETargetPlatformFeatures::Packaging:
			return true;
		case ETargetPlatformFeatures::MobileRendering:
			return true;
		case ETargetPlatformFeatures::DeferredRendering:
			return false;
		}

		return TTargetPlatformBase<FHTML5PlatformProperties>::SupportsFeature(Feature);
	}

	virtual bool IsSdkInstalled(bool bProjectHasCode, FString& OutDocumentationPath) const override;

#if WITH_ENGINE
	virtual void GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const override;

	virtual void GetAllTargetedShaderFormats( TArray<FName>& OutFormats ) const override;
	virtual const class FStaticMeshLODSettings& GetStaticMeshLODSettings() const override;

	virtual void GetTextureFormats( const UTexture* InTexture, TArray<FName>& OutFormats ) const override;

	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override;

	virtual void GetReflectionCaptureFormats( TArray<FName>& OutFormats ) const override
	{
		OutFormats.Add(FName(TEXT("EncodedHDR")));
	}

	virtual const UTextureLODSettings& GetTextureLODSettings() const override;

	virtual void RegisterTextureLODSettings(const UTextureLODSettings* InTextureLODSettings) override
	{
		HTML5LODSettings = InTextureLODSettings;
	}

	virtual FName GetWaveFormat( const class USoundWave* Wave ) const override;
	virtual void GetAllWaveFormats(TArray<FName>& OutFormats) const override;
#endif // WITH_ENGINE

	DECLARE_DERIVED_EVENT(FHTML5TargetPlatform, ITargetPlatform::FOnTargetDeviceDiscovered, FOnTargetDeviceDiscovered);
	virtual FOnTargetDeviceDiscovered& OnDeviceDiscovered( ) override
	{
		return DeviceDiscoveredEvent;
	}

	DECLARE_DERIVED_EVENT(FHTML5TargetPlatform, ITargetPlatform::FOnTargetDeviceLost, FOnTargetDeviceLost);
	virtual FOnTargetDeviceLost& OnDeviceLost( ) override
	{
		return DeviceLostEvent;
	}

	//~ End ITargetPlatform Interface

private:

	// Holds the HTML5 engine settings.
	FConfigFile HTML5EngineSettings;

	// Holds the local device.
	TMap<FString, FHTML5TargetDevicePtr> Devices;

#if WITH_ENGINE
	// Holds the cached target LOD settings.
	const UTextureLODSettings* HTML5LODSettings;

	// Holds the static mesh LOD settings.
	FStaticMeshLODSettings StaticMeshLODSettings;
#endif

	// Holds an event delegate that is executed when a new target device has been discovered.
	FOnTargetDeviceDiscovered DeviceDiscoveredEvent;

	// Holds an event delegate that is executed when a target device has been lost, i.e. disconnected or timed out.
	FOnTargetDeviceLost DeviceLostEvent;

	// The name of the default device
	FString DefaultDeviceName;

	// Holds a critical section for locking access to the collection of devices.
	static FCriticalSection DevicesCriticalSection;

	void PopulateDevices(TArray<FString>& DeviceMaps, FString prefix);

};
