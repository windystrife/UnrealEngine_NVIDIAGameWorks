// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ImageWrapperPrivate.h"

#include "CoreTypes.h"
#include "Modules/ModuleManager.h"

#include "Formats/BmpImageWrapper.h"
#include "Formats/ExrImageWrapper.h"
#include "Formats/IcnsImageWrapper.h"
#include "Formats/IcoImageWrapper.h"
#include "Formats/JpegImageWrapper.h"
#include "Formats/PngImageWrapper.h"
#include "IImageWrapperModule.h"


DEFINE_LOG_CATEGORY(LogImageWrapper);


namespace
{
	static const uint8 IMAGE_MAGIC_PNG[]  = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
	static const uint8 IMAGE_MAGIC_JPEG[] = {0xFF, 0xD8, 0xFF};
	static const uint8 IMAGE_MAGIC_BMP[]  = {0x42, 0x4D};
	static const uint8 IMAGE_MAGIC_ICO[]  = {0x00, 0x00, 0x01, 0x00};
	static const uint8 IMAGE_MAGIC_EXR[]  = {0x76, 0x2F, 0x31, 0x01};
	static const uint8 IMAGE_MAGIC_ICNS[] = {0x69, 0x63, 0x6E, 0x73};

	/** Internal helper function to verify image signature. */
	template <int32 MagicCount> bool StartsWith(const uint8* Content, int32 ContentSize, const uint8 (&Magic)[MagicCount])
	{
		if (ContentSize < MagicCount)
		{
			return false;
		}

		for (int32 I = 0; I < MagicCount; ++I)
		{
			if (Content[I] != Magic[I])
			{
				return false;
			}
		}

		return true;
	}
}


/**
 * Image Wrapper module.
 */
class FImageWrapperModule
	: public IImageWrapperModule
{
public:

	//~ IImageWrapperModule interface

	virtual TSharedPtr<IImageWrapper> CreateImageWrapper(const EImageFormat InFormat) override
	{
		FImageWrapperBase* ImageWrapper = NULL;

		// Allocate a helper for the format type
		switch(InFormat)
		{
#if WITH_UNREALPNG
		case EImageFormat::PNG:
			ImageWrapper = new FPngImageWrapper();
			break;
#endif	// WITH_UNREALPNG

#if WITH_UNREALJPEG
		case EImageFormat::JPEG:
			ImageWrapper = new FJpegImageWrapper();
			break;

		case EImageFormat::GrayscaleJPEG:
			ImageWrapper = new FJpegImageWrapper(1);
			break;
#endif	//WITH_UNREALJPEG

		case EImageFormat::BMP:
			ImageWrapper = new FBmpImageWrapper();
			break;

		case EImageFormat::ICO:
			ImageWrapper = new FIcoImageWrapper();
			break;

#if WITH_UNREALEXR
		case EImageFormat::EXR:
			ImageWrapper = new FExrImageWrapper();
			break;
#endif
		case EImageFormat::ICNS:
			ImageWrapper = new FIcnsImageWrapper();
			break;

		default:
			break;
		}

		return MakeShareable(ImageWrapper);
	}

	virtual EImageFormat DetectImageFormat(const void* CompressedData, int32 CompressedSize) override
	{
		EImageFormat Format = EImageFormat::Invalid;
		if (StartsWith((uint8*) CompressedData, CompressedSize, IMAGE_MAGIC_PNG))
		{
			Format = EImageFormat::PNG;
		}
		else if (StartsWith((uint8*) CompressedData, CompressedSize, IMAGE_MAGIC_JPEG))
		{
			Format = EImageFormat::JPEG; // @Todo: Should we detect grayscale vs non-grayscale?
		}
		else if (StartsWith((uint8*) CompressedData, CompressedSize, IMAGE_MAGIC_BMP))
		{
			Format = EImageFormat::BMP;
		}
		else if (StartsWith((uint8*) CompressedData, CompressedSize, IMAGE_MAGIC_ICO))
		{
			Format = EImageFormat::ICO;
		}
		else if (StartsWith((uint8*) CompressedData, CompressedSize, IMAGE_MAGIC_EXR))
		{
			Format = EImageFormat::EXR;
		}
		else if (StartsWith((uint8*) CompressedData, CompressedSize, IMAGE_MAGIC_ICNS))
		{
			Format = EImageFormat::ICNS;
		}

		return Format;
	}

public:

	//~ IModuleInterface interface

	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }
};


IMPLEMENT_MODULE(FImageWrapperModule, ImageWrapper);
