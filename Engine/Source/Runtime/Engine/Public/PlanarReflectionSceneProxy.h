// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PlanarReflectionSceneProxy.h: 
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RenderResource.h"
#include "UnrealClient.h"

// Currently we support at most 2 views for each planar reflection, one view per stereo pass
// Must match shader code
const int32 GMaxPlanarReflectionViews = 2;

class UPlanarReflectionComponent;

class FPlanarReflectionRenderTarget : public FTexture, public FRenderTarget
{
public:

	FPlanarReflectionRenderTarget(FIntPoint InSize) :
		Size(InSize)
	{}

	virtual const FTexture2DRHIRef& GetRenderTargetTexture() const 
	{
		return (const FTexture2DRHIRef&)TextureRHI;
	}

	virtual void InitDynamicRHI()
	{
		// Create the sampler state RHI resource.
		FSamplerStateInitializerRHI SamplerStateInitializer
		(
			SF_Bilinear,
			AM_Clamp,
			AM_Clamp,
			AM_Clamp
		);
		SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);

		FTexture2DRHIRef Texture2DRHI;
		FRHIResourceCreateInfo CreateInfo = { FClearValueBinding(FLinearColor::Black) };

		RHICreateTargetableShaderResource2D(
			GetSizeX(), 
			GetSizeY(), 
			PF_FloatRGBA,
			1,
			0,
			TexCreate_RenderTargetable,
			false,
			CreateInfo,
			RenderTargetTextureRHI,
			Texture2DRHI
			);
		TextureRHI = (FTextureRHIRef&)Texture2DRHI;
	}

	virtual FIntPoint GetSizeXY() const { return Size; }

	/** Returns the width of the texture in pixels. */
	virtual uint32 GetSizeX() const
	{
		return Size.X;
	}
	/** Returns the height of the texture in pixels. */
	virtual uint32 GetSizeY() const
	{
		return Size.Y;
	}

	virtual float GetDisplayGamma() const { return 1.0f; }

	virtual FString GetFriendlyName() const override { return TEXT("FPlanarReflectionRenderTarget"); }

private:

	FIntPoint Size;
};

class FPlanarReflectionSceneProxy
{
public:

	FPlanarReflectionSceneProxy(UPlanarReflectionComponent* Component, FPlanarReflectionRenderTarget* InRenderTarget);

	void UpdateTransform(const FMatrix& NewTransform)
	{

		PlanarReflectionOrigin = NewTransform.TransformPosition(FVector::ZeroVector);
		ReflectionPlane = FPlane(PlanarReflectionOrigin, NewTransform.TransformVector(FVector(0, 0, 1)));

		// Extents of the mesh used to visualize the reflection plane
		const float MeshExtent = 2000.0f;
		FVector LocalExtent(MeshExtent, MeshExtent, DistanceFromPlaneFadeEnd);
		FBox LocalBounds(-LocalExtent, LocalExtent);
		WorldBounds = LocalBounds.TransformBy(NewTransform);

		const FVector XAxis = NewTransform.TransformVector(FVector(1, 0, 0));
		const float XAxisLength = XAxis.Size();
		PlanarReflectionXAxis = FVector4(XAxis / FMath::Max(XAxisLength, DELTA), XAxisLength * MeshExtent);

		const FVector YAxis = NewTransform.TransformVector(FVector(0, 1, 0));
		const float YAxisLength = YAxis.Size();
		PlanarReflectionYAxis = FVector4(YAxis / FMath::Max(YAxisLength, DELTA), YAxisLength * MeshExtent);

		const FMirrorMatrix MirrorMatrix(ReflectionPlane);
		// Using TransposeAdjoint instead of full inverse because we only care about transforming normals
		const FMatrix InverseTransposeMirrorMatrix4x4 = MirrorMatrix.TransposeAdjoint();
		InverseTransposeMirrorMatrix4x4.GetScaledAxes((FVector&)InverseTransposeMirrorMatrix[0], (FVector&)InverseTransposeMirrorMatrix[1], (FVector&)InverseTransposeMirrorMatrix[2]);
	}

	FBox WorldBounds;
	FPlane ReflectionPlane;
	FVector PlanarReflectionOrigin;
	float DistanceFromPlaneFadeEnd;
	FVector4 PlanarReflectionXAxis;
	FVector4 PlanarReflectionYAxis;
	FVector PlanarReflectionParameters;
	FVector2D PlanarReflectionParameters2;
	FMatrix ProjectionWithExtraFOV[GMaxPlanarReflectionViews];
	FVector4 ScreenScaleBias[GMaxPlanarReflectionViews];
	FVector4 InverseTransposeMirrorMatrix[3];
	FName OwnerName;
	int32 PlanarReflectionId;
	float PrefilterRoughness;
	float PrefilterRoughnessDistance;
	bool bIsStereo;
// WaveWorks Start
	bool bAlwaysVisible;
	FVector4 PlanarReflectionWaveWorksParameters;
// WaveWorks End

	/** This is specific to a certain view and should actually be stored in FSceneViewState. */
	FPlanarReflectionRenderTarget* RenderTarget;
};
