// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeBool.h"

class FSceneViewport;
class IImageWrapper;
class UMaterial;

DECLARE_LOG_CATEGORY_EXTERN(LogHighResScreenshot, Log, All);

struct ENGINE_API FHighResScreenshotConfig
{
	static const float MinResolutionMultipler;
	static const float MaxResolutionMultipler;

	FIntRect UnscaledCaptureRegion;
	FIntRect CaptureRegion;
	float ResolutionMultiplier;
	float ResolutionMultiplierScale;
	bool bMaskEnabled;
	bool bDumpBufferVisualizationTargets;
	TWeakPtr<FSceneViewport> TargetViewport;
	bool bDisplayCaptureRegion;
	bool bCaptureHDR;
	bool bForce128BitRendering;
	FString FilenameOverride;

	// Materials used in the editor to help with the capture of highres screenshots
	UMaterial* HighResScreenshotMaterial;
	UMaterial* HighResScreenshotMaskMaterial;
	UMaterial* HighResScreenshotCaptureRegionMaterial;

	FHighResScreenshotConfig();

	/** Initialize the image wrapper modules (required for SaveImage) **/
	void Init(uint32 NumAsyncWriters = 6);

	/** Point the screenshot UI at a different viewport **/
	void ChangeViewport(TWeakPtr<FSceneViewport> InViewport);

	/** Parse screenshot parameters from the supplied console command line **/
	bool ParseConsoleCommand(const FString& InCmd, FOutputDevice& Ar);

	/** Utility function for merging the mask buffer into the alpha channel of the supplied bitmap, if masking is enabled.
	  * Returns true if the mask was written, and false otherwise.
	**/
	bool MergeMaskIntoAlpha(TArray<FColor>& InBitmap);

	/** Enable/disable HDR capable captures **/
	void SetHDRCapture(bool bCaptureHDRIN);

	/** Enable/disable forcing 128-bit rendering pipeline for capture **/
	void SetForce128BitRendering(bool bForce);

	/** Configure taking a high res screenshot */
	bool SetResolution(uint32 ResolutionX, uint32 ResolutionY, float ResolutionScale = 1.0f);

	/** Save to image file **/
	template<typename TPixelType>
	ENGINE_API bool SaveImage(const FString& File, const TArray<TPixelType>& Bitmap, const FIntPoint& BitmapSize, FString* OutFilename = nullptr) const;

private:
	struct FImageWriter
	{
		FImageWriter(const TSharedPtr<class IImageWrapper>& InWrapper);

		TSharedPtr<class IImageWrapper> ImageWrapper;
		mutable FThreadSafeBool bInUse;
	};
	TArray<FImageWriter> ImageCompressorsLDR;
	TArray<FImageWriter> ImageCompressorsHDR;
};

ENGINE_API FHighResScreenshotConfig& GetHighResScreenshotConfig();
