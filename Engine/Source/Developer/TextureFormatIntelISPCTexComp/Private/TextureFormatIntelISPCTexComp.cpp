// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Containers/IndirectArray.h"
#include "Stats/Stats.h"
#include "Async/AsyncWork.h"
#include "Misc/ConfigCacheIni.h"
#include "ImageCore.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/ITextureFormat.h"
#include "Interfaces/ITextureFormatModule.h"
#include "TextureCompressorModule.h"
#include "PixelFormat.h"

#include "ispc_texcomp.h"

DEFINE_LOG_CATEGORY_STATIC(LogTextureFormatIntelISPCTexComp, Log, All);

// increment this if you change anything that will affect compression in this file, including FORCED_NORMAL_MAP_COMPRESSION_SIZE_VALUE
#define BASE_ISPC_DX11_FORMAT_VERSION 3

// For debugging intermediate image results by saving them out as files.
#define DEBUG_SAVE_INTERMEDIATE_IMAGES 0

/**
 * Macro trickery for supported format names.
 */
#define ENUM_SUPPORTED_FORMATS(op) \
	op(BC6H) \
	op(BC7)

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

#define ENUM_ASTC_FORMATS(op) \
	op(ASTC_RGB) \
	op(ASTC_RGBA) \
	op(ASTC_RGBAuto) \
	op(ASTC_NormalAG) \
	op(ASTC_NormalRG)

	#define DECL_FORMAT_NAME(FormatName) static FName GTextureFormatName##FormatName = FName(TEXT(#FormatName));
	ENUM_ASTC_FORMATS(DECL_FORMAT_NAME);
	#undef DECL_FORMAT_NAME
#undef ENUM_ASTC_FORMATS

// BC6H, BC7, ASTC all have 16-byte block size
#define BLOCK_SIZE_IN_BYTES 16


	// Bitmap compression types.
enum EBitmapCompression
{
	BCBI_RGB = 0,
	BCBI_RLE8 = 1,
	BCBI_RLE4 = 2,
	BCBI_BITFIELDS = 3,
};

// .BMP file header.
#pragma pack(push,1)
struct FBitmapFileHeader
{
	uint16 bfType;
	uint32 bfSize;
	uint16 bfReserved1;
	uint16 bfReserved2;
	uint32 bfOffBits;
	friend FArchive& operator<<(FArchive& Ar, FBitmapFileHeader& H)
	{
		Ar << H.bfType << H.bfSize << H.bfReserved1 << H.bfReserved2 << H.bfOffBits;
		return Ar;
	}
};
#pragma pack(pop)

// .BMP subheader.
#pragma pack(push,1)
struct FBitmapInfoHeader
{
	uint32 biSize;
	uint32 biWidth;
	int32 biHeight;
	uint16 biPlanes;
	uint16 biBitCount;
	uint32 biCompression;
	uint32 biSizeImage;
	uint32 biXPelsPerMeter;
	uint32 biYPelsPerMeter;
	uint32 biClrUsed;
	uint32 biClrImportant;
	friend FArchive& operator<<(FArchive& Ar, FBitmapInfoHeader& H)
	{
		Ar << H.biSize << H.biWidth << H.biHeight;
		Ar << H.biPlanes << H.biBitCount;
		Ar << H.biCompression << H.biSizeImage;
		Ar << H.biXPelsPerMeter << H.biYPelsPerMeter;
		Ar << H.biClrUsed << H.biClrImportant;
		return Ar;
	}
};
#pragma pack(pop)


void SaveImageAsBMP( FArchive& Ar, const uint8* RawData, int SourceBytesPerPixel, int SizeX, int SizeY )
{
	FBitmapFileHeader bmf;
	FBitmapInfoHeader bmhdr;

	// File header.
	bmf.bfType = 'B' + (256 * (int32)'M');
	bmf.bfReserved1 = 0;
	bmf.bfReserved2 = 0;
	int32 biSizeImage = SizeX * SizeY * 3;
	bmf.bfOffBits = sizeof(FBitmapFileHeader) + sizeof(FBitmapInfoHeader);
	bmhdr.biBitCount = 24;

	bmf.bfSize = bmf.bfOffBits + biSizeImage;
	Ar << bmf;

	// Info header.
	bmhdr.biSize = sizeof(FBitmapInfoHeader);
	bmhdr.biWidth = SizeX;
	bmhdr.biHeight = SizeY;
	bmhdr.biPlanes = 1;
	bmhdr.biCompression = BCBI_RGB;
	bmhdr.biSizeImage = biSizeImage;
	bmhdr.biXPelsPerMeter = 0;
	bmhdr.biYPelsPerMeter = 0;
	bmhdr.biClrUsed = 0;
	bmhdr.biClrImportant = 0;
	Ar << bmhdr;

	bool bIsRGBA16 = (SourceBytesPerPixel == 8);

	//NOTE: Each row must be 4-byte aligned in a BMP.
	int PaddingX = Align(SizeX * 3, 4) - SizeX * 3;

	// Upside-down scanlines.
	for (int32 i = SizeY - 1; i >= 0; i--)
	{
		const uint8* ScreenPtr = &RawData[i*SizeX*SourceBytesPerPixel];
		for (int32 j = SizeX; j > 0; j--)
		{
			uint8 R, G, B;
			if (bIsRGBA16)
			{
				R = ScreenPtr[1];
				G = ScreenPtr[3];
				B = ScreenPtr[5];
				ScreenPtr += 8;
			}
			else
			{
				R = ScreenPtr[0];
				G = ScreenPtr[1];
				B = ScreenPtr[2];
				ScreenPtr += 4;
			}
			Ar << R;
			Ar << G;
			Ar << B;
		}
		for (int32 j = 0; j < PaddingX; ++j)
		{
			int8 PadByte = 0;
			Ar << PadByte;
		}
	}
}

#define MAGIC_FILE_CONSTANT 0x5CA1AB13

// little endian
#pragma pack(push,1)
struct astc_header
{
	uint8_t magic[4];
	uint8_t blockdim_x;
	uint8_t blockdim_y;
	uint8_t blockdim_z;
	uint8_t xsize[3];
	uint8_t ysize[3];			// x-size, y-size and z-size are given in texels;
	uint8_t zsize[3];			// block count is inferred
};
#pragma pack(pop)

void SaveImageAsASTC(FArchive& Ar, uint8* RawData, int SizeX, int SizeY, int block_width, int block_height)
{
	astc_header file_header;

	uint32_t magic = MAGIC_FILE_CONSTANT;
	FMemory::Memcpy(file_header.magic, &magic, 4);
	file_header.blockdim_x = block_width;
	file_header.blockdim_y = block_height;
	file_header.blockdim_z = 1;

	int32 xsize = SizeX;
	int32 ysize = SizeY;
	int32 zsize = 1;

	FMemory::Memcpy(file_header.xsize, &xsize, 3);
	FMemory::Memcpy(file_header.ysize, &ysize, 3);
	FMemory::Memcpy(file_header.zsize, &zsize, 3);

	Ar.Serialize(&file_header, sizeof(file_header));

	size_t height_in_blocks = (SizeY + block_height - 1) / block_height;
	size_t width_in_blocks = (SizeX + block_width - 1) / block_width;
	int stride = width_in_blocks * BLOCK_SIZE_IN_BYTES;
	Ar.Serialize(RawData, height_in_blocks * stride);
}

struct FMultithreadSettings
{
	int iScansPerTask;
	int iNumTasks;
};

template <typename EncoderSettingsType>
struct FMultithreadedCompression
{
	typedef void(*CompressFunction)(EncoderSettingsType* pEncSettings, FImage* pInImage, FCompressedImage2D* pOutImage, int yStart, int yEnd, int SliceIndex);

	static void Compress(FMultithreadSettings &MultithreadSettings, EncoderSettingsType &EncoderSettings, FImage &Image, FCompressedImage2D &OutCompressedImage, CompressFunction FunctionCallback, bool bUseTasks)
	{
		if (bUseTasks)
		{
			class FIntelCompressWorker : public FNonAbandonableTask
			{
			public:
				FIntelCompressWorker(EncoderSettingsType* pEncSettings, FImage* pInImage, FCompressedImage2D* pOutImage, int yStart, int yEnd, int SliceIndex, CompressFunction InFunctionCallback)
				:	mpEncSettings(pEncSettings)
				,	mpInImage(pInImage)
				,	mpOutImage(pOutImage)
				,	mYStart(yStart)
				,	mYEnd(yEnd)
				,	mSliceIndex(SliceIndex)
				,	mCallback(InFunctionCallback)
				{
				}

				void DoWork()
				{
					mCallback(mpEncSettings, mpInImage, mpOutImage, mYStart, mYEnd, mSliceIndex);
				}

				FORCEINLINE TStatId GetStatId() const
				{
					RETURN_QUICK_DECLARE_CYCLE_STAT(FIntelCompressWorker, STATGROUP_ThreadPoolAsyncTasks);
				}

				EncoderSettingsType*	mpEncSettings;
				FImage*					mpInImage;
				FCompressedImage2D*		mpOutImage;
				int						mYStart;
				int						mYEnd;
				int						mSliceIndex;
				CompressFunction		mCallback;
			};
			typedef FAsyncTask<FIntelCompressWorker> FIntelCompressTask;

			// One less task because we'll do the final + non multiple of 4 inside this task
			TIndirectArray<FIntelCompressTask> CompressionTasks;
			const int NumStasksPerSlice = MultithreadSettings.iNumTasks + 1;
			CompressionTasks.Reserve(NumStasksPerSlice * Image.NumSlices - 1);
			for (int SliceIndex = 0; SliceIndex < Image.NumSlices; ++SliceIndex)
			{
				for (int iTask = 0; iTask < NumStasksPerSlice; ++iTask)
				{
					// Create a new task unless it's the last task in the last slice (that one will run on current thread, after these threads have been started)
					if (SliceIndex < (Image.NumSlices - 1) || iTask < (NumStasksPerSlice - 1))
					{
						auto* AsyncTask = new(CompressionTasks) FIntelCompressTask(&EncoderSettings, &Image, &OutCompressedImage, iTask * MultithreadSettings.iScansPerTask, (iTask + 1) * MultithreadSettings.iScansPerTask, SliceIndex, FunctionCallback);
						AsyncTask->StartBackgroundTask();
					}
				}
			}
			FunctionCallback(&EncoderSettings, &Image, &OutCompressedImage, MultithreadSettings.iScansPerTask * MultithreadSettings.iNumTasks, Image.SizeY, Image.NumSlices - 1);

			// Wait for all tasks to complete
			for (int32 TaskIndex = 0; TaskIndex < CompressionTasks.Num(); ++TaskIndex)
			{
				CompressionTasks[TaskIndex].EnsureCompletion();
			}
		}
		else
		{
			for (int SliceIndex = 0; SliceIndex < Image.NumSlices; ++SliceIndex)
			{
				FunctionCallback(&EncoderSettings, &Image, &OutCompressedImage, 0, Image.SizeY, SliceIndex);
			}
		}
	}
};

/**
 * BC6H Compression function
 */
static void IntelBC6HCompressScans(bc6h_enc_settings* pEncSettings, FImage* pInImage, FCompressedImage2D* pOutImage, int yStart, int yEnd, int SliceIndex)
{
	check(pInImage->Format == ERawImageFormat::RGBA16F);
	check((yStart % 4) == 0);
	check((pInImage->SizeX % 4) == 0);
	check((yStart >= 0) && (yStart <= pInImage->SizeY));
	check((yEnd   >= 0) && (yEnd   <= pInImage->SizeY));

	const int InStride = pInImage->SizeX * 8;
	const int OutStride = pInImage->SizeX / 4 * BLOCK_SIZE_IN_BYTES;
	const int InSliceSize = pInImage->SizeY * InStride;
	const int OutSliceSize = pInImage->SizeY / 4 * OutStride;

	uint8* pInTexels = reinterpret_cast<uint8*>(&pInImage->RawData[0]) + InSliceSize * SliceIndex;
	uint8* pOutTexels = reinterpret_cast<uint8*>(&pOutImage->RawData[0]) + OutSliceSize * SliceIndex;

	rgba_surface insurface;
	insurface.ptr		= pInTexels + (yStart * InStride);
	insurface.width		= pInImage->SizeX;
	insurface.height	= yEnd - yStart;
	insurface.stride	= pInImage->SizeX * 8;

	pOutTexels += yStart / 4 * OutStride;
	CompressBlocksBC6H(&insurface, pOutTexels, pEncSettings);
}

/**
 * BC7 Compression function
 */
static void IntelBC7CompressScans(bc7_enc_settings* pEncSettings, FImage* pInImage, FCompressedImage2D* pOutImage, int yStart, int yEnd, int SliceIndex)
{
	check(pInImage->Format == ERawImageFormat::BGRA8);
	check((yStart % 4) == 0);
	check((pInImage->SizeX % 4) == 0);
	check((yStart >= 0) && (yStart <= pInImage->SizeY));
	check((yEnd   >= 0) && (yEnd   <= pInImage->SizeY));

	const int InStride = pInImage->SizeX * 4;
	const int OutStride = pInImage->SizeX / 4 * BLOCK_SIZE_IN_BYTES;
	const int InSliceSize = pInImage->SizeY * InStride;
	const int OutSliceSize = pInImage->SizeY / 4 * OutStride;

	uint8* pInTexels = reinterpret_cast<uint8*>(&pInImage->RawData[0]) + InSliceSize * SliceIndex;
	uint8* pOutTexels = reinterpret_cast<uint8*>(&pOutImage->RawData[0]) + OutSliceSize * SliceIndex;

	// Switch byte order for compressors input
	for ( int y=yStart; y < yEnd; ++y )
	{
		uint8* pInTexelsSwap = pInTexels + (y * InStride);
		for ( int x=0; x < pInImage->SizeX; ++x )
		{
			const uint8 r = pInTexelsSwap[0];
			pInTexelsSwap[0] = pInTexelsSwap[2];
			pInTexelsSwap[2] = r;

			pInTexelsSwap += 4;
		}
	}

	rgba_surface insurface;
	insurface.ptr		= pInTexels + (yStart * InStride);
	insurface.width		= pInImage->SizeX;
	insurface.height	= yEnd - yStart;
	insurface.stride	= pInImage->SizeX * 4;

	pOutTexels += yStart / 4 * OutStride;
	CompressBlocksBC7(&insurface, pOutTexels, pEncSettings);
}

#define MAX_QUALITY_BY_SIZE 4
#define FORCED_NORMAL_MAP_COMPRESSION_SIZE_VALUE 4

static uint16 GetDefaultCompressionBySizeValue()
{
	// start at default quality, then lookup in .ini file
	int32 CompressionModeValue = 0;
	GConfig->GetInt(TEXT("/Script/UnrealEd.CookerSettings"), TEXT("DefaultASTCQualityBySize"), CompressionModeValue, GEngineIni);

	FParse::Value(FCommandLine::Get(), TEXT("-astcqualitybysize="), CompressionModeValue);
	CompressionModeValue = FMath::Min<uint32>(CompressionModeValue, MAX_QUALITY_BY_SIZE);

	return CompressionModeValue;
}

static EPixelFormat GetQualityFormat(int& BlockWidth, int& BlockHeight, int32 OverrideSizeValue = -1)
{
	// Note: ISPC only supports 8x8 and higher quality, and only one speed (fast)
	// convert to a string
	EPixelFormat Format = PF_Unknown;
	switch (OverrideSizeValue >= 0 ? OverrideSizeValue : GetDefaultCompressionBySizeValue())
	{
		case 0:	//Format = PF_ASTC_12x12; BlockWidth = BlockHeight = 12; break;
		case 1:	//Format = PF_ASTC_10x10; BlockWidth = BlockHeight = 10; break;
		case 2:	Format = PF_ASTC_8x8; BlockWidth = BlockHeight = 8; break;
		case 3:	Format = PF_ASTC_6x6; BlockWidth = BlockHeight = 6; break;
		case 4:	Format = PF_ASTC_4x4; BlockWidth = BlockHeight = 4; break;
		default: UE_LOG(LogTemp, Fatal, TEXT("Max quality higher than expected"));
	}

	return Format;
}

struct FASTCEncoderSettings : public astc_enc_settings
{
	FName TextureFormatName;
};


/**
 * ASTC Compression function
 */
static void IntelASTCCompressScans(FASTCEncoderSettings* pEncSettings, FImage* pInImage, FCompressedImage2D* pOutImage, int yStart, int yEnd, int SliceIndex)
{
	check(pInImage->Format == ERawImageFormat::BGRA8);
	check((yStart % pEncSettings->block_height) == 0);
	check((pInImage->SizeX % pEncSettings->block_width) == 0);
	check((yStart >= 0) && (yStart <= pInImage->SizeY));
	check((yEnd   >= 0) && (yEnd   <= pInImage->SizeY));

	const int InStride = pInImage->SizeX * 4;
	const int OutStride = pInImage->SizeX / pEncSettings->block_width * BLOCK_SIZE_IN_BYTES;
	const int InSliceSize = pInImage->SizeY * InStride;
	const int OutSliceSize = pInImage->SizeY / pEncSettings->block_height * OutStride;

	uint8* pInTexels = reinterpret_cast<uint8*>(&pInImage->RawData[0]) + InSliceSize * SliceIndex;
	uint8* pOutTexels = reinterpret_cast<uint8*>(&pOutImage->RawData[0]) + OutSliceSize * SliceIndex;

	if (pEncSettings->TextureFormatName == GTextureFormatNameASTC_RGB)
	{
		// Switch byte order for compressors input (BGRA -> RGBA)
		// Force A=255
		for (int y = yStart; y < yEnd; ++y)
		{
			uint8* pInTexelsSwap = pInTexels + (y * InStride);
			for (int x = 0; x < pInImage->SizeX; ++x)
			{
				const uint8 r = pInTexelsSwap[0];
				pInTexelsSwap[0] = pInTexelsSwap[2];
				pInTexelsSwap[2] = r;
				pInTexelsSwap[3] = 255;

				pInTexelsSwap += 4;
			}
		}
	}
	else if (pEncSettings->TextureFormatName == GTextureFormatNameASTC_RGBA)
	{
		// Switch byte order for compressors input (BGRA -> RGBA)
		for (int y = yStart; y < yEnd; ++y)
		{
			uint8* pInTexelsSwap = pInTexels + (y * InStride);
			for (int x = 0; x < pInImage->SizeX; ++x)
			{
				const uint8 r = pInTexelsSwap[0];
				pInTexelsSwap[0] = pInTexelsSwap[2];
				pInTexelsSwap[2] = r;

				pInTexelsSwap += 4;
			}
		}
	}
	else if (pEncSettings->TextureFormatName == GTextureFormatNameASTC_NormalAG)
	{
		// Switch byte order for compressors input (BGRA -> RGBA)
		// Re-normalize
		// Set any unused RGB components to 0, an unused A to 255.
		for (int y = yStart; y < yEnd; ++y)
		{
			uint8* pInTexelsSwap = pInTexels + (y * InStride);
			for (int x = 0; x < pInImage->SizeX; ++x)
			{
				FVector Normal = FVector(pInTexelsSwap[2] / 255.0f * 2.0f - 1.0f, pInTexelsSwap[1] / 255.0f * 2.0f - 1.0f, pInTexelsSwap[0] / 255.0f * 2.0f - 1.0f);
				Normal = Normal.GetSafeNormal();
				pInTexelsSwap[0] = 0;
				pInTexelsSwap[1] = FMath::FloorToInt((Normal.Y * 0.5f + 0.5f) * 255.999f);
				pInTexelsSwap[2] = 0;
				pInTexelsSwap[3] = FMath::FloorToInt((Normal.X * 0.5f + 0.5f) * 255.999f);

				pInTexelsSwap += 4;
			}
		}
	}
	else if (pEncSettings->TextureFormatName == GTextureFormatNameASTC_NormalRG)
	{
		// Switch byte order for compressors input (BGRA -> RGBA)
		// Re-normalize
		// Set any unused RGB components to 0, an unused A to 255.
		for (int y = yStart; y < yEnd; ++y)
		{
			uint8* pInTexelsSwap = pInTexels + (y * InStride);
			for (int x = 0; x < pInImage->SizeX; ++x)
			{
				FVector Normal = FVector(pInTexelsSwap[2] / 255.0f * 2.0f - 1.0f, pInTexelsSwap[1] / 255.0f * 2.0f - 1.0f, pInTexelsSwap[0] / 255.0f * 2.0f - 1.0f);
				Normal = Normal.GetSafeNormal();
				pInTexelsSwap[0] = FMath::FloorToInt((Normal.X * 0.5f + 0.5f) * 255.999f);
				pInTexelsSwap[1] = FMath::FloorToInt((Normal.Y * 0.5f + 0.5f) * 255.999f);
				pInTexelsSwap[2] = 0;
				pInTexelsSwap[3] = 255;

				pInTexelsSwap += 4;
			}
		}
	}

	rgba_surface insurface;
	insurface.ptr		= pInTexels + (yStart * InStride);
	insurface.width		= pInImage->SizeX;
	insurface.height	= yEnd - yStart;
	insurface.stride	= pInImage->SizeX * 4;

	pOutTexels += yStart / pEncSettings->block_height * OutStride;
	CompressBlocksASTC(&insurface, pOutTexels, pEncSettings);
}

/**
 * Intel BC texture format handler.
 */
class FTextureFormatIntelISPCTexComp : public ITextureFormat
{
public:
	FTextureFormatIntelISPCTexComp()
	{
	}
	virtual ~FTextureFormatIntelISPCTexComp()
	{
	}
	virtual bool AllowParallelBuild() const override
	{
		return true;
	}

	// Return the version for the DX11 formats BC6H and BC7 (not ASTC)
	virtual uint16 GetVersion(
		FName Format,
		const struct FTextureBuildSettings* BuildSettings = nullptr
	) const override
	{
		return BASE_ISPC_DX11_FORMAT_VERSION;
	}

	// Since we want to have per texture [group] compression settings, we need to have the key based on the texture
	virtual FString GetDerivedDataKeyString(const class UTexture& Texture) const override
	{
		return TEXT("");
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

	static void SetupScans(const FImage& InImage, int BlockWidth, int BlockHeight, FCompressedImage2D& OutCompressedImage, FMultithreadSettings &MultithreadSettings)
	{
		const int AlignedSizeX = AlignArbitrary(InImage.SizeX, BlockWidth);
		const int AlignedSizeY = AlignArbitrary(InImage.SizeY, BlockHeight);
		const int WidthInBlocks = AlignedSizeX / BlockWidth;
		const int HeightInBlocks = AlignedSizeY / BlockHeight;
		const int SizePerSlice = WidthInBlocks * HeightInBlocks * BLOCK_SIZE_IN_BYTES;
		OutCompressedImage.RawData.AddUninitialized(SizePerSlice * InImage.NumSlices);
		OutCompressedImage.SizeX = FMath::Max(AlignedSizeX, BlockWidth);
		OutCompressedImage.SizeY = FMath::Max(AlignedSizeY, BlockHeight);

		// When we allow async tasks to execute we do so with BlockHeight lines of the image per task
		// This isn't optimal for long thin textures, but works well with how ISPC works
		MultithreadSettings.iScansPerTask = BlockHeight;
		MultithreadSettings.iNumTasks = FMath::Max((AlignedSizeY / MultithreadSettings.iScansPerTask) - 1, 0);
	}

	static void PadImageToBlockSize(FImage &InOutImage, int BlockWidth, int BlockHeight, int BytesPerPixel)
	{
		const int AlignedSizeX = AlignArbitrary(InOutImage.SizeX, BlockWidth);
		const int AlignedSizeY = AlignArbitrary(InOutImage.SizeY, BlockHeight);
		const int AlignedSliceSize = AlignedSizeX * AlignedSizeY * BytesPerPixel;
		const int AlignedTotalSize = AlignedSliceSize * InOutImage.NumSlices;
		const int OriginalSliceSize = InOutImage.SizeX * InOutImage.SizeY * BytesPerPixel;

		// Early out if no padding is necessary
		if (AlignedSizeX == InOutImage.SizeX && AlignedSizeY == InOutImage.SizeY)
		{
			return;
		}

		// Allocate temp buffer
		//@TODO: Optimize away this temp buffer (could avoid last FMemory::Memcpy)
		TArray<uint8> TempBuffer;
		TempBuffer.SetNumUninitialized(AlignedTotalSize);

		const int PaddingX = AlignedSizeX - InOutImage.SizeX;
		const int PaddingY = AlignedSizeY - InOutImage.SizeY;
		const int SrcStride = InOutImage.SizeX * BytesPerPixel;
		const int DstStride = AlignedSizeX * BytesPerPixel;

		for (int SliceIndex = 0; SliceIndex < InOutImage.NumSlices; ++SliceIndex)
		{
			uint8* DstData = ((uint8*)TempBuffer.GetData()) + SliceIndex * AlignedSliceSize;
			const uint8* SrcData = ((uint8*)InOutImage.RawData.GetData()) + SliceIndex * OriginalSliceSize;

			// Copy all of SrcData and pad on X-axis:
			for (int Y = 0; Y < InOutImage.SizeY; ++Y)
			{
				FMemory::Memcpy(DstData, SrcData, SrcStride);
				SrcData += SrcStride - BytesPerPixel;	// Src: Last pixel on this row
				DstData += SrcStride;					// Dst: Beginning of the padded region at the end of this row
				for (int PadX = 0; PadX < PaddingX; PadX++)
				{
					// Replicate right-most pixel as padding on X-axis
					FMemory::Memcpy(DstData, SrcData, BytesPerPixel);
					DstData += BytesPerPixel;
				}
				SrcData += BytesPerPixel;				// Src & Dst: Beginning of next row
			}

			// Replicate last row as padding on Y-axis:
			SrcData = DstData - DstStride;				// Src: Beginning of the last row (of DstData)
			for (int PadY = 0; PadY < PaddingY; PadY++)
			{
				FMemory::Memcpy(DstData, SrcData, DstStride);
				DstData += DstStride;					// Dst: Beginning of the padded region at the end of this row
			}
		}

		// Replace InOutImage with the new data
		InOutImage.RawData.Empty(AlignedTotalSize);
		InOutImage.RawData.SetNumUninitialized(AlignedTotalSize);
		FMemory::Memcpy(InOutImage.RawData.GetData(), TempBuffer.GetData(), AlignedTotalSize);
		InOutImage.SizeX = AlignedSizeX;
		InOutImage.SizeY = AlignedSizeY;
	}

	virtual bool CompressImage(
		const FImage& InImage,
		const struct FTextureBuildSettings& BuildSettings,
		bool bImageHasAlphaChannel,
		FCompressedImage2D& OutCompressedImage
		) const override
	{
		check(InImage.SizeX > 0);
		check(InImage.SizeY > 0);
		check(InImage.NumSlices > 0);

		bool bCompressionSucceeded = false;

		int BlockWidth = 0;
		int BlockHeight = 0;

		const bool bUseTasks = true;
		FMultithreadSettings MultithreadSettings;

		EPixelFormat CompressedPixelFormat = PF_Unknown;
		if ( BuildSettings.TextureFormatName == GTextureFormatNameBC6H )
		{
			FImage Image;
			InImage.CopyTo(Image, ERawImageFormat::RGBA16F, EGammaSpace::Linear);

			bc6h_enc_settings settings;
			GetProfile_bc6h_basic(&settings);

			SetupScans(Image, 4, 4, OutCompressedImage, MultithreadSettings);
			PadImageToBlockSize(Image, 4, 4, 4*2);
			FMultithreadedCompression<bc6h_enc_settings>::Compress(MultithreadSettings, settings, Image, OutCompressedImage, &IntelBC6HCompressScans, bUseTasks);

			CompressedPixelFormat = PF_BC6H;
			bCompressionSucceeded = true;
		}
		else if ( BuildSettings.TextureFormatName == GTextureFormatNameBC7 )
		{
			FImage Image;
			InImage.CopyTo(Image, ERawImageFormat::BGRA8, BuildSettings.GetGammaSpace());

			bc7_enc_settings settings;
			if ( bImageHasAlphaChannel )
			{
				GetProfile_alpha_basic(&settings);
			}
			else
			{
				GetProfile_basic(&settings);
			}

			SetupScans(Image, 4, 4, OutCompressedImage, MultithreadSettings);
			PadImageToBlockSize(Image, 4, 4, 4*1);
			FMultithreadedCompression<bc7_enc_settings>::Compress(MultithreadSettings, settings, Image, OutCompressedImage, &IntelBC7CompressScans, bUseTasks);

			CompressedPixelFormat = PF_BC7;
			bCompressionSucceeded = true;
		}
		else
		{
			bool bIsRGBColorASTC = (BuildSettings.TextureFormatName == GTextureFormatNameASTC_RGB ||
				((BuildSettings.TextureFormatName == GTextureFormatNameASTC_RGBAuto) && !bImageHasAlphaChannel));
			bool bIsRGBAColorASTC = (BuildSettings.TextureFormatName == GTextureFormatNameASTC_RGBA ||
				((BuildSettings.TextureFormatName == GTextureFormatNameASTC_RGBAuto) && bImageHasAlphaChannel));
			bool bIsNormalMap = (BuildSettings.TextureFormatName == GTextureFormatNameASTC_NormalAG ||
				BuildSettings.TextureFormatName == GTextureFormatNameASTC_NormalRG);

			CompressedPixelFormat = GetQualityFormat( BlockWidth, BlockHeight, bIsNormalMap ? FORCED_NORMAL_MAP_COMPRESSION_SIZE_VALUE : BuildSettings.CompressionQuality );

			FASTCEncoderSettings EncoderSettings;
			if (BuildSettings.TextureFormatName == GTextureFormatNameASTC_NormalAG)
			{
				GetProfile_astc_alpha_fast(&EncoderSettings, BlockWidth, BlockHeight);
				EncoderSettings.TextureFormatName = BuildSettings.TextureFormatName;
				bCompressionSucceeded = true;
			}
			else if (BuildSettings.TextureFormatName == GTextureFormatNameASTC_NormalRG)
			{
				GetProfile_astc_fast(&EncoderSettings, BlockWidth, BlockHeight);
				EncoderSettings.TextureFormatName = BuildSettings.TextureFormatName;
				bCompressionSucceeded = true;
			}
			else if (bIsRGBColorASTC)
			{
				GetProfile_astc_fast(&EncoderSettings, BlockWidth, BlockHeight);
				EncoderSettings.TextureFormatName = GTextureFormatNameASTC_RGB;
				bCompressionSucceeded = true;
			}
			else if (bIsRGBAColorASTC)
			{
				GetProfile_astc_alpha_fast(&EncoderSettings, BlockWidth, BlockHeight);
				EncoderSettings.TextureFormatName = GTextureFormatNameASTC_RGBA;
				bCompressionSucceeded = true;
			}

			if (bCompressionSucceeded)
			{
				FImage Image;
				InImage.CopyTo(Image, ERawImageFormat::BGRA8, BuildSettings.GetGammaSpace());

				SetupScans(Image, EncoderSettings.block_width, EncoderSettings.block_height, OutCompressedImage, MultithreadSettings);
				PadImageToBlockSize(Image, EncoderSettings.block_width, EncoderSettings.block_height, 4 * 1);
				
#if DEBUG_SAVE_INTERMEDIATE_IMAGES
				//@DEBUG (save padded input as BMP):
				static bool SaveInputOutput = false;
				static volatile int32 Counter = 0;
				int LocalCounter = Counter;
				if (SaveInputOutput)	// && LocalCounter < 10 && Image.SizeX >= 1024)
				{
					const FString FileName = FString::Printf(TEXT("Smedis-Input-%d.bmp"), FPlatformTLS::GetCurrentThreadId());
					FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*FileName);
					SaveImageAsBMP(*FileWriter, Image.RawData.GetData(), 4, Image.SizeX, Image.SizeY);
					delete FileWriter;
					FPlatformAtomics::InterlockedIncrement(&Counter);
				}
#endif

				FMultithreadedCompression<FASTCEncoderSettings>::Compress(MultithreadSettings, EncoderSettings, Image, OutCompressedImage, &IntelASTCCompressScans, bUseTasks);

#if DEBUG_SAVE_INTERMEDIATE_IMAGES
				//@DEBUG (save swizzled/fixed-up input as BMP):
				if (SaveInputOutput)	// && LocalCounter < 10 && Image.SizeX >= 1024)
				{
					const FString FileName = FString::Printf(TEXT("Smedis-InputSwizzled-%d.bmp"), FPlatformTLS::GetCurrentThreadId());
					FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*FileName);
					SaveImageAsBMP(*FileWriter, Image.RawData.GetData(), 4, Image.SizeX, Image.SizeY);
					delete FileWriter;
					FPlatformAtomics::InterlockedIncrement(&Counter);
				}

				//@DEBUG (save output as .astc file):
				if (SaveInputOutput)// && LocalCounter < 10 && Image.SizeX >= 1024)
				{
					const FString FileName = FString::Printf(TEXT("Smedis-Output-%d.astc"), FPlatformTLS::GetCurrentThreadId());
					FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*FileName);
					SaveImageAsASTC(*FileWriter, OutCompressedImage.RawData.GetData(), OutCompressedImage.SizeX, OutCompressedImage.SizeY, EncoderSettings.block_width, EncoderSettings.block_height);
					delete FileWriter;
				}
#endif
			}
		}
		OutCompressedImage.PixelFormat = CompressedPixelFormat;
		OutCompressedImage.SizeX = InImage.SizeX;
		OutCompressedImage.SizeY = InImage.SizeY;

		return bCompressionSucceeded;
	}
};

/**
 * Module for DXT texture compression.
 */
static ITextureFormat* Singleton = NULL;

class FTextureFormatIntelISPCTexCompModule : public ITextureFormatModule
{
public:
	FTextureFormatIntelISPCTexCompModule()
	{
		mDllHandle = nullptr;
	}

	virtual ~FTextureFormatIntelISPCTexCompModule()
	{
		delete Singleton;
		Singleton = NULL;

		if ( mDllHandle != nullptr )
		{
			FPlatformProcess::FreeDllHandle(mDllHandle);
			mDllHandle = nullptr;
		}
	}

	virtual ITextureFormat* GetTextureFormat()
	{
		if (!Singleton)
		{
#if PLATFORM_WINDOWS
	#if PLATFORM_64BITS
			mDllHandle = FPlatformProcess::GetDllHandle(TEXT("../../../Engine/Binaries/ThirdParty/IntelISPCTexComp/Win64-Release/ispc_texcomp.dll"));
	#else	//32-bit platform
			mDllHandle = FPlatformProcess::GetDllHandle(TEXT("../../../Engine/Binaries/ThirdParty/IntelISPCTexComp/Win32-Release/ispc_texcomp.dll"));
	#endif
#elif PLATFORM_MAC
			mDllHandle = FPlatformProcess::GetDllHandle(TEXT("libispc_texcomp.dylib"));
#elif PLATFORM_LINUX
			mDllHandle = FPlatformProcess::GetDllHandle(TEXT("../../../Engine/Binaries/ThirdParty/IntelISPCTexComp/Linux64-Release/libispc_texcomp.so"));
#endif
			Singleton = new FTextureFormatIntelISPCTexComp();
		}
		return Singleton;
	}

	void* mDllHandle;
};

IMPLEMENT_MODULE(FTextureFormatIntelISPCTexCompModule, TextureFormatIntelISPCTexComp);
