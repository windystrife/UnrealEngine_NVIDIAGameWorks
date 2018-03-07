// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "GenericImgMediaReader.h"
#include "ImgMediaPrivate.h"

#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"


/* Local helpers
 *****************************************************************************/

TSharedPtr<IImageWrapper> LoadImage(const FString& ImagePath, IImageWrapperModule& ImageWrapperModule, TArray<uint8>& OutBuffer, FImgMediaFrameInfo& OutInfo)
{
	// load image into buffer
	if (!FFileHelper::LoadFileToArray(OutBuffer, *ImagePath))
	{
		UE_LOG(LogImgMedia, Warning, TEXT("FGenericImgMediaReader: Failed to load %s"), *ImagePath);
		return nullptr;
	}

	// determine image format
	EImageFormat ImageFormat;

	const FString Extension = FPaths::GetExtension(ImagePath).ToLower();

	if (Extension == TEXT("bmp"))
	{
		ImageFormat = EImageFormat::BMP;
		OutInfo.FormatName = TEXT("BMP");
	}
	else if ((Extension == TEXT("jpg")) || (Extension == TEXT("jpeg")))
	{
		ImageFormat = EImageFormat::JPEG;
		OutInfo.FormatName = TEXT("JPEG");
	}
	else if (Extension == TEXT("png"))
	{
		ImageFormat = EImageFormat::PNG;
		OutInfo.FormatName = TEXT("PNG");
	}
	else
	{
		UE_LOG(LogImgMedia, Warning, TEXT("FGenericImgMediaReader: Unsupported file format in %s"), *ImagePath);
		return nullptr;
	}

	// create image wrapper
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);

	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(OutBuffer.GetData(), OutBuffer.Num()))
	{
		UE_LOG(LogImgMedia, Warning, TEXT("FGenericImgMediaReader: Failed to create image wrapper for %s"), *ImagePath);
		return nullptr;
	}

	// get file info
	OutInfo.CompressionName = TEXT("");
	OutInfo.Dim.X = ImageWrapper->GetWidth();
	OutInfo.Dim.Y = ImageWrapper->GetHeight();
	OutInfo.Fps = IMGMEDIA_DEFAULT_FPS;
	OutInfo.Srgb = true;
	OutInfo.UncompressedSize = OutInfo.Dim.X * OutInfo.Dim.Y * 4;

	return ImageWrapper;
}


/* FGenericImgMediaReader structors
 *****************************************************************************/

FGenericImgMediaReader::FGenericImgMediaReader(IImageWrapperModule& InImageWrapperModule)
	: ImageWrapperModule(InImageWrapperModule)
{ }


/* FGenericImgMediaReader interface
 *****************************************************************************/

bool FGenericImgMediaReader::GetFrameInfo(const FString& ImagePath, FImgMediaFrameInfo& OutInfo)
{
	TArray<uint8> InputBuffer;
	TSharedPtr<IImageWrapper> ImageWrapper = LoadImage(ImagePath, ImageWrapperModule, InputBuffer, OutInfo);

	return ImageWrapper.IsValid();
}


bool FGenericImgMediaReader::ReadFrame(const FString& ImagePath, FImgMediaFrame& OutFrame)
{
	TArray<uint8> InputBuffer;
	TSharedPtr<IImageWrapper> ImageWrapper = LoadImage(ImagePath, ImageWrapperModule, InputBuffer, OutFrame.Info);

	if (!ImageWrapper.IsValid())
	{
		UE_LOG(LogImgMedia, Warning, TEXT("FGenericImgMediaReader: Failed to load image %s"), *ImagePath);
		return false;
	}

	const TArray<uint8>* RawData = nullptr;

	if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
	{
		UE_LOG(LogImgMedia, Warning, TEXT("FGenericImgMediaReader: Failed to get image data for %s"), *ImagePath);
		return false;
	}

	const int32 RawNum = RawData->Num();
	void* Buffer = FMemory::Malloc(RawNum);
	FMemory::Memcpy(Buffer, RawData->GetData(), RawNum);

	OutFrame.Data = MakeShareable(Buffer, [](void* ObjectToDelete) { FMemory::Free(ObjectToDelete); });
	OutFrame.Format = EMediaTextureSampleFormat::CharBGRA;
	OutFrame.Stride = OutFrame.Info.Dim.X * 4;

	return true;
}
