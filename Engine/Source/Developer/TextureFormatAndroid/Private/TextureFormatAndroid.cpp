// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformStackWalk.h"
#include "Misc/Paths.h"
#include "ImageCore.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/ITextureFormat.h"
#include "Interfaces/ITextureFormatModule.h"
#include "TextureCompressorModule.h"
#include "PixelFormat.h"
#include "TextureConverter.h"
#include "HAL/PlatformProcess.h"

DEFINE_LOG_CATEGORY_STATIC(LogTextureFormatAndroid, Log, All);


/**
 * Macro trickery for supported format names.
 */
#define ENUM_SUPPORTED_FORMATS(op) \
	op(ATC_RGB) \
	op(ATC_RGBA_E) \
	op(ATC_RGBA_I) \
	op(AutoATC) \
	op(ETC1) \
	op(AutoETC1) \
	op(ETC2_RGB) \
	op(ETC2_RGBA) \
	op(AutoETC2)

#define DECL_FORMAT_NAME(FormatName) static FName GTextureFormatName##FormatName = FName(TEXT(#FormatName));
ENUM_SUPPORTED_FORMATS(DECL_FORMAT_NAME);
#undef DECL_FORMAT_NAME

#define DECL_FORMAT_NAME_ENTRY(FormatName) GTextureFormatName##FormatName ,
static FName GSupportedTextureFormatNames[] =
{
	ENUM_SUPPORTED_FORMATS(DECL_FORMAT_NAME_ENTRY)
};
#undef DECL_FORMAT_NAME_ENTRY

#undef ENUM_SUPPORTED_FORMATS


/**
 * Compresses an image using Qonvert.
 * @param SourceData			Source texture data to compress, in BGRA 8bit per channel unsigned format.
 * @param PixelFormat			Texture format
 * @param SizeX					Number of texels along the X-axis
 * @param SizeY					Number of texels along the Y-axis
 * @param OutCompressedData		Compressed image data output by Qonvert.
 */
static bool CompressImageUsingQonvert(
	const void* SourceData,
	EPixelFormat PixelFormat,
	int32 SizeX,
	int32 SizeY,
	TArray<uint8>& OutCompressedData
	)
{
	// Avoid dependency on GPixelFormats in RenderCore.
	const int32 BlockSizeX = 4;
	const int32 BlockSizeY = 4;
	const int32 BlockBytes = (PixelFormat == PF_ATC_RGBA_E || PixelFormat == PF_ATC_RGBA_I || PixelFormat == PF_ETC2_RGBA) ? 16 : 8;
	const int32 ImageBlocksX = FMath::Max(SizeX / BlockSizeX, 1);
	const int32 ImageBlocksY = FMath::Max(SizeY / BlockSizeY, 1);

	// Allocate space to store compressed data.
	OutCompressedData.Empty(ImageBlocksX * ImageBlocksY * BlockBytes);
	OutCompressedData.AddUninitialized(ImageBlocksX * ImageBlocksY * BlockBytes);

	TQonvertImage SrcImg;
	TQonvertImage DstImg;

	FMemory::Memzero(SrcImg);
	FMemory::Memzero(DstImg);

	SrcImg.nWidth    = SizeX;
	SrcImg.nHeight   = SizeY;
	SrcImg.nFormat   = Q_FORMAT_BGRA_8888;
	SrcImg.nDataSize = SizeX * SizeY * 4;
	SrcImg.pData     = (unsigned char*)SourceData;

	DstImg.nWidth    = SizeX;
	DstImg.nHeight   = SizeY;
	DstImg.nDataSize = ImageBlocksX * ImageBlocksY * BlockBytes;
	DstImg.pData     = OutCompressedData.GetData();

	switch (PixelFormat)
	{
	case PF_ETC1:
		DstImg.nFormat = Q_FORMAT_ETC1_RGB8;
		break;
	case PF_ETC2_RGB:
		DstImg.nFormat = Q_FORMAT_ETC2_RGB8;
		break;
	case PF_ETC2_RGBA:
		DstImg.nFormat = Q_FORMAT_ETC2_RGBA8;
		break;
	case PF_ATC_RGB:
		DstImg.nFormat = Q_FORMAT_ATC_RGB;
		break;
	case PF_ATC_RGBA_E:
		DstImg.nFormat = Q_FORMAT_ATC_RGBA_EXPLICIT_ALPHA;
		break;
	case PF_ATC_RGBA_I:
		DstImg.nFormat = Q_FORMAT_ATC_RGBA_INTERPOLATED_ALPHA;
		break;
	default:
		UE_LOG(LogTextureFormatAndroid, Fatal, TEXT("Unsupported EPixelFormat for compression: %u"), (uint32)PixelFormat);
		return false;
	}

	if (Qonvert(&SrcImg, &DstImg) != Q_SUCCESS)
	{
		return false;
	}

	return true;
}


/**
 * ATITC and ETC1/2 texture format handler.
 */
class FTextureFormatAndroid : public ITextureFormat
{
	virtual bool AllowParallelBuild() const override
	{
		return true;
	}

	virtual uint16 GetVersion(
		FName Format,
		const struct FTextureBuildSettings* BuildSettings = nullptr
	) const override
	{
		return 0;
	}

	virtual void GetSupportedFormats(TArray<FName>& OutFormats) const override
	{
		for (int32 i = 0; i < ARRAY_COUNT(GSupportedTextureFormatNames); ++i)
		{
			OutFormats.Add(GSupportedTextureFormatNames[i]);
		}
	}

	virtual FTextureFormatCompressorCaps GetFormatCapabilities() const override
	{
		return FTextureFormatCompressorCaps(); // Default capabilities.
	}

	virtual bool CompressImage(
		const FImage& InImage,
		const struct FTextureBuildSettings& BuildSettings,
		bool bImageHasAlphaChannel,
		FCompressedImage2D& OutCompressedImage
		) const override
	{
		FImage Image;
		InImage.CopyTo(Image, ERawImageFormat::BGRA8, BuildSettings.GetGammaSpace());

		EPixelFormat CompressedPixelFormat = PF_Unknown;

		if (BuildSettings.TextureFormatName == GTextureFormatNameAutoETC1)
		{
			if (bImageHasAlphaChannel)
			{
				// ETC1 can't support an alpha channel, store uncompressed
				OutCompressedImage.SizeX = Image.SizeX;
				OutCompressedImage.SizeY = Image.SizeY;
				OutCompressedImage.PixelFormat = PF_B8G8R8A8;
				OutCompressedImage.RawData = Image.RawData;
				return true;
			}
			else
			{
				CompressedPixelFormat = PF_ETC1;
			}
		}
		else		
		if (BuildSettings.TextureFormatName == GTextureFormatNameETC1)
		{
			CompressedPixelFormat = PF_ETC1;
		}
		else
		if (BuildSettings.TextureFormatName == GTextureFormatNameETC2_RGB ||
		   (BuildSettings.TextureFormatName == GTextureFormatNameAutoETC2 && !bImageHasAlphaChannel))
		{
			CompressedPixelFormat = PF_ETC2_RGB;
		}
		else
		if (BuildSettings.TextureFormatName == GTextureFormatNameETC2_RGBA ||
		   (BuildSettings.TextureFormatName == GTextureFormatNameAutoETC2 && bImageHasAlphaChannel))
		{
			CompressedPixelFormat = PF_ETC2_RGBA;
		}
		else
		if (BuildSettings.TextureFormatName == GTextureFormatNameATC_RGB || 
		   (BuildSettings.TextureFormatName == GTextureFormatNameAutoATC && !bImageHasAlphaChannel))
		{
			CompressedPixelFormat = PF_ATC_RGB;
		}
		else
		if (BuildSettings.TextureFormatName == GTextureFormatNameATC_RGBA_I ||
		   (BuildSettings.TextureFormatName == GTextureFormatNameAutoATC && bImageHasAlphaChannel) )
		{
			CompressedPixelFormat = PF_ATC_RGBA_I;
		}
		else
		if (BuildSettings.TextureFormatName == GTextureFormatNameATC_RGBA_E)
		{
			CompressedPixelFormat = PF_ATC_RGBA_E;
		}

		check(CompressedPixelFormat != PF_Unknown);

		bool bCompressionSucceeded = true;
		int32 SliceSize = Image.SizeX * Image.SizeY;
		for (int32 SliceIndex = 0; SliceIndex < Image.NumSlices && bCompressionSucceeded; ++SliceIndex)
		{
			TArray<uint8> CompressedSliceData;
			bCompressionSucceeded = CompressImageUsingQonvert(
				Image.AsBGRA8() + SliceIndex * SliceSize,
				CompressedPixelFormat,
				Image.SizeX,
				Image.SizeY,
				CompressedSliceData
				);
			OutCompressedImage.RawData.Append(CompressedSliceData);
		}

		if (bCompressionSucceeded)
		{
			OutCompressedImage.SizeX = FMath::Max(Image.SizeX, 4);
			OutCompressedImage.SizeY = FMath::Max(Image.SizeY, 4);
			OutCompressedImage.PixelFormat = CompressedPixelFormat;
		}

		return bCompressionSucceeded;
	}
};

/**
 * Module for ATITC and ETC1/2 texture compression.
 */
static ITextureFormat* Singleton = NULL;



#if PLATFORM_WINDOWS
	void*	TextureConverterHandle = NULL;
	FString QualCommBinariesRoot = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/QualComm/Win64/");
#endif


class FTextureFormatAndroidModule : public ITextureFormatModule
{
public:

	FTextureFormatAndroidModule()
	{
#if PLATFORM_WINDOWS
		TextureConverterHandle = FPlatformProcess::GetDllHandle(*(QualCommBinariesRoot + "TextureConverter.dll"));
#endif
	}

	virtual ~FTextureFormatAndroidModule()
	{
		delete Singleton;
		Singleton = NULL;

#if PLATFORM_WINDOWS
		FPlatformProcess::FreeDllHandle(TextureConverterHandle);
#endif
	}
	virtual ITextureFormat* GetTextureFormat()
	{
		if (!Singleton)
		{
#if PLATFORM_WINDOWS
			TextureConverterHandle = FPlatformProcess::GetDllHandle(*(QualCommBinariesRoot + "TextureConverter.dll"));
#endif
			Singleton = new FTextureFormatAndroid();
		}
		return Singleton;
	}
};

IMPLEMENT_MODULE(FTextureFormatAndroidModule, TextureFormatAndroid);
