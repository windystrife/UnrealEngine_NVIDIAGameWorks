// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "Layout/ArrangedChildren.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "Rendering/SlateDrawBuffer.h"
#include "Engine/TextureRenderTarget2D.h"

#if !UE_SERVER
#include "Interfaces/ISlateRHIRendererModule.h"
#endif // !UE_SERVER

#include "Widgets/LayerManager/STooltipPresenter.h"
#include "Widgets/Layout/SPopup.h"

FWidgetRenderer::FWidgetRenderer(bool bUseGammaCorrection, bool bInClearTarget)
	: bPrepassNeeded(true)
	, bClearHitTestGrid(true)
	, bUseGammaSpace(bUseGammaCorrection)
	, bClearTarget(bInClearTarget)
	, ViewOffset(0.0f, 0.0f)
{
#if !UE_SERVER
	if ( LIKELY(FApp::CanEverRender()) )
	{
		Renderer = FModuleManager::Get().LoadModuleChecked<ISlateRHIRendererModule>("SlateRHIRenderer").CreateSlate3DRenderer(bUseGammaSpace);
	}
#endif
}

FWidgetRenderer::~FWidgetRenderer()
{
}

ISlate3DRenderer* FWidgetRenderer::GetSlateRenderer()
{
	return Renderer.Get();
}

void FWidgetRenderer::SetUseGammaCorrection(bool bInUseGammaSpace)
{
	bUseGammaSpace = bInUseGammaSpace;

#if !UE_SERVER
	if (LIKELY(FApp::CanEverRender()))
	{
		Renderer->SetUseGammaCorrection(bInUseGammaSpace);
	}
#endif
}

UTextureRenderTarget2D* FWidgetRenderer::DrawWidget(const TSharedRef<SWidget>& Widget, FVector2D DrawSize)
{
	if ( LIKELY(FApp::CanEverRender()) )
	{
		UTextureRenderTarget2D* RenderTarget = FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, bUseGammaSpace);

		DrawWidget(RenderTarget, Widget, DrawSize, 0);

		return RenderTarget;
	}

	return nullptr;
}

UTextureRenderTarget2D* FWidgetRenderer::CreateTargetFor(FVector2D DrawSize, TextureFilter InFilter, bool bUseGammaCorrection)
{
	if ( LIKELY(FApp::CanEverRender()) )
	{
		const bool bIsLinearSpace = !bUseGammaCorrection;

		UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>();
		RenderTarget->Filter = InFilter;
		RenderTarget->ClearColor = FLinearColor::Transparent;
		RenderTarget->SRGB = bIsLinearSpace;
		RenderTarget->TargetGamma = 1;
		RenderTarget->InitCustomFormat(DrawSize.X, DrawSize.Y, PF_B8G8R8A8, bIsLinearSpace);
		RenderTarget->UpdateResourceImmediate(true);

		return RenderTarget;
	}

	return nullptr;
}

void FWidgetRenderer::DrawWidget(UTextureRenderTarget2D* RenderTarget, const TSharedRef<SWidget>& Widget, FVector2D DrawSize, float DeltaTime)
{
	TSharedRef<SVirtualWindow> Window = SNew(SVirtualWindow).Size(DrawSize);
	TSharedRef<FHittestGrid> HitTestGrid = MakeShareable(new FHittestGrid());

	Window->SetContent(Widget);
	Window->Resize(DrawSize);

	DrawWindow(RenderTarget, HitTestGrid, Window, 1, DrawSize, DeltaTime);
}

void FWidgetRenderer::DrawWindow(
	UTextureRenderTarget2D* RenderTarget,
	TSharedRef<FHittestGrid> HitTestGrid,
	TSharedRef<SWindow> Window,
	float Scale,
	FVector2D DrawSize,
	float DeltaTime)
{
	FGeometry WindowGeometry = FGeometry::MakeRoot(DrawSize * ( 1 / Scale ), FSlateLayoutTransform(Scale));

	DrawWindow(
		RenderTarget,
		HitTestGrid,
		Window,
		WindowGeometry,
		WindowGeometry.GetLayoutBoundingRect(),
		DeltaTime
		);
}

void FWidgetRenderer::DrawWindow(
	UTextureRenderTarget2D* RenderTarget,
	TSharedRef<FHittestGrid> HitTestGrid,
	TSharedRef<SWindow> Window,
	FGeometry WindowGeometry,
	FSlateRect WindowClipRect,
	float DeltaTime)
{
	FPaintArgs PaintArgs(Window.Get(), HitTestGrid.Get(), FVector2D::ZeroVector, FApp::GetCurrentTime(), DeltaTime);
	DrawWindow(PaintArgs, RenderTarget, Window, WindowGeometry, WindowClipRect, DeltaTime);
}

void FWidgetRenderer::DrawWindow(
	const FPaintArgs& PaintArgs,
	UTextureRenderTarget2D* RenderTarget,
	TSharedRef<SWindow> Window,
	FGeometry WindowGeometry,
	FSlateRect WindowClipRect,
	float DeltaTime)
{
#if !UE_SERVER
	if ( LIKELY(FApp::CanEverRender()) )
	{
	    if ( bPrepassNeeded )
	    {
		    // Ticking can cause geometry changes.  Recompute
		    Window->SlatePrepass(WindowGeometry.Scale);
	    }
    
		if ( bClearHitTestGrid )
		{
			// Prepare the test grid 
			PaintArgs.GetGrid().ClearGridForNewFrame(WindowClipRect);
		}
    
	    // Get the free buffer & add our virtual window
	    FSlateDrawBuffer& DrawBuffer = Renderer->GetDrawBuffer();
	    FSlateWindowElementList& WindowElementList = DrawBuffer.AddWindowElementList(Window);
    
	    int32 MaxLayerId = 0;
	    {
		    // Paint the window
		    MaxLayerId = Window->Paint(
			    PaintArgs,
			    WindowGeometry, WindowClipRect,
			    WindowElementList,
			    0,
			    FWidgetStyle(),
			    Window->IsEnabled());
	    }

		//MaxLayerId = WindowElementList.PaintDeferred(MaxLayerId);
		DeferredPaints = WindowElementList.GetDeferredPaintList();

		Renderer->DrawWindow_GameThread(DrawBuffer);

		DrawBuffer.ViewOffset = ViewOffset;

		struct FRenderThreadContext
		{
			FSlateDrawBuffer* DrawBuffer;
			FTextureRenderTarget2DResource* RenderTargetResource;
			TSharedPtr<ISlate3DRenderer, ESPMode::ThreadSafe> Renderer;
			bool bClearTarget;
		}
		Context =		
		{
			&DrawBuffer,
			static_cast<FTextureRenderTarget2DResource*>(RenderTarget->GameThread_GetRenderTargetResource()),
			Renderer,
			bClearTarget
		};

		// Enqueue a command to unlock the draw buffer after all windows have been drawn
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(FWidgetRenderer_DrawWindow,
			FRenderThreadContext, InContext, Context,
			{
				InContext.Renderer->DrawWindowToTarget_RenderThread(RHICmdList, InContext.RenderTargetResource, *InContext.DrawBuffer, InContext.bClearTarget);
			});
	}
#endif // !UE_SERVER
}
