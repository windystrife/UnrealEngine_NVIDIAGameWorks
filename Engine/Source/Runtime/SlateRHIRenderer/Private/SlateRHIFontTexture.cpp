// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SlateRHIFontTexture.h"
#include "Rendering/SlateRenderer.h"
#include "RenderingThread.h"
#include "RenderUtils.h"

FSlateFontTextureRHIResource::FSlateFontTextureRHIResource(uint32 InWidth, uint32 InHeight)
	: Width( InWidth )
	, Height( InHeight )
{
}

void FSlateFontTextureRHIResource::InitDynamicRHI()
{
	check( IsInRenderingThread() );

	// Create the texture
	if( Width > 0 && Height > 0 )
	{
		check( !IsValidRef( ShaderResource) );
		FRHIResourceCreateInfo CreateInfo;
		ShaderResource = RHICreateTexture2D( Width, Height, PF_A8, 1, 1, TexCreate_Dynamic, CreateInfo );
		check( IsValidRef( ShaderResource ) );

		// Also assign the reference to the FTextureResource variable so that the Engine can access it
		TextureRHI = ShaderResource;

		// Create the sampler state RHI resource.
		FSamplerStateInitializerRHI SamplerStateInitializer
		(
		  SF_Bilinear,
		  AM_Clamp,
		  AM_Clamp,
		  AM_Wrap,
		  0,
		  1, // Disable anisotropic filtering, since aniso doesn't respect MaxLOD
		  0,
		  0
		);
		SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);

		// Create a custom sampler state for using this texture in a deferred pass, where ddx / ddy are discontinuous
		FSamplerStateInitializerRHI DeferredPassSamplerStateInitializer
		(
		  SF_Bilinear,
		  AM_Clamp,
		  AM_Clamp,
		  AM_Wrap,
		  0,
		  1, // Disable anisotropic filtering, since aniso doesn't respect MaxLOD
		  0,
		  0
		);
		DeferredPassSamplerStateRHI = RHICreateSamplerState(DeferredPassSamplerStateInitializer);

		INC_MEMORY_STAT_BY(STAT_SlateTextureGPUMemory, Width*Height*GPixelFormats[PF_A8].BlockBytes);
	}
}

void FSlateFontTextureRHIResource::ReleaseDynamicRHI()
{
	check( IsInRenderingThread() );

	// Release the texture
	if( IsValidRef(ShaderResource) )
	{
		DEC_MEMORY_STAT_BY(STAT_SlateTextureGPUMemory, Width*Height*GPixelFormats[PF_A8].BlockBytes);
	}

	ShaderResource.SafeRelease();
}

FSlateFontAtlasRHI::FSlateFontAtlasRHI( uint32 Width, uint32 Height )
	: FSlateFontAtlas( Width, Height ) 
	, FontTexture( new FSlateFontTextureRHIResource( Width, Height ) )
{
}

FSlateFontAtlasRHI::~FSlateFontAtlasRHI()
{
}

void FSlateFontAtlasRHI::ReleaseResources()
{
	checkSlow( IsThreadSafeForSlateRendering() );

	BeginReleaseResource( FontTexture.Get() );
}

void FSlateFontAtlasRHI::ConditionalUpdateTexture()
{
	if( bNeedsUpdate )
	{
		if (IsInRenderingThread())
		{
			FontTexture->InitResource();

			uint32 DestStride;
			uint8* TempData = (uint8*)RHILockTexture2D( FontTexture->GetTypedResource(), 0, RLM_WriteOnly, /*out*/ DestStride, false );
			// check( DestStride == Atlas.BytesPerPixel * Atlas.AtlasWidth ); // Temporarily disabling check
			FMemory::Memcpy( TempData, AtlasData.GetData(), BytesPerPixel*AtlasWidth*AtlasHeight );
			RHIUnlockTexture2D( FontTexture->GetTypedResource(),0,false );
		}
		else
		{
			checkSlow( IsThreadSafeForSlateRendering() );

			BeginInitResource( FontTexture.Get() );

			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER( SlateUpdateFontAtlasTextureCommand,
				FSlateFontAtlasRHI&, Atlas, *this,
			{
				uint32 DestStride;
				uint8* TempData = (uint8*)RHILockTexture2D( Atlas.FontTexture->GetTypedResource(), 0, RLM_WriteOnly, /*out*/ DestStride, false );
				// check( DestStride == Atlas.BytesPerPixel * Atlas.AtlasWidth ); // Temporarily disabling check
				FMemory::Memcpy( TempData, Atlas.AtlasData.GetData(), Atlas.BytesPerPixel*Atlas.AtlasWidth*Atlas.AtlasHeight );
				RHIUnlockTexture2D( Atlas.FontTexture->GetTypedResource(),0,false );
			});
		}

		bNeedsUpdate = false;
	}
}

FSlateFontTextureRHI::FSlateFontTextureRHI(const uint32 InWidth, const uint32 InHeight, const TArray<uint8>& InRawData)
	: FontTexture(new FSlateFontTextureRHIResource(InWidth, InHeight))
{
	if (IsInRenderingThread())
	{
		FontTexture->InitResource();
		UpdateTextureFromSource(InWidth, InHeight, InRawData);
	}
	else
	{
		checkSlow(IsThreadSafeForSlateRendering());

		PendingSourceData.Reset(new FPendingSourceData(InWidth, InHeight, InRawData));

		BeginInitResource(FontTexture.Get());

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(SlateUpdateFontTextureCommand,
			FSlateFontTextureRHI&, FontTexture, *this,
			{
				FontTexture.UpdateTextureFromSource(FontTexture.PendingSourceData->SourceWidth, FontTexture.PendingSourceData->SourceHeight, FontTexture.PendingSourceData->SourceData);
				FontTexture.PendingSourceData.Reset();
			});
	}
}

FSlateFontTextureRHI::~FSlateFontTextureRHI()
{
}

void FSlateFontTextureRHI::ReleaseResources()
{
	checkSlow(IsThreadSafeForSlateRendering());

	BeginReleaseResource(FontTexture.Get());
}

void FSlateFontTextureRHI::UpdateTextureFromSource(const uint32 SourceWidth, const uint32 SourceHeight, const TArray<uint8>& SourceData)
{
	static const uint32 BytesPerPixel = sizeof(uint8);

	uint32 DestStride;
	uint8* LockedTextureData = static_cast<uint8*>(RHILockTexture2D(FontTexture->GetTypedResource(), 0, RLM_WriteOnly, /*out*/ DestStride, false));

	// If our stride matches our width, we can copy in a single call, rather than copy each line
	if (BytesPerPixel * SourceWidth == DestStride)
	{
		FMemory::Memcpy(LockedTextureData, SourceData.GetData(), BytesPerPixel * SourceWidth * SourceHeight);
	}
	else
	{
		for (uint32 RowIndex = 0; RowIndex < SourceHeight; ++RowIndex)
		{
			FMemory::Memcpy(LockedTextureData + (RowIndex * DestStride), SourceData.GetData() + (RowIndex * SourceWidth), BytesPerPixel * SourceWidth);
		}
	}
	
	RHIUnlockTexture2D(FontTexture->GetTypedResource(), 0, false);
}
