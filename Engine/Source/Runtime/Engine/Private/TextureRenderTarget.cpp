// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TextureRenderTarget.cpp: UTextureRenderTarget implementation
=============================================================================*/

#include "Engine/TextureRenderTarget.h"
#include "TextureResource.h"

/*-----------------------------------------------------------------------------
UTextureRenderTarget
-----------------------------------------------------------------------------*/

UTextureRenderTarget::UTextureRenderTarget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NeverStream = true;
	SRGB = true;
	LODGroup = TEXTUREGROUP_RenderTarget;	
	bNeedsTwoCopies = false;
#if WITH_EDITORONLY_DATA
	CompressionNone = true;
#endif // #if WITH_EDITORONLY_DATA
}


FTextureRenderTargetResource* UTextureRenderTarget::GetRenderTargetResource()
{
	check(IsInRenderingThread() || 
		(IsInParallelRenderingThread() && (!Resource || Resource->IsInitialized()))); // we allow this in parallel, but only if the resource is initialized...otherwise it might be a race on intialization
	FTextureRenderTargetResource* Result = NULL;
	if( Resource &&
		Resource->IsInitialized() )
	{
		Result = static_cast<FTextureRenderTargetResource*>( Resource );
	}
	return Result;
}


FTextureRenderTargetResource* UTextureRenderTarget::GameThread_GetRenderTargetResource()
{
	check( IsInGameThread() );
	return static_cast< FTextureRenderTargetResource*>( Resource );
}


FTextureResource* UTextureRenderTarget::CreateResource()
{
	return NULL;
}


EMaterialValueType UTextureRenderTarget::GetMaterialType() const
{
	return MCT_Texture;
}

/*-----------------------------------------------------------------------------
	FTextureRenderTargetResource
-----------------------------------------------------------------------------*/

/** 
 * Return true if a render target of the given format is allowed
 * for creation
 */
bool FTextureRenderTargetResource::IsSupportedFormat( EPixelFormat Format )
{
	switch( Format )
	{
	case PF_B8G8R8A8:
	case PF_A16B16G16R16:
	case PF_FloatRGB:
	case PF_FloatRGBA: // for exporting materials to .obj/.mtl
	case PF_A2B10G10R10: //Pixel inspector for normal buffer
	case PF_DepthStencil: //Pixel inspector for depth and stencil buffer
		return true;
	default:
		return false;
	}
}

/** 
* Render target resource should be sampled in linear color space
*
* @return display gamma expected for rendering to this render target 
*/
float FTextureRenderTargetResource::GetDisplayGamma() const
{
	return 2.2f;  
}

/*-----------------------------------------------------------------------------
FDeferredUpdateResource
-----------------------------------------------------------------------------*/

/** 
* if true then FDeferredUpdateResource::UpdateResources needs to be called 
* (should only be set on the rendering thread)
*/
bool FDeferredUpdateResource::bNeedsUpdate = true;

/** 
* Resources can be added to this list if they need a deferred update during scene rendering.
* @return global list of resource that need to be updated. 
*/
TLinkedList<FDeferredUpdateResource*>*& FDeferredUpdateResource::GetUpdateList()
{		
	static TLinkedList<FDeferredUpdateResource*>* FirstUpdateLink = NULL;
	return FirstUpdateLink;
}

/**
* Iterate over the global list of resources that need to
* be updated and call UpdateResource on each one.
*/
void FDeferredUpdateResource::UpdateResources(FRHICommandListImmediate& RHICmdList)
{
	if( bNeedsUpdate )
	{
		TLinkedList<FDeferredUpdateResource*>*& UpdateList = FDeferredUpdateResource::GetUpdateList();
		for( TLinkedList<FDeferredUpdateResource*>::TIterator ResourceIt(UpdateList);ResourceIt; )
		{
			FDeferredUpdateResource* RTResource = *ResourceIt;
			// iterate to next resource before removing an entry
			ResourceIt.Next();
			if( RTResource )
			{
				// update each resource
				RTResource->UpdateDeferredResource(RHICmdList);
				if( RTResource->bOnlyUpdateOnce )
				{
					// remove from list if only a single update was requested
					RTResource->RemoveFromDeferredUpdateList();
				}
			}
		}
		// since the updates should only occur once globally
		// then we need to reset this before rendering any viewports
		bNeedsUpdate = false;
	}
}

/**
* Add this resource to deferred update list
* @param OnlyUpdateOnce - flag this resource for a single update if true
*/
void FDeferredUpdateResource::AddToDeferredUpdateList( bool OnlyUpdateOnce )
{
	bool bExists=false;
	TLinkedList<FDeferredUpdateResource*>*& UpdateList = FDeferredUpdateResource::GetUpdateList();
	for( TLinkedList<FDeferredUpdateResource*>::TIterator ResourceIt(UpdateList);ResourceIt;ResourceIt.Next() )
	{
		if( (*ResourceIt) == this )
		{
			bExists=true;
			break;
		}
	}
	if( !bExists )
	{
		UpdateListLink = TLinkedList<FDeferredUpdateResource*>(this);
		UpdateListLink.LinkHead(UpdateList);
		bNeedsUpdate = true;
	}
	bOnlyUpdateOnce=OnlyUpdateOnce;
}

/**
* Remove this resource from deferred update list
*/
void FDeferredUpdateResource::RemoveFromDeferredUpdateList()
{
	UpdateListLink.Unlink();
}
