// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Textures/TextureAtlas.h"
#include "UObject/GCObject.h"
#include "Containers/Queue.h"
#include "Rendering/ShaderResourceManager.h"
#include "Rendering/RenderingCommon.h"
#include "Rendering/DrawElements.h"
#include "Materials/MaterialInterface.h"
#include "Tickable.h"
#include "SlateElementIndexBuffer.h"
#include "SlateElementVertexBuffer.h"

class FSlateAtlasedTextureResource;
class FSlateDynamicTextureResource;
class FSlateMaterialResource;
class FSlateUTextureResource;
class ILayoutCache;
class ISlateStyle;
class UTexture;
class FSceneInterface;

/** 
 * Lookup key for materials.  Sometimes the same material is used with different masks so there must be
 * unique resource per material/mask combination
 */
struct FMaterialKey
{
	TWeakObjectPtr<UMaterialInterface>	Material;
	int32 MaskKey;

	FMaterialKey(const UMaterialInterface* InMaterial, int32 InMaskKey)
		: Material(InMaterial)
		, MaskKey(InMaskKey)
	{}

	bool operator==(const FMaterialKey& Other) const
	{
		return Material == Other.Material && MaskKey == Other.MaskKey;
	}

	friend uint32 GetTypeHash(const FMaterialKey& Key)
	{
		return HashCombine(GetTypeHash(Key.Material), Key.MaskKey);
	}
};

struct FDynamicResourceMap
{
public:
	FDynamicResourceMap();

	TSharedPtr<FSlateDynamicTextureResource> GetDynamicTextureResource( FName ResourceName ) const;

	TSharedPtr<FSlateUTextureResource> GetUTextureResource( UTexture* TextureObject ) const;

	TSharedPtr<FSlateAtlasedTextureResource> GetAtlasedTextureResource(UTexture* InObject) const;

	TSharedPtr<FSlateMaterialResource> GetMaterialResource( const FMaterialKey& InKey ) const;

	void AddUTextureResource( UTexture* TextureObject, TSharedRef<FSlateUTextureResource> InResource );
	void RemoveUTextureResource( UTexture* TextureObject );

	void AddDynamicTextureResource( FName ResourceName, TSharedRef<FSlateDynamicTextureResource> InResource);
	void RemoveDynamicTextureResource( FName ResourceName );

	void AddMaterialResource( const FMaterialKey& InKey, TSharedRef<FSlateMaterialResource> InResource );
	void RemoveMaterialResource( const FMaterialKey& InKey );

	void AddAtlasedTextureResource(UTexture* TextureObject, TSharedRef<FSlateAtlasedTextureResource> InResource);
	void RemoveAtlasedTextureResource(UTexture* TextureObject);

	FSlateShaderResourceProxy* FindOrCreateAtlasedProxy(UObject* InObject);

	void Empty();

	void EmptyUTextureResources();
	void EmptyMaterialResources();
	void EmptyDynamicTextureResources();

	void ReleaseResources();

	uint32 GetNumObjectResources() const { return TextureMap.Num() + MaterialMap.Num(); }

public:
	void RemoveExpiredTextureResources(TArray< TSharedPtr<FSlateUTextureResource> >& RemovedTextures);
	void RemoveExpiredMaterialResources(TArray< TSharedPtr<FSlateMaterialResource> >& RemovedMaterials);

private:
	TMap<FName, TSharedPtr<FSlateDynamicTextureResource> > NativeTextureMap;
	
	/** Map of all texture resources */
	typedef TMap<TWeakObjectPtr<UTexture>, TSharedPtr<FSlateUTextureResource> > FTextureResourceMap;
	FTextureResourceMap TextureMap;

	/** Map of all material resources */
	typedef TMap<FMaterialKey, TSharedPtr<FSlateMaterialResource> > FMaterialResourceMap;
	FMaterialResourceMap MaterialMap;

	/** Map of all object resources */
	typedef TMap<TWeakObjectPtr<UObject>, TSharedPtr<FSlateAtlasedTextureResource> > FObjectResourceMap;
	FObjectResourceMap ObjectMap;

	uint64 TextureMemorySincePurge;

	int32 LastExpiredMaterialNumMarker;
};


struct FCachedRenderBuffers
{
	TSlateElementVertexBuffer<FSlateVertex> VertexBuffer;
	FSlateElementIndexBuffer IndexBuffer;

	FGraphEventRef ReleaseResourcesFence;
};


/**
 * Stores a mapping of texture names to their RHI texture resource               
 */
class FSlateRHIResourceManager : public ISlateAtlasProvider, public ISlateRenderDataManager, public FSlateShaderResourceManager, public FTickableGameObject, public FGCObject
{
public:
	FSlateRHIResourceManager();
	virtual ~FSlateRHIResourceManager();

	/** ISlateAtlasProvider interface */
	virtual int32 GetNumAtlasPages() const override;
	virtual FIntPoint GetAtlasPageSize() const override;
	virtual FSlateShaderResource* GetAtlasPageResource(const int32 InIndex) const override;
	virtual bool IsAtlasPageResourceAlphaOnly() const override;

	/** ISlateRenderDataManager interface */
	virtual void BeginReleasingRenderData(const FSlateRenderDataHandle* RenderHandle) override;

	/** FTickableGameObject interface */
	virtual bool IsTickable() const override { return true; }
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return true; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FSlateRHIResourceManager, STATGROUP_Tickables); }
	virtual void Tick(float DeltaSeconds) override;

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	/**
	 * Loads and creates rendering resources for all used textures.  
	 * In this implementation all textures must be known at startup time or they will not be found
	 */
	void LoadUsedTextures();

	void LoadStyleResources( const ISlateStyle& Style );

	/**
	 * Clears accessed UTexture and Material resources from the previous frame
	 * The accessed textures is used to determine which textures need be updated on the render thread
	 * so they can be used by slate
	 */
	void BeginReleasingAccessedResources(bool bImmediatelyFlush);

	/**
	 * Updates texture atlases if needed
	 */
	void UpdateTextureAtlases();

	/** FSlateShaderResourceManager interface */
	virtual FSlateShaderResourceProxy* GetShaderResource( const FSlateBrush& InBrush ) override;
	virtual FSlateShaderResource* GetFontShaderResource( int32 InTextureAtlasIndex, FSlateShaderResource* FontTextureAtlas, const class UObject* FontMaterial ) override;
	virtual ISlateAtlasProvider* GetTextureAtlasProvider() override;

	/**
	 * Makes a dynamic texture resource and begins use of it
	 *
	 * @param InTextureObject	The texture object to create the resource from
	 */
	TSharedPtr<FSlateUTextureResource> MakeDynamicUTextureResource( UTexture* InTextureObject);
	
	/**
	 * Makes a dynamic texture resource and begins use of it
	 *
	 * @param ResourceName			The name identifier of the resource
	 * @param Width					The width of the resource
	 * @param Height				The height of the resource
	 * @param Bytes					The payload containing the resource
	 */
	TSharedPtr<FSlateDynamicTextureResource> MakeDynamicTextureResource( FName ResourceName, uint32 Width, uint32 Height, const TArray< uint8 >& Bytes );

	TSharedPtr<FSlateDynamicTextureResource> MakeDynamicTextureResource(FName ResourceName, FSlateTextureDataRef TextureData);

	/**
	 * Find a dynamic texture resource 
	 *
	 * @param ResourceName			The name identifier of the resource
	 */
	TSharedPtr<FSlateDynamicTextureResource> GetDynamicTextureResourceByName( FName ResourceName );

	/**
	 * Returns true if a texture resource with the passed in resource name is available 
	 */
	bool ContainsTexture( const FName& ResourceName ) const;

	/** Releases a specific dynamic resource */
	void ReleaseDynamicResource( const FSlateBrush& InBrush );

	/** 
	 * Creates a new texture from the given texture name
	 *
	 * @param TextureName	The name of the texture to load
	 */
	virtual bool LoadTexture( const FName& TextureName, const FString& ResourcePath, uint32& Width, uint32& Height, TArray<uint8>& DecodedImage );
	virtual bool LoadTexture( const FSlateBrush& InBrush, uint32& Width, uint32& Height, TArray<uint8>& DecodedImage );

	/**
	 * 
	 */
	FCachedRenderBuffers* FindCachedBuffersForHandle(const FSlateRenderDataHandle* RenderHandle) const
	{
		return CachedBuffers.FindRef(RenderHandle);
	}

	/**
	 * Creates a vertex and index buffer for a given render handle that it can use to store cached widget geometry to.
	 */
	FCachedRenderBuffers* FindOrCreateCachedBuffersForHandle(const TSharedRef<FSlateRenderDataHandle, ESPMode::ThreadSafe>& RenderHandle);

	/**
	 * Releases all cached buffers allocated by a given layout cacher.  This would happen when an invalidation panel is destroyed.
	 */
	void ReleaseCachingResourcesFor(FRHICommandListImmediate& RHICmdList, const ILayoutCache* Cacher);

	/**
	 * Releases rendering resources
	 */
	void ReleaseResources();

	/**
	 * Reloads texture resources for all used textures.  
	 *
	 * @param InExtraResources     Optional list of textures to create that aren't in the style.
	 */
	void ReloadTextures();

	int32 GetSceneCount();
	FSceneInterface* GetSceneAt(int32 Index);
	void AddSceneAt(FSceneInterface* Scene, int32 Index);
	void ClearScenes();

private:
	void ReleaseCachedBuffer(FRHICommandListImmediate& RHICmdList, FCachedRenderBuffers* PooledBuffer);

	/**
	* Releases the cached render data for a given render handle.  The layout cacher that owned the handle must also be
	* provided, as render handle is likely no longer a valid object.
	*/
	void ReleaseCachedRenderData(FRHICommandListImmediate& RHICmdList, const FSlateRenderDataHandle* RenderHandle, const ILayoutCache* LayoutCacher);

private:
	/**
	 * Gets the current accessed UObject tracking set.
	 */
	TSet<UObject*>& GetAccessedUObjects();

	/**
	 * Deletes resources created by the manager
	 */
	void DeleteResources();

	/** 
	 * Creates textures from files on disk and atlases them if possible
	 *
	 * @param Resources	The brush resources to load images for
	 */
	void CreateTextures( const TArray< const FSlateBrush* >& Resources );

	/**
	 * Generates rendering resources for a texture
	 * 
	 * @param Info	Information on how to generate the texture
	 */
	FSlateShaderResourceProxy* GenerateTextureResource( const FNewTextureInfo& Info );
	
	/**
	 * Returns a texture rendering resource from for a dynamically loaded texture or utexture object
	 * Note: this will load the UTexture or image if needed 
	 *
	 * @param InBrush	Slate brush for the dynamic resource
	 */
	FSlateShaderResourceProxy* FindOrCreateDynamicTextureResource( const FSlateBrush& InBrush );

	/**
	 * Returns a rendering resource for a material
	 *
	 * @param InMaterial	The material object
	 */
	FSlateMaterialResource* GetMaterialResource( const UObject* InMaterial, FVector2D ImageSize, FSlateShaderResource* TextureMask, int32 InMaskKey );

	/**
	 * Called when the application exists before the UObject system shuts down so we can free object resources
	 */
	void OnAppExit();

	/**
	 * Get or create the bad resource texture.
	 */
	UTexture* GetBadResourceTexture();

private:
	/** Map of all active dynamic resources being used by brushes */
	FDynamicResourceMap DynamicResourceMap;
	/**
	 * All sets of accessed UObjects.  We have to track multiple sets, because a single set
	 * needs to follow the set of objects through the renderer safely.  So we round robin
	 * the buffers.
	 */
	TArray< TSet<UObject*>* > AllAccessedUObject;
	/**
	 * Tracks a pointer to the current accessed UObject set we're builing this frame, 
	 * don't use this directly, use GetAccessedUObjects().
	 */
	TSet<UObject*>* CurrentAccessedUObject;
	/**
	 * Used accessed UObject sets are added to this queue from the Game Thread.
	 * The RenderThread moves them onto the CleanAccessedObjectSets queue.
	 */
	TQueue< TSet<UObject*>* > DirtyAccessedObjectSets;
	/**
	 * The RenderThread moves previously dirty UObject sets onto this queue.
	 */
	TQueue< TSet<UObject*>* > CleanAccessedObjectSets;
	/** List of old utexture resources that are free to use as new resources */
	TArray< TSharedPtr<FSlateUTextureResource> > UTextureFreeList;
	/** List of old dynamic resources that are free to use as new resources */
	TArray< TSharedPtr<FSlateDynamicTextureResource> > DynamicTextureFreeList;
	/** List of old material resources that are free to use as new resources */
	TArray< TSharedPtr<FSlateMaterialResource> > MaterialResourceFreeList;
	/** Static Texture atlases which have been created */
	TArray<class FSlateTextureAtlasRHI*> TextureAtlases;
	/** Static Textures created that are not atlased */	
	TArray<class FSlateTexture2DRHIRef*> NonAtlasedTextures;
	/** The size of each texture atlas (square) */
	uint32 AtlasSize;
	/** This max size of each texture in an atlas */
	FIntPoint MaxAltasedTextureSize;
	/** Needed for displaying an error texture when we end up with bad resources. */
	UTexture* BadResourceTexture;

	typedef TMap< FSlateRenderDataHandle*, FCachedRenderBuffers* > TCachedBufferMap;
	TCachedBufferMap CachedBuffers;

	typedef TMap< const ILayoutCache*, TArray< FCachedRenderBuffers* > > TCachedBufferPoolMap;
	TCachedBufferPoolMap CachedBufferPool;

	/**
	 * Holds onto a list of pooled buffers that are no longer being used but still need 
	 * to be deleted after the RHI thread is done with them.
	 */
	TArray<FCachedRenderBuffers*> PooledBuffersPendingRelease;


	TArray<FSceneInterface*> ActiveScenes;
};

