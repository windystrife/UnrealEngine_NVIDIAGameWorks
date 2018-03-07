// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/SlateRect.h"
#include "Layout/Geometry.h"
#include "Input/PopupMethodReply.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/HittestGrid.h"
#include "Rendering/DrawElements.h"
#include "Engine/Texture.h"
#include "Widgets/SWindow.h"
#include "Widgets/SVirtualWindow.h"

class FArrangedChildren;
class FSlateDrawBuffer;
class ISlate3DRenderer;
class STooltipPresenter;
class UTextureRenderTarget2D;

/**
 * 
 */
class UMG_API FWidgetRenderer
{
public:
	FWidgetRenderer(bool bUseGammaCorrection = false, bool bInClearTarget = true);
	~FWidgetRenderer();

	bool GetIsPrepassNeeded() const { return bPrepassNeeded; }
	void SetIsPrepassNeeded(bool bInPrepassNeeded) { bPrepassNeeded = bInPrepassNeeded; }

	bool GetClearHitTestGrid() const { return bClearHitTestGrid; }
	void SetClearHitTestGrid(bool bInClearHitTestGrid) { bClearHitTestGrid = bInClearHitTestGrid; }

	void SetShouldClearTarget(bool bShouldClear) { bClearTarget = bShouldClear; }

	bool GetUseGammaCorrection() const { return bUseGammaSpace; }
	void SetUseGammaCorrection(bool bInUseGammaSpace);

	ISlate3DRenderer* GetSlateRenderer();

	static UTextureRenderTarget2D* CreateTargetFor(FVector2D DrawSize, TextureFilter InFilter, bool bUseGammaCorrection);

	UTextureRenderTarget2D* DrawWidget(const TSharedRef<SWidget>& Widget, FVector2D DrawSize);

	void DrawWidget(
		UTextureRenderTarget2D* RenderTarget,
		const TSharedRef<SWidget>& Widget,
		FVector2D DrawSize,
		float DeltaTime);

	void DrawWindow(
		UTextureRenderTarget2D* RenderTarget,
		TSharedRef<FHittestGrid> HitTestGrid,
		TSharedRef<SWindow> Window,
		float Scale,
		FVector2D DrawSize,
		float DeltaTime);

	void DrawWindow(
		UTextureRenderTarget2D* RenderTarget,
		TSharedRef<FHittestGrid> HitTestGrid,
		TSharedRef<SWindow> Window,
		FGeometry WindowGeometry,
		FSlateRect WindowClipRect,
		float DeltaTime);

	void DrawWindow(
		const FPaintArgs& PaintArgs,
		UTextureRenderTarget2D* RenderTarget,
		TSharedRef<SWindow> Window,
		FGeometry WindowGeometry,
		FSlateRect WindowClipRect,
		float DeltaTime);

	TArray< TSharedPtr<FSlateWindowElementList::FDeferredPaint> > DeferredPaints;

private:
	/** The slate 3D renderer used to render the user slate widget */
	TSharedPtr<ISlate3DRenderer, ESPMode::ThreadSafe> Renderer;
	/** Prepass Needed when drawing the widget? */
	bool bPrepassNeeded;
	/** Clearing hit test grid needed? */
	bool bClearHitTestGrid;
	/** Is gamma space needed? */
	bool bUseGammaSpace;
	/** Should we clear the render target before rendering. */
	bool bClearTarget;

public:
	FVector2D ViewOffset;
};
