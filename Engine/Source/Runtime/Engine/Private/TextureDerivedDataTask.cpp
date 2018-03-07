// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TextureDerivedDataTask.cpp: Tasks to update texture DDC.
=============================================================================*/

#include "TextureDerivedDataTask.h"
#include "Misc/CommandLine.h"
#include "Stats/Stats.h"
#include "Serialization/MemoryWriter.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "Serialization/MemoryReader.h"
#include "UObject/Package.h"
#include "RenderUtils.h"
#include "TextureResource.h"
#include "Engine/Texture.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "DeviceProfiles/DeviceProfileManager.h"

#if WITH_EDITOR

#include "DerivedDataCacheInterface.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Interfaces/ITextureFormat.h"
#include "Engine/TextureCube.h"
#include "ProfilingDebugging/CookStats.h"

class FTextureStatusMessageContext : public FScopedSlowTask
{
public:
	explicit FTextureStatusMessageContext(const FText& InMessage)
		: FScopedSlowTask(0, InMessage, IsInGameThread())
	{
		UE_LOG(LogTexture,Display,TEXT("%s"),*InMessage.ToString());
	}
};


void FTextureCacheDerivedDataWorker::FTextureSourceData::Init(UTexture& InTexture, const FTextureBuildSettings& InBuildSettings, bool bAllowAsyncLoading)
{
	switch (InTexture.Source.GetFormat())
	{
	case TSF_G8:		ImageFormat = ERawImageFormat::G8;		break;
	case TSF_BGRA8:		ImageFormat = ERawImageFormat::BGRA8;	break;
	case TSF_BGRE8:		ImageFormat = ERawImageFormat::BGRE8;	break;
	case TSF_RGBA16:	ImageFormat = ERawImageFormat::RGBA16;	break;
	case TSF_RGBA16F:	ImageFormat = ERawImageFormat::RGBA16F; break;
	default: 
		UE_LOG(LogTexture,Fatal,TEXT("Texture %s has source art in an invalid format."), *InTexture.GetName());
		return;
	}

	NumMips = InTexture.Source.GetNumMips();
	NumSlices = InTexture.Source.GetNumSlices();

	if (NumMips < 1 || NumSlices < 1)
	{
		UE_LOG(LogTexture,Warning, TEXT("Texture has no source mips: %s"), *InTexture.GetPathName());
		return;
	}

	if (InBuildSettings.MipGenSettings != TMGS_LeaveExistingMips)
	{
		NumMips = 1;
	}

	if (!InBuildSettings.bCubemap)
	{
		NumSlices = 1;
	}

	TextureName = InTexture.GetFName();
	GammaSpace = InTexture.SRGB ? (InTexture.bUseLegacyGamma ? EGammaSpace::Pow22 : EGammaSpace::sRGB) : EGammaSpace::Linear;

	if (bAllowAsyncLoading && !InTexture.Source.IsBulkDataLoaded())
	{
		// Prepare the async source to be later able to load it from file if required.
		AsyncSource = InTexture.Source; // This copies information required to make a safe IO load async.
	}

	bValid = true;
}

void FTextureCacheDerivedDataWorker::FTextureSourceData::GetSourceMips(FTextureSource& Source, IImageWrapperModule* InImageWrapper)
{
	if (bValid && !Mips.Num()) // If we already got valid data, nothing to do.
	{
		if (Source.HasHadBulkDataCleared())
		{	// don't do any work we can't reload this
			UE_LOG(LogTexture, Error, TEXT("Unable to get texture source mips because its bulk data was released. %s"), *TextureName.ToString())
			return;
		}

		for (int32 MipIndex = 0; MipIndex < NumMips; ++MipIndex)
		{
			FImage* SourceMip = new(Mips) FImage(
				(MipIndex == 0) ? Source.GetSizeX() : FMath::Max(1, Mips[MipIndex - 1].SizeX >> 1),
				(MipIndex == 0) ? Source.GetSizeY() : FMath::Max(1, Mips[MipIndex - 1].SizeY >> 1),
				NumSlices,
				ImageFormat,
				GammaSpace
				);

			if (!Source.GetMipData(SourceMip->RawData, MipIndex, InImageWrapper))
			{
				UE_LOG(LogTexture,Warning, TEXT("Cannot retrieve source data for mip %d of texture %s"), MipIndex, *TextureName.ToString());
				ReleaseMemory();
				bValid = false;
				break;
			}
		}
	}
}

void FTextureCacheDerivedDataWorker::FTextureSourceData::GetAsyncSourceMips(IImageWrapperModule* InImageWrapper)
{
	if (bValid && !Mips.Num() && AsyncSource.GetSizeOnDisk())
	{
		if (AsyncSource.LoadBulkDataWithFileReader())
		{
			GetSourceMips(AsyncSource, InImageWrapper);
		}
	}
}

void FTextureCacheDerivedDataWorker::BuildTexture()
{
	ensure(Compressor);
	if (Compressor && TextureData.Mips.Num())
	{
		FFormatNamedArguments Args; 
		Args.Add( TEXT("TextureName"), FText::FromString( Texture.GetName() ) );
		Args.Add( TEXT("TextureFormatName"), FText::FromString( BuildSettings.TextureFormatName.GetPlainNameString() ) );
		Args.Add( TEXT("TextureResolutionX"), FText::FromString( FString::FromInt(TextureData.Mips[0].SizeX) ) );
		Args.Add( TEXT("TextureResolutionY"), FText::FromString( FString::FromInt(TextureData.Mips[0].SizeY) ) );
		FTextureStatusMessageContext StatusMessage( FText::Format( NSLOCTEXT("Engine", "BuildTextureStatus", "Building textures: {TextureName} ({TextureFormatName}, {TextureResolutionX}X{TextureResolutionY})"), Args ) );

		check(DerivedData->Mips.Num() == 0);
		DerivedData->SizeX = 0;
		DerivedData->SizeY = 0;
		DerivedData->PixelFormat = PF_Unknown;

		// Compress the texture.
		TArray<FCompressedImage2D> CompressedMips;
		if (Compressor->BuildTexture(TextureData.Mips, CompositeTextureData.Mips, BuildSettings, CompressedMips))
		{
			check(CompressedMips.Num());

			// Build the derived data.
			const int32 MipCount = CompressedMips.Num();
			for (int32 MipIndex = 0; MipIndex < MipCount; ++MipIndex)
			{
				const FCompressedImage2D& CompressedImage = CompressedMips[MipIndex];
				FTexture2DMipMap* NewMip = new(DerivedData->Mips) FTexture2DMipMap();
				NewMip->SizeX = CompressedImage.SizeX;
				NewMip->SizeY = CompressedImage.SizeY;
				NewMip->BulkData.Lock(LOCK_READ_WRITE);
				check(CompressedImage.RawData.GetTypeSize() == 1);
				void* NewMipData = NewMip->BulkData.Realloc(CompressedImage.RawData.Num());
				FMemory::Memcpy(NewMipData, CompressedImage.RawData.GetData(), CompressedImage.RawData.Num());
				NewMip->BulkData.Unlock();

				if (MipIndex == 0)
				{
					DerivedData->SizeX = CompressedImage.SizeX;
					DerivedData->SizeY = CompressedImage.SizeY;
					DerivedData->PixelFormat = (EPixelFormat)CompressedImage.PixelFormat;
				}
				else
				{
					check(CompressedImage.PixelFormat == DerivedData->PixelFormat);
				}
			}
			DerivedData->NumSlices = BuildSettings.bCubemap ? 6 : 1;

			// Store it in the cache.
			// @todo: This will remove the streaming bulk data, which we immediately reload below!
			// Should ideally avoid this redundant work, but it only happens when we actually have 
			// to build the texture, which should only ever be once.
			this->BytesCached = PutDerivedDataInCache(DerivedData, KeySuffix);
		}

		if (DerivedData->Mips.Num())
		{
			bool bInlineMips = (CacheFlags & ETextureCacheFlags::InlineMips) != 0;
			bSucceeded = !bInlineMips || DerivedData->TryInlineMipData();
		}
		else
		{
			UE_LOG(LogTexture, Warning, TEXT("Failed to build %s derived data for %s"), *BuildSettings.TextureFormatName.GetPlainNameString(), *Texture.GetPathName());
		}
	}
}

FTextureCacheDerivedDataWorker::FTextureCacheDerivedDataWorker(
	ITextureCompressorModule* InCompressor,
	FTexturePlatformData* InDerivedData,
	UTexture* InTexture,
	const FTextureBuildSettings& InSettings,
	uint32 InCacheFlags
	)
	: Compressor(InCompressor)
	, ImageWrapper(nullptr)
	, DerivedData(InDerivedData)
	, Texture(*InTexture)
	, BuildSettings(InSettings)
	, CacheFlags(InCacheFlags)
	, bSucceeded(false)
{
	check(DerivedData);

	// At this point, the texture *MUST* have a valid GUID.
	if (!Texture.Source.GetId().IsValid())
	{
		UE_LOG(LogTexture, Warning, TEXT("Building texture with an invalid GUID: %s"), *Texture.GetPathName());
		Texture.Source.ForceGenerateGuid();
	}
	check(Texture.Source.GetId().IsValid());

	// Dump any existing mips.
	DerivedData->Mips.Empty();
	UTexture::GetPixelFormatEnum();
	GetTextureDerivedDataKeySuffix(Texture, BuildSettings, KeySuffix);
		
	const bool bAllowAsyncBuild = (CacheFlags & ETextureCacheFlags::AllowAsyncBuild) != 0;
	const bool bAllowAsyncLoading = (CacheFlags & ETextureCacheFlags::AllowAsyncLoading) != 0;

	if (bAllowAsyncLoading)
	{
		ImageWrapper = &FModuleManager::LoadModuleChecked<IImageWrapperModule>( FName("ImageWrapper") );
	}

	TextureData.Init(Texture, BuildSettings, bAllowAsyncLoading); 
	if (Texture.CompositeTexture && Texture.CompositeTextureMode != CTM_Disabled)
	{
		const int32 SizeX = Texture.CompositeTexture->Source.GetSizeX();
		const int32 SizeY = Texture.CompositeTexture->Source.GetSizeY();
		if (FMath::IsPowerOfTwo(SizeX) && FMath::IsPowerOfTwo(SizeY))
		{
			CompositeTextureData.Init(*Texture.CompositeTexture, BuildSettings, bAllowAsyncLoading);
		}
	}

	// If the bulkdata is loaded and async build is allowed, get the source mips now (safe) to allow building the DDC if required.
	// If the bulkdata is not loaded, the DDC will be built in Finalize() unless async loading is enabled (which won't allow reuse of the source for later use).
	if (bAllowAsyncBuild)
	{
		if (TextureData.IsValid() && Texture.Source.IsBulkDataLoaded())
		{
			TextureData.GetSourceMips(Texture.Source, ImageWrapper);
		}
		if (CompositeTextureData.IsValid() && Texture.CompositeTexture && Texture.CompositeTexture->Source.IsBulkDataLoaded())
		{
			CompositeTextureData.GetSourceMips(Texture.CompositeTexture->Source, ImageWrapper);
		}
	}
}

void FTextureCacheDerivedDataWorker::DoWork()
{
	const bool bForceRebuild = (CacheFlags & ETextureCacheFlags::ForceRebuild) != 0;
	const bool bAllowAsyncBuild = (CacheFlags & ETextureCacheFlags::AllowAsyncBuild) != 0;
	const bool bAllowAsyncLoading = (CacheFlags & ETextureCacheFlags::AllowAsyncLoading) != 0;

	TArray<uint8> RawDerivedData;

	if (!bForceRebuild && GetDerivedDataCacheRef().GetSynchronous(*DerivedData->DerivedDataKey, RawDerivedData))
	{
		const bool bInlineMips = (CacheFlags & ETextureCacheFlags::InlineMips) != 0;
		const bool bForDDC = (CacheFlags & ETextureCacheFlags::ForDDCBuild) != 0;

		BytesCached = RawDerivedData.Num();
		FMemoryReader Ar(RawDerivedData, /*bIsPersistent=*/ true);
		DerivedData->Serialize(Ar, NULL);
		bSucceeded = true;
		// Load any streaming (not inline) mips that are necessary for our platform.
		if (bForDDC)
		{
			bSucceeded = DerivedData->TryLoadMips(0,NULL);
		}
		else if (bInlineMips)
		{
			bSucceeded = DerivedData->TryInlineMipData();
		}
		else
		{
			bSucceeded = DerivedData->AreDerivedMipsAvailable();
		}
		bLoadedFromDDC = true;

		// Reset everything derived data so that we can do a clean load from the source data
		if (!bSucceeded)
		{
			DerivedData->Mips.Empty();
		}
	}
	
	if (!bSucceeded && bAllowAsyncBuild)
	{
		if (bAllowAsyncLoading)
		{
			TextureData.GetAsyncSourceMips(ImageWrapper);
			CompositeTextureData.GetAsyncSourceMips(ImageWrapper);
		}

		if (TextureData.Mips.Num() && (!CompositeTextureData.IsValid() || CompositeTextureData.Mips.Num()))
		{
			BuildTexture();
			bSucceeded = true;
		}
		else
		{
			bSucceeded = false;
		}
	}

	if (bSucceeded)
	{
		TextureData.ReleaseMemory();
		CompositeTextureData.ReleaseMemory();
	}
}

void FTextureCacheDerivedDataWorker::Finalize()
{
	check(IsInGameThread());
	// if we couldn't get from the DDC or didn't build synchronously, then we have to build now. 
	// This is a super edge case that should rarely happen.
	if (!bSucceeded)
	{
		TextureData.GetSourceMips(Texture.Source, ImageWrapper);
		if (Texture.CompositeTexture)
		{
			CompositeTextureData.GetSourceMips(Texture.CompositeTexture->Source, ImageWrapper);
		}
		BuildTexture();
	}
}

#endif // WITH_EDITOR