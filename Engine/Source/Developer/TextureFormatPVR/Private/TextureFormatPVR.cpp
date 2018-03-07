// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Misc/ConfigCacheIni.h"
#include "ImageCore.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/ITextureFormat.h"
#include "Interfaces/ITextureFormatModule.h"
#include "TextureCompressorModule.h"
#include "PixelFormat.h"


#define MAX_QUALITY 4


DEFINE_LOG_CATEGORY_STATIC(LogTextureFormatPVR, Log, All);

/**
 * Macro trickery for supported format names.
 */
#define ENUM_SUPPORTED_FORMATS(op) \
	op(PVRTC2) \
	op(PVRTC4) \
	op(PVRTCN) \
	op(AutoPVRTC)

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

// PVR file header format
#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack(push, 4)
#endif
struct FPVRHeader
{
	uint32 Version;
	uint32 Flags;
	uint64 PixelFormat ;
	uint32 ColorSpace;
	uint32 ChannelType;
	uint32 Height;
	uint32 Width;
	uint32 Depth;
	uint32 NumSurfaces;
	uint32 NumFaces;
	uint32 NumMipmaps;
	uint32 MetaDataSize;
};
#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

/**
 * Converts a power-of-two image to a square format (ex: 256x512 -> 512x512). Be wary of memory waste when too many texture are not square.
 *
 * @param Image The image to be converted to a square
 * @return true if the image was converted successfully, else false
 */
static bool SquarifyImage(FImage& Image, uint32 MinSquareSize)
{
	// Early out
	if (Image.SizeX == Image.SizeY && Image.SizeX >= int32(MinSquareSize))
	{
		return true;
	}

	// Figure out the squarified size
	uint32 SquareSize = FMath::Max(Image.SizeX, Image.SizeY);
	if(SquareSize < MinSquareSize)
	{
		SquareSize = MinSquareSize;
	}
	
	// Calculate how many times to duplicate each row of column
	uint32 MultX = SquareSize / Image.SizeX;
	uint32 MultY = SquareSize / Image.SizeY;

	// Only give memory overhead warning if we're actually going to use a larger image
	// Small mips that have to be upscaled for compression only save the smaller mip for use
	if(MultX == 1 || MultY == 1)
	{
		float FOverhead = float(FMath::Min(Image.SizeX, Image.SizeY)) / float(SquareSize);
		int32 POverhead = FMath::RoundToInt(100.0f - (FOverhead * 100.0f));
		UE_LOG(LogTextureFormatPVR, Warning, TEXT("Expanding mip (%d,%d) to (%d, %d). Memory overhead: ~%d%%"),
			Image.SizeX, Image.SizeY, SquareSize, SquareSize, POverhead);
	}
	else if (MultX != MultY)
	{
		float FOverhead = float(FMath::Min(Image.SizeX, Image.SizeY)) / float(FMath::Max(Image.SizeX, Image.SizeY));
		int32 POverhead = FMath::RoundToInt(100.0f - (FOverhead * 100.0f));
		UE_LOG(LogTextureFormatPVR, Warning, TEXT("Expanding mip (%d,%d) to (%d, %d). Memory overhead: ~%d%%"),
			Image.SizeX, Image.SizeY, FMath::Max(Image.SizeX, Image.SizeY), FMath::Max(Image.SizeX, Image.SizeY), POverhead);
	}
	
	// Allocate room to fill out into
	TArray<uint32> SquareRawData;
	SquareRawData.SetNumUninitialized(SquareSize * SquareSize * Image.NumSlices);
	
	int32 SourceSliceSize = Image.SizeX * Image.SizeY;
	int32 DestSliceSize = SquareSize * SquareSize;
	for ( int32 SliceIndex=0; SliceIndex < Image.NumSlices; ++SliceIndex )
	{
		uint32* RectData = ((uint32*)Image.RawData.GetData()) + SliceIndex * SourceSliceSize;
		uint32* SquareData = ((uint32*)SquareRawData.GetData()) + SliceIndex * DestSliceSize;
		for ( int32 Y = 0; Y < Image.SizeY; ++Y )
		{
			for ( int32 X = 0; X < Image.SizeX; ++X )
			{
				uint32 SourceColor = *(RectData + Y * Image.SizeX + X);
			
				for ( uint32 YDup = 0; YDup < MultY; ++YDup )
				{
					for ( uint32 XDup = 0; XDup < MultX; ++XDup )
					{
						uint32* DestColor = SquareData + ((Y * MultY + YDup) * SquareSize + (X * MultX + XDup));
						*DestColor = SourceColor;
					}
				}
			}
		}
	}

	// Put the new image data into the existing Image (copying from uint32 array to uint8 array)
	Image.RawData.Empty(SquareSize * SquareSize * Image.NumSlices * sizeof(uint32));
	Image.RawData.SetNumUninitialized(SquareSize * SquareSize * Image.NumSlices * sizeof(uint32));
	uint32* FinalData = (uint32*)Image.RawData.GetData();
	FMemory::Memcpy(Image.RawData.GetData(), SquareRawData.GetData(), SquareSize * SquareSize * Image.NumSlices * sizeof(uint32));

	Image.SizeX = SquareSize;
	Image.SizeY = SquareSize;

	return true;
}

static void DeriveNormalZ(FImage& Image)
{
	int32 SliceSize = Image.SizeX * Image.SizeY;
	for ( int32 SliceIndex=0; SliceIndex < Image.NumSlices; ++SliceIndex )
	{
		FColor* RectData = (FColor*)Image.RawData.GetData() + SliceIndex * SliceSize;
		for(int32 Y = 0; Y < Image.SizeY; ++Y)
		{
			for(int32 X = 0; X < Image.SizeX; ++X)
			{
				FColor& SourceColor = *(RectData + Y * Image.SizeX + X);

				const float NormalX = SourceColor.R / 255.0f * 2 - 1;
				const float NormalY = SourceColor.G / 255.0f * 2 - 1;
				const float NormalZ = FMath::Sqrt(FMath::Clamp<float>(1 - (NormalX * NormalX + NormalY * NormalY), 0, 1));
				SourceColor.B = FMath::TruncToInt((NormalZ + 1) / 2.0f * 255.0f);
			}
		}
	}
}

/**
 * Checks if the passed image is a proper power-of-2 image
 *
 * @param Image The Image to evaluate
 * @return true if the image is a power of 2, else false
 */
static bool ValidateImagePower(const FImage& Image)
{
	// Image must already have power of 2 dimensions
	bool bDimensionsValid = true;
	int DimX = Image.SizeX;
	int DimY = Image.SizeY;
	while(DimX >= 2)
	{
		if(DimX % 2 == 1)
		{
			bDimensionsValid = false;
			break;
		}
		DimX /= 2;
	}
	while(DimY >= 2 && bDimensionsValid)
	{
		if(DimY % 2 == 1)
		{
			bDimensionsValid = false;
			break;
		}
		DimY /= 2;
	}
	
	if(!bDimensionsValid)
	{
		return false;
	}

	return true;
}

/**
 * Fills the output structure with the original uncompressed mip information
 *
 * @param InImage The mip to compress
 * @param OutCompressImage The output image (uncompressed in this case)
 */
static void UseOriginal(const FImage& InImage, FCompressedImage2D& OutCompressedImage, EPixelFormat CompressedPixelFormat, EGammaSpace GammaSpace)
{   
	// Get Raw Data
	FImage Image;
	InImage.CopyTo(Image, ERawImageFormat::BGRA8, GammaSpace);

	// Fill out the output information
	OutCompressedImage.SizeX = Image.SizeX;
	OutCompressedImage.SizeY = Image.SizeY;
	OutCompressedImage.PixelFormat = CompressedPixelFormat;
	
	// Output Data
	OutCompressedImage.RawData.SetNumUninitialized(Image.SizeX * Image.SizeY * 4);
	void* MipData = (void*)Image.RawData.GetData();
	FMemory::Memcpy(MipData, Image.RawData.GetData(), Image.SizeX * Image.SizeY * 4);
}

static int32 GetDefaultCompressionValue()
{
	// start at default quality, then lookup in .ini file
	int32 CompressionModeValue = 0;
	GConfig->GetInt(TEXT("/Script/UnrealEd.CookerSettings"), TEXT("DefaultPVRTCQuality"), CompressionModeValue, GEngineIni);

	FParse::Value(FCommandLine::Get(), TEXT("-pvrtcquality="), CompressionModeValue);
	CompressionModeValue = FMath::Min<uint32>(CompressionModeValue, MAX_QUALITY);
	
	return CompressionModeValue;
}

static FString GetPVRTCQualityString(int32 OverrideSizeValue = -1)
{
	// convert to a string
	FString CompressionMode;
	switch (OverrideSizeValue >= 0 ? OverrideSizeValue : GetDefaultCompressionValue())
	{
		case 0:	CompressionMode = TEXT("fastest"); break;
		case 1:	CompressionMode = TEXT("fast"); break;
		case 2:	CompressionMode = TEXT("normal"); break;
		case 3:	CompressionMode = TEXT("high"); break;
		case 4:	CompressionMode = TEXT("best"); break;
		default: UE_LOG(LogTemp, Fatal, TEXT("Max quality higher than expected"));
	}

	return CompressionMode;
}

static uint16 GetPVRTCQualityForVersion(int32 OverrideSizeValue = -1)
{
	// top 3 bits for compression value
	return (OverrideSizeValue >= 0 ? OverrideSizeValue : GetDefaultCompressionValue()) << 13;
}

/**
 * PVR texture format handler.
 */
class FTextureFormatPVR : public ITextureFormat
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
		return 7 + GetPVRTCQualityForVersion(BuildSettings ? BuildSettings->CompressionQuality : -1);
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
		FTextureFormatCompressorCaps RetCaps;
		// PVR compressor is limited to <=4096 in any direction.
		RetCaps.MaxTextureDimension = 4096;
		return RetCaps;
	}

	virtual bool CompressImage(
		const FImage& InImage,
		const struct FTextureBuildSettings& BuildSettings,
		bool bImageHasAlphaChannel,
		FCompressedImage2D& OutCompressedImage
		) const override
	{
		// Get Raw Image Data from passed in FImage
		FImage Image;
		InImage.CopyTo(Image, ERawImageFormat::BGRA8, BuildSettings.GetGammaSpace());
		
		// Get the compressed format
		EPixelFormat CompressedPixelFormat = PF_Unknown;
		if (BuildSettings.TextureFormatName == GTextureFormatNamePVRTC2)
		{
			CompressedPixelFormat = PF_PVRTC2;
		}
		else if (BuildSettings.TextureFormatName == GTextureFormatNamePVRTC4 || BuildSettings.TextureFormatName == GTextureFormatNamePVRTCN)
		{
			CompressedPixelFormat = PF_PVRTC4;
		}
		else if (BuildSettings.TextureFormatName == GTextureFormatNameAutoPVRTC)
		{
			CompressedPixelFormat = bImageHasAlphaChannel ? PF_PVRTC4 : PF_PVRTC2;
		}

		// Verify Power of 2
		if ( !ValidateImagePower(Image) )
		{
			UE_LOG(LogTextureFormatPVR, Warning, TEXT("Mip size (%d,%d) does not have power-of-two dimensions and cannot be compressed to PVRTC%d"), 
				Image.SizeX, Image.SizeY, CompressedPixelFormat == PF_PVRTC2 ? 2 : 4);
			return false;
		}

		// Squarify image
		int32 FinalSquareSize = FGenericPlatformMath::Max(Image.SizeX, Image.SizeY);
		SquarifyImage(Image, (CompressedPixelFormat == PF_PVRTC2) ? 16 : 8);
		check(Image.SizeX == Image.SizeY);

		if ( BuildSettings.TextureFormatName == GTextureFormatNamePVRTCN )
		{
			// Derive Z from X and Y to be consistent with BC5 normal maps used on PC (toss the texture's actual Z)
			DeriveNormalZ(Image);
		}

		bool bCompressionSucceeded = true;
		int32 SliceSize = Image.SizeX * Image.SizeY;
		for (int32 SliceIndex = 0; SliceIndex < Image.NumSlices && bCompressionSucceeded; ++SliceIndex)
		{
			TArray<uint8> CompressedSliceData;
			bCompressionSucceeded = CompressImageUsingPVRTexTool(
				Image.AsBGRA8() + SliceIndex * SliceSize,
				CompressedPixelFormat,
				Image.SizeX,
				Image.SizeY,
				Image.IsGammaCorrected(),
				FinalSquareSize,
				CompressedSliceData,
				BuildSettings
				);
			OutCompressedImage.RawData.Append(CompressedSliceData);
		}

		if ( bCompressionSucceeded )
		{
			OutCompressedImage.SizeX = FinalSquareSize;
			OutCompressedImage.SizeY = FinalSquareSize;
			OutCompressedImage.PixelFormat = CompressedPixelFormat;
		}

		// Return success status
		return bCompressionSucceeded;
	}

	static bool CompressImageUsingPVRTexTool( void* SourceData, EPixelFormat PixelFormat, int32 SizeX, int32 SizeY, bool bSRGB, int32 FinalSquareSize, TArray<uint8>& OutCompressedData, const struct FTextureBuildSettings& BuildSettings )
	{
		// Figure out whether to use 2 bits or 4 bits per pixel (PVRTC2/PVRTC4)
		bool bIsPVRTC2 = (PixelFormat == PF_PVRTC2);

		const int32 BlockSizeX = bIsPVRTC2 ? 8 : 4;		// PVRTC2 uses 8x4 blocks, PVRTC4 uses 4x4 blocks
		const int32 BlockSizeY = 4;
		const int32 BlockBytes = 8;						// Both PVRTC2 and PVRTC4 are 8 bytes per block
		const uint32 DestSizeX = FinalSquareSize;
		const uint32 DestSizeY = FinalSquareSize;

		// min 2x2 blocks per mip
		const uint32 DestBlocksX = FGenericPlatformMath::Max<uint32>(DestSizeX / BlockSizeX, 2);
		const uint32 DestBlocksY = FGenericPlatformMath::Max<uint32>(DestSizeY / BlockSizeY, 2);
		const uint32 DestNumBytes = DestBlocksX * DestBlocksY * BlockBytes;

		// If using an image that's too small, compressor needs to generate mips for us with an upscaled image
		check(SizeX == SizeY);
		const int32 SourceSquareSize = SizeX;
		bool bGenerateMips = (FinalSquareSize < SourceSquareSize) ? true : false;

		// Allocate space to store compressed data.
		OutCompressedData.Empty(DestNumBytes);
		OutCompressedData.AddUninitialized(DestNumBytes);
		void* MipData = OutCompressedData.GetData();

		// Write SourceData into PVR file on disk

		// Init Header
		FPVRHeader PVRHeader;
		PVRHeader.Version = 0x03525650; // endianess does not match
		PVRHeader.Flags = 0;
		PVRHeader.PixelFormat = 0x0808080861726762; // Format of the UTexture 8bpp and BGRA ordered
		PVRHeader.ColorSpace = 0; // Setting to 1 indicates SRGB, but causes PVRTexTool to unpack to linear. We want the image to remain in sRGB space as we do the conversion in the shader.
		PVRHeader.ChannelType = 0;
		PVRHeader.Height = SizeY;
		PVRHeader.Width = SizeX;
		PVRHeader.Depth = 1;
		PVRHeader.NumSurfaces = 1;
		PVRHeader.NumFaces = 1;
		PVRHeader.NumMipmaps = 1;
		PVRHeader.MetaDataSize = 0;

		// Get file paths for intermediates. Unique path to avoid filename collision
		FGuid Guid;
		FPlatformMisc::CreateGuid(Guid);
		FString InputFilePath = FString::Printf(TEXT("Cache/%x%x%x%xRGBToPVRIn.pvr"), Guid.A, Guid.B, Guid.C, Guid.D);
		InputFilePath = FPaths::ProjectIntermediateDir() + InputFilePath;
		FString OutputFilePath = FString::Printf(TEXT("Cache/%x%x%x%xRGBToPVROut.pvr"), Guid.A, Guid.B, Guid.C, Guid.D);
		OutputFilePath = FPaths::ProjectIntermediateDir() + OutputFilePath;

		FArchive* PVRFile = NULL;
		while(!PVRFile)
		{
			PVRFile = IFileManager::Get().CreateFileWriter(*InputFilePath);	// Occasionally returns NULL due to error code ERROR_SHARING_VIOLATION
			FPlatformProcess::Sleep(0.01f);									// ... no choice but to wait for the file to become free to access
		}

		// Write out header
		uint32 HeaderSize = sizeof( PVRHeader );
		check(HeaderSize==52);
		PVRFile->Serialize(&PVRHeader, HeaderSize);

		// Write out uncompressed data
		PVRFile->Serialize(SourceData, SizeX * SizeY * sizeof(uint32));

		// Finished writing file
		PVRFile->Close();
		delete PVRFile;

		// Compress PVR file to PVRTC
		FString CompressionMode = GetPVRTCQualityString(BuildSettings.CompressionQuality);

		// Use PowerVR's new CLI tool commandline
		FString Params = FString::Printf(TEXT("-i \"%s\" -o \"%s\" %s -legacypvr -q pvrtc%s -f PVRTC1_%d"),
			*InputFilePath, *OutputFilePath,
			bGenerateMips ? TEXT("-m") : TEXT(""),
			*CompressionMode,
			bIsPVRTC2 ? 2 : 4);
#if PLATFORM_MAC
		FString CompressorPath(FPaths::EngineDir() + TEXT("Binaries/ThirdParty/ImgTec/PVRTexToolCLI"));
#elif PLATFORM_LINUX
		FString CompressorPath(FPaths::EngineDir() + TEXT("Binaries/ThirdParty/ImgTec/PVRTexToolCLI.lnx"));
#elif PLATFORM_WINDOWS
		FString CompressorPath(FPaths::EngineDir() + TEXT("Binaries/ThirdParty/ImgTec/PVRTexToolCLI.exe"));
#else
#error Unsupported platform
#endif
		UE_LOG(LogTemp, Log, TEXT("Running texturetool with '%s'"), *Params);

		// Give a debug message about the process
		if (IsRunningCommandlet())
		{
			//UE_LOG(LogTextureFormatPVR, Display, TEXT("Compressing mip (%dx%d) to PVRTC%d for mobile devices..."), ImageSizeX, ImageSizeY, bIsPVRTC2 ? 2 : 4);
		}

		// Start Compressor
		FProcHandle Proc = FPlatformProcess::CreateProc(*CompressorPath, *Params, true, false, false, NULL, -1, NULL, NULL);
		bool bConversionWasSuccessful = true;

		// Failed to start the compressor process?
		if (!Proc.IsValid())
		{
			UE_LOG(LogTextureFormatPVR, Error, TEXT("Failed to start PVR compressor tool. (Path:%s)"), *CompressorPath);
			bConversionWasSuccessful = false;
		}

		if ( bConversionWasSuccessful )
		{
			// Wait for the process to complete
			int ReturnCode;
			while (!FPlatformProcess::GetProcReturnCode(Proc, &ReturnCode))
			{
				FPlatformProcess::Sleep(0.01f);
			}
			FPlatformProcess::CloseProc(Proc);

			// Did it fail?
			if ( ReturnCode != 0 )
			{
				UE_LOG(LogTextureFormatPVR, Error, TEXT("PVR tool Failed with Return Code %d Mip Size (%d,%d)"), ReturnCode, SizeX, SizeY);
				bConversionWasSuccessful = false;
			}
		}

		// Open compressed file and put the data in OutCompressedImage
		if ( bConversionWasSuccessful )
		{
			// Calculate which mip to pull from compressed image
			int32 MipLevel = 0;
			{
				int32 i = SizeX;
				while ( i > FinalSquareSize )
				{
					i /= 2;
					++MipLevel;
				}
			}

			// Get Raw File Data
			TArray<uint8> PVRData;
			FFileHelper::LoadFileToArray(PVRData, *OutputFilePath);

			// Process It
			FPVRHeader* Header = (FPVRHeader*)PVRData.GetData();

			// Calculate the offset to get to the mip data
			int FileOffset = HeaderSize;
			for(int32 i = 0; i < MipLevel; ++i)
			{
				// Get the mip size for each image before the mip we want
				uint32 LocalMipSizeX = FGenericPlatformMath::Max<uint32>(SizeX >> i, 1);
				uint32 LocalMipSizeY = LocalMipSizeX;
				uint32 LocalBlocksX = FGenericPlatformMath::Max<uint32>(LocalMipSizeX / BlockSizeX, 2);
				uint32 LocalBlocksY = FGenericPlatformMath::Max<uint32>(LocalMipSizeY / BlockSizeY, 2);
				uint32 LocalMipSize = LocalBlocksX * LocalBlocksY * BlockBytes;

				// Add that mip's size to the offset
				FileOffset += LocalMipSize;
			}

			// Copy compressed data
			FMemory::Memcpy(MipData, PVRData.GetData() + FileOffset, DestNumBytes);
		}

		// Delete intermediate files
		IFileManager::Get().Delete(*InputFilePath);
		IFileManager::Get().Delete(*OutputFilePath);

		return bConversionWasSuccessful;
	}
};

/**
 * Module for PVR texture compression.
 */
static ITextureFormat* Singleton = NULL;

class FTextureFormatPVRModule : public ITextureFormatModule
{
public:
	virtual ~FTextureFormatPVRModule()
	{
		delete Singleton;
		Singleton = NULL;
	}
	virtual ITextureFormat* GetTextureFormat()
	{
		if (!Singleton)
		{
			Singleton = new FTextureFormatPVR();
		}
		return Singleton;
	}
};

IMPLEMENT_MODULE(FTextureFormatPVRModule, TextureFormatPVR);
