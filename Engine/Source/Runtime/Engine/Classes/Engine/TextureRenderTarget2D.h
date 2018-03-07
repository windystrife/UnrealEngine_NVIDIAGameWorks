// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "RenderUtils.h"
#include "Engine/TextureRenderTarget.h"
#include "TextureRenderTarget2D.generated.h"

class FTextureResource;
class UTexture2D;
struct FPropertyChangedEvent;

extern ENGINE_API int32 GTextureRenderTarget2DMaxSizeX;
extern ENGINE_API int32 GTextureRenderTarget2DMaxSizeY;

/** Subset of EPixelFormat exposed to UTextureRenderTarget2D */
UENUM()
enum ETextureRenderTargetFormat
{
	/** R channel, 8 bit per channel fixed point, range [0, 1]. */
	RTF_R8,
	/** RG channels, 8 bit per channel fixed point, range [0, 1]. */
	RTF_RG8,
	/** RGBA channels, 8 bit per channel fixed point, range [0, 1]. */
	RTF_RGBA8,
	/** R channel, 16 bit per channel floating point, range [-65504, 65504] */
	RTF_R16f,
	/** RG channels, 16 bit per channel floating point, range [-65504, 65504] */
	RTF_RG16f,
	/** RGBA channels, 16 bit per channel floating point, range [-65504, 65504] */
	RTF_RGBA16f,
	/** R channel, 32 bit per channel floating point, range [-3.402823 x 10^38, 3.402823 x 10^38] */
	RTF_R32f,
	/** RG channels, 32 bit per channel floating point, range [-3.402823 x 10^38, 3.402823 x 10^38] */
	RTF_RG32f,
	/** RGBA channels, 32 bit per channel floating point, range [-3.402823 x 10^38, 3.402823 x 10^38] */
	RTF_RGBA32f
};

inline EPixelFormat GetPixelFormatFromRenderTargetFormat(ETextureRenderTargetFormat RTFormat)
{
	switch (RTFormat)
	{
	case RTF_R8: return PF_G8; break;
	case RTF_RG8: return PF_R8G8; break;
	case RTF_RGBA8: return PF_B8G8R8A8; break;

	case RTF_R16f: return PF_R16F; break;
	case RTF_RG16f: return PF_G16R16F; break;
	case RTF_RGBA16f: return PF_FloatRGBA; break;

	case RTF_R32f: return PF_R32_FLOAT; break;
	case RTF_RG32f: return PF_G32R32F; break;
	case RTF_RGBA32f: return PF_A32B32G32R32F; break;
	}

	ensureMsgf(false, TEXT("Unhandled ETextureRenderTargetFormat entry %u"), (uint32)RTFormat);
	return PF_Unknown;
}

/**
 * TextureRenderTarget2D
 *
 * 2D render target texture resource. This can be used as a target
 * for rendering as well as rendered as a regular 2D texture resource.
 *
 */
UCLASS(hidecategories=Object, hidecategories=Texture, hidecategories=Compression, hidecategories=Adjustments, hidecategories=Compositing, MinimalAPI)
class UTextureRenderTarget2D : public UTextureRenderTarget
{
	GENERATED_UCLASS_BODY()

	/** The width of the texture. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=TextureRenderTarget2D, AssetRegistrySearchable)
	int32 SizeX;

	/** The height of the texture. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=TextureRenderTarget2D, AssetRegistrySearchable)
	int32 SizeY;

	/** the color the texture is cleared to */
	UPROPERTY()
	FLinearColor ClearColor;

	/** The addressing mode to use for the X axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TextureRenderTarget2D, AssetRegistrySearchable)
	TEnumAsByte<enum TextureAddress> AddressX;

	/** The addressing mode to use for the Y axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TextureRenderTarget2D, AssetRegistrySearchable)
	TEnumAsByte<enum TextureAddress> AddressY;

	/** True to force linear gamma space for this render target */
	UPROPERTY()
	uint32 bForceLinearGamma:1;

	/** Whether to support storing HDR values, which requires more memory. */
	UPROPERTY()
	uint32 bHDR_DEPRECATED:1;

	/** 
	 * Format of the texture render target. 
	 * Data written to the render target will be quantized to this format, which can limit the range and precision.
	 * The largest format (RTF_RGBA32f) uses 16x more memory and bandwidth than the smallest (RTF_R8) and can greatly affect performance.  
	 * Use the smallest format that has enough precision and range for what you are doing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=TextureRenderTarget2D, AssetRegistrySearchable)
	TEnumAsByte<enum ETextureRenderTargetFormat> RenderTargetFormat;

	/** Whether to support GPU sharing of the underlying native texture resource. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = TextureRenderTarget2D, meta=(DisplayName = "Shared"), AssetRegistrySearchable, AdvancedDisplay)
	uint32 bGPUSharedFlag : 1;

	/** Whether to support Mip maps for this render target texture */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=TextureRenderTarget2D, AssetRegistrySearchable)
	uint32 bAutoGenerateMips:1;

	/** Normally the format is derived from RenderTargetFormat, this allows code to set the format explicitly. */
	UPROPERTY()
	TEnumAsByte<enum EPixelFormat> OverrideFormat;

	/**
	 * Initialize the settings needed to create a render target texture
	 * and create its resource
	 * @param	InSizeX - width of the texture
	 * @param	InSizeY - height of the texture
	 * @param	InFormat - format of the texture
	 * @param	bInForceLinearGame - forces render target to use linear gamma space
	 */
	ENGINE_API void InitCustomFormat(uint32 InSizeX, uint32 InSizeY, EPixelFormat InOverrideFormat, bool bInForceLinearGamma);

	/** Initializes the render target, the format will be derived from the value of bHDR. */
	ENGINE_API void InitAutoFormat(uint32 InSizeX, uint32 InSizeY);

	/**
	 * Utility for creating a new UTexture2D from a TextureRenderTarget2D
	 * TextureRenderTarget2D must be square and a power of two size.
	 * @param Outer - Outer to use when constructing the new Texture2D.
	 * @param NewTexName - Name of new UTexture2D object.
	 * @param ObjectFlags - Flags to apply to the new Texture2D object
	 * @param Flags - Various control flags for operation (see EConstructTextureFlags)
	 * @param AlphaOverride - If specified, the values here will become the alpha values in the resulting texture
	 * @return New UTexture2D object.
	 */
	ENGINE_API UTexture2D* ConstructTexture2D(UObject* InOuter, const FString& NewTexName, EObjectFlags InObjectFlags, uint32 Flags=CTF_Default, TArray<uint8>* AlphaOverride=NULL);

	/**
	 * Updates (resolves) the render target texture immediately.
	 * Optionally clears the contents of the render target to green.
	 */
	ENGINE_API void UpdateResourceImmediate(bool bClearRenderTarget=true);

	//~ Begin UTexture Interface.
	virtual float GetSurfaceWidth() const override { return SizeX; }
	virtual float GetSurfaceHeight() const override { return SizeY; }
	ENGINE_API virtual FTextureResource* CreateResource() override;
	ENGINE_API virtual EMaterialValueType GetMaterialType() const override;
	//~ End UTexture Interface.

	//~ Begin UObject Interface
#if WITH_EDITOR
	ENGINE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	ENGINE_API virtual void Serialize(FArchive& Ar) override;
	ENGINE_API virtual void PostLoad() override;
	ENGINE_API virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	ENGINE_API virtual FString GetDesc() override;
	//~ End UObject Interface

	FORCEINLINE int32 GetNumMips() const
	{
		return NumMips;
	}


	FORCEINLINE EPixelFormat GetFormat() const
	{
		if (OverrideFormat == PF_Unknown)
		{
			return GetPixelFormatFromRenderTargetFormat(RenderTargetFormat);
		}
		else
		{
			return OverrideFormat;
		}
	}

private:
	int32	NumMips;
};



