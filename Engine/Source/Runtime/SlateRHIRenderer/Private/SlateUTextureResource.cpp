// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SlateUTextureResource.h"

FSlateBaseUTextureResource::FSlateBaseUTextureResource(UTexture* InTexture)
	: TextureObject(InTexture)
{
}

FSlateBaseUTextureResource::~FSlateBaseUTextureResource()
{
}

uint32 FSlateBaseUTextureResource::GetWidth() const
{
	return TextureObject->GetSurfaceWidth();
}

uint32 FSlateBaseUTextureResource::GetHeight() const
{
	return TextureObject->GetSurfaceHeight();
}

ESlateShaderResource::Type FSlateBaseUTextureResource::GetType() const
{
	return ESlateShaderResource::TextureObject;
}


TSharedPtr<FSlateUTextureResource> FSlateUTextureResource::NullResource = MakeShareable( new FSlateUTextureResource(nullptr) );

FSlateUTextureResource::FSlateUTextureResource(UTexture* InTexture)
	: FSlateBaseUTextureResource(InTexture)
	, Proxy(new FSlateShaderResourceProxy)
{
	if(TextureObject)
	{
		Proxy->ActualSize = FIntPoint(InTexture->GetSurfaceWidth(), InTexture->GetSurfaceHeight());
		Proxy->Resource = this;
	}
}

FSlateUTextureResource::~FSlateUTextureResource()
{
	if ( Proxy )
	{
		delete Proxy;
	}
}

void FSlateUTextureResource::UpdateRenderResource(FTexture* InFTexture)
{
	if ( InFTexture )
	{
		// If the RHI data has changed, it's possible the underlying size of the texture has changed,
		// if that's true we need to update the actual size recorded on the proxy as well, otherwise 
		// the texture will continue to render using the wrong size.
		Proxy->ActualSize = FIntPoint(InFTexture->GetSizeX(), InFTexture->GetSizeY());
	}
}
