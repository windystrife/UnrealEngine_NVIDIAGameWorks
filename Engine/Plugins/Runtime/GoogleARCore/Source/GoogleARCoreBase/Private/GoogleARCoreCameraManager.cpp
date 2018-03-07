// Copyright 2017 Google Inc.

#include "GoogleARCoreCameraManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/UnrealMemory.h"
#include "Engine/Engine.h"
#include "Misc/ScopeLock.h"
#include "TextureResource.h"
#include "GoogleARCorePassthroughCameraRenderer.h"
#include "Engine/GameViewportClient.h"
#if PLATFORM_ANDROID
#include "GoogleARCoreAndroidHelper.h"
#include "tango_client_api2.h"
#include "tango_support_api.h"

#define BUFFER_NOT_ASSIGNED -1
#endif

#define ENABLE_CAMERABUFFER_DEBUG_LOGGING 0

////////////////////////////////////////
// Begin TangoARCamera Public Methods //
////////////////////////////////////////

FGoogleARCoreCameraManager::FGoogleARCoreCameraManager()
	: ColorCameraTextureId(0)
	, ColorCameraImageTimestamp(-1.0f)
	, PrevViewRectSize(0, 0)
	, bSyncGameFramerateToCamera(false)
	, NewTextureAvailableEvent(nullptr)
	, CameraImageOffset(0.0f, 0.0f)
	, TargetCameraImageDimension(0, 0)
	, bCopyCameraImageEnabled(false)
	, ColorCameraCopyTimestamp(-1.0f)
	, ColorCameraRenderTarget(nullptr)
	, CameraImageUVs{ 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0 }
	, bNewCameraTextureAvailable(false)
#if PLATFORM_ANDROID
	, CurrentCameraBuffer(BUFFER_NOT_ASSIGNED)
	, PreviousCameraBuffer(BUFFER_NOT_ASSIGNED)
#endif
	, LatestPixelIntensity(0.0f)
	, LastLightEsimationTimestamp(0.0f)
{
	VideoOverlayRendererRHI = new FGoogleARCorePassthroughCameraRenderer();
	CameraImageDateBuffers.SetNum(2);
	CurrentImageDataBuffer = &CameraImageDateBuffers[0];
	BackImageDataBuffer = &CameraImageDateBuffers[1];
}

FGoogleARCoreCameraManager::~FGoogleARCoreCameraManager()
{
	if (ColorCameraRenderTarget != nullptr)
	{
		ColorCameraRenderTarget->RemoveFromRoot();
		ColorCameraRenderTarget = nullptr;
	}
	delete VideoOverlayRendererRHI;
}
#if PLATFORM_ANDROID
static void OnTextureAvailableCallback(void* TangoARCameraContext, TangoCameraId id)
{
	((FGoogleARCoreCameraManager*)TangoARCameraContext)->OnNewTextureAvailable();
}

static void OnImageAvailableCallback(void* TangoARCameraContext, TangoCameraId id, const TangoImage* image, const TangoCameraMetadata* metadata)
{
	((FGoogleARCoreCameraManager*)TangoARCameraContext)->OnImageBufferAvailable(image, metadata);
}

void FGoogleARCoreCameraManager::OnNewTextureAvailable()
{
	FScopeLock ScopeLock(&NewCameraAvailableLock);
	if (bSyncGameFramerateToCamera)
	{
		NewTextureAvailableEvent->Trigger();
	}
	bNewCameraTextureAvailable = true;

#if ENABLE_CAMERABUFFER_DEBUG_LOGGING
	UE_LOG(LogGoogleARCoreARCamera, Log, TEXT("OnNewTextureAvailable!"));
#endif
}

void FGoogleARCoreCameraManager::OnImageBufferAvailable(const TangoImage* image, const TangoCameraMetadata* metadata)
{
	{
		FScopeLock ScopeLock(&NewImageDataLock);
		BackImageDataBuffer->Width = image->width;
		BackImageDataBuffer->Height = image->height;
		BackImageDataBuffer->TimestampInNs = image->timestamp_ns;
		BackImageDataBuffer->NumOfPlanes = image->num_planes;

		uint8* StartPtr = image->plane_data[0];
		uint8* EndPtr = image->plane_data[0] + image->plane_size[0];
		for (int i = 1; i < image->num_planes; i++)
		{
			if (image->plane_data[i] < StartPtr)
			{
				StartPtr = image->plane_data[i];
			}

			if (EndPtr <= image->plane_data[i] + image->plane_size[i])
			{
				EndPtr = image->plane_data[i] + image->plane_size[i];
			}
		}
		check(EndPtr != nullptr);
		check(StartPtr < EndPtr);

		int DataLength = EndPtr - StartPtr;

		for (int i = 0; i < image->num_planes; i++)
		{
			int StartIndex = image->plane_data[i] - StartPtr;
			BackImageDataBuffer->ImagePlaneIndex[i] = StartIndex;
		}

		BackImageDataBuffer->ImagePlaneData.Empty(DataLength);
		BackImageDataBuffer->ImagePlaneData.Append(StartPtr, DataLength);

		FMemory::Memcpy(&BackImageDataBuffer->ImagePlaneSize, &image->plane_size, sizeof(image->plane_size));
		FMemory::Memcpy(&BackImageDataBuffer->PlaneRowStride, &image->plane_row_stride, sizeof(image->plane_size));
		FMemory::Memcpy(&BackImageDataBuffer->PlanePixelStride, &image->plane_pixel_stride, sizeof(image->plane_size));
	}

	{
		FScopeLock ScopeLock(&NewCameraMetadataLock);
		FMemory::Memcpy(&LatestCameraMetaData, metadata, sizeof(TangoCameraMetadata));
	}

}
#endif

void FGoogleARCoreCameraManager::SetDefaultCameraOverlayMaterial(UMaterialInterface* InDefaultCameraOverlayMaterial)
{
	VideoOverlayRendererRHI->SetDefaultCameraOverlayMaterial(InDefaultCameraOverlayMaterial);
}

bool FGoogleARCoreCameraManager::ConnectTangoColorCamera()
{
#if PLATFORM_ANDROID
	TangoErrorType RequestResult = TangoService_connectOnTextureAvailable(TANGO_CAMERA_COLOR, this, OnTextureAvailableCallback);
	if (RequestResult != TANGO_SUCCESS)
	{
		UE_LOG(LogGoogleARCoreARCamera, Error, TEXT("Failed to connect OnTextureAvailable"));
		return false;
	}

	RequestResult = TangoService_connectOnImageAvailable(TANGO_CAMERA_COLOR, this, OnImageAvailableCallback);
	if (RequestResult != TANGO_SUCCESS)
	{
		UE_LOG(LogGoogleARCoreARCamera, Error, TEXT("Failed to connect onImageAvailable"));
		return false;
	}

	return true;
#endif
	return false;
}

void FGoogleARCoreCameraManager::DisconnectTangoColorCamera()
{
	// We shoudn't call disconnect OnTextureAvailable callback anymore in client api2 so do nothing here.
}

void FGoogleARCoreCameraManager::UpdateCameraParameters(bool bDisplayOrientationChanged)
{
	if (bDisplayOrientationChanged)
	{
		ETangoRotation CurrentDisplayRotation = GetDisplayOrientation();
		UpdateOrientationAlignedCameraIntrinsics(CurrentDisplayRotation);
		if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
		{
			UpdateCameraImageOffset(CurrentDisplayRotation, GEngine->GameViewport->Viewport->GetSizeXY());
		}
	}
}

FMatrix FGoogleARCoreCameraManager::CalculateColorCameraProjectionMatrix(FIntPoint ViewRectSize)
{
#if PLATFORM_ANDROID
	// We only need to update the camera image offset when view rect size chagned.
	// Orientation change will be handled in UpdateCameraParameters
	if (ViewRectSize != PrevViewRectSize)
	{
		ETangoRotation CurrentDisplayRotation = GetDisplayOrientation();
		UpdateCameraImageOffset(CurrentDisplayRotation, ViewRectSize);

		TargetCameraImageDimension.X = ViewRectSize.X;
		TargetCameraImageDimension.Y = ViewRectSize.Y;
		if (ColorCameraRenderTarget != nullptr)
		{
			ColorCameraRenderTarget->InitAutoFormat(TargetCameraImageDimension.X, TargetCameraImageDimension.Y);
		}
	}

	float TanHalfFOVX = 0.5f * OrientationAlignedIntrinsics.width / OrientationAlignedIntrinsics.fx * (1.0f - 2 * CameraImageOffset.X);
	float Width = ViewRectSize.X;
	float Height = ViewRectSize.Y;
	float MinZ = GNearClippingPlane;

	// We force it to use infinite far plane.
	FMatrix ProjectionMatrix = FMatrix(
		FPlane(1.0f / TanHalfFOVX,	0.0f,							0.0f,	0.0f),
		FPlane(0.0f,				Width / TanHalfFOVX / Height,	0.0f,	0.0f),
		FPlane(0.0f,				0.0f,							0.0f,	1.0f),
		FPlane(0.0f,				0.0f,							MinZ,	0.0f));

	// TODO: Verify the offset is correct
	float OffCenterProjectionOffsetX = 2.0f * (OrientationAlignedIntrinsics.cx / (float)OrientationAlignedIntrinsics.width - 0.5f);
	float OffCenterProjectionOffsetY = 2.0f * (OrientationAlignedIntrinsics.cy / (float)OrientationAlignedIntrinsics.height - 0.5f);

	float Left = -1.0f + OffCenterProjectionOffsetX;
	float Right = Left + 2.0f;
	float Bottom = -1.0f + OffCenterProjectionOffsetY;
	float Top = Bottom + 2.0f;
	ProjectionMatrix.M[2][0] = (Left + Right) / (Left - Right);
	ProjectionMatrix.M[2][1] = (Bottom + Top) / (Bottom - Top);

	PrevViewRectSize = ViewRectSize;

	return ProjectionMatrix;
#else
	return FMatrix::Identity;
#endif
}

void FGoogleARCoreCameraManager::OnBeginRenderView()
{
	if (ColorCameraRenderTarget == nullptr && bCopyCameraImageEnabled)
	{
		ColorCameraRenderTarget = NewObject<UTextureRenderTarget2D>();
		check(ColorCameraRenderTarget);
		ColorCameraRenderTarget->AddToRoot();
		ColorCameraRenderTarget->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
		ColorCameraRenderTarget->InitCustomFormat(TargetCameraImageDimension.X, TargetCameraImageDimension.Y, PF_B8G8R8A8, false);
	}

	VideoOverlayRendererRHI->InitializeOverlayMaterial();
}

void FGoogleARCoreCameraManager::LockColorCameraTexture_GameThread()
{
//#if PLATFORM_ANDROID
//	check(IsInGameThread());
//
//	FScopeLock ScopeLock(&NewCameraAvailableLock);
//
//#if ENABLE_CAMERABUFFER_DEBUG_LOGGING
//	UE_LOG(LogGoogleARCoreARCamera, Log, TEXT("GameThread: LockColorCameraTexture"));
//#endif
//	// Early return if no camera texture is available
//	if (!bNewCameraTextureAvailable)
//	{
//#if ENABLE_CAMERABUFFER_DEBUG_LOGGING
//		UE_LOG(LogGoogleARCoreARCamera, Log, TEXT("GameThread: No camera texture available. Skip buffer lock."));
//#endif
//		return;
//	}
//
//	double LatestTimestamp;
//	TangoBufferId BufferId;
//	TangoErrorType Status = TangoService_lockCameraBuffer(TANGO_CAMERA_COLOR, &LatestTimestamp, &BufferId);
//	TangoErrorType Status = TANGO_ERROR;
//	if (Status == TANGO_SUCCESS)
//	{
//#if ENABLE_CAMERABUFFER_DEBUG_LOGGING
//		UE_LOG(LogGoogleARCoreARCamera, Log, TEXT("GameThread: Lock Camera buffer %lld at timestamp %f"), BufferId, LatestTimestamp);
//#endif
//		bNewCameraTextureAvailable = false;
//
//		// Dequeue command on render thread to assign the new locked buffer.
//		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
//			AssignCameraBuffer,
//			TangoBufferId, NewBufferId, BufferId,
//			FGoogleARCoreCameraManager*, TangoARCamera, this,
//			{
//				if (TangoARCamera->CurrentCameraBuffer != NewBufferId)
//				{
//					TangoARCamera->PreviousCameraBuffer = TangoARCamera->CurrentCameraBuffer;
//					TangoARCamera->CurrentCameraBuffer = NewBufferId;
//#if ENABLE_CAMERABUFFER_DEBUG_LOGGING
//					UE_LOG(LogGoogleARCoreARCamera, Log, TEXT("RenderThread: Assign new camera buffer %lld, previous buffer %lld"), NewBufferId, TangoARCamera->PreviousCameraBuffer);
//#endif
//					TangoARCamera->UpdateLockedColorCameraTexture_RenderThread();
//
//				}
//				else
//				{
//					UE_LOG(LogGoogleARCoreARCamera, Warning, TEXT("RenderThread: Skippping assigning the same camera buffer."));
//				}
//			}
//		);
//		ColorCameraImageTimestamp = LatestTimestamp;
//	}
//	else if (Status == TANGO_INVALID)
//	{
//		UE_LOG(LogGoogleARCoreARCamera, Error, TEXT("TangoService_lockCameraBuffer returns TANGO_INVALID."));
//		return;
//	}
//	else if (Status == TANGO_ERROR)
//	{
//		UE_LOG(LogGoogleARCoreARCamera, Warning, TEXT("TangoService_lockCameraBuffer returns TANGO_ERROR."));
//	}
//#endif
}

void FGoogleARCoreCameraManager::UpdateLockedColorCameraTexture_RenderThread()
{
//#if PLATFORM_ANDROID
//	check(IsInRenderingThread());
//
//#if ENABLE_CAMERABUFFER_DEBUG_LOGGING
//	UE_LOG(LogGoogleARCoreARCamera, Log, TEXT("RenderThread: UpdateLockedColorCameraTexture"))
//#endif
//
//	if (ColorCameraTextureId == 0)
//	{
//		// Allocate camera texture
//		ColorCameraTextureId = VideoOverlayRendererRHI->AllocateVideoTexture_RenderThread();
//	}
//
//#if ENABLE_CAMERABUFFER_DEBUG_LOGGING
//	UE_LOG(LogGoogleARCoreARCamera, Log, TEXT("RenderThread: Updating camera texture with camera buffer %lld"), CurrentCameraBuffer);
//#endif
//
//	TangoErrorType Status = TangoService_updateTextureExternalOesForBuffer(TANGO_CAMERA_COLOR, ColorCameraTextureId, CurrentCameraBuffer);
//	if (Status != TANGO_SUCCESS)
//	{
//		UE_LOG(LogGoogleARCoreARCamera, Error, TEXT("Failed to update camera texture from locked buffer %lld"), CurrentCameraBuffer);
//		return;
//	}
//
//	if (PreviousCameraBuffer != BUFFER_NOT_ASSIGNED)
//	{
//		// We only unlock the previous buffer when there is a new buffer is locked.
//		TangoService_unlockCameraBuffer(TANGO_CAMERA_COLOR, PreviousCameraBuffer);
//
//#if ENABLE_CAMERABUFFER_DEBUG_LOGGING
//		UE_LOG(LogGoogleARCoreARCamera, Log, TEXT("RenderThread: Unlock camera buffer %lld"), PreviousCameraBuffer);
//#endif
//	}
//#endif
}

void FGoogleARCoreCameraManager::LateUpdateColorCameraTexture_RenderThread()
{
	check(IsInRenderingThread());

	if (ColorCameraTextureId == 0)
	{
		// Allocate camera texture
		ColorCameraTextureId = VideoOverlayRendererRHI->AllocateVideoTexture_RenderThread();
	}

#if PLATFORM_ANDROID
	double LatestTimestamp;
	bool bGotTimestamp = false;
	while (!bGotTimestamp)
	{
		TangoErrorType Status = TangoService_updateTextureExternalOes(TANGO_CAMERA_COLOR, ColorCameraTextureId, &LatestTimestamp);
		if (Status != TANGO_SUCCESS)
		{
			UE_LOG(LogGoogleARCoreARCamera, Error, TEXT("(RenderThread) Failed to update color camera texture with error code: %d"), Status);
			return;
		}
		// We'll exit the loop if we got a new timestamp or we're not syncing with the camera
		bGotTimestamp = !bSyncGameFramerateToCamera || ColorCameraImageTimestamp != LatestTimestamp;
		if (!bGotTimestamp)
		{
			// Wait for the signal from tango core that a new texture is available
			if (!NewTextureAvailableEvent->Wait(100))
			{
				// tango core probably disconnected, give up
				UE_LOG(LogGoogleARCoreARCamera, Error, TEXT("Timed out waiting for camera frame"));
				return;
			}
		}
	}
	ColorCameraImageTimestamp = LatestTimestamp;
#endif
}

void FGoogleARCoreCameraManager::RenderColorCameraOverlay_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
{
    check(IsInRenderingThread());
    VideoOverlayRendererRHI->RenderVideoOverlay_RenderThread(RHICmdList, InView);

}

void FGoogleARCoreCameraManager::UpdateCameraImageBuffer()
{
	FScopeLock ScopeLock(&NewImageDataLock);
	// Swap the back buffer
	CameraImageData* TempPtr = BackImageDataBuffer;
	BackImageDataBuffer = CurrentImageDataBuffer;
	CurrentImageDataBuffer = TempPtr;
}

void FGoogleARCoreCameraManager::SetCameraOverlayMaterial(UMaterialInterface* NewMaterialInstance)
{
	VideoOverlayRendererRHI->SetOverlayMaterialInstance(NewMaterialInstance);
}

void FGoogleARCoreCameraManager::ResetCameraOverlayMaterialToDefault()
{
	VideoOverlayRendererRHI->ResetOverlayMaterialToDefault();
}

void FGoogleARCoreCameraManager::SetSyncGameFramerateWithCamera(bool bNewValue)
{
#if PLATFORM_ANDROID
	bool bOldValue = bSyncGameFramerateToCamera;
	if (bNewValue != bOldValue)
	{
		if (bOldValue)
		{
			FPlatformProcess::ReturnSynchEventToPool(NewTextureAvailableEvent);
		}
		else
		{
			NewTextureAvailableEvent = FPlatformProcess::GetSynchEventFromPool(false);
		}
		bSyncGameFramerateToCamera = bNewValue;
	}
#endif
}

void FGoogleARCoreCameraManager::SetCopyCameraImageEnabled(bool bInEnabled)
{
	bCopyCameraImageEnabled = bInEnabled;
}

double FGoogleARCoreCameraManager::GetColorCameraImageTimestamp()
{
	return ColorCameraImageTimestamp;
}

float FGoogleARCoreCameraManager::GetCameraFOV()
{
#if PLATFORM_ANDROID
	return FMath::Atan(0.5f * OrientationAlignedIntrinsics.width / OrientationAlignedIntrinsics.fx * (1.0f - 2 * CameraImageOffset.X)) * 2;
#endif
	return 60;
}

uint32 FGoogleARCoreCameraManager::GetColorCameraTextureId()
{
	return ColorCameraTextureId;
}

UTexture* FGoogleARCoreCameraManager::GetColorCameraTexture()
{
	return static_cast<UTexture*>(ColorCameraRenderTarget);
}

FIntPoint FGoogleARCoreCameraManager::GetCameraImageDimension()
{
#if PLATFORM_ANDROID
	return FIntPoint(OrientationAlignedIntrinsics.width, OrientationAlignedIntrinsics.height);
#endif
	return FIntPoint(1, 1);
}

void FGoogleARCoreCameraManager::GetCameraImageUV(TArray<FVector2D>& OutCameraImageUV)
{
	OutCameraImageUV.Empty();
	if (CameraImageUVs.Num() == 8)
	{
		for (int i = 0; i < 4; i++)
		{
			OutCameraImageUV.Add(FVector2D(CameraImageUVs[2 * i], CameraImageUVs[2 * i + 1]));
		}
	}
}

void FGoogleARCoreCameraManager::GetLatestLightEstimation(float& PixelIntensity)
{
	PixelIntensity = LatestPixelIntensity;
}

/////////////////////////////////////////
// Begin TangoARCamera Private Methods //
/////////////////////////////////////////
ETangoRotation FGoogleARCoreCameraManager::GetDisplayOrientation()
{
	ETangoRotation AndroidDisplayRotation = ETangoRotation::R_90;
#if PLATFORM_ANDROID
	int32 DisplayRotation = FGoogleARCoreAndroidHelper::GetDisplayRotation();
	AndroidDisplayRotation = static_cast<ETangoRotation>(DisplayRotation);
#endif

	return AndroidDisplayRotation;
}

void FGoogleARCoreCameraManager::UpdateOrientationAlignedCameraIntrinsics(ETangoRotation CurrentDisplayRotation)
{
#if PLATFORM_ANDROID
	TangoSupport_getCameraIntrinsicsBasedOnDisplayRotation(TANGO_CAMERA_COLOR, static_cast<TangoSupport_Rotation>(CurrentDisplayRotation), &OrientationAlignedIntrinsics);
#endif
}

void FGoogleARCoreCameraManager::UpdateCameraImageOffset(ETangoRotation CurrentDisplayRotation, FIntPoint ViewRectSize)
{
#if PLATFORM_ANDROID
	float WidthRatio = (float)OrientationAlignedIntrinsics.width / (float)ViewRectSize.X;
	float HeightRatio = (float)OrientationAlignedIntrinsics.height / (float)ViewRectSize.Y;

	UE_LOG(LogGoogleARCoreARCamera, Log, TEXT("Camera Image Size, %d, %d"), OrientationAlignedIntrinsics.width , OrientationAlignedIntrinsics.height);

	if (WidthRatio >= HeightRatio)
	{
		CameraImageOffset.X = (WidthRatio / HeightRatio - 1) / 2;
		CameraImageOffset.Y = 0;
	}
	else
	{
		CameraImageOffset.X = 0;
		CameraImageOffset.Y = (HeightRatio / WidthRatio - 1) / 2;
	}

	float OffsetU = CameraImageOffset.X;
	float OffsetV = CameraImageOffset.Y;

	UE_LOG(LogGoogleARCoreARCamera, Log, TEXT("Scene View Rect Size: %d x %d"), ViewRectSize.X, ViewRectSize.Y);
	UE_LOG(LogGoogleARCoreARCamera, Log, TEXT("CameraImageOffset: %f, %f"), CameraImageOffset.X, CameraImageOffset.Y);

	float UVs[8] = {
		0.0f + OffsetU, 0.0f + OffsetV,
		0.0f + OffsetU, 1.0f - OffsetV,
		1.0f - OffsetU, 0.0f + OffsetV,
		1.0f - OffsetU, 1.0f - OffsetV
	};
	float AlignedUVs[8] = {0};
	TangoSupport_getVideoOverlayUVBasedOnDisplayRotation(UVs, static_cast<TangoSupport_Rotation>(CurrentDisplayRotation), AlignedUVs);

	CameraImageUVs.Empty();
	CameraImageUVs.Append(AlignedUVs, 8);

    ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
        UpdateCameraImageUV,
        FGoogleARCorePassthroughCameraRenderer*, VideoOverlayRendererRHIPtr, VideoOverlayRendererRHI,
        TArray<float>, NewUVs, CameraImageUVs,
        {
            VideoOverlayRendererRHIPtr->UpdateOverlayUVCoordinate_RenderThread(NewUVs);
        }
    );
#endif
}

void FGoogleARCoreCameraManager::UpdateLightEstimation()
{
#if PLATFORM_ANDROID
	if (CurrentImageDataBuffer->TimestampInNs == LastLightEsimationTimestamp || CurrentImageDataBuffer->ImagePlaneData.Num() == 0)
	{
		return;
	}

	LastLightEsimationTimestamp = CurrentImageDataBuffer->TimestampInNs;

	uint8* DataPtr = CurrentImageDataBuffer->ImagePlaneData.GetData();
	int YOffset = CurrentImageDataBuffer->ImagePlaneIndex[0];

	uint8* YPlane  = CurrentImageDataBuffer->ImagePlaneData.GetData() + CurrentImageDataBuffer->ImagePlaneIndex[0];
	long YPlaneSize = (long) CurrentImageDataBuffer->ImagePlaneSize[0];

	float PixelIntensity = 0;
	TangoErrorType Status = TangoService_getPixelIntensity(YPlane, CurrentImageDataBuffer->Width, CurrentImageDataBuffer->Height, CurrentImageDataBuffer->PlaneRowStride[0], &PixelIntensity);
	if (Status == TANGO_SUCCESS)
	{
		LatestPixelIntensity = PixelIntensity;
	}
#endif
}
