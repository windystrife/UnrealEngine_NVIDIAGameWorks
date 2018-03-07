// Copyright 2017 Google Inc.

#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"

#if PLATFORM_ANDROID
#include "tango_client_api.h"
#endif

#include "GoogleARCorePrimitives.generated.h"

/// @defgroup GoogleARCoreBase Google ARCore Base
/// The base module for Google ARCore plugin

 /**
  * Wrapper for double-value timestamp used by Google ARCore tracking session.
  */
USTRUCT(BlueprintType)
struct GOOGLEARCOREBASE_API FGoogleARCoreTimestamp
{
	GENERATED_BODY()
public:
	/** Constructor from a timestamp value as double. */
	FGoogleARCoreTimestamp(double Timestamp = 0) : TimestampValue(Timestamp) {};

	/** The timestamp value as double. */
	double TimestampValue;
};

/**
 * A struct that represents the FTransform in Unreal world space along with an ARCore timestamp
 * representing when the transform gets updated.
 */
USTRUCT(BlueprintType)
struct GOOGLEARCOREBASE_API FGoogleARCorePose
{
	GENERATED_BODY()

	/** Position and rotation of the pose. */
	UPROPERTY(BlueprintReadOnly, Category = "GoogleARCore|Pose")
	FTransform Pose;

	/** The ARCore timestamp when the pose is updated. */
	UPROPERTY(BlueprintReadOnly, Category = "GoogleARCore|Pose")
	FGoogleARCoreTimestamp Timestamp;
};

/**
 * @ingroup GoogleARCoreBase
 * Describes what type of plane detection will be performed in ARCore session.
 */
UENUM(BlueprintType)
enum class EGoogleARCorePlaneDetectionMode : uint8
{
	/** Disable plane detection. */
	None,
	/** Track for horizontal plane. */
	HorizontalPlane,
};

/**
 * Holds settings that are used to configure the ARCore session.
 */
USTRUCT(BlueprintType)
struct GOOGLEARCOREBASE_API FGoogleARCoreSessionConfig
{
	GENERATED_BODY()
public:
	/** Indicates whether to automatically start GoogleARCore tracking session after the map loaded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ARCoreSessionConfig")
	bool bAutoConnect = true;

	/** Indicates whether to automatically request the required runtime permissions for this configuration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ARCoreSessionConfig")
	bool bAutoRequestRuntimePermissions = true;

	/** The type of plane detection the tracking session will use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ARCoreSessionConfig")
	EGoogleARCorePlaneDetectionMode PlaneDetectionMode = EGoogleARCorePlaneDetectionMode::HorizontalPlane;

	/** Indicates whether to synchronize the game frame rate with the passthrough camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ARCoreCameraConfig")
	bool bSyncGameFrameRateWithPassthroughCamera = false;

	/**
	 * Link the camera component with the GoogleARCore tracking pose. When enabled, GoogleARCore
	 * HMD will be used to update the camera pose and rendering pass through camera image when
	 * enabled.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ARCoreCameraConfig")
	bool bLinkCameraToGoogleARDevice = true;

	/**
	 * Indicates whether to enable the pass through camera rendering controlled by the GoogleARCore HMD.
	 * If enabled, the camera component field of view will always match the physical camera
	 * on the device.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ARCoreCameraConfig", meta = (EditCondition = "bLinkCameraToGoogleARDevice"))
	bool bEnablePassthroughCameraRendering = true;

	/** Indicates whether two FGoogleARCoreSessionConfig objects are equal. */
	inline bool operator==(const FGoogleARCoreSessionConfig& rhs) const
	{
		return bAutoConnect == rhs.bAutoConnect &&
			bLinkCameraToGoogleARDevice == rhs.bLinkCameraToGoogleARDevice &&
			bEnablePassthroughCameraRendering == rhs.bEnablePassthroughCameraRendering &&
			bSyncGameFrameRateWithPassthroughCamera == rhs.bSyncGameFrameRateWithPassthroughCamera &&
			bAutoRequestRuntimePermissions == rhs.bAutoRequestRuntimePermissions &&
			PlaneDetectionMode == rhs.PlaneDetectionMode;
	}
};

/**
 * @ingroup GoogleARCoreBase
 * The type of pose that is supported to query on every frame.
 */
UENUM(BlueprintType)
enum class EGoogleARCorePoseType : uint8
{
	/**
	 * The physical device.
	 */
	DEVICE = 4,
	/**
	 * The back facing color camera.
	 */
	CAMERA_COLOR = 7
};

/**
 * Hold point cloud data.
 */
struct FGoogleARCorePointCloud
{
	/** A transform that can be used to convert the local point to Unreal world space. */
	FTransform LocalToWorldTransfrom;
	/** The ARCore timestamp indicating when the point cloud is updated. */
	double PointCloudTimestamp;
#if PLATFORM_ANDROID
	/** A pointer to the raw point cloud data. */
	TangoPointCloud* RawPointCloud;
#endif
};

// @cond EXCLUDE_FROM_DOXYGEN
/** A helper class that is used to load the GoogleARCorePassthroughCameraMaterial from its default object. */
UCLASS()
class GOOGLEARCOREBASE_API UGoogleARCoreCameraOverlayMaterialLoader : public UObject
{
	GENERATED_BODY()

public:
	/** A pointer to the camera overlay material that will be used to render the passthrough camera texture as background. */
	UPROPERTY()
	UMaterialInterface* DefaultCameraOverlayMaterial;

	UGoogleARCoreCameraOverlayMaterialLoader()
	{
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultOverlayMaterialRef(TEXT("/GoogleARCore/GoogleARCorePassthroughCameraMaterial.GoogleARCorePassthroughCameraMaterial"));
		DefaultCameraOverlayMaterial = DefaultOverlayMaterialRef.Object;
	}
};

/**
 * Helper class used to expose FGoogleARCoreSessionConfig setting in the Editor plugin settings.
 */
UCLASS(config = Engine, defaultconfig)
class GOOGLEARCOREBASE_API UGoogleARCoreEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, config, Category = "ARCoreProjectConfig", meta = (ShowOnlyInnerProperties))
	FGoogleARCoreSessionConfig DefaultSessionConfig;
};

/**
 * Coordinate reference frames natively supported by GoogleARCore.
 *
 * Values correspond to Tango's TangoCoordinateFrameType enumeration.
 */
enum class EGoogleARCoreReferenceFrame : uint8
{
	/**
	 * Coordinate system for the entire Earth.
	 * See WGS84:
	 * <a href="http://en.wikipedia.org/wiki/World_Geodetic_System">World
	 * Geodetic System</a>
	 */
	GLOBAL_WGS84 = 0 UMETA(Hidden),
	/** Origin within a saved area description. */
	AREA_DESCRIPTION = 1,
	/** Origin when the device started tracking. */
	START_OF_SERVICE = 2,
	/** Immediately previous device pose (deprecated / not well supported). */
	PREVIOUS_DEVICE_POSE UMETA(Hidden) = 3,
	/** Device coordinate frame. */
	DEVICE = 4,
	/** Inertial Measurement Unit. */
	IMU UMETA(Hidden) = 5,
	/** Display. */
	DISPLAY UMETA(Hidden) = 6,
	/** Color camera. */
	CAMERA_COLOR = 7,
	/** Depth camera. */
	CAMERA_DEPTH UMETA(Hidden) = 8,
	/** Fisheye camera. */
	CAMERA_FISHEYE = 9,
	/** Tango unique id. */
	UUID UMETA(Hidden) = 10,
	/** Invalid. */
	INVALID UMETA(Hidden) = 11,
	/** Maximum Allowed. */
	MAX UMETA(Hidden) = 12
};
// @endcond
