// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/App.h"
#include "Textures/SlateShaderResource.h"
#include "TextureResource.h"
#include "Engine/Texture.h"

/** A resource for rendering a UTexture object in Slate */
class FSlateBaseUTextureResource : public FSlateShaderResource
{
public:
	FSlateBaseUTextureResource(UTexture* InTexture);
	virtual ~FSlateBaseUTextureResource();

	/**
	* FSlateShaderRsourceInterface
	*/
	virtual uint32 GetWidth() const;
	virtual uint32 GetHeight() const;
	virtual ESlateShaderResource::Type GetType() const;

	/** Gets the RHI resource used for rendering and updates the last render time for texture streaming */
	FTextureRHIRef AccessRHIResource()
	{
		if ( TextureObject && TextureObject->Resource )
		{
			FTexture* TextureResource = TextureObject->Resource;
			TextureResource->LastRenderTime = FApp::GetCurrentTime();

			return TextureResource->TextureRHI;
		}

		return FTextureRHIRef();
	}

public:
	/** Texture UObject.  Note: lifetime is managed externally */
	UTexture* TextureObject;
};


/** A resource for rendering a UTexture object in Slate */
class FSlateUTextureResource : public FSlateBaseUTextureResource
{
public:
	static TSharedPtr<FSlateUTextureResource> NullResource;

	FSlateUTextureResource(UTexture* InTexture);
	virtual ~FSlateUTextureResource();

	/**
	 * Updates the rendering resource with a potentially new texture
	 */
	void UpdateRenderResource(FTexture* InFTexture);

public:
	/** Slate rendering proxy */
	FSlateShaderResourceProxy* Proxy;
};
