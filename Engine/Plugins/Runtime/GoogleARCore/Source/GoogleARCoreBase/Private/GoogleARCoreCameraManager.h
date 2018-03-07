// Copyright 2017 Google Inc.

#pragma once
#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "HAL/Event.h"
#include "HAL/CriticalSection.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInterface.h"
#include "HAL/ThreadSafeBool.h"

#if PLATFORM_ANDROID
#include "tango_client_api.h"
#endif

class FGoogleARCorePassthroughCameraRenderer;

enum class ETangoRotation
{
	R_0,
	R_90,
	R_180,
	R_270
};

enum class ImageBufferType{
	RGBA_8888 = 1,       ///< RGBA 8888
	YV12 = 0x32315659,   ///< YV12
	YCrCb_420_SP = 0x11  ///< NV21
};

#if PLATFORM_ANDROID
#define MAX_IMAGE_PLANES TANGO_MAX_IMAGE_PLANES
#else
#define MAX_IMAGE_PLANES 4
#endif
struct CameraImageData
{
	/// The width of the image data.
	uint32 Width;
	/// The height of the image data.
	uint32 Height;
	/// Pixel format of data.
	ImageBufferType Format;
	/// The timestamp of this image.
	int64 TimestampInNs;
	/// Number of planes for the image format of this buffer.
	uint32 NumOfPlanes;
	/// Pointers to the pixel data for each image plane.
	TArray<uint8> ImagePlaneData;
	/// The start index of the each plane.
	int32 ImagePlaneIndex[MAX_IMAGE_PLANES];
	/// Sizes of the image planes.
	int32 ImagePlaneSize[MAX_IMAGE_PLANES];
	/// Row strides for each image plane.
	int32 PlaneRowStride[MAX_IMAGE_PLANES];
	/// Pixel strides for each image plane.
	int32 PlanePixelStride[MAX_IMAGE_PLANES];
};

class FGoogleARCoreCameraManager
{
public:
	FGoogleARCoreCameraManager();
	~FGoogleARCoreCameraManager();

	// public variables:

	// public methods:
	// Called once when tango plugin is loaded.
	void SetDefaultCameraOverlayMaterial(UMaterialInterface* InDefaultCameraOverlayMaterial);
	// Called when Tango service is connected
	bool ConnectTangoColorCamera();
	void DisconnectTangoColorCamera();
	// Get the projection matrix align with the Tango color camera
	FMatrix CalculateColorCameraProjectionMatrix(FIntPoint ViewRectSize);
	// Call this in game thread before enqueue rendering command
	void OnBeginRenderView();

	// Called on game thread to early update the color camera image
	void LockColorCameraTexture_GameThread();

	// Called on render thread to update the camera texture form locked buffer
	void UpdateLockedColorCameraTexture_RenderThread();

	// Called on render thread from Tango HMD to late update the color camera image
	void LateUpdateColorCameraTexture_RenderThread();

	// Called on render thread to render the color camera image to the current render target
	void RenderColorCameraOverlay_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView);

	void UpdateCameraParameters(bool bDisplayOrientationUpdated);

	// Get the camera image buffer for CPU access for this frame.
	void UpdateCameraImageBuffer();

	void UpdateLightEstimation();

	void SetCameraOverlayMaterial(UMaterialInterface* NewMaterialInstance);

	void ResetCameraOverlayMaterialToDefault();

	// Config the class if we want to sync game framerate with color camera rate
	void SetSyncGameFramerateWithCamera(bool bNewValue);

	void SetCopyCameraImageEnabled(bool bInEnabled);

	bool IsCameraParamsInitialized();

	void InitializeCameraParams();
	// Return the last update color camera timestamp
	double GetColorCameraImageTimestamp();
	// Return the color camera FOV;
	float GetCameraFOV();
	// Return the oes texture id
	uint32 GetColorCameraTextureId();
	// Return the color camera texture in UTexture format
	UTexture* GetColorCameraTexture();
	// Return the color camera image dimension based on the current screen rotation
	FIntPoint GetCameraImageDimension();
	// Return the rotation of the color camera image.
	void GetCameraImageUV(TArray<FVector2D>& CameraImageUV);

	void GetLatestLightEstimation(float& PixelIntensity);
#if PLATFORM_ANDROID
	// Called when there is a new texture available.
	void OnNewTextureAvailable();
	void OnImageBufferAvailable(const TangoImage* image, const TangoCameraMetadata* metadata);
#endif

private:
	uint32 ColorCameraTextureId;
	double ColorCameraImageTimestamp;
	FIntPoint PrevViewRectSize;
	bool bSyncGameFramerateToCamera;
	FEvent* NewTextureAvailableEvent;
	FVector2D CameraImageOffset;
	FIntPoint TargetCameraImageDimension;
	bool bCopyCameraImageEnabled;
	double ColorCameraCopyTimestamp;
	UTextureRenderTarget2D* ColorCameraRenderTarget;

	TArray<float> CameraImageUVs;
	FGoogleARCorePassthroughCameraRenderer* VideoOverlayRendererRHI;
	bool bNewCameraTextureAvailable;
	FCriticalSection NewCameraAvailableLock;
	FCriticalSection NewImageDataLock;
	FCriticalSection NewCameraMetadataLock;
#if PLATFORM_ANDROID
	TangoBufferId CurrentCameraBuffer;
	TangoBufferId PreviousCameraBuffer;
	TangoCameraIntrinsics ColorCameraIntrinsics;
	TangoCameraIntrinsics OrientationAlignedIntrinsics;
	TangoCameraMetadata LatestCameraMetaData;
#endif
	CameraImageData* CurrentImageDataBuffer;
	CameraImageData* BackImageDataBuffer;
	TArray<CameraImageData> CameraImageDateBuffers;
	float LatestPixelIntensity;
	long LastLightEsimationTimestamp;

	// private methods:
	ETangoRotation GetDisplayOrientation();
	void UpdateOrientationAlignedCameraIntrinsics(ETangoRotation CurrentCamToDisplayRotation);
	void UpdateCameraImageOffset(ETangoRotation CurrentCamToDisplayRotation, FIntPoint ViewRectSize);
};

DEFINE_LOG_CATEGORY_STATIC(LogGoogleARCoreARCamera, Log, All);
