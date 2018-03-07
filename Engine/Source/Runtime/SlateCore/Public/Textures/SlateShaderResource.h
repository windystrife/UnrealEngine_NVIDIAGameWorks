// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "SlateGlobals.h"

class FSlateShaderResourceProxy;

DECLARE_MEMORY_STAT_EXTERN(TEXT("Texture Data Memory (GPU)"), STAT_SlateTextureGPUMemory, STATGROUP_SlateMemory, SLATECORE_API);

namespace ESlateShaderResource
{
	/**
	 * Enumerates Slate render resource types.
	 */
	enum Type
	{
		/** Texture resource. */
		NativeTexture,

		/** UTexture object resource */
		TextureObject,

		/** Material resource. */
		Material,

		/** Post Process. */
		PostProcess, 

		/** No Resource. */
		Invalid,
	};
}


/** 
 * Base class for all platform independent texture types
 */
class SLATECORE_API FSlateShaderResource
{
public:

	/**
	 * Gets the width of the resource.
	 *
	 * @return Resource width (in pixels).
	 */
	virtual uint32 GetWidth() const = 0;

	/**
	 * Gets the height of the resource.
	 *
	 * @return Resource height(in pixels).
	 */
	virtual uint32 GetHeight() const = 0;

	/**
	 * Gets the type of the resource.
	 *
	 * @return Resource type.
	 */
	virtual ESlateShaderResource::Type GetType() const = 0;

public:

	/** Virtual destructor. */
	virtual ~FSlateShaderResource() { }
};

/**
 * Data used to lookup a resource from a handle and to determine validity of the handle.  
 * This is shared between the handle and the resource
 *
 */
class FSlateSharedHandleData
{
public:
	FSlateSharedHandleData( FSlateShaderResourceProxy* InProxy = nullptr )
		: Proxy( InProxy )
	{}

public:
	/** Rendering proxy used to directly access the rendering resource quickly */
	FSlateShaderResourceProxy* Proxy;
};

/** 
 * A proxy resource.  
 *
 * May point to a full resource or point or to a texture resource in an atlas
 * Note: This class does not free any resources.  Resources should be owned and freed elsewhere
 */
class SLATECORE_API FSlateShaderResourceProxy
{
public:

	/** The start uv of the texture.  If atlased this is some subUV of the atlas, 0,0 otherwise */
	FVector2D StartUV;

	/** The size of the texture in UV space.  If atlas this some sub uv of the atlas.  1,1 otherwise */
	FVector2D SizeUV;

	/** The resource to be used for rendering */
	FSlateShaderResource* Resource;

	/** The size of the texture.  Regardless of atlasing this is the size of the actual texture */
	FIntPoint ActualSize;

	/** Shared data between resources and handles to this resource.  Is created the first time a handle is needed */
	TSharedPtr<FSlateSharedHandleData> HandleData;

	/** Default constructor. */
	FSlateShaderResourceProxy( )
		: StartUV(0.0f, 0.0f)
		, SizeUV(1.0f, 1.0f)
		, Resource(nullptr)
		, ActualSize(0.0f, 0.0f)
	{ }

	~FSlateShaderResourceProxy()
	{
		if( HandleData.IsValid() )
		{
			// Handles exist for this proxy.
			// Null out the proxy to invalidate all handles
			HandleData->Proxy = nullptr; 
		}
	}
};


/** 
 * Abstract base class for platform independent texture resource accessible by the shader.
 */
template <typename ResourceType>
class TSlateTexture
	: public FSlateShaderResource
{
public:

	/** Default constructor. */
	TSlateTexture( ) { }

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InShaderResource The resource to use.
	 */
	TSlateTexture( ResourceType& InShaderResource )
		: ShaderResource( InShaderResource )
	{ }

	virtual ~TSlateTexture() { }

public:

	/**
	 * Gets the resource used by the shader.
	 *
	 * @return The resource.
	 */
	ResourceType& GetTypedResource()
	{
		return ShaderResource;
	}

public:

	// FSlateShaderResource interface
	virtual ESlateShaderResource::Type GetType() const override
	{
		return ESlateShaderResource::NativeTexture;
	}

protected:

	// Holds the resource.
	ResourceType ShaderResource;
};

class IViewportRenderTargetProvider
{
public:
	virtual FSlateShaderResource* GetViewportRenderTargetTexture() = 0;
};

/**
 * A SlateResourceHandle is used as fast path for looking up a rendering resource for a given brush when adding Slate draw elements
 * This can be cached and stored safely in code.  It will become invalid when a resource is destroyed
*/
class FSlateResourceHandle
{
	friend class FSlateShaderResourceManager;
	friend class FSlateDrawElement;
public:
	/**
	 * @return true if the handle still points to a valid rendering resource
	 */
	bool IsValid() const { return Data.IsValid() && Data->Proxy; }
private:
	/** Internal data to pair the handle to the resource */
	TSharedPtr<FSlateSharedHandleData> Data;
};
