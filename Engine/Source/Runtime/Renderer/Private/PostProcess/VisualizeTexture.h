// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VisualizeTexture.h: Post processing visualize texture.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"

class FSceneView;
class FViewInfo;

class FVisualizeTexture
{
public:
	FVisualizeTexture();
	void Destroy();

	/** renders the VisualizeTextureContent to the current render target */
	void PresentContent(FRHICommandListImmediate& RHICmdList, const FViewInfo& View);
	/** */
	void OnStartFrame(const FSceneView& View);
	/** */
	void SetObserveTarget(const FString& InObservedDebugName, uint32 InObservedDebugNameReusedGoal = 0xffffffff);
	// @return 0 if not found
	IPooledRenderTarget* GetObservedElement() const;
	/**
	 * calling this allows to grab the state of the texture at this point to be queried by visualizetexture e.g. "vis LightAttenuation@2"
	 * @param PooledRenderTarget 0 is silently ignored
	 * Warning: this may change the active render target and other state
	 */
	void SetCheckPoint(FRHICommandList& RHICmdList, const IPooledRenderTarget* PooledRenderTarget);

	// @param bExtended true: with more convenience - not needed for crashes but useful from the console
	void DebugLog(bool bExtended);

	void QueryInfo( FQueryVisualizeTexureInfo& Out );

	// VisualizeTexture console command settings:
	// written on game thread, read on render thread (uses FlushRenderingCommands to avoid the threading issues)

	// 0=off, >0=texture id, changed by "VisualizeTexture" console command, useful for debugging
	int32 Mode;
	//
	float RGBMul;

	// -1=off, 0=R, 1=G, 2=B, 3=A
	int32 SingleChannel;

	// Multiplier for the single channel
	float SingleChannelMul;

	//
	float AMul;
	// 0=view in left top, 1=whole texture, 2=pixel perfect centered, 3=Picture in Picture
	int32 UVInputMapping;
	// bit 1: if 1, saturation mode, if 0, frac mode
	int32 Flags;
	//
	int32 CustomMip;
	//
	int32 ArrayIndex;
	//
	bool bSaveBitmap;
	// stencil normally displays in the alpha channel of depth buffer visualization.  This option is just for BMP writeout to get a stencil only BMP.
	bool bOutputStencil;
	//
	bool bFullList;
	// -1:by index, 0:by name, 1:by size
	int32 SortOrder;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// [DebugName of the RT] = ReuseCount this frame
	TMap<const TCHAR*, uint32> VisualizeTextureCheckpoints;
#endif

	// render target DebugName that is observed, "" if the feature is deactivated
	FString ObservedDebugName;
	// each frame this is counting up each time a RT with the same name is reused
	uint32 ObservedDebugNameReusedCurrent;
	// this is the count we want to reach, 0xffffffff if the last one
	uint32 ObservedDebugNameReusedGoal;

private:
	TRefCountPtr<IPooledRenderTarget> VisualizeTextureContent;
	// only valid/useful if VisualizeTextureContent is set
	FPooledRenderTargetDesc VisualizeTextureDesc;
	TRefCountPtr<FRHIShaderResourceView> StencilSRV;
	FTextureRHIRef StencilSRVSrc;

	// The view rectangle that we are drawing to
	FIntRect ViewRect;

	// View rectange, constrained to the camera aspect ratio (if required). In game modes, the view rectangle is set to the correct aspect ratio constrained rectangle, but
	// in the editor it is set to the full viewport window, and separate black bars are drawn to simulate the contrained area. We need to know about that so we can keep
	// the texture visualization image inside this area
	FIntRect AspectRatioConstrainedViewRect;

	// Flag to determine whether texture visualization is enabled, currently based on the feature level we are rendering with
	bool bEnabled;

	// Store feature level that we're currently using
	ERHIFeatureLevel::Type FeatureLevel;

	// is called by FPooledRenderTarget

	void GenerateContent(FRHICommandListImmediate& RHICmdList, const FSceneRenderTargetItem& RenderTargetItem, const FPooledRenderTargetDesc& Desc);

	FIntRect ComputeVisualizeTextureRect(FIntPoint InputTextureSize) const;
};


struct FVisualizeTextureData 
{
	FVisualizeTextureData(const FSceneRenderTargetItem& InRenderTargetItem, const FPooledRenderTargetDesc& InDesc)
		: RenderTargetItem(InRenderTargetItem), Desc(InDesc), StencilSRV(nullptr)
	{
	}

	const FSceneRenderTargetItem& RenderTargetItem;
	const FPooledRenderTargetDesc& Desc;
	TRefCountPtr<FRHIShaderResourceView> StencilSRV;
	float RGBMul;
	float SingleChannelMul;
	int32 SingleChannel;
	float AMul;
	FVector2D Tex00;
	FVector2D Tex11;
	bool bSaturateInsteadOfFrac;
	int32 InputValueMapping;
	int32 ArrayIndex;
	int32 CustomMip;
};
