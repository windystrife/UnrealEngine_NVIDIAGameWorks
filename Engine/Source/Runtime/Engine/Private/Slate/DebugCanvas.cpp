// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Slate/DebugCanvas.h"
#include "RenderingThread.h"
#include "UnrealClient.h"
#include "CanvasTypes.h"
#include "Engine/Engine.h"
#include "EngineModule.h"
#include "Framework/Application/SlateApplication.h"
#include "IStereoLayers.h"
#include "StereoRendering.h"

/**
 * Simple representation of the backbuffer that the debug canvas renders to
 * This class may only be accessed from the render thread
 */
class FSlateCanvasRenderTarget : public FRenderTarget
{
public:
	/** FRenderTarget interface */
	virtual FIntPoint GetSizeXY() const
	{
		return ViewRect.Size();
	}

	/** Sets the texture that this target renders to */
	void SetRenderTargetTexture( FTexture2DRHIRef& InRHIRef )
	{
		RenderTargetTextureRHI = InRHIRef;
	}

	/** Clears the render target texture */
	void ClearRenderTargetTexture()
	{
		RenderTargetTextureRHI.SafeRelease();
	}

	/** Sets the viewport rect for the render target */
	void SetViewRect( const FIntRect& InViewRect ) 
	{ 
		ViewRect = InViewRect;
	}

	/** Gets the viewport rect for the render target */
	const FIntRect& GetViewRect() const 
	{
		return ViewRect; 
	}
private:
	FIntRect ViewRect;
};

#define INVALID_LAYER_ID UINT_MAX

FDebugCanvasDrawer::FDebugCanvasDrawer()
	: GameThreadCanvas( NULL )
	, RenderThreadCanvas( NULL )
	, RenderTarget( new FSlateCanvasRenderTarget )
	, LayerID(INVALID_LAYER_ID)
{}

void FDebugCanvasDrawer::ReleaseTexture()
{
	LayerTexture.SafeRelease();
}

void FDebugCanvasDrawer::ReleaseResources()
{
	FDebugCanvasDrawer* const ReleaseMe = this;

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ReleaseCommand,
		FDebugCanvasDrawer*, ReleaseMe, ReleaseMe,
		{
			ReleaseMe->ReleaseTexture();
		});

	FlushRenderingCommands();
}

FDebugCanvasDrawer::~FDebugCanvasDrawer()
{
	delete RenderTarget;

	// We assume that the render thread is no longer utilizing any canvases
	if( GameThreadCanvas.IsValid() && RenderThreadCanvas != GameThreadCanvas )
	{
		GameThreadCanvas.Reset();
	}

	if( RenderThreadCanvas.IsValid() )
	{
		// Capture a copy of the canvas until the render thread can delete it
		FCanvasPtr RTCanvas = RenderThreadCanvas;
		ENQUEUE_RENDER_COMMAND(DeleteDebugRenderThreadCanvas)(
			[RTCanvas](FRHICommandListImmediate& RHICmdList)
		{
		});

		RenderThreadCanvas = nullptr;
	}

	if (LayerID != INVALID_LAYER_ID)
	{
		GEngine->StereoRenderingDevice->GetStereoLayers()->DestroyLayer(LayerID);
		LayerID = INVALID_LAYER_ID;
	}
}

FCanvas* FDebugCanvasDrawer::GetGameThreadDebugCanvas()
{
	return GameThreadCanvas.Get();
}


void FDebugCanvasDrawer::BeginRenderingCanvas( const FIntRect& CanvasRect )
{
	if( CanvasRect.Size().X > 0 && CanvasRect.Size().Y > 0 )
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER
		(
			BeginRenderingDebugCanvas,
			FDebugCanvasDrawer*, CanvasDrawer, this, 
			FCanvasPtr, CanvasToRender, GameThreadCanvas,
			FIntRect, CanvasRect, CanvasRect,
			{
				// Delete the old rendering thread canvas
				if( CanvasDrawer->GetRenderThreadCanvas().IsValid() && CanvasToRender.IsValid() )
				{
					CanvasDrawer->DeleteRenderThreadCanvas();
				}

				if(!CanvasToRender.IsValid())
				{
					CanvasToRender = CanvasDrawer->GetRenderThreadCanvas();
				}

				CanvasDrawer->SetRenderThreadCanvas( CanvasRect, CanvasToRender );
			}
		);
		
		// Gave the canvas to the render thread
		GameThreadCanvas = nullptr;
	}
}


void FDebugCanvasDrawer::InitDebugCanvas(UWorld* InWorld)
{
	// If the canvas is not null there is more than one viewport draw call before slate draws.  This can happen on resizes. 
	// We need to delete the old canvas
		// This can also happen if we are debugging a HUD blueprint and in that case we need to continue using
		// the same canvas
	if (FSlateApplication::Get().IsNormalExecution())
	{
		GameThreadCanvas = MakeShared<FCanvas, ESPMode::ThreadSafe>(RenderTarget, nullptr, InWorld, InWorld ? InWorld->FeatureLevel : GMaxRHIFeatureLevel);

		// Do not allow the canvas to be flushed outside of our debug rendering path
		GameThreadCanvas->SetAllowedModes(FCanvas::Allow_DeleteOnRender);
	}

	if (GameThreadCanvas.IsValid())
	{
		const bool bIsStereoscopic3D = GEngine && GEngine->IsStereoscopic3D();
		IStereoLayers* const StereoLayers = (bIsStereoscopic3D && GEngine && GEngine->StereoRenderingDevice.IsValid()) ? GEngine->StereoRenderingDevice->GetStereoLayers() : nullptr;
		const bool bHMDAvailable = StereoLayers && bIsStereoscopic3D;

		static const auto DebugCanvasInLayerCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.DebugCanvasInLayer"));
		const bool bDebugInLayer = bHMDAvailable && DebugCanvasInLayerCVar && DebugCanvasInLayerCVar->GetValueOnAnyThread() != 0;
		GameThreadCanvas->SetUseInternalTexture(bDebugInLayer);

		if (bDebugInLayer && LayerTexture && bCanvasRenderedLastFrame)
		{
			if (StereoLayers)
			{
				const IStereoLayers::FLayerDesc StereoLayerDesc = StereoLayers->GetDebugCanvasLayerDesc(LayerTexture->GetRenderTargetItem().ShaderResourceTexture);
				if (LayerID == INVALID_LAYER_ID)
				{
					LayerID = StereoLayers->CreateLayer(StereoLayerDesc);
				}
				else
				{
					StereoLayers->SetLayerDesc(LayerID, StereoLayerDesc);
				}
			}
		}

		if (LayerID != INVALID_LAYER_ID && (!bDebugInLayer || !bCanvasRenderedLastFrame))
		{
			if (StereoLayers)
			{
				StereoLayers->DestroyLayer(LayerID);
			}
			LayerID = INVALID_LAYER_ID;
		}
	}
}

void FDebugCanvasDrawer::DrawRenderThread(FRHICommandListImmediate& RHICmdList, const void* InWindowBackBuffer)
{
	check( IsInRenderingThread() );

	if( RenderThreadCanvas.IsValid() )
	{
		FTexture2DRHIRef& RT = *(FTexture2DRHIRef*)InWindowBackBuffer;
		if (RenderThreadCanvas->IsUsingInternalTexture())
		{
			if (LayerTexture && RenderThreadCanvas->GetParentCanvasSize() != LayerTexture->GetDesc().Extent)
			{
				LayerTexture.SafeRelease();
			}

			if (!LayerTexture)
			{
				const FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(RenderThreadCanvas->GetParentCanvasSize(), PF_B8G8R8A8, FClearValueBinding(), TexCreate_SRGB, TexCreate_RenderTargetable, false));
				GetRendererModule().RenderTargetPoolFindFreeElement(RHICmdList, Desc, LayerTexture, TEXT("DebugCanvasLayerTexture"));
				UE_LOG(LogProfilingDebugging, Log, TEXT("Allocated a %d x %d texture for HMD canvas layer"), RenderThreadCanvas->GetParentCanvasSize().X, RenderThreadCanvas->GetParentCanvasSize().Y);
			}

			FTexture2DRHIRef& LayerTextureRT = *reinterpret_cast<FTexture2DRHIRef*>(&LayerTexture->GetRenderTargetItem().ShaderResourceTexture);
			RenderTarget->SetRenderTargetTexture(LayerTextureRT);
		}
		else
		{
			RenderTarget->SetRenderTargetTexture(RT);
		}

		bool bNeedToFlipVertical = RenderThreadCanvas->GetAllowSwitchVerticalAxis();
		// Do not flip when rendering to the back buffer
		RenderThreadCanvas->SetAllowSwitchVerticalAxis(false);
		if (RenderThreadCanvas->IsScaledToRenderTarget() && IsValidRef(RT)) 
		{
			RenderThreadCanvas->SetRenderTargetRect( FIntRect(0, 0, RT->GetSizeX(), RT->GetSizeY()) );
		}
		else
		{
			RenderThreadCanvas->SetRenderTargetRect( RenderTarget->GetViewRect() );
		}

		bCanvasRenderedLastFrame = RenderThreadCanvas->HasBatchesToRender();
		RenderThreadCanvas->Flush_RenderThread(RHICmdList, true);
		RenderThreadCanvas->SetAllowSwitchVerticalAxis(bNeedToFlipVertical);
		RenderTarget->ClearRenderTargetTexture();
	}
}

FCanvasPtr FDebugCanvasDrawer::GetRenderThreadCanvas()
{
	check( IsInRenderingThread() );
	return RenderThreadCanvas;
}

void FDebugCanvasDrawer::DeleteRenderThreadCanvas()
{
	check( IsInRenderingThread() );
	RenderThreadCanvas.Reset();
}

void FDebugCanvasDrawer::SetRenderThreadCanvas( const FIntRect& InCanvasRect, FCanvasPtr& Canvas )
{
	check( IsInRenderingThread() );
	RenderTarget->SetViewRect( InCanvasRect );
	RenderThreadCanvas = Canvas;
}
