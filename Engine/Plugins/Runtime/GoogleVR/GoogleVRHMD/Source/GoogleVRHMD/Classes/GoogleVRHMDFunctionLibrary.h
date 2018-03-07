// Copyright 2017 Google Inc.

#pragma once

#include "CoreMinimal.h"
#include "IHeadMountedDisplay.h"
#include "IGoogleVRHMDPlugin.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GoogleVRHMDFunctionLibrary.generated.h"

 /** Maps to gvr_safety_region_type */
UENUM(BlueprintType)
enum class ESafetyRegionType : uint8
{
	INVALID		UMETA(DisplayName = "Invalid Safety Region Type"),
	CYLINDER	UMETA(DisplayName = "Cylinder Safety Region Type")
};

/** Enum to specify distortion mesh size */
UENUM(BlueprintType)
enum class EDistortionMeshSizeEnum : uint8
{
	DMS_VERYSMALL	UMETA(DisplayName = "Distortion Mesh Size Very Small 20x20"),
	DMS_SMALL		UMETA(DisplayName = "Distortion Mesh Size Small 40x40"),
	DMS_MEDIUM		UMETA(DisplayName = "Distortion Mesh Size Medium 60x60"),
	DMS_LARGE		UMETA(DisplayName = "Distortion Mesh Size Large 80x80"),
	DMS_VERYLARGE	UMETA(DisplayName = "Distortion Mesh Size Very Large 100x100")
};

/**
 * GoogleVRHMD Extensions Function Library
 */
UCLASS()
class GOOGLEVRHMD_API UGoogleVRHMDFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static bool IsGoogleVRHMDEnabled();

	UFUNCTION(BlueprintPure, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static bool IsGoogleVRStereoRenderingEnabled();

	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	/** Set if the app use sustained performance mode. This can be toggled at run time but note that this function only works on Android build*/
	static void SetSustainedPerformanceModeEnabled(bool bEnable);

	/** Enable/disable distortion correction */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static void SetDistortionCorrectionEnabled(bool bEnable);

	/** Change the default viewer profile */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static bool SetDefaultViewerProfile(const FString& ViewerProfileURL);

	/** Change the size of Distortion mesh */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static void SetDistortionMeshSize(EDistortionMeshSizeEnum MeshSize);

	/** Check if distortion correction is enabled */
	UFUNCTION(BlueprintPure, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static bool GetDistortionCorrectionEnabled();

	/** Get the currently set viewer model */
	UFUNCTION(BlueprintPure, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static FString GetViewerModel();

	/** Get the currently set viewer vendor */
	UFUNCTION(BlueprintPure, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static FString GetViewerVendor();

	/** Was the application launched in Vr. */
	UFUNCTION(BlueprintPure, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static bool IsVrLaunch();

	/** Is the application running in Daydream mode. */
	UFUNCTION(BlueprintPure, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static bool IsInDaydreamMode();

	/** Get the RenderTarget size GoogleVRHMD is using for rendering the scene.
	 *  @return The render target size that is used when rendering the scene.
	*/
	UFUNCTION(BlueprintPure, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static FIntPoint GetGVRHMDRenderTargetSize();

	/** Set the GoogleVR render target size to default value.
	 *  @return The default render target size.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static FIntPoint SetRenderTargetSizeToDefault();

	/** Set the RenderTarget size with a scale factor.
	 *  The scale factor will be multiplied by the maximal effective render target size based on the window size and the viewer.
	 *
	 *  @param ScaleFactor - A float number that is within [0.1, 1.0].
	 *  @param OutRenderTargetSize - The actual render target size it is set to.
	 *  @return true if the render target size changed.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static bool SetGVRHMDRenderTargetScale(float ScaleFactor, FIntPoint& OutRenderTargetSize);

	/** Set the RenderTargetSize with the desired resolution.
	 *  @param DesiredWidth - The width of the render target.
	 *  @param DesiredHeight - The height of the render target.
	 *  @param OutRenderTargetSize - The actually render target size it is set to.
	 *  @return true if the render target size changed.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static bool SetGVRHMDRenderTargetSize(int DesiredWidth, int DesiredHeight, FIntPoint& OutRenderTargetSize);

	/** A scaling factor for the neck model offset, clamped from 0 to 1.
	 * This should be 1 for most scenarios, while 0 will effectively disable
	 * neck model application. This value can be animated to smoothly
	 * interpolate between alternative (client-defined) neck models.
	 *  @param ScaleFactor The new neck model scale.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static void SetNeckModelScale(float ScaleFactor);

	/** A scaling factor for the neck model offset, clamped from 0 to 1.
	 * This should be 1 for most scenarios, while 0 will effectively disable
	 * neck model application. This value can be animated to smoothly
	 * interpolate between alternative (client-defined) neck models.
	 *  @return the current neck model scale.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static float GetNeckModelScale();

	/**
	 * Returns the string representation of the data URI on which this activity's intent is operating.
	 * See Intent.getDataString() in the Android documentation.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static FString GetIntentData();

	/**
	 * Set whether to enable the loading splash screen in daydream app.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR Splash"))
	static void SetDaydreamLoadingSplashScreenEnable(bool enable);

	/**
	 * Set the loading splash screen texture the daydream app wil be using.
	 * Note that this function only works for daydream app.
	 *
	 * @param Texture		A texture asset to be used for rendering the splash screen.
	 * @param UVOffset		A 2D vector for offset the splash screen texture. Default value is (0.0, 0.0)
	 * @param UVSize		A 2D vector specifies which part of the splash texture will be rendered on the screen. Default value is (1.0, 1.0)
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR Splash"))
	static void SetDaydreamLoadingSplashScreenTexture(class UTexture2D* Texture, FVector2D UVOffset = FVector2D(0.0f, 0.0f), FVector2D UVSize = FVector2D(1.0f, 1.0f));

	/**
	 * Get the distance in meter the daydream splash screen will be rendered at
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR Splash"))
	static float GetDaydreamLoadingSplashScreenDistance();

	/**
	 * Set the distance in meter the daydream splash screen will be rendered at
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR Splash"))
	static void SetDaydreamLoadingSplashScreenDistance(float NewDistance);

	/**
	 * Get the render scale of the dayderam splash screen
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR Splash"))
	static float GetDaydreamLoadingSplashScreenScale();

	/**
	 * Set the render scale of the dayderam splash screen
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR Splash"))
	static void SetDaydreamLoadingSplashScreenScale(float NewSize);

	/**
	 * Get the view angle of the dayderam splash screen
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR Splash"))
	static float GetDaydreamLoadingSplashScreenViewAngle();

	/**
	 * Set the view angle of the dayderam splash screen
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR Splash"))
	static void SetDaydreamLoadingSplashScreenViewAngle(float NewViewAngle);

	/**
	 * Clear the loading splash texture it is current using. This will make the loading screen to black if the loading splash screen is still enabled.
	 * Note that this function only works for daydream app.
	 */
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR Splash"))
	static void ClearDaydreamLoadingSplashScreenTexture();

	/**
	* Tries to get the floor height if available
	* @param FloorHeight where the floor height read will get stored
	* returns true is the read was successful, false otherwise
	*/
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static bool GetFloorHeight(float& FloorHeight);

	/**
	* Tries to get the Safety Cylinder Inner Radius if available
	* @param InnerRadius where the Safety Cylinder Inner Radius read will get stored
	* returns true is the read was successful, false otherwise
	*/
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static bool GetSafetyCylinderInnerRadius(float& InnerRadius);

	/**
	* Tries to get the Safety Cylinder Outer Radius if available
	* @param OuterRadius where the Safety Cylinder Outer Radius read will get stored
	* returns true is the read was successful, false otherwise
	*/
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static bool GetSafetyCylinderOuterRadius(float& OuterRadius);

	/**
	* Tries to get the Safety Region Type if available
	* @param RegionType where the Safety Region Type read will get stored
	* returns true is the read was successful, false otherwise
	*/
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static bool GetSafetyRegion(ESafetyRegionType& RegionType);

	/**
	* Tries to get the Recenter Transform if available
	* @param RecenterOrientation where the Recenter Orientation read will get stored
	* @param RecenterPosition where the Recenter Position read will get stored
	* returns true is the read was successful, false otherwise
	*/
	UFUNCTION(BlueprintCallable, Category = "GoogleVRHMD", meta = (Keywords = "Cardboard AVR GVR"))
	static bool GetRecenterTransform(FQuat& RecenterOrientation, FVector& RecenterPosition);
};
