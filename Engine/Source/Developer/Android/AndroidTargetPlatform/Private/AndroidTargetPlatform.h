// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidTargetPlatform.h: Declares the FAndroidTargetPlatform class.
=============================================================================*/

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Delegates/IDelegateInstance.h"
#include "Containers/Map.h"
#include "Delegates/Delegate.h"
#include "Containers/Ticker.h"
#include "Misc/ScopeLock.h"

#if WITH_ENGINE
#include "Internationalization/Text.h"
#include "StaticMeshResources.h"
#endif // WITH_ENGINE

class FTargetDeviceId;
class IAndroidDeviceDetection;
class ITargetPlatform;
class UTextureLODSettings;
enum class ETargetPlatformFeatures;
template<typename TPlatformProperties> class TTargetPlatformBase;

template< typename InElementType, typename KeyFuncs , typename Allocator > class TSet;
template<typename KeyType,typename ValueType,typename SetAllocator ,typename KeyFuncs > class TMap;
template<typename KeyType,typename ValueType,typename SetAllocator ,typename KeyFuncs > class TMultiMap;
template<typename TPlatformProperties> class TTargetPlatformBase;

/**
 * Defines supported texture format names.
 */
namespace AndroidTexFormat
{
	// Compressed Texture Formats
	static FName NamePVRTC2(TEXT("PVRTC2"));
	static FName NamePVRTC4(TEXT("PVRTC4"));
	static FName NameAutoPVRTC(TEXT("AutoPVRTC"));
	static FName NameDXT1(TEXT("DXT1"));
	static FName NameDXT5(TEXT("DXT5"));
	static FName NameAutoDXT(TEXT("AutoDXT"));
	static FName NameATC_RGB(TEXT("ATC_RGB"));
	static FName NameATC_RGBA_E(TEXT("ATC_RGBA_E"));		// explicit alpha
	static FName NameATC_RGBA_I(TEXT("ATC_RGBA_I"));		// interpolated alpha
	static FName NameAutoATC(TEXT("AutoATC"));
	static FName NameETC1(TEXT("ETC1"));
	static FName NameAutoETC1(TEXT("AutoETC1"));			// ETC1 or uncompressed RGBA, if alpha channel required
	static FName NameETC2_RGB(TEXT("ETC2_RGB"));
	static FName NameETC2_RGBA(TEXT("ETC2_RGBA"));
	static FName NameAutoETC2(TEXT("AutoETC2"));
	static FName NameASTC_4x4(TEXT("ASTC_4x4"));
	static FName NameASTC_6x6(TEXT("ASTC_6x6"));
	static FName NameASTC_8x8(TEXT("ASTC_8x8"));
	static FName NameASTC_10x10(TEXT("ASTC_10x10"));
	static FName NameASTC_12x12(TEXT("ASTC_12x12"));
	static FName NameAutoASTC(TEXT("AutoASTC"));

	// Uncompressed Texture Formats
	static FName NameBGRA8(TEXT("BGRA8"));
	static FName NameG8(TEXT("G8"));
	static FName NameVU8(TEXT("VU8"));
	static FName NameRGBA16F(TEXT("RGBA16F"));

	// Error "formats" (uncompressed)
	static FName NamePOTERROR(TEXT("POTERROR"));
}


/**
 * FAndroidTargetPlatform, abstraction for cooking Android platforms
 */
template<class TPlatformProperties>
class FAndroidTargetPlatform
	: public TTargetPlatformBase< TPlatformProperties >
{
public:

	/**
	 * Default constructor.
	 */
	FAndroidTargetPlatform( );

	/**
	 * Destructor
	 */
	~FAndroidTargetPlatform();

public:

	/**
	 * Gets the name of the Android platform variant, i.e. ATC, DXT, PVRTC, etc.
	 *
	 * @param Variant name.
	 */
	virtual FString GetAndroidVariantName( )
	{
		return FString();	
	}

public:

	//~ Begin ITargetPlatform Interface

	virtual void EnableDeviceCheck(bool OnOff) override {}

	virtual bool AddDevice( const FString& DeviceName, bool bDefault ) override
	{
		return false;
	}

	virtual void GetAllDevices( TArray<ITargetDevicePtr>& OutDevices ) const override;

	virtual ECompressionFlags GetBaseCompressionMethod( ) const override;

	virtual bool GenerateStreamingInstallManifest(const TMultiMap<FString, int32>& ChunkMap, const TSet<int32>& ChunkIDsInUse) const override
	{
		return true;
	}

	virtual ITargetDevicePtr GetDefaultDevice( ) const override;

	virtual ITargetDevicePtr GetDevice( const FTargetDeviceId& DeviceId ) override;

	virtual bool IsRunningPlatform( ) const override;

	virtual bool IsServerOnly( ) const override
	{
		return false;
	}

	virtual bool IsSdkInstalled(bool bProjectHasCode, FString& OutDocumentationPath) const override;

	virtual int32 CheckRequirements(const FString& ProjectPath, bool bProjectHasCode, FString& OutTutorialPath, FString& OutDocumentationPath, FText& CustomizedLogMessage) const override;

	virtual bool SupportsFeature( ETargetPlatformFeatures Feature ) const override;

	virtual bool SupportsTextureFormat( FName Format ) const 
	{
		// By default we support all texture formats.
		return true;
	}

	virtual bool SupportsCompressedNonPOT( ) const
	{
		// most formats do support non-POT compressed textures
		return true;
	}

#if WITH_ENGINE
	virtual void GetReflectionCaptureFormats( TArray<FName>& OutFormats ) const override;

	virtual void GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const override;

	virtual void GetAllTargetedShaderFormats(TArray<FName>& OutFormats) const override;
	
	virtual const class FStaticMeshLODSettings& GetStaticMeshLODSettings() const override;

	virtual void GetTextureFormats( const UTexture* InTexture, TArray<FName>& OutFormats ) const override;

	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override;

	virtual const UTextureLODSettings& GetTextureLODSettings() const override;

	virtual void RegisterTextureLODSettings(const UTextureLODSettings* InTextureLODSettings) override
	{
		TextureLODSettings = InTextureLODSettings;
	}

	virtual FName GetWaveFormat( const class USoundWave* Wave ) const override;
	virtual void GetAllWaveFormats( TArray<FName>& OutFormats) const override;
#endif //WITH_ENGINE

	virtual bool SupportsVariants() const override;

	virtual FText GetVariantTitle() const override;

	virtual void GetBuildProjectSettingKeys(FString& OutSection, TArray<FString>& InBoolKeys, TArray<FString>& InIntKeys, TArray<FString>& InStringKeys) const override
	{
		OutSection = TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings");
		InBoolKeys.Add(TEXT("bBuildForArmV7")); InBoolKeys.Add(TEXT("bBuildForArm64")); InBoolKeys.Add(TEXT("bBuildForX86"));
		InBoolKeys.Add(TEXT("bBuildForX8664")); InBoolKeys.Add(TEXT("bBuildForES2"));
		InBoolKeys.Add(TEXT("bBuildForES31")); InBoolKeys.Add(TEXT("bBuildWithHiddenSymbolVisibility"));
	}

	DECLARE_DERIVED_EVENT(FAndroidTargetPlatform, ITargetPlatform::FOnTargetDeviceDiscovered, FOnTargetDeviceDiscovered);
	virtual FOnTargetDeviceDiscovered& OnDeviceDiscovered( ) override
	{
		return DeviceDiscoveredEvent;
	}

	DECLARE_DERIVED_EVENT(FAndroidTargetPlatform, ITargetPlatform::FOnTargetDeviceLost, FOnTargetDeviceLost);
	virtual FOnTargetDeviceLost& OnDeviceLost( ) override
	{
		return DeviceLostEvent;
	}

	//~ End ITargetPlatform Interface

protected:

	/**
	 * Adds the specified texture format to the OutFormats if this android target platforms supports it.
	 *
	 * @param Format - The format to add.
	 * @param OutFormats - The collection of formats to add to.
	 * @param bIsCompressedNonPOT - If this is true, the texture wants to be compressed but is not a power of 2
	 */
	void AddTextureFormatIfSupports( FName Format, TArray<FName>& OutFormats, bool bIsCompressedNonPOT=false ) const;

	/**
	 * Return true if this device has a supported set of extensions for this platform.
	 *
	 * @param Extensions - The GL extensions string.
	 * @param GLESVersion - The GLES version reported by this device.
	 */
	virtual bool SupportedByExtensionsString( const FString& ExtensionsString, const int GLESVersion ) const
	{
		return true;
	}

#if WITH_ENGINE
	// Holds the Engine INI settings (for quick access).
	FConfigFile EngineSettings;
#endif //WITH_ENGINE

private:

	// Handles when the ticker fires.
	bool HandleTicker( float DeltaTime );

	// Holds a map of valid devices.
	TMap<FString, FAndroidTargetDevicePtr> Devices;

	// Holds a delegate to be invoked when the widget ticks.
	FTickerDelegate TickDelegate;

	// Handle to the registered TickDelegate.
	FDelegateHandle TickDelegateHandle;

	// Pointer to the device detection handler that grabs device ids in another thread
	IAndroidDeviceDetection* DeviceDetection;

#if WITH_ENGINE
	// Holds a cache of the target LOD settings.
	const UTextureLODSettings* TextureLODSettings;

	// Holds the static mesh LOD settings.
	FStaticMeshLODSettings StaticMeshLODSettings;

	ITargetDevicePtr DefaultDevice;
#endif //WITH_ENGINE

	// Holds an event delegate that is executed when a new target device has been discovered.
	FOnTargetDeviceDiscovered DeviceDiscoveredEvent;

	// Holds an event delegate that is executed when a target device has been lost, i.e. disconnected or timed out.
	FOnTargetDeviceLost DeviceLostEvent;
};


#include "AndroidTargetPlatform.inl"
