// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DefaultStereoLayers.h"
#include "HeadMountedDisplayBase.h"

#include "EngineModule.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "RendererInterface.h"
#include "StereoLayerRendering.h"
#include "RHIStaticStates.h"
#include "StaticBoundShaderState.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"
#include "SceneView.h"

namespace 
{

	/*=============================================================================
	*
	* Helper functions
	*
	*/

	//=============================================================================
	static FMatrix ConvertTransform(const FTransform& In)
	{

		const FQuat InQuat = In.GetRotation();
		FQuat OutQuat(-InQuat.Y, -InQuat.Z, -InQuat.X, -InQuat.W);

		const FVector InPos = In.GetTranslation();
		FVector OutPos(InPos.Y, InPos.Z, InPos.X);

		const FVector InScale = In.GetScale3D();
		FVector OutScale(InScale.Y, InScale.Z, InScale.X);

		return FTransform(OutQuat, OutPos, OutScale).ToMatrixWithScale() * FMatrix(
			FPlane(0, 1, 0, 0),
			FPlane(0, 0, 1, 0),
			FPlane(1, 0, 0, 0),
			FPlane(0, 0, 0, 1));
	}

}

FDefaultStereoLayers::FDefaultStereoLayers(const FAutoRegister& AutoRegister, FHeadMountedDisplayBase* InHMDDevice) 
	: FSceneViewExtensionBase(AutoRegister)
	, HMDDevice(InHMDDevice)
{

}

//=============================================================================
void FDefaultStereoLayers::StereoLayerRender(FRHICommandListImmediate& RHICmdList, const TArray<uint32> & LayersToRender, const FLayerRenderParams& RenderParams) const
{
	check(IsInRenderingThread());
	if (!LayersToRender.Num())
	{
		return;
	}

	IRendererModule& RendererModule = GetRendererModule();

	// Set render state
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	bool bLastNoAlpha = (RenderThreadLayers[LayersToRender[0]].Flags & LAYER_FLAG_TEX_NO_ALPHA_CHANNEL) != 0;
	if (bLastNoAlpha)
	{
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI();
	}
	else
	{
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI();
	}

	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None, true, false>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
	RHICmdList.SetViewport(RenderParams.Viewport.Min.X, RenderParams.Viewport.Min.Y, 0, RenderParams.Viewport.Max.X, RenderParams.Viewport.Max.Y, 1.0f);

	// Set shader state
	auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FStereoLayerVS> VertexShader(ShaderMap);
	TShaderMapRef<FStereoLayerPS> PixelShader(ShaderMap);

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = RendererModule.GetFilterVertexDeclaration().VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;
	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	// For each layer
	for (uint32 LayerIndex : LayersToRender)
	{
		const FLayerDesc& Layer = RenderThreadLayers[LayerIndex];
		check(Layer.Texture.IsValid());
		const bool bNoAlpha = (Layer.Flags & LAYER_FLAG_TEX_NO_ALPHA_CHANNEL) != 0;
		if (bNoAlpha != bLastNoAlpha)
		{
			// Updater render state
			if (bNoAlpha)
			{
				GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_InverseSourceAlpha, BF_Zero>::GetRHI();
			}
			else
			{
				GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI();
			}
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
			bLastNoAlpha = bNoAlpha;
		}

		FMatrix LayerMatrix = ConvertTransform(Layer.Transform);

		FVector2D QuadSize = Layer.QuadSize * 0.5f;
		if (Layer.Flags & LAYER_FLAG_QUAD_PRESERVE_TEX_RATIO)
		{
			const FRHITexture2D* Tex2D = Layer.Texture->GetTexture2D();
			if (Tex2D)
			{
				const float SizeX = Tex2D->GetSizeX();
				const float SizeY = Tex2D->GetSizeY();
				if (SizeX != 0)
				{
					const float AspectRatio = SizeY / SizeX;
					QuadSize.Y = QuadSize.X * AspectRatio;
				}
			}
		}

		// Set shader uniforms
		VertexShader->SetParameters(
			RHICmdList,
			QuadSize,
			Layer.UVRect,
			RenderParams.RenderMatrices[static_cast<int>(Layer.PositionType)],
			LayerMatrix);

		PixelShader->SetParameters(
			RHICmdList,
			TStaticSamplerState<SF_Trilinear>::GetRHI(),
			Layer.Texture);

		const FIntPoint TargetSize = RenderParams.Viewport.Size();
		// Draw primitive
		RendererModule.DrawRectangle(
			RHICmdList,
			0.0f, 0.0f,
			TargetSize.X, TargetSize.Y,
			0.0f, 0.0f,
			1.0f, 1.0f,
			TargetSize,
			FIntPoint(1, 1),
			*VertexShader
		);
	}
}

void FDefaultStereoLayers::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
	check(IsInRenderingThread());

	if (!GetStereoLayersDirty())
	{
		return;
	}

	CopyLayers(RenderThreadLayers);

	// Sort layers
	SortedSceneLayers.Reset();
	SortedOverlayLayers.Reset();
	uint32 LayerCount = RenderThreadLayers.Num();
	for (uint32 LayerIndex = 0; LayerIndex < LayerCount; ++LayerIndex)
	{
		const auto& Layer = RenderThreadLayers[LayerIndex];
		if (!Layer.Texture.IsValid())
		{
			continue;
		}
		if (Layer.PositionType == ELayerType::FaceLocked)
		{
			SortedOverlayLayers.Add(LayerIndex);
		}
		else
		{
			SortedSceneLayers.Add(LayerIndex);
		}
	}

	auto SortLayersPredicate = [&](const uint32& A, const uint32& B)
	{
		return RenderThreadLayers[A].Priority < RenderThreadLayers[B].Priority;
	};
	SortedSceneLayers.Sort(SortLayersPredicate);
	SortedOverlayLayers.Sort(SortLayersPredicate);
}


void FDefaultStereoLayers::PostRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
{
	if (InView.StereoPass != EStereoscopicPass::eSSP_LEFT_EYE && InView.StereoPass != EStereoscopicPass::eSSP_RIGHT_EYE)
	{
		return;
	}

	FViewMatrices ModifiedViewMatrices = InView.ViewMatrices;
	ModifiedViewMatrices.HackRemoveTemporalAAProjectionJitter();
	const FMatrix& ProjectionMatrix = ModifiedViewMatrices.GetProjectionMatrix();
	const FMatrix& ViewProjectionMatrix = ModifiedViewMatrices.GetViewProjectionMatrix();

	// Calculate a view matrix that only adjusts for eye position, ignoring head position, orientation and world position.
	FVector EyeShift;
	FQuat EyeOrientation;
	HMDDevice->GetRelativeEyePose(IXRTrackingSystem::HMDDeviceId, InView.StereoPass, EyeOrientation, EyeShift);

	FMatrix EyeMatrix = FTranslationMatrix(-EyeShift) * FInverseRotationMatrix(EyeOrientation.Rotator()) * FMatrix(
		FPlane(0, 0, 1, 0),
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 0, 1));

	FQuat HmdOrientation = HmdTransform.GetRotation();
	FVector HmdLocation = HmdTransform.GetTranslation();
	FMatrix TrackerMatrix = FTranslationMatrix(-HmdLocation) * FInverseRotationMatrix(HmdOrientation.Rotator()) * EyeMatrix;

	FLayerRenderParams RenderParams{
		InView.ViewRect, // Viewport
		{
			ViewProjectionMatrix,				// WorldLocked,
			TrackerMatrix * ProjectionMatrix,	// TrackerLocked,
			EyeMatrix * ProjectionMatrix		// FaceLocked
		}
	};
	
	FTexture2DRHIRef RenderTarget = HMDDevice->GetSceneLayerTarget_RenderThread(InView.StereoPass, RenderParams.Viewport);
	if (!RenderTarget.IsValid())
	{
		RenderTarget = InView.Family->RenderTarget->GetRenderTargetTexture();
	}
	SetRenderTarget(RHICmdList, RenderTarget, FTextureRHIRef());
	RHICmdList.SetViewport(RenderParams.Viewport.Min.X, RenderParams.Viewport.Min.Y, 0, RenderParams.Viewport.Max.X, RenderParams.Viewport.Max.Y, 1.0f);
	StereoLayerRender(RHICmdList, SortedSceneLayers, RenderParams);
	
	// Optionally render face-locked layers into a non-reprojected target if supported by the HMD platform
	FTexture2DRHIRef OverlayRenderTarget = HMDDevice->GetOverlayLayerTarget_RenderThread(InView.StereoPass, RenderParams.Viewport);
	if (OverlayRenderTarget.IsValid())
	{
		SetRenderTarget(RHICmdList, OverlayRenderTarget, FTextureRHIRef());
		DrawClearQuad(RHICmdList, FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		RHICmdList.SetViewport(RenderParams.Viewport.Min.X, RenderParams.Viewport.Min.Y, 0, RenderParams.Viewport.Max.X, RenderParams.Viewport.Max.Y, 1.0f);
	}

	StereoLayerRender(RHICmdList, SortedOverlayLayers, RenderParams);
}

bool FDefaultStereoLayers::IsActiveThisFrame(class FViewport* InViewport) const
{
	return GEngine && GEngine->IsStereoscopic3D(InViewport);
}

void FDefaultStereoLayers::UpdateSplashScreen()
{
	FTexture2DRHIRef Texture = (bSplashShowMovie && SplashMovie.IsValid()) ? SplashMovie : SplashTexture;
	if (bSplashIsShown && Texture.IsValid())
	{
		FQuat Orientation;
		FVector Position;

		HMDDevice->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, Orientation, Position);
		FLayerDesc LayerDesc;
		LayerDesc.Flags = ELayerFlags::LAYER_FLAG_TEX_NO_ALPHA_CHANNEL;
		LayerDesc.PositionType = ELayerType::TrackerLocked;
		LayerDesc.Texture = Texture;
		LayerDesc.UVRect = FBox2D(SplashOffset, SplashOffset + SplashScale);

		FTransform Translation(FVector(500.0f, 0.0f, 100.0f));
		FRotator Rotation(Orientation);
		Rotation.Pitch = 0.0f;
		Rotation.Roll = 0.0f;
		LayerDesc.Transform = Translation * FTransform(Rotation.Quaternion());

		LayerDesc.QuadSize = FVector2D(800.0f, 450.0f);

		if (SplashLayerHandle)
		{
			SetLayerDesc(SplashLayerHandle, LayerDesc);
		}
		else
		{
			SplashLayerHandle = CreateLayer(LayerDesc);
		}
	}
	else
	{
		if (SplashLayerHandle)
		{
			DestroyLayer(SplashLayerHandle);
			SplashLayerHandle = 0;
		}
	}
}

void FDefaultStereoLayers::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	// Initialize HMD position.
	FQuat HmdOrientation = FQuat::Identity;
	FVector HmdPosition = FVector::ZeroVector;
	HMDDevice->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, HmdOrientation, HmdPosition);
	HmdTransform = FTransform(HmdOrientation, HmdPosition);
}

