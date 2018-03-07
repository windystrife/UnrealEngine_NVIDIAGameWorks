// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/ScopeLock.h"
#include "Containers/IndirectArray.h"
#include "Stats/Stats.h"
#include "Async/AsyncWork.h"
#include "HAL/IConsoleManager.h"
#include "ImageCore.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/ITextureFormat.h"
#include "Interfaces/ITextureFormatModule.h"
#include "TextureCompressorModule.h"
#include "PixelFormat.h"

THIRD_PARTY_INCLUDES_START
	#include "nvtt/nvtt.h"
THIRD_PARTY_INCLUDES_END

DEFINE_LOG_CATEGORY_STATIC(LogTextureFormatDXT, Log, All);

/**
 * Macro trickery for supported format names.
 */
#define ENUM_SUPPORTED_FORMATS(op) \
	op(DXT1) \
	op(DXT3) \
	op(DXT5) \
	op(AutoDXT) \
	op(DXT5n) \
	op(BC4)	\
	op(BC5)

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
 * NVTT output handler.
 */
struct FNVOutputHandler : public nvtt::OutputHandler
{
	explicit FNVOutputHandler( uint8* InBuffer, int32 InBufferSize )
		: Buffer(InBuffer)
		, BufferEnd(InBuffer + InBufferSize)
	{
	}

	~FNVOutputHandler()
	{
	}

	virtual void beginImage( int size, int width, int height, int depth, int face, int miplevel )
	{
	}

	virtual bool writeData( const void* data, int size )
	{
		check(data);
		check(Buffer + size <= BufferEnd);
		FMemory::Memcpy(Buffer, data, size);
		Buffer += size;
		return true;
	}

    virtual void endImage()
    {
    }

	uint8* Buffer;
	uint8* BufferEnd;
};

/**
 * NVTT error handler.
 */
struct FNVErrorHandler : public nvtt::ErrorHandler
{
	FNVErrorHandler() : 
		bSuccess(true)
	{}

	virtual void error(nvtt::Error e)
	{
		UE_LOG(LogTextureFormatDXT, Warning, TEXT("nvtt::compress() failed with error '%s'"), ANSI_TO_TCHAR(nvtt::errorString(e)));
		bSuccess = false;
	}

	bool bSuccess;
};

/** Critical section to isolate construction of nvtt objects */
FCriticalSection GNVCompressionCriticalSection;

/**
 * All state objects needed for NVTT.
 */
class FNVTTCompressor
{
	FNVOutputHandler			OutputHandler;
	FNVErrorHandler				ErrorHandler;
	nvtt::InputOptions			InputOptions;
	nvtt::CompressionOptions	CompressionOptions;
	nvtt::OutputOptions			OutputOptions;
	nvtt::Compressor			Compressor;

public:
	/** Initialization constructor. */
	FNVTTCompressor(
		const void* SourceData,
		EPixelFormat PixelFormat,
		int32 SizeX,
		int32 SizeY,
		bool bSRGB,
		bool bIsNormalMap,
		uint8* OutBuffer,
		int32 BufferSize, 
		bool bPreview = false)
		: OutputHandler(OutBuffer, BufferSize)
	{
		// CUDA acceleration currently disabled, needs more robust error handling
		// With one core of a Xeon 3GHz CPU, compressing a 2048^2 normal map to DXT1 with NVTT 2.0.4 takes 7.49s.
		// With the same settings but using CUDA and a Geforce 8800 GTX it takes 1.66s.
		// To use CUDA, a CUDA 2.0 capable driver is required (178.08 or greater) and a Geforce 8 or higher.
		const bool bUseCUDAAcceleration = false;

		// DXT1a support is currently not exposed.
		const bool bSupportDXT1a = false;

		// Quality level is hardcoded to production quality for now.
		const nvtt::Quality QualityLevel = bPreview ? nvtt::Quality_Fastest : nvtt::Quality_Production;

		nvtt::Format TextureFormat = nvtt::Format_DXT1;
		if (PixelFormat == PF_DXT1)
		{
			TextureFormat = bSupportDXT1a ? nvtt::Format_DXT1a : nvtt::Format_DXT1;
		}
		else if (PixelFormat == PF_DXT3)
		{
			TextureFormat = nvtt::Format_DXT3;
		}
		else if (PixelFormat == PF_DXT5 && bIsNormalMap)
		{
			TextureFormat = nvtt::Format_DXT5n;
		}
		else if (PixelFormat == PF_DXT5)
		{
			TextureFormat = nvtt::Format_DXT5;
		}
		else if (PixelFormat == PF_B8G8R8A8)
		{
			TextureFormat = nvtt::Format_RGBA;
		}
		else if (PixelFormat == PF_BC4)
		{
			TextureFormat = nvtt::Format_BC4;
		}
		else if (PixelFormat == PF_BC5)
		{
			TextureFormat = nvtt::Format_BC5;
		}
		else
		{
			UE_LOG(LogTextureFormatDXT,Fatal,
				TEXT("Unsupported EPixelFormat for compression: %u"),
				(uint32)PixelFormat
				);
		}

		InputOptions.setTextureLayout(nvtt::TextureType_2D, SizeX, SizeY);

		// Not generating mips with NVTT, we will pass each mip in and compress it individually
		InputOptions.setMipmapGeneration(false, -1);
		verify(InputOptions.setMipmapData(SourceData, SizeX, SizeY));

		if (bSRGB)
		{
			InputOptions.setGamma(2.2f, 2.2f);
		}
		else
		{
			InputOptions.setGamma(1.0f, 1.0f);
		}

		// Only used for mip and normal map generation
		InputOptions.setWrapMode(nvtt::WrapMode_Mirror);
		InputOptions.setFormat(nvtt::InputFormat_BGRA_8UB);

		// Highest quality is 2x slower with only a small visual difference
		// Might be worthwhile for normal maps though
		CompressionOptions.setQuality(QualityLevel);
		CompressionOptions.setFormat(TextureFormat);

		if ( bIsNormalMap )
		{
			// For BC5 normal maps we don't care about the blue channel.
			CompressionOptions.setColorWeights( 1.0f, 1.0f, 0.0f );

			// Don't tell NVTT it's a normal map. It was producing noticeable artifacts during BC5 compression.
			//InputOptions.setNormalMap(true);
		}
		else
		{
			CompressionOptions.setColorWeights(1, 1, 1);
		}

		Compressor.enableCudaAcceleration(bUseCUDAAcceleration);
		//OutputHandler.ReserveMemory( Compressor.estimateSize(InputOptions, CompressionOptions) );
		check(OutputHandler.BufferEnd - OutputHandler.Buffer <= Compressor.estimateSize(InputOptions, CompressionOptions));

		// We're not outputting a dds file so disable the header
		OutputOptions.setOutputHeader( false );
		OutputOptions.setOutputHandler( &OutputHandler );
		OutputOptions.setErrorHandler( &ErrorHandler );
	}

	/** Run the compressor. */
	bool Compress()
	{
		return Compressor.process(InputOptions, CompressionOptions, OutputOptions) && ErrorHandler.bSuccess;
	}
};

/**
 * Asynchronous NVTT worker.
 */
class FAsyncNVTTWorker : public FNonAbandonableTask 
{
public:
	/**
	 * Initializes the data and creates the async compression task.
	 */
	FAsyncNVTTWorker(FNVTTCompressor* InCompressor)
		: Compressor(InCompressor)
	{
		check(Compressor);
	}

	/** Compresses the texture. */
	void DoWork()
	{
		bCompressionResults = Compressor->Compress();
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncNVTTWorker, STATGROUP_ThreadPoolAsyncTasks);
	}

	/** Retrieve compression results. */
	bool GetCompressionResults() const { return bCompressionResults; }

private:
	/** The NVTT compressor. */
	FNVTTCompressor* Compressor;
	/** true if compression was successful. */
	bool bCompressionResults;
};
typedef FAsyncTask<FAsyncNVTTWorker> FAsyncNVTTTask;

namespace CompressionSettings
{
	int32 BlocksPerBatch = 2048;
	FAutoConsoleVariableRef BlocksPerBatch_CVar(
		TEXT("Tex.AsyncDXTBlocksPerBatch"),
		BlocksPerBatch,
		TEXT("The number of blocks to compress in parallel for DXT compression.")
		);
}

/**
 * Compresses an image using NVTT.
 * @param SourceData			Source texture data to DXT compress, in BGRA 8bit per channel unsigned format.
 * @param PixelFormat			Texture format
 * @param SizeX					Number of texels along the X-axis
 * @param SizeY					Number of texels along the Y-axis
 * @param bSRGB					Whether the texture is in SRGB space
 * @param bIsNormalMap			Whether the texture is a normal map
 * @param OutCompressedData		Compressed image data output by nvtt.
 */
static bool CompressImageUsingNVTT(
	const void* SourceData,
	EPixelFormat PixelFormat,
	int32 SizeX,
	int32 SizeY,
	bool bSRGB,
	bool bIsNormalMap,
	bool bIsPreview,
	TArray<uint8>& OutCompressedData
	)
{
	check(PixelFormat == PF_DXT1 || PixelFormat == PF_DXT3 || PixelFormat == PF_DXT5 || PixelFormat == PF_BC4 || PixelFormat == PF_BC5);

	// Avoid dependency on GPixelFormats in RenderCore.
	const int32 BlockSizeX = 4;
	const int32 BlockSizeY = 4;
	const int32 BlockBytes = (PixelFormat == PF_DXT1 || PixelFormat == PF_BC4) ? 8 : 16;
	const int32 ImageBlocksX = FMath::Max(SizeX / BlockSizeX, 1);
	const int32 ImageBlocksY = FMath::Max(SizeY / BlockSizeY, 1);
	const int32 BlocksPerBatch = FMath::Max<int32>(ImageBlocksX, FMath::RoundUpToPowerOfTwo(CompressionSettings::BlocksPerBatch));
	const int32 RowsPerBatch = BlocksPerBatch / ImageBlocksX;
	const int32 NumBatches = ImageBlocksY / RowsPerBatch;

	// Allocate space to store compressed data.
	OutCompressedData.Empty(ImageBlocksX * ImageBlocksY * BlockBytes);
	OutCompressedData.AddUninitialized(ImageBlocksX * ImageBlocksY * BlockBytes);

	if (ImageBlocksX * ImageBlocksY <= BlocksPerBatch ||
		BlocksPerBatch % ImageBlocksX != 0 ||
		RowsPerBatch * NumBatches != ImageBlocksY)
	{
		FNVTTCompressor* Compressor = NULL;
		{
			FScopeLock ScopeLock(&GNVCompressionCriticalSection);
			Compressor = new FNVTTCompressor(
				SourceData,
				PixelFormat,
				SizeX,
				SizeY,
				bSRGB,
				bIsNormalMap,
				OutCompressedData.GetData(),
				OutCompressedData.Num(),
				bIsPreview
				);
		}
		bool bSuccess = Compressor->Compress();
		{
			FScopeLock ScopeLock(&GNVCompressionCriticalSection);
			delete Compressor;
			Compressor = NULL;
		}
		return bSuccess;
	}

	int32 UncompressedStride = RowsPerBatch * BlockSizeY * SizeX * sizeof(FColor);
	int32 CompressedStride = RowsPerBatch * ImageBlocksX * BlockBytes;

	// Create compressors for each batch.
	TIndirectArray<FNVTTCompressor> Compressors;
	Compressors.Empty(NumBatches);
	{
		FScopeLock ScopeLock(&GNVCompressionCriticalSection);
		const uint8* Src = (const uint8*)SourceData;
		uint8* Dest = OutCompressedData.GetData();
		for (int32 BatchIndex = 0; BatchIndex < NumBatches; ++BatchIndex)
		{
			new(Compressors) FNVTTCompressor(
				Src,
				PixelFormat,
				SizeX,
				RowsPerBatch * BlockSizeY,
				bSRGB,
				bIsNormalMap,
				Dest,
				CompressedStride
				);
			Src += UncompressedStride;
			Dest += CompressedStride;
		}
	}

	// Asynchronously compress each batch.
	bool bSuccess = true;
	{
		TIndirectArray<FAsyncNVTTTask> AsyncTasks;
		for (int32 BatchIndex = 0; BatchIndex < NumBatches; ++BatchIndex)
		{
			FAsyncNVTTTask* AsyncTask = new(AsyncTasks) FAsyncNVTTTask(&Compressors[BatchIndex]);
#if WITH_EDITOR
			AsyncTask->StartBackgroundTask(GLargeThreadPool);
#else
			AsyncTask->StartBackgroundTask();
#endif
		}
		for (int32 BatchIndex = 0; BatchIndex < NumBatches; ++BatchIndex)
		{
			FAsyncNVTTTask& AsyncTask = AsyncTasks[BatchIndex];
			AsyncTask.EnsureCompletion();
			bSuccess = bSuccess && AsyncTask.GetTask().GetCompressionResults();
		}
	}

	// Release compressors
	{
		FScopeLock ScopeLock(&GNVCompressionCriticalSection);
		Compressors.Empty();
	}

	return bSuccess;
}

/**
 * DXT texture format handler.
 */
class FTextureFormatDXT : public ITextureFormat
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

		bool bIsNormalMap = false;
		EPixelFormat CompressedPixelFormat = PF_Unknown;
		if (BuildSettings.TextureFormatName == GTextureFormatNameDXT1)
		{
			CompressedPixelFormat = PF_DXT1;
		}
		else if (BuildSettings.TextureFormatName == GTextureFormatNameDXT3)
		{
			CompressedPixelFormat = PF_DXT3;
		}
		else if (BuildSettings.TextureFormatName == GTextureFormatNameDXT5)
		{
			CompressedPixelFormat = PF_DXT5;
		}
		else if (BuildSettings.TextureFormatName == GTextureFormatNameAutoDXT)
		{
			CompressedPixelFormat = bImageHasAlphaChannel ? PF_DXT5 : PF_DXT1;
		}
		else if (BuildSettings.TextureFormatName == GTextureFormatNameDXT5n)
		{
			CompressedPixelFormat = PF_DXT5;
			bIsNormalMap = true;
		}
		else if (BuildSettings.TextureFormatName == GTextureFormatNameBC5)
		{
			CompressedPixelFormat = PF_BC5;
			bIsNormalMap = true;
		}
		else if (BuildSettings.TextureFormatName == GTextureFormatNameBC4)
		{
			CompressedPixelFormat = PF_BC4;
		}

		bool bCompressionSucceeded = true;
		int32 SliceSize = Image.SizeX * Image.SizeY;
		for (int32 SliceIndex = 0; SliceIndex < Image.NumSlices && bCompressionSucceeded; ++SliceIndex)
		{
			TArray<uint8> CompressedSliceData;
			bCompressionSucceeded = CompressImageUsingNVTT(
				Image.AsBGRA8() + SliceIndex * SliceSize,
				CompressedPixelFormat,
				Image.SizeX,
				Image.SizeY,
				Image.IsGammaCorrected(),
				bIsNormalMap,
				false, // Daniel Lamb: Testing with this set to true didn't give large performance gain to lightmaps.  Encoding of 140 lightmaps was 19.2seconds with preview 20.1 without preview.  11/30/2015
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
 * Module for DXT texture compression.
 */
static ITextureFormat* Singleton = NULL;

class FTextureFormatDXTModule : public ITextureFormatModule
{
public:
	virtual ~FTextureFormatDXTModule()
	{
		delete Singleton;
		Singleton = NULL;
	}
	virtual ITextureFormat* GetTextureFormat()
	{
		if (!Singleton)
		{
			Singleton = new FTextureFormatDXT();
		}
		return Singleton;
	}
};

IMPLEMENT_MODULE(FTextureFormatDXTModule, TextureFormatDXT);
