// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SlateRHITextureAtlas.h"
#include "Textures/SlateTextureData.h"
#include "Rendering/SlateRenderer.h"
#include "RenderingThread.h"
#include "RenderUtils.h"
#include "Slate/SlateTextures.h"


/* FSlateTextureAtlasRHI structors
 *****************************************************************************/

FSlateTextureAtlasRHI::FSlateTextureAtlasRHI( uint32 InWidth, uint32 InHeight, ESlateTextureAtlasPaddingStyle PaddingStyle )
	: FSlateTextureAtlas(InWidth, InHeight, GPixelFormats[PF_B8G8R8A8].BlockBytes, PaddingStyle)
	, AtlasTexture(new FSlateTexture2DRHIRef(InWidth, InHeight, PF_B8G8R8A8, NULL, TexCreate_SRGB, true)) 
{
}


FSlateTextureAtlasRHI::~FSlateTextureAtlasRHI( )
{
	if (AtlasTexture != nullptr)
	{
		delete AtlasTexture;
	}
}


/* FSlateTextureAtlasRHI interface
 *****************************************************************************/

void FSlateTextureAtlasRHI::ReleaseAtlasTexture( )
{
	bNeedsUpdate = false;
	BeginReleaseResource(AtlasTexture);
}


void FSlateTextureAtlasRHI::UpdateTexture_RenderThread( FSlateTextureData* RenderThreadData )
{
	check(IsInRenderingThread());

	if (!AtlasTexture->IsInitialized())
	{
		AtlasTexture->InitResource();
	}

	check(AtlasTexture->IsInitialized());

	uint32 DestStride;
	uint8* TempData = (uint8*)RHILockTexture2D(AtlasTexture->GetTypedResource(), 0, RLM_WriteOnly, /*out*/ DestStride, false);
	// check(DestStride == (RenderThreadData->GetBytesPerPixel() * RenderThreadData->GetWidth())); // Temporarily disable check

	FMemory::Memcpy(TempData, RenderThreadData->GetRawBytes().GetData(), RenderThreadData->GetBytesPerPixel() * RenderThreadData->GetWidth() * RenderThreadData->GetHeight());

	RHIUnlockTexture2D(AtlasTexture->GetTypedResource(), 0, false);

	delete RenderThreadData;
}


/* FSlateTextureAtlas overrides
 *****************************************************************************/

void FSlateTextureAtlasRHI::ConditionalUpdateTexture( )
{
	checkSlow(IsThreadSafeForSlateRendering());

	if (bNeedsUpdate)
	{
		// Copy the game thread data. This is deleted on the render thread
		FSlateTextureData* RenderThreadData = new FSlateTextureData( AtlasWidth, AtlasHeight, BytesPerPixel, AtlasData ); 
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER( SlateUpdateAtlasTextureCommand,
			FSlateTextureAtlasRHI&, Atlas, *this,
			FSlateTextureData*, InRenderThreadData, RenderThreadData,
		{
			Atlas.UpdateTexture_RenderThread( InRenderThreadData );
		});

		bNeedsUpdate = false;
	}
}
