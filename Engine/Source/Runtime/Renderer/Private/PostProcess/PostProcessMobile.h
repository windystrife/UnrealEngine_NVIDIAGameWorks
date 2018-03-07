// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessMobile.h: Mobile uber post processing.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "PostProcess/RenderingCompositionGraph.h"

class FViewInfo;

// return Depth of Field Scale if Gaussian DoF mode is active. 0.0f otherwise.
float GetMobileDepthOfFieldScale(const FViewInfo& View);

class FRCPassPostProcessBloomSetupES2 : public TRenderingCompositePassBase<1, 1>
{
public:
	FRCPassPostProcessBloomSetupES2(FIntRect InPrePostSourceViewportRect, bool bInUseViewRectSource) : PrePostSourceViewportRect(InPrePostSourceViewportRect), bUseViewRectSource(bInUseViewRectSource) { }
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual void Release() override { delete this; }
private:
	FIntRect PrePostSourceViewportRect;
	bool bUseViewRectSource;
	void SetShader(const FRenderingCompositePassContext& Context);
};

class FRCPassPostProcessBloomSetupSmallES2 : public TRenderingCompositePassBase<1, 1>
{
public:
	FRCPassPostProcessBloomSetupSmallES2(FIntPoint InPrePostSourceViewportSize, bool bInUseViewRectSource) : PrePostSourceViewportSize(InPrePostSourceViewportSize), bUseViewRectSource(bInUseViewRectSource) { }
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual void Release() override { delete this; }
private:
	FIntPoint PrePostSourceViewportSize;
	bool bUseViewRectSource;
	void SetShader(const FRenderingCompositePassContext& Context);
};

class FRCPassPostProcessDofNearES2 : public TRenderingCompositePassBase<1, 1>
{
public:
	FRCPassPostProcessDofNearES2(FIntPoint InPrePostSourceViewportSize) : PrePostSourceViewportSize(InPrePostSourceViewportSize) { }
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual void Release() override { delete this; }
private:
	FIntPoint PrePostSourceViewportSize;
	void SetShader(const FRenderingCompositePassContext& Context);
};

class FRCPassPostProcessDofDownES2 : public TRenderingCompositePassBase<2, 1>
{
public:
	FRCPassPostProcessDofDownES2(FIntRect InPrePostSourceViewportRect, bool bInUseViewRectSource) : PrePostSourceViewportRect(InPrePostSourceViewportRect), bUseViewRectSource(bInUseViewRectSource) { }
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual void Release() override { delete this; }
private:
	FIntRect PrePostSourceViewportRect;
	bool bUseViewRectSource;
	void SetShader(const FRenderingCompositePassContext& Context);
};

class FRCPassPostProcessDofBlurES2 : public TRenderingCompositePassBase<2, 1>
{
public:
	FRCPassPostProcessDofBlurES2(FIntPoint InPrePostSourceViewportSize) : PrePostSourceViewportSize(InPrePostSourceViewportSize) { }
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual void Release() override { delete this; }
private:
	FIntPoint PrePostSourceViewportSize;
};

class FRCPassPostProcessBloomDownES2 : public TRenderingCompositePassBase<1, 1>
{
public:
	FRCPassPostProcessBloomDownES2(FIntPoint InPrePostSourceViewportSize, float InScale) : PrePostSourceViewportSize(InPrePostSourceViewportSize), Scale(InScale) { }
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual void Release() override { delete this; }
private:
	FIntPoint PrePostSourceViewportSize;
	float Scale;
};

class FRCPassPostProcessBloomUpES2 : public TRenderingCompositePassBase<2, 1>
{
public:
	FRCPassPostProcessBloomUpES2(FIntPoint InPrePostSourceViewportSize, FVector2D InScaleAB, FVector4& InTintA, FVector4& InTintB) : PrePostSourceViewportSize(InPrePostSourceViewportSize), ScaleAB(InScaleAB), TintA(InTintA), TintB(InTintB) { }
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual void Release() override { delete this; }
private:
	FIntPoint PrePostSourceViewportSize;
	FVector2D ScaleAB;
	FVector4 TintA;
	FVector4 TintB;
};

class FRCPassPostProcessSunMaskES2 : public TRenderingCompositePassBase<1, 1>
{
public:
	FRCPassPostProcessSunMaskES2(FIntPoint InPrePostSourceViewportSize, bool bInOnChip) : PrePostSourceViewportSize(InPrePostSourceViewportSize), bOnChip(bInOnChip) { }
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual void Release() override { delete this; }
private:
	FIntPoint PrePostSourceViewportSize;
	bool bOnChip;
	template <bool bUseDepthTexture>
	void SetShader(const FRenderingCompositePassContext& Context);
};

class FRCPassPostProcessSunAlphaES2 : public TRenderingCompositePassBase<1, 1>
{
public:
	FRCPassPostProcessSunAlphaES2(FIntPoint InPrePostSourceViewportSize) : PrePostSourceViewportSize(InPrePostSourceViewportSize) { }
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual void Release() override { delete this; }
private:
	FIntPoint PrePostSourceViewportSize;
	void SetShader(const FRenderingCompositePassContext& Context);
};

class FRCPassPostProcessSunBlurES2 : public TRenderingCompositePassBase<1, 1>
{
public:
	FRCPassPostProcessSunBlurES2(FIntPoint InPrePostSourceViewportSize) : PrePostSourceViewportSize(InPrePostSourceViewportSize) { }
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual void Release() override { delete this; }
private:
	FIntPoint PrePostSourceViewportSize;
};

class FRCPassPostProcessSunMergeES2 : public TRenderingCompositePassBase<3, 1>
{
public:
	FRCPassPostProcessSunMergeES2(FIntPoint InPrePostSourceViewportSize) : PrePostSourceViewportSize(InPrePostSourceViewportSize) { }
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual void Release() override { delete this; }
private:
	FIntPoint PrePostSourceViewportSize;
	FShader* SetShader(const FRenderingCompositePassContext& Context);
};

class FRCPassPostProcessSunMergeSmallES2 : public TRenderingCompositePassBase<2, 1>
{
public:
	FRCPassPostProcessSunMergeSmallES2(FIntPoint InPrePostSourceViewportSize) : PrePostSourceViewportSize(InPrePostSourceViewportSize) { }
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual void Release() override { delete this; }
private:
	FIntPoint PrePostSourceViewportSize;
	void SetShader(const FRenderingCompositePassContext& Context);
};

class FRCPassPostProcessSunAvgES2 : public TRenderingCompositePassBase<2, 1>
{
public:
	FRCPassPostProcessSunAvgES2(FIntPoint InPrePostSourceViewportSize) : PrePostSourceViewportSize(InPrePostSourceViewportSize) { }
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual void Release() override { delete this; }
private:
	FIntPoint PrePostSourceViewportSize;
	void SetShader(const FRenderingCompositePassContext& Context);
};

class FRCPassPostProcessAaES2 : public TRenderingCompositePassBase<2, 1>
{
public:
	virtual void Process(FRenderingCompositePassContext& Context) override;
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const override;
	virtual void Release() override { delete this; }
private:
	void SetShader(const FRenderingCompositePassContext& Context);
};

