// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * TextureRenderTarget
 *
 * Base for all render target texture resources
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/Texture.h"
#include "TextureRenderTarget.generated.h"

UCLASS(abstract, MinimalAPI)
class UTextureRenderTarget : public UTexture
{
	GENERATED_UCLASS_BODY()	

	/** Will override FTextureRenderTarget2DResource::GetDisplayGamma if > 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TextureRenderTarget)
	float TargetGamma;

	/** If true, there will be two copies in memory - one for the texture and one for the render target. If false, they will share memory if possible. This is useful for scene capture textures that are used in the scene. */
	uint32 bNeedsTwoCopies:1;

	/**
	 * Render thread: Access the render target resource for this texture target object
	 * @return pointer to resource or NULL if not initialized
	 */
	ENGINE_API class FTextureRenderTargetResource* GetRenderTargetResource();

	/**
	 * Returns a pointer to the (game thread managed) render target resource.  Note that you're not allowed
	 * to deferenced this pointer on the game thread, you can only pass the pointer around and check for NULLness
	 * @return pointer to resource
	 */
	ENGINE_API class FTextureRenderTargetResource* GameThread_GetRenderTargetResource();


	//~ Begin UTexture Interface
	ENGINE_API virtual class FTextureResource* CreateResource() override;
	ENGINE_API virtual EMaterialValueType GetMaterialType() const override;
	//~ End UTexture Interface
};



