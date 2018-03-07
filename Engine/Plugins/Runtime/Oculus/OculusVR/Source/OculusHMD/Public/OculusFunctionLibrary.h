// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "IHeadMountedDisplay.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "IOculusHMDModule.h"
#include "OculusFunctionLibrary.generated.h"

namespace OculusHMD
{
	class FOculusHMD;
}

/* Tracked device types corresponding to ovrTrackedDeviceType enum*/
UENUM(BlueprintType)
enum class ETrackedDeviceType : uint8
{
	None UMETA(DisplayName = "No Devices"),
	HMD	UMETA(DisplayName = "HMD"),
	LTouch	UMETA(DisplayName = "Left Hand"),
	RTouch	UMETA(DisplayName = "Right Hand"),
	Touch		UMETA(DisplayName = "All Hands"),
    DeviceObjectZero    UMETA(DisplayName = "DeviceObject Zero"),
	All	UMETA(DisplayName = "All Devices")
};

USTRUCT(BlueprintType, meta = (DisplayName = "HMD User Profile Data Field"))
struct FHmdUserProfileField
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	FString FieldName;

	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	FString FieldValue;

	FHmdUserProfileField() {}
	FHmdUserProfileField(const FString& Name, const FString& Value) :
		FieldName(Name), FieldValue(Value) {}
};

USTRUCT(BlueprintType, meta = (DisplayName = "HMD User Profile Data"))
struct FHmdUserProfile
{
	GENERATED_USTRUCT_BODY()

	/** Name of the user's profile. */
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	FString Name;

	/** Gender of the user ("male", "female", etc). */
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	FString Gender;

	/** Height of the player, in meters */
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	float PlayerHeight;

	/** Height of the player, in meters */
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	float EyeHeight;

	/** Interpupillary distance of the player, in meters */
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	float IPD;

	/** Neck-to-eye distance, in meters. X - horizontal, Y - vertical. */
	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	FVector2D NeckToEyeDistance;

	UPROPERTY(BlueprintReadWrite, Category = "Input|HeadMountedDisplay")
	TArray<FHmdUserProfileField> ExtraFields;

	FHmdUserProfile() :
		PlayerHeight(0.f), EyeHeight(0.f), IPD(0.f), NeckToEyeDistance(FVector2D::ZeroVector) {}
};

/** DEPRECATED in 4.17 : All functions using this enum have been deprecated and it will be removed with them. If you are still needing this, then please create your own replacement.*/
UENUM(BlueprintType)
enum class EGearVRControllerHandedness_DEPRECATED : uint8
{
	RightHanded_DEPRECATED,
	LeftHanded_DEPRECATED,
	Unknown_DEPRECATED
};

UCLASS()
class OCULUSHMD_API UOculusFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/**
	 * Grabs the current orientation and position for the HMD.  If positional tracking is not available, DevicePosition will be a zero vector
	 *
	 * @param DeviceRotation	(out) The device's current rotation
	 * @param DevicePosition	(out) The device's current position, in its own tracking space
	 * @param NeckPosition		(out) The estimated neck position, calculated using NeckToEye vector from User Profile. Same coordinate space as DevicePosition.
	 * @param bUseOrienationForPlayerCamera	(in) Should be set to 'true' if the orientation is going to be used to update orientation of the camera manually.
	 * @param bUsePositionForPlayerCamera	(in) Should be set to 'true' if the position is going to be used to update position of the camera manually.
	 * @param PositionScale		(in) The 3D scale that will be applied to position.
	 */
	UFUNCTION(BlueprintPure, Category="Input|OculusLibrary")
	static void GetPose(FRotator& DeviceRotation, FVector& DevicePosition, FVector& NeckPosition, bool bUseOrienationForPlayerCamera = false, bool bUsePositionForPlayerCamera = false, const FVector PositionScale = FVector::ZeroVector);

	/**
	* Reports raw sensor data. If HMD doesn't support any of the parameters then it will be set to zero.
	*
	* @param AngularAcceleration	(out) Angular acceleration in radians per second per second.
	* @param LinearAcceleration		(out) Acceleration in meters per second per second.
	* @param AngularVelocity		(out) Angular velocity in radians per second.
	* @param LinearVelocity			(out) Velocity in meters per second.
	* @param TimeInSeconds			(out) Time when the reported IMU reading took place, in seconds.
	*/
	UFUNCTION(BlueprintPure, Category = "Input|OculusLibrary")
	static void GetRawSensorData(FVector& AngularAcceleration, FVector& LinearAcceleration, FVector& AngularVelocity, FVector& LinearVelocity, float& TimeInSeconds, ETrackedDeviceType DeviceType = ETrackedDeviceType::HMD);

	/**
	* Returns if the device is currently tracked by the runtime or not.
	*/
	UFUNCTION(BlueprintPure, Category = "Input|OculusLibrary")
	static bool IsDeviceTracked(ETrackedDeviceType DeviceType);

	/**
	* Returns if the device is currently tracked by the runtime or not.
	*/
	UFUNCTION(BlueprintCallable, Category = "Input|OculusLibrary")
	static void SetCPUAndGPULevels(int CPULevel, int GPULevel);

	/**
	* Returns current user profile.
	*
	* @param Profile		(out) Structure to hold current user profile.
	* @return (boolean)	True, if user profile was acquired.
	*/
	UFUNCTION(BlueprintPure, Category = "Input|OculusLibrary")
	static bool GetUserProfile(FHmdUserProfile& Profile);

	/**
	* Sets 'base rotation' - the rotation that will be subtracted from
	* the actual HMD orientation.
	* Sets base position offset (in meters). The base position offset is the distance from the physical (0, 0, 0) position
	* to current HMD position (bringing the (0, 0, 0) point to the current HMD position)
	* Note, this vector is set by ResetPosition call; use this method with care.
	* The axis of the vector are the same as in Unreal: X - forward, Y - right, Z - up.
	*
	* @param Rotation			(in) Rotator object with base rotation
	* @param BaseOffsetInMeters (in) the vector to be set as base offset, in meters.
	* @param Options			(in) specifies either position, orientation or both should be set.
	*/
	UFUNCTION(BlueprintCallable, Category = "Input|OculusLibrary")
	static void SetBaseRotationAndBaseOffsetInMeters(FRotator Rotation, FVector BaseOffsetInMeters, EOrientPositionSelector::Type Options);

	/**
	* Returns current base rotation and base offset.
	* The base offset is currently used base position offset, previously set by the
	* ResetPosition or SetBasePositionOffset calls. It represents a vector that translates the HMD's position
	* into (0,0,0) point, in meters.
	* The axis of the vector are the same as in Unreal: X - forward, Y - right, Z - up.
	*
	* @param OutRotation			(out) Rotator object with base rotation
	* @param OutBaseOffsetInMeters	(out) base position offset, vector, in meters.
	*/
	UFUNCTION(BlueprintPure, Category = "Input|OculusLibrary")
	static void GetBaseRotationAndBaseOffsetInMeters(FRotator& OutRotation, FVector& OutBaseOffsetInMeters);

	/**
	 * Scales the HMD position that gets added to the virtual camera position.
	 *
	 * @param PosScale3D	(in) the scale to apply to the HMD position.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "This feature is no longer supported."))
	static void SetPositionScale3D(FVector PosScale3D) { }

	/**
	 * Sets 'base rotation' - the rotation that will be subtracted from
	 * the actual HMD orientation.
	 * The position offset might be added to current HMD position,
	 * effectively moving the virtual camera by the specified offset. The addition
	 * occurs after the HMD orientation and position are applied.
	 *
	 * @param BaseRot			(in) Rotator object with base rotation
	 * @param PosOffset			(in) the vector to be added to HMD position.
	 * @param Options			(in) specifies either position, orientation or both should be set.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "A hack, proper camera positioning should be used"))
	static void SetBaseRotationAndPositionOffset(FRotator BaseRot, FVector PosOffset, EOrientPositionSelector::Type Options);

	/**
	 * Returns current base rotation and position offset.
	 *
	 * @param OutRot			(out) Rotator object with base rotation
	 * @param OutPosOffset		(out) the vector with previously set position offset.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "A hack, proper camera positioning should be used"))
	static void GetBaseRotationAndPositionOffset(FRotator& OutRot, FVector& OutPosOffset);

	/**
	 * Adds loading splash screen with parameters
	 *
	 * @param Texture			(in) A texture asset to be used for the splash. GearVR uses it as a path for loading icon; all other params are currently ignored by GearVR.
	 * @param TranslationInMeters (in) Initial translation of the center of the splash screen (in meters).
	 * @param Rotation			(in) Initial rotation of the splash screen, with the origin at the center of the splash screen.
	 * @param SizeInMeters		(in) Size, in meters, of the quad with the splash screen.
	 * @param DeltaRotation		(in) Incremental rotation, that is added each 2nd frame to the quad transform. The quad is rotated around the center of the quad.
	 * @param bClearBeforeAdd	(in) If true, clears splashes before adding a new one.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|OculusLibrary")
	static void AddLoadingSplashScreen(class UTexture2D* Texture, FVector TranslationInMeters, FRotator Rotation, FVector2D SizeInMeters = FVector2D(1.0f, 1.0f), FRotator DeltaRotation = FRotator::ZeroRotator, bool bClearBeforeAdd = false);

	/**
	 * Removes all the splash screens.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|OculusLibrary")
	static void ClearLoadingSplashScreens();

	/**
	 * Shows loading splash screen.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|OculusLibrary")
	static void ShowLoadingSplashScreen();

	/**
	 * Hides loading splash screen.
	 *
	 * @param	bClear	(in) Clear all splash screens after hide.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|OculusLibrary")
	static void HideLoadingSplashScreen(bool bClear = false);

	/**
	 * Enables/disables splash screen to be automatically shown when LoadMap is called.
	 *
	 * @param	bAutoShowEnabled	(in)	True, if automatic showing of splash screens is enabled when map is being loaded.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|OculusLibrary")
	static void EnableAutoLoadingSplashScreen(bool bAutoShowEnabled);

	/**
	 * Returns true, if the splash screen is automatically shown when LoadMap is called.
	 */
	UFUNCTION(BlueprintPure, Category = "Input|OculusLibrary")
	static bool IsAutoLoadingSplashScreenEnabled();

	/**
	 * Sets a texture for loading icon mode and shows it. This call will clear all the splashes.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|OculusLibrary")
	static void ShowLoadingIcon(class UTexture2D* Texture);

	/**
	 * Clears the loading icon. This call will clear all the splashes.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|OculusLibrary")
	static void HideLoadingIcon();

	/**
	 * Returns true, if the splash screen is in loading icon mode.
	 */
	UFUNCTION(BlueprintPure, Category = "Input|OculusLibrary")
	static bool IsLoadingIconEnabled();

	/**
	 * Sets loading splash screen parameters.
	 *
	 * @param TexturePath		(in) A path to the texture asset to be used for the splash. GearVR uses it as a path for loading icon; all other params are currently ignored by GearVR.
	 * @param DistanceInMeters	(in) Distance, in meters, to the center of the splash screen.
	 * @param SizeInMeters		(in) Size, in meters, of the quad with the splash screen.
	 * @param RotationAxes		(in) A vector that specifies the axis of the splash screen rotation (if RotationDelta is specified).
	 * @param RotationDeltaInDeg (in) Rotation delta, in degrees, that is added each 2nd frame to the quad transform. The quad is rotated around the vector "RotationAxes".
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "use AddLoadingSplashScreen / ClearLoadingSplashScreens"))
	static void SetLoadingSplashParams(FString TexturePath, FVector DistanceInMeters, FVector2D SizeInMeters, FVector RotationAxis, float RotationDeltaInDeg);

	/**
	 * Returns loading splash screen parameters.
	 *
	 * @param TexturePath		(out) A path to the texture asset to be used for the splash. GearVR uses it as a path for loading icon; all other params are currently ignored by GearVR.
	 * @param DistanceInMeters	(out) Distance, in meters, to the center of the splash screen.
	 * @param SizeInMeters		(out) Size, in meters, of the quad with the splash screen.
	 * @param RotationAxes		(out) A vector that specifies the axis of the splash screen rotation (if RotationDelta is specified).
	 * @param RotationDeltaInDeg (out) Rotation delta, in degrees, that is added each 2nd frame to the quad transform. The quad is rotated around the vector "RotationAxes".
	 */
	UFUNCTION(BlueprintPure, Category = "Input|OculusLibrary", meta = (DeprecatedFunction, DeprecationMessage = "use AddLoadingSplashScreen / ClearLoadingSplashScreens"))
	static void GetLoadingSplashParams(FString& TexturePath, FVector& DistanceInMeters, FVector2D& SizeInMeters, FVector& RotationAxis, float& RotationDeltaInDeg);

	/**
	* Returns true, if the app has input focus.
	*/
	UFUNCTION(BlueprintPure, Category = "Input|OculusLibrary")
	static bool HasInputFocus();

	/**
	* Returns true, if the system overlay is present.
	*/
	UFUNCTION(BlueprintPure, Category = "Input|OculusLibrary")
	static bool HasSystemOverlayPresent();

	/**
	 * Returns IStereoLayers interface to work with overlays.
	 */
	static class IStereoLayers* GetStereoLayers();

protected:
	static class OculusHMD::FOculusHMD* GetOculusHMD();

private:
	//
	// DEPRECATED functions from the old GearVR module

	/** Set Gear VR CPU and GPU Levels */
	DEPRECATED(4.17, "The Oculus API no longer supports this function. Therefore it has been deprecated, and is no longer available.")
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "GearVR", meta=(DeprecatedFunction, DeprecationMessage="The Oculus API no longer supports this GearVR function."))
	static bool IsPowerLevelStateMinimum();
	DECLARE_FUNCTION(execIsPowerLevelStateMinimum);

	/** Set Gear VR CPU and GPU Levels */
	DEPRECATED(4.17, "The Oculus API no longer supports this function. Therefore it has been deprecated, and is no longer available.")
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "GearVR", meta=(DeprecatedFunction, DeprecationMessage="The Oculus API no longer supports this GearVR function."))
	static bool IsPowerLevelStateThrottled();
	DECLARE_FUNCTION(execIsPowerLevelStateThrottled);

	/** Set Gear VR CPU and GPU Levels */
	DEPRECATED(4.17, "The Oculus API no longer supports this function. Therefore it has been deprecated, and is no longer available.")
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "GearVR", meta=(DeprecatedFunction, DeprecationMessage="The Oculus API no longer supports this GearVR function."))
	static bool AreHeadPhonesPluggedIn();
	DECLARE_FUNCTION(execAreHeadPhonesPluggedIn);

	/** Set Gear VR CPU and GPU Levels */
	DEPRECATED(4.17, "The Oculus API no longer supports this function. Therefore it has been deprecated, and is no longer available.")
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "GearVR", meta=(DeprecatedFunction, DeprecationMessage="The Oculus API no longer supports this GearVR function."))
	static float GetTemperatureInCelsius();
	DECLARE_FUNCTION(execGetTemperatureInCelsius);

	/** Set Gear VR CPU and GPU Levels */
	DEPRECATED(4.17, "The Oculus API no longer supports this function. Therefore it has been deprecated, and is no longer available.")
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "GearVR", meta=(DeprecatedFunction, DeprecationMessage="The Oculus API no longer supports this GearVR function."))
	static float GetBatteryLevel();
	DECLARE_FUNCTION(execGetBatteryLevel);

	/** Set Gear VR CPU and GPU Levels */
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	DEPRECATED(4.17, "The Oculus API no longer supports this function. Therefore it has been deprecated, and is no longer available.")
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "GearVRController", meta=(DeprecatedFunction, DeprecationMessage="The Oculus API no longer supports this GearVR function."))
	static EGearVRControllerHandedness_DEPRECATED GetGearVRControllerHandedness();
	DECLARE_FUNCTION(execGetGearVRControllerHandedness);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** Set Gear VR CPU and GPU Levels */
	DEPRECATED(4.17, "The Oculus API no longer supports this function. Therefore it has been deprecated, and is no longer available.")
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "GearVRController", meta=(DeprecatedFunction, DeprecationMessage="The Oculus API no longer supports this GearVR function."))
	static void EnableArmModel(bool bArmModelEnable);
	DECLARE_FUNCTION(execEnableArmModel);

	/** Is controller active */
	DEPRECATED(4.17, "The Oculus API no longer supports this function. Therefore it has been deprecated, and is no longer available.")
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "GearVRController", meta=(DeprecatedFunction, DeprecationMessage="The Oculus API no longer supports this GearVR function."))
	static bool IsControllerActive();
	DECLARE_FUNCTION(execIsControllerActive);
};
