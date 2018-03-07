// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/Texture.h"
#include "Texture2DDynamic.generated.h"

class FTextureResource;

UCLASS(hidecategories=Object, MinimalAPI)
class UTexture2DDynamic : public UTexture
{
	GENERATED_UCLASS_BODY()

	/** The width of the texture. */
	int32 SizeX;

	/** The height of the texture. */
	int32 SizeY;

	/** The format of the texture. */
	UPROPERTY(transient)
	TEnumAsByte<enum EPixelFormat> Format;

	/** The number of mip-maps in the texture. */
	int32 NumMips;

	/** Whether the texture can be used as a resolve target. */
	uint32 bIsResolveTarget:1;


public:
	//~ Begin UTexture Interface.
	ENGINE_API virtual FTextureResource* CreateResource() override;
	virtual EMaterialValueType GetMaterialType() const override { return MCT_Texture2D; }
	ENGINE_API virtual float GetSurfaceWidth() const override;
	ENGINE_API virtual float GetSurfaceHeight() const override;
	//~ End UTexture Interface.
	
	/**
	 * Initializes the texture with 1 mip-level and creates the render resource.
	 *
	 * @param InSizeX			- Width of the texture, in texels
	 * @param InSizeY			- Height of the texture, in texels
	 * @param InFormat			- Format of the texture, defaults to PF_B8G8R8A8
	 * @param InIsResolveTarget	- Whether the texture can be used as a resolve target
	 */
	ENGINE_API void Init(int32 InSizeX, int32 InSizeY, EPixelFormat InFormat = PF_B8G8R8A8, bool InIsResolveTarget = false);
	
	/** Creates and initializes a new Texture2DDynamic with the requested settings */
	ENGINE_API static UTexture2DDynamic* Create(int32 InSizeX, int32 InSizeY, EPixelFormat InFormat = PF_B8G8R8A8, bool InIsResolveTarget = false);
};
