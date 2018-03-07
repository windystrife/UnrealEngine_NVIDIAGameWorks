// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

/*=============================================================================
	Texture.h: Unreal texture related classes.
=============================================================================*/

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "Containers/IndirectArray.h"
#include "Stats/Stats.h"
#include "Containers/List.h"
#include "Templates/ScopedPointer.h"
#include "Async/AsyncWork.h"
#include "Async/AsyncFileHandle.h"
#include "RHI.h"
#include "RenderResource.h"
#include "Serialization/BulkData.h"
#include "Engine/TextureDefines.h"
#include "UnrealClient.h"
#include "UniquePtr.h"

class FTexture2DResourceMem;
class UTexture2D;

/** Maximum number of slices in texture source art. */
#define MAX_TEXTURE_SOURCE_SLICES 6

/**
 * A 2D texture mip-map.
 */
struct FTexture2DMipMap
{
	/** Width of the mip-map. */
	int32 SizeX;
	/** Height of the mip-map. */
	int32 SizeY;
	/** Bulk data if stored in the package. */
	FByteBulkData BulkData;

	/** Default constructor. */
	FTexture2DMipMap()
		: SizeX(0)
		, SizeY(0)
	{
	}

	/** Serialization. */
	ENGINE_API void Serialize(FArchive& Ar, UObject* Owner, int32 MipIndex);

#if WITH_EDITORONLY_DATA
	/** Key if stored in the derived data cache. */
	FString DerivedDataKey;

	/**
	 * Place mip-map data in the derived data cache associated with the provided
	 * key.
	 */
	uint32 StoreInDerivedDataCache(const FString& InDerivedDataKey);
#endif // #if WITH_EDITORONLY_DATA
};

/** 
 * The rendering resource which represents a texture.
 */
class FTextureResource : public FTexture
{
public:

	FTextureResource()
	{}
	virtual ~FTextureResource() {}

#if STATS
	/* The Stat_ FName corresponding to each TEXTUREGROUP */
	static FName TextureGroupStatFNames[TEXTUREGROUP_MAX];
#endif
};

/**
 * FTextureResource implementation for streamable 2D textures.
 */
class FTexture2DResource : public FTextureResource
{
public:
	/**
	 * Minimal initialization constructor.
	 *
	 * @param InOwner			UTexture2D which this FTexture2DResource represents.
	 * @param InitialMipCount	Initial number of miplevels to upload to card
	 * @param InFilename		Filename to read data from
 	 */
	FTexture2DResource( UTexture2D* InOwner, int32 InitialMipCount );

	/**
	 * Destructor, freeing MipData in the case of resource being destroyed without ever 
	 * having been initialized by the rendering thread via InitRHI.
	 */
	virtual ~FTexture2DResource();

	// FRenderResource interface.

	/**
	 * Called when the resource is initialized. This is only called by the rendering thread.
	 */
	virtual void InitRHI() override;
	/**
	 * Called when the resource is released. This is only called by the rendering thread.
	 */
	virtual void ReleaseRHI() override;

	/** Returns the width of the texture in pixels. */
	virtual uint32 GetSizeX() const override;

	/** Returns the height of the texture in pixels. */
	virtual uint32 GetSizeY() const override;

	/** 
	 * Accessor
	 * @return Texture2DRHI
	 */
	FTexture2DRHIRef GetTexture2DRHI()
	{
		return Texture2DRHI;
	}

	virtual FString GetFriendlyName() const override;

	//Returns the current first mip (always valid)
	int32 GetCurrentFirstMip() const
	{
		return CurrentFirstMip;
	}

	void UpdateTexture(FTexture2DRHIRef& InTextureRHI, int32 InFirstMip);

private:
	/** Texture streaming command classes that need to be friends in order to call Update/FinalizeMipCount.	*/
	friend class UTexture2D;
	friend class FTexture2DUpdate;

	/** The UTexture2D which this resource represents.														*/
	const UTexture2D*	Owner;
	/** Resource memory allocated by the owner for serialize bulk mip data into								*/
	FTexture2DResourceMem* ResourceMem;

	/** Whether the texture RHI has been initialized.														*/
	bool bReadyForStreaming;

	/** Whether this texture should be updated using the virtual allocations.								*/
	bool bUseVirtualUpdatePath;

	EMipFadeSettings MipFadeSetting;

	/** First mip level used in Texture2DRHI. This is always correct as long as Texture2DRHI is allocated, regardless of streaming status. */
	int32 CurrentFirstMip;
	
	/** Local copy/ cache of mip data between creation and first call to InitRHI.							*/
	void*				MipData[MAX_TEXTURE_MIP_COUNT];

	/** 2D texture version of TextureRHI which is used to lock the 2D texture during mip transitions.		*/
	FTexture2DRHIRef	Texture2DRHI;

#if STATS
	/** Cached texture size for stats.																		*/
	int32					TextureSize;
	/** Cached intermediate texture size for stats.															*/
	int32					IntermediateTextureSize;
	/** The FName of the LODGroup-specific stat																*/
	FName					LODGroupStatName;
#endif

	/**
	 * Writes the data for a single mip-level into a destination buffer.
	 * @param MipIndex	The index of the mip-level to read.
	 * @param Dest		The address of the destination buffer to receive the mip-level's data.
	 * @param DestPitch	Number of bytes per row
	 */
	void GetData( uint32 MipIndex,void* Dest,uint32 DestPitch );

	/** Create RHI sampler states. */
	void CreateSamplerStates(float MipMapBias);

	/** Returns the default mip map bias for this texture. */
	int32 GetDefaultMipMapBias() const;

	// releases and recreates sampler state objects.
	// used when updating mip map bias offset
	void RefreshSamplerStates();
};

/** A dynamic 2D texture resource. */
class FTexture2DDynamicResource : public FTextureResource
{
public:
	/** Initialization constructor. */
	ENGINE_API FTexture2DDynamicResource(class UTexture2DDynamic* InOwner);

	/** Returns the width of the texture in pixels. */
	virtual uint32 GetSizeX() const override;

	/** Returns the height of the texture in pixels. */
	virtual uint32 GetSizeY() const override;

	/** Called when the resource is initialized. This is only called by the rendering thread. */
	ENGINE_API virtual void InitRHI() override;

	/** Called when the resource is released. This is only called by the rendering thread. */
	ENGINE_API virtual void ReleaseRHI() override;

	/** Returns the Texture2DRHI, which can be used for locking/unlocking the mips. */
	ENGINE_API FTexture2DRHIRef GetTexture2DRHI();

private:
	/** The owner of this resource. */
	class UTexture2DDynamic* Owner;
	/** Texture2D reference, used for locking/unlocking the mips. */
	FTexture2DRHIRef Texture2DRHI;
};

/** Stores information about a mip map, used by FTexture2DArrayResource to mirror game thread data. */
class FMipMapDataEntry
{
public:
	uint32 SizeX;
	uint32 SizeY;
	TArray<uint8> Data;
};

/** Stores information about a single texture in FTexture2DArrayResource. */
class FTextureArrayDataEntry
{
public:

	FTextureArrayDataEntry() : 
		NumRefs(0)
	{}

	/** Number of FTexture2DArrayResource::AddTexture2D calls that specified this texture. */
	int32 NumRefs;

	/** Mip maps of the texture. */
	TArray<FMipMapDataEntry, TInlineAllocator<MAX_TEXTURE_MIP_COUNT> > MipData;
};

/** 
 * Stores information about a UTexture2D so the rendering thread can access it, 
 * Even though the UTexture2D may have changed by the time the rendering thread gets around to it.
 */
class FIncomingTextureArrayDataEntry : public FTextureArrayDataEntry
{
public:

	FIncomingTextureArrayDataEntry() {}

	FIncomingTextureArrayDataEntry(UTexture2D* InTexture);

	int32 SizeX;
	int32 SizeY;
	int32 NumMips;
	TextureGroup LODGroup;
	EPixelFormat Format;
	ESamplerFilter Filter;
	bool bSRGB;
};

/** Represents a 2D Texture Array to the renderer. */
class FTexture2DArrayResource : public FTextureResource
{
public:

	FTexture2DArrayResource() :
		SizeX(0),
		bDirty(false),
		bPreventingReallocation(false)
	{}

	// Rendering thread functions

	/** 
	 * Adds a texture to the texture array.  
	 * This is called on the rendering thread, so it must not dereference NewTexture.
	 */
	void AddTexture2D(UTexture2D* NewTexture, const FIncomingTextureArrayDataEntry* InEntry);

	/** Removes a texture from the texture array, and potentially removes the CachedData entry if the last ref was removed. */
	void RemoveTexture2D(const UTexture2D* NewTexture);

	/** Updates a CachedData entry (if one exists for this texture), with a new texture. */
	void UpdateTexture2D(UTexture2D* NewTexture, const FIncomingTextureArrayDataEntry* InEntry);

	/** Initializes the texture array resource if needed, and re-initializes if the texture array has been made dirty since the last init. */
	void UpdateResource();

	/** Returns the index of a given texture in the texture array. */
	int32 GetTextureIndex(const UTexture2D* Texture) const;
	int32 GetNumValidTextures() const;

	/**
	* Called when the resource is initialized. This is only called by the rendering thread.
	*/
	virtual void InitRHI() override;

	/** Returns the width of the texture in pixels. */
	virtual uint32 GetSizeX() const override
	{
		return SizeX;
	}

	/** Returns the height of the texture in pixels. */
	virtual uint32 GetSizeY() const override
	{
		return SizeY;
	}

	/** Prevents reallocation from removals of the texture array until EndPreventReallocation is called. */
	void BeginPreventReallocation();

	/** Restores the ability to reallocate the texture array. */
	void EndPreventReallocation();

private:

	/** Texture data, has to persist past the first InitRHI call, because more textures may be added later. */
	TMap<const UTexture2D*, FTextureArrayDataEntry> CachedData;
	uint32 SizeX;
	uint32 SizeY;
	uint32 NumMips;
	TextureGroup LODGroup;
	EPixelFormat Format;
	ESamplerFilter Filter;

	bool bSRGB;
	bool bDirty;
	bool bPreventingReallocation;

	/** Copies data from DataEntry into Dest, taking stride into account. */
	void GetData(const FTextureArrayDataEntry& DataEntry, int32 MipIndex, void* Dest, uint32 DestPitch);
};

/**
 * FDeferredUpdateResource for resources that need to be updated after scene rendering has begun
 * (should only be used on the rendering thread)
 */
class FDeferredUpdateResource
{
public:

	/**
	 * Constructor, initializing UpdateListLink.
	 */
	FDeferredUpdateResource()
		:	UpdateListLink(NULL)
		,	bOnlyUpdateOnce(false)
	{ }

public:

	/**
	 * Iterate over the global list of resources that need to
	 * be updated and call UpdateResource on each one.
	 */
	ENGINE_API static void UpdateResources( FRHICommandListImmediate& RHICmdList );

	/** 
	 * This is reset after all viewports have been rendered
	 */
	static void ResetNeedsUpdate()
	{
		bNeedsUpdate = true;
	}

protected:

	/**
	 * Updates (resolves) the render target texture.
	 * Optionally clears the contents of the render target to green.
	 * This is only called by the rendering thread.
	 */
	virtual void UpdateDeferredResource( FRHICommandListImmediate& RHICmdList, bool bClearRenderTarget=true ) = 0;

	/**
	 * Add this resource to deferred update list
	 * @param OnlyUpdateOnce - flag this resource for a single update if true
	 */
	ENGINE_API void AddToDeferredUpdateList( bool OnlyUpdateOnce );

	/**
	 * Remove this resource from deferred update list
	 */
	ENGINE_API void RemoveFromDeferredUpdateList();

private:

	/** 
	 * Resources can be added to this list if they need a deferred update during scene rendering.
	 * @return global list of resource that need to be updated. 
	 */
	static TLinkedList<FDeferredUpdateResource*>*& GetUpdateList();
	/** This resource's link in the global list of resources needing clears. */
	TLinkedList<FDeferredUpdateResource*> UpdateListLink;
	/** if true then UpdateResources needs to be called */
	ENGINE_API static bool bNeedsUpdate;
	/** if true then remove this resource from the update list after a single update */
	bool bOnlyUpdateOnce;
};

/**
 * FTextureResource type for render target textures.
 */
class FTextureRenderTargetResource : public FTextureResource, public FRenderTarget, public FDeferredUpdateResource
{
public:
	/**
	 * Constructor, initializing ClearLink.
	 */
	FTextureRenderTargetResource()
	{}

	/** 
	 * Return true if a render target of the given format is allowed
	 * for creation
	 */
	ENGINE_API static bool IsSupportedFormat( EPixelFormat Format );

	// FTextureRenderTargetResource interface
	
	virtual class FTextureRenderTarget2DResource* GetTextureRenderTarget2DResource()
	{
		return NULL;
	}
	virtual void ClampSize(int32 SizeX,int32 SizeY) {}

	// FRenderTarget interface.
	virtual uint32 GetSizeX() const = 0;
	virtual uint32 GetSizeY() const = 0;
	virtual FIntPoint GetSizeXY() const = 0;

	/** 
	 * Render target resource should be sampled in linear color space
	 *
	 * @return display gamma expected for rendering to this render target 
	 */
	virtual float GetDisplayGamma() const;
};

/**
 * FTextureResource type for 2D render target textures.
 */
class FTextureRenderTarget2DResource : public FTextureRenderTargetResource
{
public:
	
	/** 
	 * Constructor
	 * @param InOwner - 2d texture object to create a resource for
	 */
	FTextureRenderTarget2DResource(const class UTextureRenderTarget2D* InOwner);

	FORCEINLINE FLinearColor GetClearColor()
	{
		return ClearColor;
	}

	// FTextureRenderTargetResource interface

	/** 
	 * 2D texture RT resource interface 
	 */
	virtual class FTextureRenderTarget2DResource* GetTextureRenderTarget2DResource() override
	{
		return this;
	}

	/**
	 * Clamp size of the render target resource to max values
	 *
	 * @param MaxSizeX max allowed width
	 * @param MaxSizeY max allowed height
	 */
	virtual void ClampSize(int32 SizeX,int32 SizeY) override;
	
	// FRenderResource interface.

	/**
	 * Initializes the dynamic RHI resource and/or RHI render target used by this resource.
	 * Called when the resource is initialized, or when reseting all RHI resources.
	 * Resources that need to initialize after a D3D device reset must implement this function.
	 * This is only called by the rendering thread.
	 */
	virtual void InitDynamicRHI() override;

	/**
	 * Releases the dynamic RHI resource and/or RHI render target resources used by this resource.
	 * Called when the resource is released, or when reseting all RHI resources.
	 * Resources that need to release before a D3D device reset must implement this function.
	 * This is only called by the rendering thread.
	 */
	virtual void ReleaseDynamicRHI() override;

	// FDeferredClearResource interface

	// FRenderTarget interface.
	/** 
	 * @return width of the target
	 */
	virtual uint32 GetSizeX() const override;

	/** 
	 * @return height of the target
	 */
	virtual uint32 GetSizeY() const override;

	/**
	 * @return dimensions of the target
	 */
	virtual FIntPoint GetSizeXY() const override;

	/** 
	 * Render target resource should be sampled in linear color space
	 *
	 * @return display gamma expected for rendering to this render target 
	 */
	virtual float GetDisplayGamma() const override;

	/** 
	 * @return TextureRHI for rendering 
	 */
	FTexture2DRHIRef GetTextureRHI() { return Texture2DRHI; }
protected:
	/**
	 * Updates (resolves) the render target texture.
	 * Optionally clears the contents of the render target to green.
	 * This is only called by the rendering thread.
	 */
	friend class UTextureRenderTarget2D;
	virtual void UpdateDeferredResource(FRHICommandListImmediate& RHICmdList, bool bClearRenderTarget=true) override;

private:
	/** The UTextureRenderTarget2D which this resource represents. */
	const class UTextureRenderTarget2D* Owner;
	/** Texture resource used for rendering with and resolving to */
	FTexture2DRHIRef Texture2DRHI;
	/** the color the texture is cleared to */
	FLinearColor ClearColor;
	EPixelFormat Format;
	int32 TargetSizeX,TargetSizeY;
};

/**
 * FTextureResource type for cube render target textures.
 */
class FTextureRenderTargetCubeResource : public FTextureRenderTargetResource
{
public:

	/** 
	 * Constructor
	 * @param InOwner - cube texture object to create a resource for
	 */
	FTextureRenderTargetCubeResource(const class UTextureRenderTargetCube* InOwner)
		:	Owner(InOwner)
	{
	}

	/** 
	 * Cube texture RT resource interface 
	 */
	virtual class FTextureRenderTargetCubeResource* GetTextureRenderTargetCubeResource()
	{
		return this;
	}

	/**
	 * Initializes the dynamic RHI resource and/or RHI render target used by this resource.
	 * Called when the resource is initialized, or when reseting all RHI resources.
	 * Resources that need to initialize after a D3D device reset must implement this function.
	 * This is only called by the rendering thread.
	 */
	virtual void InitDynamicRHI() override;

	/**
	 * Releases the dynamic RHI resource and/or RHI render target resources used by this resource.
	 * Called when the resource is released, or when reseting all RHI resources.
	 * Resources that need to release before a D3D device reset must implement this function.
	 * This is only called by the rendering thread.
	 */
	virtual void ReleaseDynamicRHI() override;

	// FRenderTarget interface.

	/** 
	 * @return width of the target
	 */
	virtual uint32 GetSizeX() const override;

	/** 
	 * @return height of the target
	 */
	virtual uint32 GetSizeY() const override;

	/**
	 * @return dimensions of the target
	 */
	virtual FIntPoint GetSizeXY() const override;

	/** 
	 * @return TextureRHI for rendering 
	 */
	FTextureCubeRHIRef GetTextureRHI() { return TextureCubeRHI; }

	/** 
	* Render target resource should be sampled in linear color space
	*
	* @return display gamma expected for rendering to this render target 
	*/
	float GetDisplayGamma() const override;

	/**
	* Copy the texels of a single face of the cube into an array.
	* @param OutImageData - float16 values will be stored in this array.
	* @param InFlags - read flags. ensure cubeface member has been set.
	* @param InRect - Rectangle of texels to copy.
	* @return true if the read succeeded.
	*/
	ENGINE_API bool ReadPixels(TArray< FColor >& OutImageData, FReadSurfaceDataFlags InFlags, FIntRect InRect = FIntRect(0, 0, 0, 0));

	/**
	* Copy the texels of a single face of the cube into an array.
	* @param OutImageData - float16 values will be stored in this array.
	* @param InFlags - read flags. ensure cubeface member has been set.
	* @param InRect - Rectangle of texels to copy.
	* @return true if the read succeeded.
	*/
	ENGINE_API bool ReadPixels(TArray<FFloat16Color>& OutImageData, FReadSurfaceDataFlags InFlags, FIntRect InRect = FIntRect(0, 0, 0, 0));

protected:
	/**
	* Updates (resolves) the render target texture.
	* Optionally clears each face of the render target to green.
	* This is only called by the rendering thread.
	*/
	virtual void UpdateDeferredResource(FRHICommandListImmediate& RHICmdList, bool bClearRenderTarget=true) override;

private:
	/** The UTextureRenderTargetCube which this resource represents. */
	const class UTextureRenderTargetCube* Owner;
	/** Texture resource used for rendering with and resolving to */
	FTextureCubeRHIRef TextureCubeRHI;
	/** Target surfaces for each cube face */
	FTexture2DRHIRef CubeFaceSurfaceRHI;

	/** Represents the current render target (from one of the cube faces)*/
	FTextureCubeRHIRef RenderTargetCubeRHI;

	/** Face currently used for target surface */
	ECubeFace CurrentTargetFace;
};

ENGINE_API FName GetDefaultTextureFormatName( const class ITargetPlatform* TargetPlatform, const class UTexture* Texture, const class FConfigFile& EngineSettings, bool bSupportDX11TextureFormats );
// returns all the texture formats which can be returned by GetDefaultTextureFormatName
ENGINE_API void GetAllDefaultTextureFormats( const class ITargetPlatform* TargetPlatform, TArray<FName>& OutFormats, bool bSupportDX11TextureFormats);