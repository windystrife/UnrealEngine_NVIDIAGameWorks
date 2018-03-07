// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateUTextureResource.h"

struct FSlateAtlasData;

/**
 * A resource for rendering a sub-rect of a UTexture atlas object in Slate. Each 
 * ISlateTextureAtlasInterface* should uniquely identify a subregion of the UTexture atlas.
 */
class FSlateAtlasedTextureResource : public FSlateBaseUTextureResource
{
public:
	static TSharedPtr<FSlateAtlasedTextureResource> NullResource;

	/** Initializes a new atlased UTexture resource, the incoming texture should be the entire atlas. */
	FSlateAtlasedTextureResource(UTexture* InTexture);
	
	/** Destructor */
	virtual ~FSlateAtlasedTextureResource();
	
	/**
	 * Finds or creates the rendering proxy for a given atlas'ed object.
	 * @param InAtlasedObject The atlased object to find a rendering proxy for.
	 * @param InAtlasedObjectInterface The Interface pointer for the InAtlasedObject.
	 */
	FSlateShaderResourceProxy* FindOrCreateAtlasedProxy(UObject* InAtlasedObject, const FSlateAtlasData& AtlasData);
	
public:
	typedef TMap<TWeakObjectPtr<UObject>, FSlateShaderResourceProxy* > FObjectResourceMap;

	/** Map of all the atlased resources. */
	FObjectResourceMap ProxyMap;
};
