// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/ITargetDevice.h"


namespace PlatformInfo
{
	// Forward declare type from DesktopPlatform rather than add an include dependency to everything using ITargetPlatform
	struct FPlatformInfo;
}


/**
 * Enumerates features that may be supported by target platforms.
 */
enum class ETargetPlatformFeatures
{
	/** Audio Streaming */
	AudioStreaming,

	/** Distance field shadows. */
	DistanceFieldShadows,

	/** Gray scale SRGB texture formats support. */
	GrayscaleSRGB,

	/** High quality light maps. */
	HighQualityLightmaps,

	/** Low quality light maps. */
	LowQualityLightmaps,

	/** Run multiple game instances on a single device. */
	MultipleGameInstances,

	/** Builds can be packaged for this platform. */
	Packaging,

	/** Connect and disconnect devices through the SDK. */
	SdkConnectDisconnect,

	/** GPU tesselation. */
	Tessellation,

	/** Texture streaming. */
	TextureStreaming,

	/** User credentials are required to use the device. */
	UserCredentials,

	/** The platform uses the mobile forward pipeline */
	MobileRendering,

	/** The platform uses the deferred pipeline, typically PC/Console platforms */
	DeferredRendering,

	/* Should split paks into smaller sized paks */
	ShouldSplitPaksIntoSmallerSizes,
};


/**
 * Flags specifying what is needed to be able to complete and deploy a build.
 */
namespace ETargetPlatformReadyStatus
{
	/** Ready */
	const int32 Ready = 0;

	/** SDK Not Found*/
	const int32 SDKNotFound = 1;

	/** Code Build Not Supported */
	const int32 CodeUnsupported = 2;

	/** Plugins Not Supported */
	const int32 PluginsUnsupported = 4;

	/** Signing Key Not Found */
	const int32 SigningKeyNotFound = 8;

	/** Provision Not Found */
	const int32 ProvisionNotFound = 16;

	/** Manifest Not Found */
	const int32 ManifestNotFound = 32;

	/** Remote Server Name Empty */
	const int32 RemoveServerNameEmpty = 64;

	/** License Not Accepted  */
	const int32 LicenseNotAccepted = 128;
};


/**
 * Interface for target platforms.
 *
 * This interface provides an abstraction for cooking platforms and enumerating actual target devices.
 */
class ITargetPlatform
{
public:

	/**
	 * Add a target device by name.
	 *
	 * @param DeviceName The name of the device to add.
	 * @param bDefault Whether the added device should be the default.
	 * @return true if the device was added, false otherwise.
	 */
	virtual bool AddDevice( const FString& DeviceName, bool bDefault ) = 0;

	/**
	* Returns the name of this platform
	*
	* @return Platform name.
	* @see DisplayName
	*/
	virtual FString PlatformName() const = 0;

	/**
	 * Gets the platform's display name.
	 *
	 * @see PlatformName
	 */
	virtual FText DisplayName() const = 0;

	/**
	 * Checks whether the platform's build requirements are met so that we can do things like package for the platform.
	 *
	 * @param ProjectPath Path to the project.
	 * @param bProjectHasCode true if the project has code, and therefore any compilation based SDK requirements should be checked.
	 * @param OutTutorialPath Let's the platform tell the editor a path to show some information about how to fix any problem.
	 * @param OutDocumentationPath Let's the platform tell the editor a documentation path.
	 * @param CustomizedLogMessage Let's the platform return a customized log message instead of the default for the returned status.
	 * @return A mask of ETargetPlatformReadyStatus flags to indicate missing requirements, or 0 if all requirements are met.
	 */
	virtual int32 CheckRequirements(const FString& ProjectPath, bool bProjectHasCode, FString& OutTutorialPath, FString& OutDocumentationPath, FText& CustomizedLogMessage) const = 0;

	/**
	 * Returns the information about this platform
	 */
	virtual const PlatformInfo::FPlatformInfo& GetPlatformInfo() const = 0;

	/**
	 * Gets the platform's INI name (so an offline tool can load the INI for the given target platform).
	 *
	 * @see PlatformName
	 */
	virtual FString IniPlatformName() const = 0;

	/**
	 * Enables/Disable the device check
	 */
	virtual void EnableDeviceCheck(bool OnOff) = 0;

	/**
	 * Returns all discoverable physical devices.
	 *
	 * @param OutDevices Will contain a list of discovered devices.
	 */
	virtual void GetAllDevices( TArray<ITargetDevicePtr>& OutDevices ) const = 0;

	/**
	 * Gets the best generic data compressor for this platform.
	 *
	 * @return Compression method.
	 */
	virtual ECompressionFlags GetBaseCompressionMethod() const = 0;

	/**
	 * Gets the bit window for compressor for this platform.
	 *
	 * @return Compression bit window.
	 */
	virtual int32 GetCompressionBitWindow() const = 0;

	/**
	 * Generates a platform specific asset manifest given an array of FAssetData.
	 *
	 * @param ChunkMap A map of asset path to ChunkIDs for all of the assets.
	 * @param ChunkIDsInUse A set of all ChunkIDs used by this set of assets.
	 * @return true if the manifest was successfully generated, or if the platform doesn't need a manifest .
	 */
	virtual bool GenerateStreamingInstallManifest( const TMultiMap<FString, int32>& ChunkMap, const TSet<int32>& ChunkIDsInUse ) const = 0;

	/**
	 * Gets the default device.
	 *
	 * Note that not all platforms may have a notion of default devices.
	 *
	 * @return Default device.
	 */
	virtual ITargetDevicePtr GetDefaultDevice() const = 0;

	/** 
	 * Gets an interface to the specified device.
	 *
	 * @param DeviceId The identifier of the device to get.
	 * @return The target device (can be nullptr). 
	 */
	virtual ITargetDevicePtr GetDevice( const FTargetDeviceId& DeviceId ) = 0;

	/**
	 * Checks whether this platform has only editor data (typically desktop platforms).
	 *
	 * @return true if this platform has editor only data, false otherwise.
	 */
	virtual bool HasEditorOnlyData() const = 0;

	/**
	 * Checks whether this platform is only a client (and must connect to a server to run).
	 *
	 * @return true if this platform must connect to a server.
	 */
	virtual bool IsClientOnly() const = 0;

	/**
	 * Checks whether this platform is little endian.
	 *
	 * @return true if this platform is little-endian, false otherwise.
	 */
	virtual bool IsLittleEndian() const = 0;

	/**
	 * Checks whether this platform is the platform that's currently running.
	 *
	 * For example, when running on Windows, the Windows ITargetPlatform will return true
	 * and all other platforms will return false.
	 *
	 * @return true if this platform is running, false otherwise.
	 */
	virtual bool IsRunningPlatform() const = 0;

	/**
	 * Checks whether this platform is only a server.
	 *
	 * @return true if this platform has no graphics or audio, etc, false otherwise.
	 */
	virtual bool IsServerOnly() const = 0;

	/**
	 * Checks whether the platform's SDK requirements are met so that we can do things like
	 * package for the platform
	 *
	 * @param bProjectHasCode true if the project has code, and therefore any compilation based SDK requirements should be checked
	 * @param OutDocumentationPath Let's the platform tell the editor a path to show some documentation about how to set up the SDK
	 * @return true if the platform is ready for use
	 */
	virtual bool IsSdkInstalled(bool bProjectHasCode, FString& OutDocumentationPath) const = 0;

	/**
	 * Checks whether this platform requires cooked data (typically console platforms).
	 *
	 * @return true if this platform requires cooked data, false otherwise.
	 */
	virtual bool RequiresCookedData() const = 0;

	/**
	 * Checks whether this platform requires user credentials (typically server platforms).
	 *
	 * @return true if this platform requires user credentials, false otherwise.
	 */
	virtual bool RequiresUserCredentials() const = 0;

	/**
	 * Returns true if the platform supports the AutoSDK system
	 */
	virtual bool SupportsAutoSDK() const = 0;

	/**
	 * Checks whether this platform supports the specified build target, i.e. Game or Editor.
	 *
	 * @param BuildTarget The build target to check.
	 * @return true if the build target is supported, false otherwise.
	 */
	virtual bool SupportsBuildTarget( EBuildTargets::Type BuildTarget ) const = 0;

	/**
	 * Checks whether the target platform supports the specified feature.
	 *
	 * @param Feature The feature to check.
	 * @return true if the feature is supported, false otherwise.
	 */
	virtual bool SupportsFeature( ETargetPlatformFeatures Feature ) const = 0;

#if WITH_ENGINE
	/**
	 * Gets the format to use for a particular body setup.
	 *
	 * @return Physics format.
	 */
	virtual FName GetPhysicsFormat( class UBodySetup* Body ) const = 0;

	/**
	 * Gets the reflection capture formats this platform needs.
	 *
	 * @param OutFormats Will contain the collection of formats.
	 */
	virtual void GetReflectionCaptureFormats( TArray<FName>& OutFormats ) const = 0;

	/**
	 * Gets the shader formats this platform can use.
	 *
	 * @param OutFormats Will contain the shader formats.
	 */
	virtual void GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const = 0;

	/**
	* Gets the shader formats that have been selected for this target platform
	*
	* @param OutFormats Will contain the shader formats.
	*/
	virtual void GetAllTargetedShaderFormats(TArray<FName>& OutFormats) const = 0;

	/**
	 * Gets the format to use for a particular texture.
	 *
	 * @param Texture The texture to get the format for.
	 * @param OutFormats Will contain the list of supported formats.
	 */
	virtual void GetTextureFormats( const class UTexture* Texture, TArray<FName>& OutFormats ) const = 0;

	/**
	 * Gets the texture formats this platform can use
	 * 
	 * @param OutFormats will contain all the texture formats which are possible for this platform
	 */
	virtual void GetAllTextureFormats( TArray<FName>& OutFormats ) const = 0;

	/**
	 * Gets the format to use for a particular piece of audio.
	 *
	 * @param Wave The sound node wave to get the format for.
	 * @return Name of the wave format.
	 */
	virtual FName GetWaveFormat( const class USoundWave* Wave ) const = 0;


	/**
	* Gets all the formats which can be returned from GetWaveFormat
	*
	* @param output array of all the formats
	*/
	virtual void GetAllWaveFormats( TArray<FName>& OutFormats ) const = 0;

	/**
	 * Gets the texture LOD settings used by this platform.
	 *
	 * @return A texture LOD settings structure.
	 */
	virtual const class UTextureLODSettings& GetTextureLODSettings() const = 0;

	/**
	* Register Basic LOD Settings for this platform
	*/
	virtual void RegisterTextureLODSettings(const class UTextureLODSettings* InTextureLODSettings) = 0;

	/**
	 * Gets the static mesh LOD settings used by this platform.
	 *
	 * @return A static mesh LOD settings structure.
	 */
	virtual const class FStaticMeshLODSettings& GetStaticMeshLODSettings() const = 0;
#endif

	/** 
	 * Package a build for the given platform 
	 * 
	 * @param InPackgeDirectory The directory that contains what needs to be packaged.
	 * @return bool true on success, false otherwise.
	 */ 
	virtual bool PackageBuild(const FString& InPackgeDirectory) = 0;

	/**
	 * Returns true if the platform is part of a family of variants
	 */
	virtual bool SupportsVariants() const = 0;

	/**
	 * Gets the variant display name of this platform.
	 * eg. For Android: "ETC1", "ETC2", ...
	 *
	 * @return FText display name for this platform variant.
	 */
	virtual FText GetVariantDisplayName() const = 0;

	/**
	 * Gets the variant title of this platform family
	 * eg. For Android: "Texture Format"
	 *
	 * @return FText title for what variants mean to this family of platforms.
	 */
	virtual FText GetVariantTitle() const = 0;

	/**
	 * Gets the variant priority of this platform
	 *
	 * @return float priority for this platform variant.
	 */
	virtual float GetVariantPriority() const = 0;

	/** 
	 * Whether or not to send all lower-case filepaths when connecting over a fileserver connection. 
	 */
	virtual bool SendLowerCaseFilePaths() const = 0;

	/**
	* Project settings to check to determine if a build should occurr
	*/
	virtual void GetBuildProjectSettingKeys(FString& OutSection, TArray<FString>& InBoolKeys, TArray<FString>& InIntKeys, TArray<FString>& InStringKeys) const = 0;

public:

	/**
	 * Gets an event delegate that is executed when a new target device has been discovered.
	 */
	DECLARE_EVENT_OneParam(ITargetPlatform, FOnTargetDeviceDiscovered, ITargetDeviceRef /*DiscoveredDevice*/);
	virtual FOnTargetDeviceDiscovered& OnDeviceDiscovered() = 0;

	/**
	 * Gets an event delegate that is executed when a target device has been lost, i.e. disconnected or timed out.
	 */
	DECLARE_EVENT_OneParam(ITargetPlatform, FOnTargetDeviceLost, ITargetDeviceRef /*LostDevice*/);
	virtual FOnTargetDeviceLost& OnDeviceLost() = 0;

public:

	/** Virtual destructor. */
	virtual ~ITargetPlatform() { }
};
