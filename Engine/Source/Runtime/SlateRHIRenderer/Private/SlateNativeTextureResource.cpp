// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SlateNativeTextureResource.h"
#include "Textures/SlateShaderResource.h"
#include "Slate/SlateTextures.h"

TSharedPtr<FSlateDynamicTextureResource> FSlateDynamicTextureResource::NullResource = MakeShareable( new FSlateDynamicTextureResource( NULL ) );

FSlateDynamicTextureResource::FSlateDynamicTextureResource(FSlateTexture2DRHIRef* ExistingTexture)
	: Proxy(new FSlateShaderResourceProxy)
	, RHIRefTexture(ExistingTexture != NULL ? ExistingTexture : new FSlateTexture2DRHIRef(NULL, 0, 0))
{
	Proxy->Resource = RHIRefTexture;
}


FSlateDynamicTextureResource::~FSlateDynamicTextureResource()
{
	if(Proxy)
	{
		delete Proxy;
	}

	if(RHIRefTexture)
	{
		delete RHIRefTexture;
	}
}
