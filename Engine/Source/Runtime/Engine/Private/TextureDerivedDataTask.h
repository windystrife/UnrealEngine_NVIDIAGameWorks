// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	TextureDerivedDataTask.h: Tasks to update texture DDC.
==============================================================================*/

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Async/AsyncWork.h"
#include "ImageCore.h"
#include "TextureCompressorModule.h"
#include "IImageWrapperModule.h"
#include "Engine/Texture2D.h"

#endif // WITH_EDITOR

enum
{
	/** The number of mips to store inline. */
	NUM_INLINE_DERIVED_MIPS = 7,
};

#if WITH_EDITOR

void GetTextureDerivedDataKeySuffix(const UTexture& Texture, const FTextureBuildSettings& BuildSettings, FString& OutKeySuffix);
uint32 PutDerivedDataInCache(FTexturePlatformData* DerivedData, const FString& DerivedDataKeySuffix);

namespace ETextureCacheFlags
{
	enum Type
	{
		None			= 0x00,
		Async			= 0x01,
		ForceRebuild	= 0x02,
		InlineMips		= 0x08,
		AllowAsyncBuild	= 0x10,
		ForDDCBuild		= 0x20,
		RemoveSourceMipDataAfterCache = 0x40,
		AllowAsyncLoading = 0x80,
	};
};

/**
 * Worker used to cache texture derived data.
 */
class FTextureCacheDerivedDataWorker : public FNonAbandonableTask
{
	// Everything required to get the texture source data.
	struct FTextureSourceData
	{
		FTextureSourceData() : NumMips(0), NumSlices(0), ImageFormat(ERawImageFormat::BGRA8), GammaSpace(EGammaSpace::sRGB), bValid(false) {}

		void Init(UTexture& InTexture, const FTextureBuildSettings& InBuildSettings, bool bAllowAsyncLoading);
		bool IsValid() const { return bValid; }

		void GetSourceMips(FTextureSource& Source, IImageWrapperModule* InImageWrapper);
		void GetAsyncSourceMips(IImageWrapperModule* InImageWrapper);

		void ReleaseMemory()
		{
			// Unload BulkData loaded with LoadBulkDataWithFileReader
			AsyncSource.RemoveBulkData();
			Mips.Empty();
		}

		FName TextureName;
		FTextureSource AsyncSource;
		TArray<FImage> Mips;
		int32 NumMips;
		int32 NumSlices;
		ERawImageFormat::Type ImageFormat;
		EGammaSpace GammaSpace;
		bool bValid;
	};



	/** Texture compressor module, must be loaded in the game thread. see FModuleManager::WarnIfItWasntSafeToLoadHere() */
	ITextureCompressorModule* Compressor;
	/** Image wrapper module, must be loaded in the game thread. see FModuleManager::WarnIfItWasntSafeToLoadHere() */
	IImageWrapperModule* ImageWrapper;
	/** Where to store derived data. */
	FTexturePlatformData* DerivedData;
	/** The texture for which derived data is being cached. */
	UTexture& Texture;
	/** Compression settings. */
	FTextureBuildSettings BuildSettings;
	/** Derived data key suffix. */
	FString KeySuffix;
	/** Source mip images. */
	FTextureSourceData TextureData;
	/** Source mip images of the composite texture (e.g. normal map for compute roughness). Not necessarily in RGBA32F, usually only top mip as other mips need to be generated first */
	FTextureSourceData CompositeTextureData;
	/** Texture cache flags. */
	uint32 CacheFlags;
	/** Have many bytes were loaded from DDC or built (for telemetry) */
	uint32 BytesCached = 0;

	/** true if caching has succeeded. */
	bool bSucceeded;
	/** true if the derived data was pulled from DDC */
	bool bLoadedFromDDC = false;

	/** Build the texture. This function is safe to call from any thread. */
	void BuildTexture();

public:

	/** Initialization constructor. */
	FTextureCacheDerivedDataWorker(
		ITextureCompressorModule* InCompressor,
		FTexturePlatformData* InDerivedData,
		UTexture* InTexture,
		const FTextureBuildSettings& InSettings,
		uint32 InCacheFlags);

	/** Does the work to cache derived data. Safe to call from any thread. */
	void DoWork();


	/** Finalize work. Must be called ONLY by the game thread! */
	void Finalize();

	/** Expose bytes cached for telemetry. */
	uint32 GetBytesCached() const
	{
		return BytesCached;
	}

	/** Expose how the resource was returned for telemetry. */
	bool WasLoadedFromDDC() const
	{
		return bLoadedFromDDC;
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FTextureCacheDerivedDataWorker, STATGROUP_ThreadPoolAsyncTasks);
	}
};

struct FTextureAsyncCacheDerivedDataTask : public FAsyncTask<FTextureCacheDerivedDataWorker>
{
	FTextureAsyncCacheDerivedDataTask(
		ITextureCompressorModule* InCompressor,
		FTexturePlatformData* InDerivedData,
		UTexture* InTexture,
		const FTextureBuildSettings& InSettings,
		uint32 InCacheFlags
		)
		: FAsyncTask<FTextureCacheDerivedDataWorker>(
			InCompressor,
			InDerivedData,
			InTexture,
			InSettings,
			InCacheFlags
			)
	{
	}
};

#endif // WITH_EDITOR
