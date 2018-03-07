// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OpenExrWrapper.h"

#include "Containers/UnrealString.h"
#include "Modules/ModuleManager.h"

THIRD_PARTY_INCLUDES_START
	#include "ImathBox.h"
	#include "ImfHeader.h"
	#include "ImfRgbaFile.h"
	#include "ImfCompressionAttribute.h"
	#include "ImfStandardAttributes.h"
THIRD_PARTY_INCLUDES_END


/* FOpenExr
 *****************************************************************************/

void FOpenExr::SetGlobalThreadCount(uint16 ThreadCount)
{
	Imf::setGlobalThreadCount(ThreadCount);
}


/* FRgbaInputFile
 *****************************************************************************/

FRgbaInputFile::FRgbaInputFile(const FString& FilePath)
{
	InputFile = new Imf::RgbaInputFile(TCHAR_TO_ANSI(*FilePath));
}


FRgbaInputFile::FRgbaInputFile(const FString& FilePath, uint16 ThreadCount)
{
	InputFile = new Imf::RgbaInputFile(TCHAR_TO_ANSI(*FilePath), ThreadCount);
}


FRgbaInputFile::~FRgbaInputFile()
{
	delete (Imf::RgbaInputFile*)InputFile;
}


const TCHAR* FRgbaInputFile::GetCompressionName() const
{
	auto CompressionAttribute = ((Imf::RgbaInputFile*)InputFile)->header().findTypedAttribute<Imf::CompressionAttribute>("compression");

	if (CompressionAttribute == nullptr)
	{
		return TEXT("");
	}

	Imf::Compression Compression = CompressionAttribute->value();

	switch (Compression)
	{
	case Imf::NO_COMPRESSION:
		return TEXT("Uncompressed");

	case Imf::RLE_COMPRESSION:
		return TEXT("RLE");

	case Imf::ZIPS_COMPRESSION:
		return TEXT("ZIPS");

	case Imf::ZIP_COMPRESSION:
		return TEXT("ZIP");

	case Imf::PIZ_COMPRESSION:
		return TEXT("PIZ");

	case Imf::PXR24_COMPRESSION:
		return TEXT("PXR24");

	case Imf::B44_COMPRESSION:
		return TEXT("B44");

	case Imf::B44A_COMPRESSION:
		return TEXT("B44A");

	default:
		return TEXT("Unknown");
	}
}


FIntPoint FRgbaInputFile::GetDataWindow() const
{
	Imath::Box2i Win = ((Imf::RgbaInputFile*)InputFile)->dataWindow();

	return FIntPoint(
		Win.max.x - Win.min.x + 1,
		Win.max.y - Win.min.y + 1
	);
}


double FRgbaInputFile::GetFramesPerSecond(double DefaultValue) const
{
	auto Attribute = ((Imf::RgbaInputFile*)InputFile)->header().findTypedAttribute<Imf::RationalAttribute>("framesPerSecond");

	if (Attribute == nullptr)
	{
		return DefaultValue;
	}

	return Attribute->value();
}


int32 FRgbaInputFile::GetUncompressedSize() const
{
	const int32 NumChannels = 4;
	const int32 ChannelSize = sizeof(int16);
	const FIntPoint Window = GetDataWindow();

	return (Window.X * Window.Y * NumChannels * ChannelSize);
}


bool FRgbaInputFile::IsComplete() const
{
	return ((Imf::RgbaInputFile*)InputFile)->isComplete();
}


void FRgbaInputFile::ReadPixels(int32 StartY, int32 EndY)
{
	Imath::Box2i Win = ((Imf::RgbaInputFile*)InputFile)->dataWindow();
	((Imf::RgbaInputFile*)InputFile)->readPixels(Win.min.y, Win.max.y);
}


void FRgbaInputFile::SetFrameBuffer(void* Buffer, const FIntPoint& BufferDim)
{
	Imath::Box2i Win = ((Imf::RgbaInputFile*)InputFile)->dataWindow();
	((Imf::RgbaInputFile*)InputFile)->setFrameBuffer((Imf::Rgba*)Buffer - Win.min.x - Win.min.y * BufferDim.X, 1, BufferDim.X);
}


IMPLEMENT_MODULE(FDefaultModuleImpl, OpenExrWrapper);
