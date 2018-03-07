// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "Slate/SlateTextures.h"
#include "RenderUtils.h"
#include "ClearQuad.h"

FSlateTexture2DRHIRef::FSlateTexture2DRHIRef( FTexture2DRHIRef InRef, uint32 InWidth, uint32 InHeight )
	: TSlateTexture( InRef )
	, Width( InWidth )
	, Height( InHeight )
	, PixelFormat( PF_Unknown )
	, bCreateEmptyTexture( false )
{

}

FSlateTexture2DRHIRef::FSlateTexture2DRHIRef( uint32 InWidth, uint32 InHeight, EPixelFormat InPixelFormat, TSharedPtr<FSlateTextureData, ESPMode::ThreadSafe> InTextureData, uint32 InTexCreateFlags, bool bInCreateEmptyTexture)
	: Width( InWidth )
	, Height( InHeight )
	, TexCreateFlags( InTexCreateFlags )
	, TextureData( InTextureData )
	, PixelFormat( InPixelFormat )
	, bCreateEmptyTexture( bInCreateEmptyTexture )
{

}

FSlateTexture2DRHIRef::~FSlateTexture2DRHIRef()
{

}

void FSlateTexture2DRHIRef::Cleanup()
{
	BeginReleaseResource(this);
	BeginCleanup(this);
}

void FSlateTexture2DRHIRef::FinishCleanup()
{
	delete this;
}

void FSlateTexture2DRHIRef::InitDynamicRHI()
{
	check( IsInRenderingThread() );

	if( Width > 0 && Height > 0 )
	{
		if( TextureData.IsValid() || bCreateEmptyTexture )
		{
			check( !IsValidRef( ShaderResource) );
			FRHIResourceCreateInfo CreateInfo;
			ShaderResource = RHICreateTexture2D( Width, Height, PixelFormat, 1, 1, TexCreateFlags, CreateInfo );
			check( IsValidRef( ShaderResource ) );

			INC_MEMORY_STAT_BY(STAT_SlateTextureGPUMemory, Width*Height*GPixelFormats[PixelFormat].BlockBytes);
		}

		if( TextureData.IsValid() && TextureData->GetRawBytes().Num() > 0 )
		{
			check(Width == TextureData->GetWidth());
			check(Height == TextureData->GetHeight());

			uint32 Stride;
			uint8* DestTextureData = (uint8*)RHILockTexture2D(ShaderResource, 0, RLM_WriteOnly, Stride, false);
			const uint8* SourceTextureData = TextureData->GetRawBytes().GetData();
			const uint32 DataStride = Width * GPixelFormats[PixelFormat].BlockBytes;
			if (Stride == DataStride)
			{
				FMemory::Memcpy(DestTextureData, SourceTextureData, DataStride * Height);
			}
			else
			{
				checkf(GPixelFormats[PixelFormat].BlockSizeX == 1 
					&& GPixelFormats[PixelFormat].BlockSizeY == 1 
					&& GPixelFormats[PixelFormat].BlockSizeZ == 1,
					TEXT("Tried to use compressed format?"));
				for (uint32 i = 0; i < Height; i++)
				{
					FMemory::Memcpy(DestTextureData, SourceTextureData, DataStride);
					DestTextureData += Stride;
					SourceTextureData += DataStride;
				}
			}
			RHIUnlockTexture2D(ShaderResource, 0, false);
			TextureData->Empty();
		}
	}
}

void FSlateTexture2DRHIRef::ReleaseDynamicRHI()
{
	check( IsInRenderingThread() );

	if( IsValidRef(ShaderResource) )
	{
		DEC_MEMORY_STAT_BY(STAT_SlateTextureGPUMemory, Width*Height*GPixelFormats[PixelFormat].BlockBytes);
	}

	ShaderResource.SafeRelease();

}

void FSlateTexture2DRHIRef::Resize( uint32 InWidth, uint32 InHeight )
{
	check( IsInRenderingThread() );
	Width = InWidth;
	Height = InHeight;
	UpdateRHI();
}

void FSlateTexture2DRHIRef::SetRHIRef( FTexture2DRHIRef InRHIRef, uint32 InWidth, uint32 InHeight )
{
	check( IsInRenderingThread() );
	ShaderResource = InRHIRef;
	Width = InWidth;
	Height = InHeight;
}

void FSlateTexture2DRHIRef::SetTextureData( FSlateTextureDataPtr NewTextureData )
{
	check( IsInRenderingThread() );
	Width = NewTextureData->GetWidth();
	Height = NewTextureData->GetHeight();
	TextureData = NewTextureData;
}

void FSlateTexture2DRHIRef::SetTextureData( FSlateTextureDataPtr NewTextureData, EPixelFormat InPixelFormat, uint32 InTexCreateFlags )
{
	check( IsInRenderingThread() );

	SetTextureData( NewTextureData );

	PixelFormat = InPixelFormat;
	TexCreateFlags = InTexCreateFlags;
}


void FSlateTexture2DRHIRef::ClearTextureData()
{
	check( IsInRenderingThread() );
	TextureData.Reset();
}

void FSlateTexture2DRHIRef::ResizeTexture(uint32 InWidth, uint32 InHeight)
{
	if (GetWidth() != InWidth || GetHeight() != InHeight)
	{
		if (IsInRenderingThread())
		{
			Resize(InWidth, InHeight);
		}
		else
		{
			FIntPoint Dimensions(InWidth, InHeight);
			ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(ResizeSlateTexture,
			FSlateTexture2DRHIRef*, TextureRHIRef, this,
			FIntPoint, InDimensions, Dimensions,
			{
				TextureRHIRef->Resize(InDimensions.X, InDimensions.Y);
			});
		}
	}
}

void FSlateTexture2DRHIRef::UpdateTexture(const TArray<uint8>& Bytes)
{
	if (IsInRenderingThread())
	{
		uint32 Stride = 0;
		void* TextureBuffer = RHILockTexture2D(GetTypedResource(), 0, RLM_WriteOnly, Stride, false);
		FMemory::Memcpy(TextureBuffer, Bytes.GetData(), Bytes.Num());
		RHIUnlockTexture2D(GetTypedResource(), 0, false);
	}
	else
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(UpdateSlateTexture,
		FSlateTexture2DRHIRef*, TextureRHIRef, this,
		const TArray<uint8>&, TextureData, Bytes,
		{
			uint32 Stride = 0;
			void* TextureBuffer = RHILockTexture2D(TextureRHIRef->GetTypedResource(), 0, RLM_WriteOnly, Stride, false);
			FMemory::Memcpy(TextureBuffer, TextureData.GetData(), TextureData.Num());
			RHIUnlockTexture2D(TextureRHIRef->GetTypedResource(), 0, false);
		});
	}
}

void FSlateTexture2DRHIRef::UpdateTextureThreadSafe(const TArray<uint8>& Bytes)
{
	if(IsInGameThread())
	{
		// Make bulk data for updating the texture memory later
		FSlateTextureData* BulkData = new FSlateTextureData( Bytes.Num(), 0, 1, Bytes );

		// Update the texture RHI
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			FSlateTexture2DRHIRef_UpdateTextureThreadSafe,
			FSlateTexture2DRHIRef*, ThisTexture, this,
			FSlateTextureData*, BulkData, BulkData,
			{
				ThisTexture->UpdateTexture( BulkData->GetRawBytes() );
				delete BulkData;
			});
	}
}

void FSlateTexture2DRHIRef::UpdateTextureThreadSafeRaw(uint32 InWidth, uint32 InHeight, const void* Buffer, const FIntRect& Dirty)
{
	if (IsInGameThread())
	{
		// No cheap way to avoid having to copy the Buffer, as we cannot guarantee it will not be touched before the rendering thread is done with it.
		FSlateTextureData* BulkData = new FSlateTextureData( (uint8*)Buffer, InWidth, InHeight, 4 );
		UpdateTextureThreadSafeWithTextureData(BulkData);
	}
}

void FSlateTexture2DRHIRef::UpdateTextureThreadSafeWithTextureData(FSlateTextureData* BulkData)
{
	check(IsInGameThread());
	// Update the texture RHI
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FSlateTexture2DRHIRef_UpdateTextureThreadSafeWithTextureData,
		FSlateTexture2DRHIRef*, ThisTexture, this,
		FSlateTextureData*, BulkData, BulkData,
		{
			if (ThisTexture->GetWidth() != BulkData->GetWidth() || ThisTexture->GetHeight() != BulkData->GetHeight())
			{
				ThisTexture->Resize(BulkData->GetWidth(), BulkData->GetHeight());
			}
			ThisTexture->UpdateTexture(BulkData->GetRawBytes());
			delete BulkData;
		});
}

void FSlateRenderTargetRHI::SetRHIRef( FTexture2DRHIRef InRenderTargetTexture, uint32 InWidth, uint32 InHeight )
{
	check( IsInRenderingThread() );
	ShaderResource = InRenderTargetTexture;
	Width = InWidth;
	Height = InHeight;
}





FSlateTextureRenderTarget2DResource::FSlateTextureRenderTarget2DResource(const FLinearColor& InClearColor, int32 InTargetSizeX, int32 InTargetSizeY, uint8 InFormat, ESamplerFilter InFilter, TextureAddress InAddressX, TextureAddress InAddressY, float InTargetGamma)
	:	ClearColor(InClearColor)
	,   TargetSizeX(InTargetSizeX)
	,	TargetSizeY(InTargetSizeY)
	,	Format(InFormat)
	,	Filter(InFilter)
	,	AddressX(InAddressX)
	,	AddressY(InAddressY)
	,	TargetGamma(InTargetGamma)
{
}

void FSlateTextureRenderTarget2DResource::SetSize(int32 InSizeX,int32 InSizeY)
{
	check(IsInRenderingThread());

	if (InSizeX != TargetSizeX || InSizeY != TargetSizeY)
	{
		TargetSizeX = InSizeX;
		TargetSizeY = InSizeY;
		// reinit the resource with new TargetSizeX,TargetSizeY
		UpdateRHI();
	}	
}

void FSlateTextureRenderTarget2DResource::ClampSize(int32 MaxSizeX,int32 MaxSizeY)
{
	check(IsInRenderingThread());

	// upsize to go back to original or downsize to clamp to max
	int32 NewSizeX = FMath::Min<int32>(TargetSizeX,MaxSizeX);
	int32 NewSizeY = FMath::Min<int32>(TargetSizeY,MaxSizeY);
	if (NewSizeX != TargetSizeX || NewSizeY != TargetSizeY)
	{
		TargetSizeX = NewSizeX;
		TargetSizeY = NewSizeY;
		// reinit the resource with new TargetSizeX,TargetSizeY
		UpdateRHI();
	}	
}

void FSlateTextureRenderTarget2DResource::InitDynamicRHI()
{
	check(IsInRenderingThread());

	if( TargetSizeX > 0 && TargetSizeY > 0 )
	{
		// Create the RHI texture. Only one mip is used and the texture is targetable for resolve.
		FRHIResourceCreateInfo CreateInfo = { FClearValueBinding(ClearColor) };
		RHICreateTargetableShaderResource2D(
			TargetSizeX, 
			TargetSizeY, 
			Format, 
			1,
			/*TexCreateFlags=*/0,
			TexCreate_RenderTargetable,
			/*bNeedsTwoCopies=*/false,
			CreateInfo,
			RenderTargetTextureRHI,
			Texture2DRHI
			);
		TextureRHI = (FTextureRHIRef&)Texture2DRHI;
	}

	// Create the sampler state RHI resource.
	FSamplerStateInitializerRHI SamplerStateInitializer
	(
		Filter,
		AddressX == TA_Wrap ? AM_Wrap : (AddressX == TA_Clamp ? AM_Clamp : AM_Mirror),
		AddressY == TA_Wrap ? AM_Wrap : (AddressY == TA_Clamp ? AM_Clamp : AM_Mirror),
		AM_Wrap
	);
	SamplerStateRHI = RHICreateSamplerState( SamplerStateInitializer );
}

void FSlateTextureRenderTarget2DResource::ReleaseDynamicRHI()
{
	check(IsInRenderingThread());

	// Release the FTexture RHI resources here as well
	ReleaseRHI();

	Texture2DRHI.SafeRelease();
	RenderTargetTextureRHI.SafeRelease();	

	// Remove from global list of deferred clears
	RemoveFromDeferredUpdateList();
}

void FSlateTextureRenderTarget2DResource::UpdateDeferredResource(FRHICommandListImmediate& RHICmdList, bool bClearRenderTarget/*=true*/)
{
	check(IsInRenderingThread());

	// Clear the target surface to green
	if (bClearRenderTarget)
	{
		FRHIRenderTargetView View = FRHIRenderTargetView(RenderTargetTextureRHI, ERenderTargetLoadAction::EClear);
		FRHISetRenderTargetsInfo Info(1, &View, FRHIDepthRenderTargetView());
		RHICmdList.SetRenderTargetsAndClear(Info);
	}

	// Copy surface to the texture for use
	RHICmdList.CopyToResolveTarget(RenderTargetTextureRHI, TextureRHI, true, FResolveParams());
}

uint32 FSlateTextureRenderTarget2DResource::GetSizeX() const
{ 
	return TargetSizeX; 
}

uint32 FSlateTextureRenderTarget2DResource::GetSizeY() const
{ 
	return TargetSizeY; 
}

FIntPoint FSlateTextureRenderTarget2DResource::GetSizeXY() const
{ 
	return FIntPoint(TargetSizeX, TargetSizeY); 
}

float FSlateTextureRenderTarget2DResource::GetDisplayGamma() const
{
	if (TargetGamma > KINDA_SMALL_NUMBER * 10.0f)
	{
		return TargetGamma;
	}
	if (Format == PF_FloatRGB || Format == PF_FloatRGBA )
	{
		return 1.0f;
	}
	return FTextureRenderTargetResource::GetDisplayGamma();
}
