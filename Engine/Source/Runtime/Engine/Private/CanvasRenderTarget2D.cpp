// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/CanvasRenderTarget2D.h"
#include "Misc/App.h"
#include "UObject/Package.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "CanvasTypes.h"
#include "Engine/Canvas.h"
#include "UObject/UObjectThreadContext.h"
#include "TextureResource.h"

UCanvasRenderTarget2D::UCanvasRenderTarget2D( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer),
	  World( nullptr )
{
	bNeedsTwoCopies = false;
	bShouldClearRenderTargetOnReceiveUpdate = true;
}


void UCanvasRenderTarget2D::UpdateResource()
{
	// Call parent implementation
	Super::UpdateResource();

	// Don't allocate canvas object for CRT2D CDO; also, we can't update it during PostLoad!
	if (IsTemplate() || FUObjectThreadContext::Get().IsRoutingPostLoad)
	{
		return;
	}

	RepaintCanvas();
}

void UCanvasRenderTarget2D::FastUpdateResource()
{
	if (Resource == nullptr)
	{
		// We don't have a resource, just take the fast path
		UpdateResource();
	}
	else
	{
		// Don't allocate canvas object for CRT2D CDO
		if (IsTemplate())
		{
			return;
		}

		RepaintCanvas();
	}
}

void UCanvasRenderTarget2D::RepaintCanvas()
{
	// Create or find the canvas object to use to render onto the texture.  Multiple canvas render target textures can share the same canvas.
	static const FName CanvasName(TEXT("CanvasRenderTarget2DCanvas"));
	UCanvas* Canvas = (UCanvas*)StaticFindObjectFast(UCanvas::StaticClass(), GetTransientPackage(), CanvasName);
	if (Canvas == nullptr)
	{
		Canvas = NewObject<UCanvas>(GetTransientPackage(), CanvasName);
		Canvas->AddToRoot();
	}

	// Create the FCanvas which does the actual rendering.
	const UWorld* WorldPtr = World.Get();
	const ERHIFeatureLevel::Type FeatureLevel = WorldPtr != nullptr ? World->FeatureLevel : GMaxRHIFeatureLevel;
	FCanvas RenderCanvas(GameThread_GetRenderTargetResource(), nullptr, FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime, FeatureLevel);

	Canvas->Init(GetSurfaceWidth(), GetSurfaceHeight(), nullptr, &RenderCanvas);
	Canvas->Update();

	// Update the resource immediately to remove it from the deferred resource update list. This prevents the texture
	// from being cleared each frame.
	UpdateResourceImmediate(bShouldClearRenderTargetOnReceiveUpdate);

	// Enqueue the rendering command to set up the rendering canvas.
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER
		(
			CanvasRenderTargetMakeCurrentCommand,
			FTextureRenderTarget2DResource*,
			TextureRenderTarget,
			(FTextureRenderTarget2DResource*)GameThread_GetRenderTargetResource(),
			{
				SetRenderTarget(RHICmdList, TextureRenderTarget->GetRenderTargetTexture(), FTexture2DRHIRef(), true);
				RHICmdList.SetViewport(0, 0, 0.0f, TextureRenderTarget->GetSizeXY().X, TextureRenderTarget->GetSizeXY().Y, 1.0f);
			}
	);


	if (!IsPendingKill() && OnCanvasRenderTargetUpdate.IsBound())
	{
		OnCanvasRenderTargetUpdate.Broadcast(Canvas, GetSurfaceWidth(), GetSurfaceHeight());
	}

	ReceiveUpdate(Canvas, GetSurfaceWidth(), GetSurfaceHeight());

	// Clean up and flush the rendering canvas.
	Canvas->Canvas = nullptr;
	RenderCanvas.Flush_GameThread();

	// Enqueue the rendering command to copy the freshly rendering texture resource back to the render target RHI 
	// so that the texture is updated and available for rendering.
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER
		(
			CanvasRenderTargetResolveCommand,
			FTextureRenderTargetResource*,
			RenderTargetResource,
			GameThread_GetRenderTargetResource(),
			{
				RHICmdList.CopyToResolveTarget(RenderTargetResource->GetRenderTargetTexture(), RenderTargetResource->TextureRHI, true, FResolveParams());
			}
	);
}

UCanvasRenderTarget2D* UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(UObject* WorldContextObject, TSubclassOf<UCanvasRenderTarget2D> CanvasRenderTarget2DClass, int32 Width, int32 Height)
{
	if ((Width > 0) && (Height > 0) && (CanvasRenderTarget2DClass != NULL))
	{
		UCanvasRenderTarget2D* NewCanvasRenderTarget = NewObject<UCanvasRenderTarget2D>(GetTransientPackage(), CanvasRenderTarget2DClass);
		if (NewCanvasRenderTarget)
		{
			NewCanvasRenderTarget->World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
			NewCanvasRenderTarget->InitAutoFormat(Width, Height);
			return NewCanvasRenderTarget;
		}
	}

	return nullptr;
}

void UCanvasRenderTarget2D::GetSize(int32& Width, int32& Height)
{
	Width = GetSurfaceWidth();
	Height = GetSurfaceHeight();
}


UWorld* UCanvasRenderTarget2D::GetWorld() const
{
	return World.Get();
}
