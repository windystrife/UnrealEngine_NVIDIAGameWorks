// Copyright 2017 Google Inc.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GoogleARCorePrimitives.h"
#include "GoogleARCoreAnchor.h"
#include "GoogleARCoreAnchorActor.h"
#include "Engine/LatentActionManager.h"
#include "GoogleARCoreFunctionLibrary.generated.h"

/**
 * @ingroup GoogleARCoreBase
 * Describes whether Google ARCore is supported on a device.
 */
UENUM(BlueprintType)
enum class EGoogleARCoreSupportEnum : uint8
{
	/** Google ARCore is not supported. */
	NotSupported,
	/** Google ARCore is supported. */
	Supported,
};

/**
 * @ingroup GoogleARCoreBase
 * Describes the Google ARCore session status.
 */
UENUM(BlueprintType)
enum class EGoogleARCoreSessionStatus : uint8
{
	/** Tracking session hasn't started yet.*/
	NotStarted,
	/** Tracking session has started but hasn't got valid tracking data yet.*/
	NotTracking,
	/** Tracking session is currently tracking.*/
	Tracking
};

/** A function library that provides static/Blueprint functions associated with GoogleARCore session.*/
UCLASS()
class GOOGLEARCOREBASE_API UGoogleARCoreSessionFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//-----------------Lifecycle---------------------
	/**
	 * Checks whether Google ARCore is supported.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|Session|Configuration", meta = (Keywords = "googlear googlearcore supported"))
	static EGoogleARCoreSupportEnum IsGoogleARCoreSupported();

	/**
	 * Gets a copy of the current FGoogleARCoreSessionConfig that Google ARCore is configured with.
	 *
	 * @param OutCurrentTangoConfig		Return a copy of the current session configuration.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|Session|Configuration", meta = (Keywords = "googlear googlearcore config"))
	static void GetCurrentSessionConfig(FGoogleARCoreSessionConfig& OutCurrentTangoConfig);

	/**
	 * Gets a list of the required runtime permissions for the current configuration suitable
	 * for use with the AndroidPermission plugin.
	 *
	 * @param OutRuntimePermissions		The returned runtime permissions.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|Session|Configuration", meta = (Keywords = "googlear googlearcore permission"))
	static void GetSessionRequiredRuntimPermissions(TArray<FString>& OutRuntimePermissions)
	{
		FGoogleARCoreSessionConfig Config;
		GetCurrentSessionConfig(Config);
		GetSessionRequiredRuntimPermissionsWithConfig(Config, OutRuntimePermissions);
	}

	/**
	 * Gets a list of the required runtime permissions for the given configuration suitable
	 * for use with the AndroidPermission plugin.
	 *
	 * @param Configuration				The configuration to check for permission.
	 * @param OutRuntimePermissions		The returned runtime permission.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|Session|Configuration", meta = (Keywords = "googlear googlearcore permission"))
	static void GetSessionRequiredRuntimPermissionsWithConfig(const FGoogleARCoreSessionConfig& Configuration, TArray<FString>& OutRuntimePermissions);

	/**
	 * Returns the current ARCore session status.
	 *
	 * @return	A EGoogleARCoreSessionStatus enum that describes the session status.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|Session|Lifecycle", meta = (Keywords = "googlear googlearcore session"))
	static EGoogleARCoreSessionStatus GetSessionStatus();

	/**
	 * Starts the ARCore tracking session with the current configuration.
	 * Note: only valid if AutoConnect is false in your settings.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|Session|Lifecycle", meta = (Keywords = "googlear googlearcore session"))
	static void StartSession();

	/**
	 * Starts a new ARCore tracking session with the provided configuration.
	 * Note: only valid if AutoConnect is false in your settings.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|Session|Lifecycle", meta = (Keywords = "googlear googlearcore session config"))
	static void StartSessionWithConfig(const FGoogleARCoreSessionConfig& Configuration);

	/**
	 * Stops the current ARCore tracking session.
	 * Note: only valid if AutoConnect is false in your settings.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|Session|Lifecycle", meta = (Keywords = "googlear googlearcore session"))
	static void StopSession();

	//-----------------PassthroughCamera---------------------

	/**
	 * Returns the state of the passthrough camera rendering in GoogleARCore HMD.
	 *
	 * @return	True if the passthrough camera is enabled.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|Session|PassthroughCamera", meta = (Keywords = "googlear googlearcore passthrough camera"))
	static bool IsPassthroughCameraRenderingEnabled();

	/**
	 * Enables/Disables the passthrough camera rendering in GoogleARCore HMD.
	 * Note that when passthrough camera rendering is enabled, the camera FOV will be forced
	 * to match FOV of the physical camera on the device.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|Session|PassthroughCamera", meta = (Keywords = "googlear googlearcore passthrough camera"))
	static void SetPassthroughCameraRenderingEnabled(bool bEnable);

	/**
	 * Gets the texture coordinate information about the passthrough camera texture.
	 *
	 * @return	An array of texture coordinates that can be used to sample the camera texture
	 * correctly based on the current screen size.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|Session|PassthroughCamera", meta = (Keywords = "googlear googlearcore passthrough camera uv"))
	static void GetPassthroughCameraImageUV(TArray<FVector2D>& CameraImageUV);

	//------------------ARAnchor---------------------
	/**
	 * Spawns a GoogleARCoreAnchorActor and creates a GoogleARCoreAnchor object at the given
	 * world transform to provide a fixed reference point in the real world. The
	 * GoogleARCoreAnchorActor will automatically update its transform using the latest pose
	 * on the GoogleARCoreAnchor object.
	 *
	 * @param WorldContextObject		The world context.
	 * @param ARAnchorActorClass		The class type of ARAnchor Actor. You can either use the GoogleARCoreAnchorActor or a subclass actor that inherits from it.
	 * @param ARAnchorWorldTransform	The world transform where the ARAnchor Actor will be spawned.
	 * @param OutARAnchorActor			The ARAnchorActor reference it spawns.
	 * @return True if the Anchor Actor created successfully.
	 */
	UFUNCTION(BlueprintCallable,  Category = "GoogleARCore|Session|ARAnchor", meta = (WorldContext = "WorldContextObject", Keywords = "googlear googlearcore anchor aranchor"))
	static bool SpawnARAnchorActor(UObject* WorldContextObject, UClass* ARAnchorActorClass, const FTransform& ARAnchorWorldTransform, AGoogleARCoreAnchorActor*& OutARAnchorActor);

	/**
	 * Creates a GoogleARCoreAnchor object at the given world transform to provide a fixed
	 * reference point in the real world that can update to reflect changing knowledge of
	 * the scene. You can either use the ARAnchor object directly by querying the pose or
	 * hook it up with an ARAnchorActor.
	 *
	 * @param ARAnchorWorldTransform	The world transform where the anchor is at.
	 * @param OutARAnchorObject			The ARAnchor object reference it created.
	 * @return True if the Anchor Actor created successfully.
	 */
	UFUNCTION(BlueprintCallable,  Category = "GoogleARCore|Session|ARAnchor", meta = (Keywords = "googlear googlearcore anchor aranchor"))
	static bool CreateARAnchorObject(const FTransform& ARAnchorWorldTransform, UGoogleARCoreAnchor*& OutARAnchorObject);

	/**
	 * Removes the ARAnchor object from the current tracking session. After removal, the
	 * ARAnchor object will stop updating the pose and will be garbage collected if no
	 * other reference is kept.
	 *
	 * @param ARAnchorObject	ARAnchor object reference to be removed.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|Session|ARAnchor", meta = (Keywords = "googlear googlearcore anchor aranchor remove"))
	static void RemoveGoogleARAnchorObject(UGoogleARCoreAnchorBase* ARAnchorObject);

	//-------------------HitTest-------------------------
	/**
	 * Traces a ray against the feature point cloud and returns the feature point that is
	 * closest to the ray.
	 *
	 * @param WorldContextObject	The world context.
	 * @param Start					Start location of the ray.
	 * @param End					End location of the ray.
	 * @param ImpactPoint			The world location of the closest feature point.
	 * @return						True if there is a hit detected.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|Session|LineTrace", meta = (WorldContext = "WorldContextObject", Keywords = "googlear googlearcore raycast hit"))
	static bool LineTraceSingleOnFeaturePoints(UObject* WorldContextObject, const FVector& Start, const FVector& End, FVector& ImpactPoint);

	/**
	 * Traces a ray against all the planes detected by GoogleARCore and returns the first
	 * hit point and the plane.
	 *
	 * @param WorldContextObject	The world context.
	 * @param Start					Start location of the ray.
	 * @param End					End location of the ray.
	 * @param ImpactPoint			The world location of the hit test.
	 * @param ImpactNormal			The world normal of the hit test.
	 * @param OutPlaneObject		The plane object that the hit test detected.
	 * @param bCheckBoundingBoxOnly	When set to true, the hit test will only test against the plane bounding box instead of the polygon.
	 * @return						True if there is a hit detected.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|Session|LineTrace", meta = (WorldContext = "WorldContextObject", Keywords = "googlear googlearcore raycast hit"))
	static bool LineTraceSingleOnPlanes(UObject* WorldContextObject, const FVector& Start, const FVector& End, FVector& ImpactPoint, FVector& ImpactNormal, UGoogleARCorePlane*& OutPlaneObject, bool bCheckBoundingBoxOnly = false);
};

/** A function library that provides static/Blueprint functions associated with most recent GoogleARCore tracking frame.*/
UCLASS()
class GOOGLEARCOREBASE_API UGoogleARCoreFrameFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Gets the latest tracking pose of the device or camera.
	 *
	 * Note that ARCore motion tracking has already integrated with HMD and the motion controller interface.
	 * Use this function only if you need to implement your own tracking component.
	 *
	 * @param PoseType			The type of pose to query.
	 * @param OutTangoPose		The requested pose data.
	 * @return					True if the pose is updated successfully for this frame.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|Frame|Motion", meta = (Keywords = "googlear googlearcore pose transform"))
	static bool GetLatestPose(EGoogleARCorePoseType PoseType, FGoogleARCorePose& OutTangoPose);

	/**
	 * Gets a list of all the GoogleARCorePlane objects that are tracked by the tracking session.
	 *
	 * @param OutARCorePlaneList	A list of pointers to GoogleARCorePlane objects.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|Frame|Plane", meta = (Keywords = "googlear googlearcore pose transform"))
	static void GetAllPlanes(TArray<UGoogleARCorePlane*>& OutARCorePlaneList);

	/**
	 * Gets the latest light estimation based on the passthrough camera image.
	 *
	 * @param OutPixelIntensity		The average pixel intensity of the latest passthrough camera image.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|Frame|LightEstimation", meta = (Keywords = "googlear googlearcore light ambient"))
	static void GetLatestLightEstimation(float& OutPixelIntensity);

	/**
	 * Gets the raw pointer to the latest point cloud in local space and the pose associated to it.
	 * You can use the LocalToWorldTransfrom in the struct to transform the point to Unreal world space.
	 * Note that the OutPointCloudData is only guaranteed to be valid for one frame. C++ only.

	 * @return  A FGoogleARCorePointCloud struct that contains a pointer to the raw point cloud data, its ARCore timestamp and a localToWorld transform.
	 */
	static FGoogleARCorePointCloud GetLatestPointCloud();

};

