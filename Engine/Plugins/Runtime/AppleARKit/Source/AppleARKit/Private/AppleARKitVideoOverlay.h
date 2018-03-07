// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/BlueprintPlatformLibrary.h"
#include "AppleARKitVideoOverlay.generated.h"

class FSceneViewFamily;

/** Helper class to ensure the ARKit camera material is cooked. */
UCLASS()
class UARKitCameraOverlayMaterialLoader : public UObject
{
	GENERATED_BODY()
	
public:
	UPROPERTY()
	UMaterialInterface* DefaultCameraOverlayMaterial;
	
	UARKitCameraOverlayMaterialLoader()
	{
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultOverlayMaterialRef(TEXT("/AppleARKit/ARKitCameraMaterial.ARKitCameraMaterial"));
		DefaultCameraOverlayMaterial = DefaultOverlayMaterialRef.Object;
	}
};

struct FAppleARKitFrame;

class FAppleARKitVideoOverlay
{
public:
	FAppleARKitVideoOverlay();

	void UpdateVideoTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FAppleARKitFrame& Frame, const FSceneViewFamily& InViewFamily);
	void RenderVideoOverlay_RenderThread(FRHICommandListImmediate& RHICmdList, const FSceneView& InView, const EScreenOrientation::Type DeviceOrientation);

private:
	FTextureRHIRef VideoTextureY;
	FTextureRHIRef VideoTextureCbCr;
	UMaterialInterface* RenderingOverlayMaterial;
	FIndexBufferRHIRef OverlayIndexBufferRHI;
	
	// Separate vertex buffer for each supported device orientation
	FVertexBufferRHIRef OverlayVertexBufferRHI[4];

	double LastUpdateTimestamp;
};
