// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/Texture.h"
#include "TextureCube.generated.h"

class FTextureResource;

UCLASS(hidecategories=Object, MinimalAPI)
class UTextureCube : public UTexture
{
	GENERATED_UCLASS_BODY()

public:
	/** Platform data. */
	FTexturePlatformData *PlatformData;
	TMap<FString, FTexturePlatformData*> CookedPlatformData;

	//~ Begin UObject Interface.
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual FString GetDesc() override;
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	//~ End UObject Interface.

	/** Trivial accessors. */
	FORCEINLINE int32 GetSizeX() const
	{
		if (PlatformData)
		{
			return PlatformData->SizeX;
		}
		return 0;
	}
	FORCEINLINE int32 GetSizeY() const
	{
		if (PlatformData)
		{
			return PlatformData->SizeY;
		}
		return 0;
	}
	FORCEINLINE int32 GetNumMips() const
	{
		if (PlatformData)
		{
			return PlatformData->Mips.Num();
		}
		return 0;
	}
	FORCEINLINE EPixelFormat GetPixelFormat() const
	{
		if (PlatformData)
		{
			return PlatformData->PixelFormat;
		}
		return PF_Unknown;
	}

	//~ Begin UTexture Interface
	virtual float GetSurfaceWidth() const override { return GetSizeX(); }
	virtual float GetSurfaceHeight() const override { return GetSizeY(); }
	virtual FTextureResource* CreateResource() override;
	virtual void UpdateResource() override;
	virtual EMaterialValueType GetMaterialType() const override { return MCT_TextureCube; }
	virtual FTexturePlatformData** GetRunningPlatformData() override { return &PlatformData; }
	virtual TMap<FString, FTexturePlatformData*> *GetCookedPlatformData() override { return &CookedPlatformData; }
	//~ End UTexture Interface
	
	/**
	 * Calculates the size of this texture in bytes if it had MipCount miplevels streamed in.
	 *
	 * @param	MipCount	Number of mips to calculate size for, counting from the smallest 1x1 mip-level and up.
	 * @return	Size of MipCount mips in bytes
	 */
	uint32 CalcTextureMemorySize( int32 MipCount ) const;

	/**
	 * Calculates the size of this texture if it had MipCount miplevels streamed in.
	 *
	 * @param	Enum	Which mips to calculate size for.
	 * @return	Total size of all specified mips, in bytes
	 */
	virtual uint32 CalcTextureMemorySizeEnum( ETextureMipCount Enum ) const override;

#if WITH_EDITOR
	/**
	* Return maximum dimension for this texture type.
	*/
	virtual uint32 GetMaximumDimension() const override;
#endif
};



