// Copyright 2017 Google Inc.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RenderResource.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture.h"


class FRHICommandListImmediate;

class GOOGLEARCORERENDERING_API FGoogleARCorePassthroughCameraRenderer
{
public:
	FGoogleARCorePassthroughCameraRenderer();

	void SetDefaultCameraOverlayMaterial(UMaterialInterface* InDefaultCameraOverlayMaterial);

	void InitializeOverlayMaterial();

	void SetOverlayMaterialInstance(UMaterialInterface* NewMaterialInstance);
	void ResetOverlayMaterialToDefault();

	void InitializeIndexBuffer_RenderThread();
	uint32 AllocateVideoTexture_RenderThread();
	void UpdateOverlayUVCoordinate_RenderThread(TArray<float>& InOverlayUVs);
	void RenderVideoOverlay_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView);
	void CopyVideoTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTextureRHIParamRef DstTexture, FIntPoint TargetSize);

private:
	bool bInitialized;
	FIndexBufferRHIRef OverlayIndexBufferRHI;
	FVertexBufferRHIRef OverlayVertexBufferRHI;
	FVertexBufferRHIRef OverlayCopyVertexBufferRHI;
	FTextureRHIRef VideoTexture;
	float OverlayTextureUVs[8];
	bool bMaterialInitialized;
	UMaterialInterface* DefaultOverlayMaterial;
	UMaterialInterface* OverrideOverlayMaterial;
	UMaterialInterface* RenderingOverlayMaterial;
};

