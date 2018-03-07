// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EmptyViewport.cpp: Empty viewport RHI implementation.
=============================================================================*/

#include "EmptyRHIPrivate.h"


FEmptyViewport::FEmptyViewport(void* WindowHandle, uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen)
{

}

FEmptyViewport::~FEmptyViewport()
{
}


/*=============================================================================
 *	The following RHI functions must be called from the main thread.
 *=============================================================================*/
FViewportRHIRef FEmptyDynamicRHI::RHICreateViewport(void* WindowHandle,uint32 SizeX,uint32 SizeY,bool bIsFullscreen,EPixelFormat PreferredPixelFormat)
{
	check( IsInGameThread() );
	return new FEmptyViewport(WindowHandle, SizeX, SizeY, bIsFullscreen);
}

void FEmptyDynamicRHI::RHIResizeViewport(FViewportRHIParamRef ViewportRHI,uint32 SizeX,uint32 SizeY,bool bIsFullscreen)
{
	check( IsInGameThread() );

	FEmptyViewport* Viewport = ResourceCast(ViewportRHI);
}

void FEmptyDynamicRHI::RHITick( float DeltaTime )
{
	check( IsInGameThread() );

}

/*=============================================================================
 *	Viewport functions.
 *=============================================================================*/

void FEmptyDynamicRHI::RHIBeginDrawingViewport(FViewportRHIParamRef ViewportRHI, FTextureRHIParamRef RenderTargetRHI)
{
	FEmptyViewport* Viewport = ResourceCast(ViewportRHI);

	//RHISetRenderTarget(RHIGetViewportBackBuffer(ViewportRHI), NULL);
}

void FEmptyDynamicRHI::RHIEndDrawingViewport(FViewportRHIParamRef ViewportRHI,bool bPresent,bool bLockToVsync)
{
	FEmptyViewport* Viewport = ResourceCast(ViewportRHI);
}

FTexture2DRHIRef FEmptyDynamicRHI::RHIGetViewportBackBuffer(FViewportRHIParamRef ViewportRHI)
{
	FEmptyViewport* Viewport = ResourceCast(ViewportRHI);

	return FTexture2DRHIRef();
}

void FEmptyDynamicRHI::RHIAdvanceFrameForGetViewportBackBuffer(FViewportRHIParamRef Viewport)
{
}
