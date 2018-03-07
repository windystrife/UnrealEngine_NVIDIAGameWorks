// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "UObject/PropertyPortFlags.h"
#include "IOSRuntimeSettings.generated.h"

UENUM()
enum class EPowerUsageFrameRateLock : uint8
{
    /** Frame rate is not limited. */
    PUFRL_None = 0 UMETA(DisplayName="None"),
        
    /** Frame rate is limited to a maximum of 20 frames per second. */
    PUFRL_20 = 20 UMETA(DisplayName="20 FPS"),
    
    /** Frame rate is limited to a maximum of 30 frames per second. */
    PUFRL_30 = 30 UMETA(DisplayName="30 FPS"),
    
    /** Frame rate is limited to a maximum of 60 frames per second. */
    PUFRL_60 = 60 UMETA(DisplayName="60 FPS"),
};

UENUM()
	enum class EIOSVersion : uint8
{
	/** iOS 6.1 */
	IOS_61 = 6 UMETA(Hidden),

	/** iOS 7 */
	IOS_7 = 7 UMETA(Hidden),

	/** iOS 8 */
	IOS_8 = 8 UMETA(Hidden),

	/** iOS 9 */
	IOS_9 = 9 UMETA(DisplayName = "9.0"),

	/** iOS 10 */
	IOS_10 = 10 UMETA(DisplayName = "10.0"),

	/** iOS 11 */
	IOS_11 = 11 UMETA(DisplayName = "11.0"),

};

UENUM()
enum class EIOSMetalShaderStandard : uint8
{
    /** Metal Shaders Compatible With iOS 8.0/tvOS 9.0 or later (std=ios-metal1.0) */
    IOSMetalSLStandard_1_0 = 0 UMETA(DisplayName="Metal v1.0 (iOS 8.0/tvOS 9.0)"),
    
    /** Metal Shaders Compatible With iOS 9.0/tvOS 9.0 or later (std=ios-metal1.1) */
    IOSMetalSLStandard_1_1 = 1 UMETA(DisplayName="Metal v1.1 (iOS 9.0/tvOS 9.0)"),
    
    /** Metal Shaders Compatible With iOS 10.0/tvOS 10.0 or later (std=ios-metal1.2) */
	IOSMetalSLStandard_1_2 = 2 UMETA(DisplayName="Metal v1.2 (iOS 10.0/tvOS 10.0)"),
	
	/** Metal Shaders Compatible With iOS 11.0/tvOS 11.0 or later (std=ios-metal2.0) */
	IOSMetalSLStandard_2_0 = 3 UMETA(DisplayName="Metal v2.0 (iOS 11.0/tvOS 11.0)"),
};

/**
 *	IOS Build resource file struct, used to serialize filepaths to the configs for use in the build system,
 */
USTRUCT()
struct FIOSBuildResourceFilePath
{
	GENERATED_USTRUCT_BODY()
	
	/**
	 * Custom export item used to serialize FIOSBuildResourceFilePath types as only a filename, no garland.
	 */
	bool ExportTextItem(FString& ValueStr, FIOSBuildResourceFilePath const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
	{
		if (0 != (PortFlags & EPropertyPortFlags::PPF_ExportCpp))
		{
			return false;
		}

		ValueStr += FilePath;
		return true;
	}

	/**
	 * Custom import item used to parse ini entries straight into the filename.
	 */
	bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
	{
		FilePath = Buffer;
		return true;
	}

	/**
	 * The path to the file.
	 */
	UPROPERTY(EditAnywhere, Category = FilePath)
	FString FilePath;
};

/**
 *	Setup our resource filepath to make it easier to parse in UBT
 */
template<>
struct TStructOpsTypeTraits<FIOSBuildResourceFilePath> : public TStructOpsTypeTraitsBase2<FIOSBuildResourceFilePath>
{
	enum
	{
		WithExportTextItem = true,
		WithImportTextItem = true,
	};
};



/**
 *	IOS Build resource file struct, used to serialize Directorys to the configs for use in the build system,
 */
USTRUCT()
struct FIOSBuildResourceDirectory
{
	GENERATED_USTRUCT_BODY()

	/**
	 * Custom export item used to serialize FIOSBuildResourceDirectory types as only a filename, no garland.
	 */
	bool ExportTextItem(FString& ValueStr, FIOSBuildResourceDirectory const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
	{
		if (0 != (PortFlags & EPropertyPortFlags::PPF_ExportCpp))
		{
			return false;
		}

		ValueStr += Path;
		return true;
	}

	/**
	 * Custom import item used to parse ini entries straight into the filename.
	 */
	bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
	{
		Path = Buffer;
		return true;
	}

	/**
	* The path to the file.
	*/
	UPROPERTY(EditAnywhere, Category = Directory)
	FString Path;
};

/**
*	Setup our resource Directory to make it easier to parse in UBT
*/
template<>
struct TStructOpsTypeTraits<FIOSBuildResourceDirectory> : public TStructOpsTypeTraitsBase2<FIOSBuildResourceDirectory>
{
	enum
	{
		WithExportTextItem = true,
		WithImportTextItem = true,
	};
};



/**
 * Implements the settings for the iOS target platform.
 */
UCLASS(config=Engine, defaultconfig)
class IOSRUNTIMESETTINGS_API UIOSRuntimeSettings : public UObject
{
public:
	GENERATED_UCLASS_BODY()

	// Should Game Center support (iOS Online Subsystem) be enabled?
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Online, meta = (ConfigHierarchyEditable))
	uint32 bEnableGameCenterSupport : 1;
	
	// Should Cloud Kit support (iOS Online Subsystem) be enabled?
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Online)
	uint32 bEnableCloudKitSupport : 1;

    // Should push/remote notifications support (iOS Online Subsystem) be enabled?
    UPROPERTY(GlobalConfig, EditAnywhere, Category = Online)
    uint32 bEnableRemoteNotificationsSupport : 1;
    
	// Whether or not to compile iOS Metal shaders for the Mobile renderer (requires iOS 8+ and an A7 processor).
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Rendering, meta = (DisplayName = "Metal Mobile Renderer"))
	bool bSupportsMetal;

	// Whether or not to compile iOS Metal shaders for the desktop Forward renderer (requires iOS 10+ and an A10 processor)
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Rendering, meta = (DisplayName = "Metal Desktop-Forward Renderer"))
	bool bSupportsMetalMRT;
	
	// Whether or not to add support for PVRTC textures
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Rendering, meta = (DisplayName = "Cook PVRTC texture data for OpenGL ES or Metal on A7 and earlier devices"))
	bool bCookPVRTCTextures;

	// Whether or not to add support for ASTC textures
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Rendering, meta = (DisplayName = "Cook ASTC texture data for Metal on A8 or later devices"))
	bool bCookASTCTextures;
	
	// Whether or not to add support for OpenGL ES2 (if this is false, then your game should specify minimum IOS8 version)
	UPROPERTY(GlobalConfig)
	bool bSupportsOpenGLES2;
	
	// Remotely compile shaders offline
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Build)
	bool EnableRemoteShaderCompile;

	// Enable generation of dSYM file
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Build, meta = (DisplayName = "Generate dSYM file for code debugging and profiling"))
	bool bGeneratedSYMFile;

	// Enable generation of dSYM bundle
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Build, meta = (DisplayName = "Generate dSYM bundle for third party crash tools"))
	bool bGeneratedSYMBundle;

	// Enable generation of xcode archive package
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Build, meta = (DisplayName = "Generate xcode archive package"))
	bool bGenerateXCArchive;	
	
	// Enable ArmV7 support? (this will be used if all type are unchecked)
	UPROPERTY(GlobalConfig)
	bool bDevForArmV7;

	// Enable Arm64 support?
	UPROPERTY(GlobalConfig)
	bool bDevForArm64;

	// Enable ArmV7s support?
	UPROPERTY(GlobalConfig)
	bool bDevForArmV7S;

	// Enable ArmV7 support? (this will be used if all type are unchecked)
	UPROPERTY(GlobalConfig)
	bool bShipForArmV7;

	// Enable Arm64 support?
	UPROPERTY(GlobalConfig)
	bool bShipForArm64;

	// Enable ArmV7s support?
	UPROPERTY(GlobalConfig)
	bool bShipForArmV7S;

	// Enable bitcode compiling?
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Build, meta = (DisplayName = "Support bitcode in Shipping"))
	bool bShipForBitcode;
	
	// Any additional linker flags to pass to the linker in non-shipping builds
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Build, meta = (DisplayName = "Additional Non-Shipping Linker Flags", ConfigHierarchyEditable))
	FString AdditionalLinkerFlags;

	// Any additional linker flags to pass to the linker in shipping builds
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Build, meta = (DisplayName = "Additional Shipping Linker Flags", ConfigHierarchyEditable))
	FString AdditionalShippingLinkerFlags;

	// The name or ip address of the remote mac which will be used to build IOS
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Build", meta = (ConfigHierarchyEditable))
	FString RemoteServerName;

	// Enable the use of RSync for remote builds on a mac
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Build", meta = (DisplayName = "Use RSync for building IOS", ConfigHierarchyEditable))
	bool bUseRSync;

	// The mac users name which matches the SSH Private Key, for remote builds using RSync.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Build", meta = (EditCondition = "bUseRSync", DisplayName = "Username on Remote Server.", ConfigHierarchyEditable))
	FString RSyncUsername;

	// The install directory of DeltaCopy.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Build", meta = (EditCondition = "bUseRSync", ConfigHierarchyEditable))
	FIOSBuildResourceDirectory DeltaCopyInstallPath;

	// The existing location of an SSH Key found by UE4.
	UPROPERTY(VisibleAnywhere, Category = "Build", meta = (DisplayName = "Found Existing SSH permissions file"))
	FString SSHPrivateKeyLocation;

	// The path of the ssh permissions key to be used when connecting to the remote server.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Build", meta = (EditCondition = "bUseRSync", DisplayName = "Override existing SSH permissions file", ConfigHierarchyEditable))
	FIOSBuildResourceFilePath SSHPrivateKeyOverridePath;

	// If checked, the Siri Remote will act as a separate controller Id from any connected controllers. If unchecked, the remote and the first connected controller will share an ID (and control the same player)
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Input, meta = (DisplayName = "Treat AppleTV Remote as separate controller"))
	bool bTreatRemoteAsSeparateController;

	// If checked, the Siri Remote can be rotated to landscape view
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Input, meta = (DisplayName = "Allow AppleTV Remote landscape mode"))
	bool bAllowRemoteRotation;
	
	// If checked, the trackpad is a virtual joystick (acts like the left stick of a controller). If unchecked, the trackpad will send touch events
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Input, meta = (DisplayName = "Use AppleTV trackpad as virtual joystick"))
	bool bUseRemoteAsVirtualJoystick;
	
	// If checked, the center of the trackpad is 0,0 (center) for the virtual joystick. If unchecked, the location the user taps becomes 0,0
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Input, meta = (DisplayName = "Use AppleTV Remote absolute trackpad values"))
	bool bUseRemoteAbsoluteDpadValues;
	
	// Supports default portrait orientation. Landscape will not be supported.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = DeviceOrientations)
	uint32 bSupportsPortraitOrientation : 1;

	// Supports upside down portrait orientation. Landscape will not be supported.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = DeviceOrientations)
	uint32 bSupportsUpsideDownOrientation : 1;

	// Supports left landscape orientation. Portrait will not be supported.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = DeviceOrientations)
	uint32 bSupportsLandscapeLeftOrientation : 1;

	// Supports right landscape orientation. Portrait will not be supported.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = DeviceOrientations)
	uint32 bSupportsLandscapeRightOrientation : 1;

	// Specifies the the display name for the application. This will be displayed under the icon on the device.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = BundleInformation)
	FString BundleDisplayName;

	// Specifies the the name of the application bundle. This is the short name for the application bundle.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = BundleInformation)
	FString BundleName;

	// Specifies the bundle identifier for the application.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = BundleInformation)
	FString BundleIdentifier;

	// Specifies the version for the application.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = BundleInformation)
	FString VersionInfo;

	/** Set the maximum frame rate to save on power consumption */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = PowerUsage, meta = (ConfigHierarchyEditable))
	EPowerUsageFrameRateLock FrameRateLock;

	// Minimum iOS version this game supports
	UPROPERTY(GlobalConfig, EditAnywhere, Category = OSInfo, meta = (DisplayName = "Minimum iOS Version"))
	EIOSVersion MinimumiOSVersion;

	// Whether or not to add support for iPad devices
	UPROPERTY(GlobalConfig, EditAnywhere, Category = DeviceUsage)
	uint32 bSupportsIPad : 1;

	// Whether or not to add support for iPhone devices
	UPROPERTY(GlobalConfig, EditAnywhere, Category = DeviceUsage)
	uint32 bSupportsIPhone : 1;

	// Any additional plist key/value data utilizing \n for a new line
	UPROPERTY(GlobalConfig, EditAnywhere, Category = ExtraData)
	FString AdditionalPlistData;

	// Whether the app supports Facebook
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Online)
	bool bEnableFacebookSupport;

	// Facebook App ID obtained from Facebook's Developer Centre
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Online, meta = (EditCondition = "bEnableFacebookSupport"))
	FString FacebookAppID;
    
    // Mobile provision to utilize when signing
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Build)
	FString MobileProvision;

	// Signing certificate to utilize when signing
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Build)
	FString SigningCertificate;
	
	// Whether to use automatic signing through Xcode
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Build)
	bool bAutomaticSigning;

    // The maximum supported Metal shader langauge version.
    // This defines what features may be used and OS versions supported.
    UPROPERTY(EditAnywhere, config, Category=Rendering, meta = (DisplayName = "Max. Metal Shader Standard To Target", ConfigRestartRequired = true))
	uint8 MaxShaderLanguageVersion;
	
	/**
	 * Whether to use the Metal shading language's "fast" intrinsics.
	 * Fast intrinsics assume that no NaN or INF value will be provided as input,
	 * so are more efficient. However, they will produce undefined results if NaN/INF
	 * is present in the argument/s.
	 */
	UPROPERTY(EditAnywhere, config, Category=Rendering, meta = (DisplayName = "Use Fast-Math intrinsics", ConfigRestartRequired = true))
	bool UseFastIntrinsics;
	
	/**
	 * Whether to use of Metal shader-compiler's -ffast-math optimisations.
	 * Fast-Math performs algebraic-equivalent & reassociative optimisations not permitted by the floating point arithmetic standard (IEEE-754).
	 * These can improve shader performance at some cost to precision and can lead to NaN/INF propagation as they rely on
	 * shader inputs or variables not containing NaN/INF values. By default fast-math is enabled for performance.
	 */
	UPROPERTY(EditAnywhere, config, Category=Rendering, meta = (DisplayName = "Enable Fast-Math optimisations", ConfigRestartRequired = true))
	bool EnableMathOptimisations;
	
	// Whether or not the keyboard should be usable on it's own without a UITextField
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Input)
	bool bUseIntegratedKeyboard;

	/** Sample rate to run the audio mixer with. */
	UPROPERTY(config, EditAnywhere, Category = "Audio", Meta = (DisplayName = "Audio Mixer Sample Rate"))
	int32 AudioSampleRate;

	/** The amount of audio to compute each callback block. Lower values decrease latency but may increase CPU cost. */
	UPROPERTY(config, EditAnywhere, Category = "Audio", meta = (ClampMin = "512", ClampMax = "4096", DisplayName = "Callback Buffer Size"))
	int32 AudioCallbackBufferFrameSize;

	/** The number of buffers to keep enqueued. More buffers increases latency, but can compensate for variable compute availability in audio callbacks on some platforms. */
	UPROPERTY(config, EditAnywhere, Category = "Audio", meta = (ClampMin = "1", UIMin = "1", DisplayName = "Number of Buffers To Enqueue"))
	int32 AudioNumBuffersToEnqueue;

	/** The max number of channels (voices) to limit for this platform. The max channels used will be the minimum of this value and the global audio quality settings. A value of 0 will not apply a platform channel count max. */
	UPROPERTY(config, EditAnywhere, Category = "Audio", meta = (ClampMin = "0", UIMin = "0", DisplayName = "Max Channels"))
	int32 AudioMaxChannels;

	/** The number of workers to use to compute source audio. Will only use up to the max number of sources. Will evenly divide sources to each source worker. */
	UPROPERTY(config, EditAnywhere, Category = "Audio", meta = (ClampMin = "0", UIMin = "0", DisplayName = "Number of Source Workers"))
	int32 AudioNumSourceWorkers;

	/** Which of the currently enabled spatialization plugins to use on Windows. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString SpatializationPlugin;

	/** Which of the currently enabled reverb plugins to use on Windows. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString ReverbPlugin;

	/** Which of the currently enabled occlusion plugins to use on Windows. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString OcclusionPlugin;

#if WITH_EDITOR
	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostInitProperties() override;
	// End of UObject interface
#endif
};
