// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ExrImgMediaReader.h"

#if IMGMEDIA_EXR_SUPPORTED_PLATFORM

#include "Misc/Paths.h"
#include "OpenExrWrapper.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"


/* FExrImgMediaReader structors
 *****************************************************************************/

FExrImgMediaReader::FExrImgMediaReader()
{
	auto Settings = GetDefault<UImgMediaSettings>();
	
	FOpenExr::SetGlobalThreadCount(Settings->ExrDecoderThreads == 0
		? FPlatformMisc::NumberOfCoresIncludingHyperthreads()
		: Settings->ExrDecoderThreads);
}


/* FExrImgMediaReader interface
 *****************************************************************************/

bool FExrImgMediaReader::GetFrameInfo(const FString& ImagePath, FImgMediaFrameInfo& OutInfo)
{
	FRgbaInputFile InputFile(ImagePath, 2);
	
	return GetInfo(InputFile, OutInfo);
}


bool FExrImgMediaReader::ReadFrame(const FString& ImagePath, FImgMediaFrame& OutFrame)
{
	FRgbaInputFile InputFile(ImagePath, 2);
	
	if (!GetInfo(InputFile, OutFrame.Info))
	{
		return false;
	}

	const FIntPoint& Dim = OutFrame.Info.Dim;

	if (Dim.GetMin() <= 0)
	{
		return false;
	}

	void* Buffer = FMemory::Malloc(Dim.X * Dim.Y * sizeof(uint16) * 4);
	
	InputFile.SetFrameBuffer(Buffer, Dim);
	InputFile.ReadPixels(0, Dim.Y - 1);

	OutFrame.Data = MakeShareable(Buffer, [](void* ObjectToDelete) { FMemory::Free(ObjectToDelete); });
	OutFrame.Format = EMediaTextureSampleFormat::FloatRGBA;
	OutFrame.Stride = Dim.X * sizeof(unsigned short) * 4;

	return true;
}


/* FExrImgMediaReader implementation
 *****************************************************************************/

bool FExrImgMediaReader::GetInfo(FRgbaInputFile& InputFile, FImgMediaFrameInfo& OutInfo) const
{
	OutInfo.CompressionName = InputFile.GetCompressionName();
	OutInfo.Dim = InputFile.GetDataWindow();
	OutInfo.FormatName = TEXT("EXR");
	OutInfo.Fps = (float)InputFile.GetFramesPerSecond(IMGMEDIA_DEFAULT_FPS);
	OutInfo.Srgb = false;
	OutInfo.UncompressedSize = InputFile.GetUncompressedSize();

	return (OutInfo.UncompressedSize > 0) && (OutInfo.Dim.GetMin() > 0);
}


#endif //IMGMEDIA_EXR_SUPPORTED_PLATFORM
