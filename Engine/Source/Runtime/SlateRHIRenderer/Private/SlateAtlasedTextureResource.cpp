// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SlateAtlasedTextureResource.h"
#include "Slate/SlateTextureAtlasInterface.h"

TSharedPtr<FSlateAtlasedTextureResource> FSlateAtlasedTextureResource::NullResource = MakeShareable( new FSlateAtlasedTextureResource(nullptr) );

FSlateAtlasedTextureResource::FSlateAtlasedTextureResource(UTexture* InTexture)
	: FSlateBaseUTextureResource(InTexture)
{
}

FSlateAtlasedTextureResource::~FSlateAtlasedTextureResource()
{
	for ( FObjectResourceMap::TIterator ProxyIt(ProxyMap); ProxyIt; ++ProxyIt )
	{
		FSlateShaderResourceProxy* Proxy = ProxyIt.Value();
		delete Proxy;
	}
}

FSlateShaderResourceProxy* FSlateAtlasedTextureResource::FindOrCreateAtlasedProxy(UObject* InAtlasedObject, const FSlateAtlasData& AtlasData)
{
	FSlateShaderResourceProxy* Proxy = ProxyMap.FindRef(InAtlasedObject);
	if ( Proxy == nullptr )
	{
		// when we use image-DrawAsBox with PaperSprite, we need to change its actual size as its actual dimension.
		FVector2D ActualSize(TextureObject->GetSurfaceWidth() * AtlasData.SizeUV.X, TextureObject->GetSurfaceHeight() * AtlasData.SizeUV.Y);

		Proxy = new FSlateShaderResourceProxy();
		Proxy->Resource = this;
		Proxy->ActualSize = ActualSize.IntPoint();
		Proxy->StartUV = AtlasData.StartUV;
		Proxy->SizeUV = AtlasData.SizeUV;

		ProxyMap.Add(InAtlasedObject, Proxy);
	}

	return Proxy;
}
