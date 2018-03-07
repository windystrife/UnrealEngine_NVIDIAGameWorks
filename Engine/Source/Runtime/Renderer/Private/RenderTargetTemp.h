// Copyright 1998-2017 Epic Games, Inc.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealClient.h"
#include "SceneView.h"

/*=============================================================================
RenderTargetTemp.h : Helper for canvas rendering
=============================================================================*/


// this is a helper class for FCanvas to be able to get screen size
class FRenderTargetTemp : public FRenderTarget
{
	const FTexture2DRHIRef Texture;
	const FIntPoint SizeXY;

public:

	FRenderTargetTemp(const FSceneView& InView, const FIntPoint& InSizeXY)
	: Texture(InView.Family->RenderTarget->GetRenderTargetTexture())
	, SizeXY(InSizeXY)
	{}

	FRenderTargetTemp(const FTexture2DRHIRef InTexture, const FIntPoint& InSizeXY)
	: Texture(InTexture)
	, SizeXY(InSizeXY)
	{}

	FRenderTargetTemp(const FSceneView& InView, const FTexture2DRHIRef InTexture) 
	: Texture(InTexture)
	, SizeXY(InView.ViewRect.Size()) 
	{}

	FRenderTargetTemp(const FSceneView& InView)
	: Texture(InView.Family->RenderTarget->GetRenderTargetTexture())
	, SizeXY(InView.ViewRect.Size())
	{}

	virtual FIntPoint GetSizeXY() const { return SizeXY; }

	virtual const FTexture2DRHIRef& GetRenderTargetTexture() const
	{
		return Texture;
	}
};
