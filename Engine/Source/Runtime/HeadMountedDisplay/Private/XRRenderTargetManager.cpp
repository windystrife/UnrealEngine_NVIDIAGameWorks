// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "XRRenderTargetManager.h"
#include "SceneViewport.h"
#include "Widgets/SViewport.h"

void FXRRenderTargetManager::CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY)
{
	check(IsInGameThread());
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.ScreenPercentage"));
	if (CVar)
	{
		float ScreenPercentage = FMath::Clamp(CVar->GetValueOnGameThread(), 30.f, 300.f);
		if (ScreenPercentage > 0.0f)
		{
			InOutSizeX = FMath::CeilToInt(InOutSizeX * ScreenPercentage / 100.f);
			InOutSizeY = FMath::CeilToInt(InOutSizeY * ScreenPercentage / 100.f);
		}
	}
}

bool FXRRenderTargetManager::NeedReAllocateViewportRenderTarget(const FViewport& Viewport)
{
	check(IsInGameThread());

	if (!ShouldUseSeparateRenderTarget()) // or should this be a check instead, as it is only called when ShouldUseSeparateRenderTarget() returns true?
	{
		return false;
	}

	const FIntPoint ViewportSize = Viewport.GetSizeXY();
	const FIntPoint RenderTargetSize = Viewport.GetRenderTargetTextureSizeXY();

	uint32 NewSizeX = ViewportSize.X;
	uint32 NewSizeY = ViewportSize.Y;
	CalculateRenderTargetSize(Viewport, NewSizeX, NewSizeY);

	return (NewSizeX != RenderTargetSize.X || NewSizeY != RenderTargetSize.Y);
}

void FXRRenderTargetManager::UpdateViewport(bool bUseSeparateRenderTarget, const class FViewport& Viewport, class SViewport* ViewportWidget /*= nullptr*/)
{
	check(IsInGameThread());

	if (GIsEditor && ViewportWidget != nullptr && !ViewportWidget->IsStereoRenderingAllowed())
	{
		return;
	}

	FRHIViewport* const ViewportRHI = Viewport.GetViewportRHI().GetReference();
	if (!ViewportRHI)
	{
		return;
	}

	if (ViewportWidget)
	{
		UpdateViewportWidget(bUseSeparateRenderTarget, Viewport, ViewportWidget);
	}

	if (!ShouldUseSeparateRenderTarget())
	{
		if ((!bUseSeparateRenderTarget || GIsEditor) && ViewportRHI)
		{
			ViewportRHI->SetCustomPresent(nullptr);
		}
		return;
	}

	UpdateViewportRHIBridge(bUseSeparateRenderTarget, Viewport, ViewportRHI);
}