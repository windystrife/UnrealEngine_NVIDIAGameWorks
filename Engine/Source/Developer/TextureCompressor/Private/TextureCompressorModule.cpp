// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TextureCompressorModule.h"
#include "Math/RandomStream.h"
#include "Containers/IndirectArray.h"
#include "Stats/Stats.h"
#include "Async/AsyncWork.h"
#include "Modules/ModuleManager.h"
#include "Engine/Texture.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Interfaces/ITextureFormat.h"
#include "ImageCore.h"

#if PLATFORM_WINDOWS
#include "WindowsHWrapper.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogTextureCompressor, Log, All);

/*------------------------------------------------------------------------------
	Mip-Map Generation
------------------------------------------------------------------------------*/

enum EMipGenAddressMode
{
	MGTAM_Wrap,
	MGTAM_Clamp,
	MGTAM_BorderBlack,
};

/**
 * 2D view into one slice of an image.
 */
struct FImageView2D
{
	/** Pointer to colors in the slice. */
	FLinearColor* SliceColors;
	/** Width of the slice. */
	int32 SizeX;
	/** Height of the slice. */
	int32 SizeY;

	/** Initialization constructor. */
	FImageView2D(FImage& Image, int32 SliceIndex)
	{
		SizeX = Image.SizeX;
		SizeY = Image.SizeY;
		SliceColors = Image.AsRGBA32F() + SliceIndex * SizeY * SizeX;
	}

	/** Access a single texel. */
	FLinearColor& Access(int32 X, int32 Y)
	{
		return SliceColors[X + Y * SizeX];
	}

	/** Const access to a single texel. */
	const FLinearColor& Access(int32 X, int32 Y) const
	{
		return SliceColors[X + Y * SizeX];
	}
};

// 2D sample lookup with input conversion
// requires SourceImageData.SizeX and SourceImageData.SizeY to be power of two
template <EMipGenAddressMode AddressMode>
FLinearColor LookupSourceMip(const FImageView2D& SourceImageData, int32 X, int32 Y)
{
	if(AddressMode == MGTAM_Wrap)
	{
		// wrap
		X = (int32)((uint32)X) & (SourceImageData.SizeX - 1);
		Y = (int32)((uint32)Y) & (SourceImageData.SizeY - 1);
	}
	else if(AddressMode == MGTAM_Clamp)
	{
		// clamp
		X = FMath::Clamp(X, 0, SourceImageData.SizeX - 1);
		Y = FMath::Clamp(Y, 0, SourceImageData.SizeY - 1);
	}
	else if(AddressMode == MGTAM_BorderBlack)
	{
		// border color 0
		if((uint32)X >= (uint32)SourceImageData.SizeX
			|| (uint32)Y >= (uint32)SourceImageData.SizeY)
		{
			return FLinearColor(0, 0, 0, 0);
		}
	}
	else
	{
		check(0);
	}
	//return *(SourceImageData.AsRGBA32F() + X + Y * SourceImageData.SizeX);
	return SourceImageData.Access(X,Y);
}

// Kernel class for image filtering operations like image downsampling
// at max MaxKernelExtend x MaxKernelExtend
class FImageKernel2D
{
public:
	FImageKernel2D() :FilterTableSize(0)
	{
	}

	// @param TableSize1D 2 for 2x2, 4 for 4x4, 6 for 6x6, 8 for 8x8
	// @param SharpenFactor can be negative to blur
	// generate normalized 2D Kernel with sharpening
	void BuildSeparatableGaussWithSharpen(uint32 TableSize1D, float SharpenFactor = 0.0f)
	{
		if(TableSize1D > MaxKernelExtend)
		{
			TableSize1D = MaxKernelExtend;
		}

		float Table1D[MaxKernelExtend];
		float NegativeTable1D[MaxKernelExtend];

		FilterTableSize = TableSize1D;

		if(SharpenFactor < 0.0f)
		{
			// blur only
			BuildGaussian1D(Table1D, TableSize1D, 1.0f, -SharpenFactor);
			BuildFilterTable2DFrom1D(KernelWeights, Table1D, TableSize1D);
			return;
		}
		else if(TableSize1D == 2)
		{
			// 2x2 kernel: simple average
			KernelWeights[0] = KernelWeights[1] = KernelWeights[2] = KernelWeights[3] = 0.25f;
			return;
		}
		else if(TableSize1D == 4)
		{
			// 4x4 kernel with sharpen or blur: can alias a bit
			BuildFilterTable1DBase(Table1D, TableSize1D, 1.0f + SharpenFactor);
			BuildFilterTable1DBase(NegativeTable1D, TableSize1D, -SharpenFactor);
			BlurFilterTable1D(NegativeTable1D, TableSize1D, 1);
		}
		else if(TableSize1D == 6)
		{
			// 6x6 kernel with sharpen or blur: still can alias
			BuildFilterTable1DBase(Table1D, TableSize1D, 1.0f + SharpenFactor);
			BuildFilterTable1DBase(NegativeTable1D, TableSize1D, -SharpenFactor);
			BlurFilterTable1D(NegativeTable1D, TableSize1D, 2);
		}
		else if(TableSize1D == 8)
		{
			//8x8 kernel with sharpen or blur

			// * 2 to get similar appearance as for TableSize 6
			SharpenFactor = SharpenFactor * 2.0f;

			BuildFilterTable1DBase(Table1D, TableSize1D, 1.0f + SharpenFactor);
			// positive lobe is blurred a bit for better quality
			BlurFilterTable1D(Table1D, TableSize1D, 1);
			BuildFilterTable1DBase(NegativeTable1D, TableSize1D, -SharpenFactor);
			BlurFilterTable1D(NegativeTable1D, TableSize1D, 3);
		}
		else 
		{
			// not yet supported
			check(0);
		}

		AddFilterTable1D(Table1D, NegativeTable1D, TableSize1D);
		BuildFilterTable2DFrom1D(KernelWeights, Table1D, TableSize1D);
	}

	inline uint32 GetFilterTableSize() const
	{
		return FilterTableSize;
	}

	inline float GetAt(uint32 X, uint32 Y) const
	{
		checkSlow(X < FilterTableSize);
		checkSlow(Y < FilterTableSize);
		return KernelWeights[X + Y * FilterTableSize];
	}

	inline float& GetRefAt(uint32 X, uint32 Y)
	{
		checkSlow(X < FilterTableSize);
		checkSlow(Y < FilterTableSize);
		return KernelWeights[X + Y * FilterTableSize];
	}

private:

	inline static float NormalDistribution(float X, float Variance)
	{
		const float StandardDeviation = FMath::Sqrt(Variance);
		return FMath::Exp(-FMath::Square(X) / (2.0f * Variance)) / (StandardDeviation * FMath::Sqrt(2.0f * (float)PI));
	}

	// support even and non even sized filters
	static void BuildGaussian1D(float *InOutTable, uint32 TableSize, float Sum, float Variance)
	{
		float Center = TableSize * 0.5f;
		float CurrentSum = 0;
		for(uint32 i = 0; i < TableSize; ++i)
		{
			float Actual = NormalDistribution(i - Center + 0.5f, Variance);
			InOutTable[i] = Actual;
			CurrentSum += Actual;
		}
		// Normalize
		float InvSum = Sum / CurrentSum;
		for(uint32 i = 0; i < TableSize; ++i)
		{
			InOutTable[i] *= InvSum;
		}
	}

	//
	static void BuildFilterTable1DBase(float *InOutTable, uint32 TableSize, float Sum )
	{
		// we require a even sized filter
		check(TableSize % 2 == 0);

		float Inner = 0.5f * Sum;

		uint32 Center = TableSize / 2;
		for(uint32 x = 0; x < TableSize; ++x)
		{
			if(x == Center || x == Center - 1)
			{
				// center elements
				InOutTable[x] = Inner;
			}
			else
			{
				// outer elements
				InOutTable[x] = 0.0f;
			}
		}
	}

	// InOutTable += InTable
	static void AddFilterTable1D( float *InOutTable, float *InTable, uint32 TableSize )
	{
		for(uint32 x = 0; x < TableSize; ++x)
		{
			InOutTable[x] += InTable[x];
		}
	}

	// @param Times 1:box, 2:triangle, 3:pow2, 4:pow3, ...
	// can be optimized with double buffering but doesn't need to be fast
	static void BlurFilterTable1D( float *InOutTable, uint32 TableSize, uint32 Times )
	{
		check(Times>0);
		check(TableSize<32);

		float Intermediate[32];

		for(uint32 Pass = 0; Pass < Times; ++Pass)
		{
			for(uint32 x = 0; x < TableSize; ++x)
			{
				Intermediate[x] = InOutTable[x];
			}

			for(uint32 x = 0; x < TableSize; ++x)
			{
				float sum = Intermediate[x];

				if(x)
				{
					sum += Intermediate[x-1];	
				}
				if(x < TableSize - 1)
				{
					sum += Intermediate[x+1];	
				}

				InOutTable[x] = sum / 3.0f;
			}
		}
	}

	static void BuildFilterTable2DFrom1D( float *OutTable2D, float *InTable1D, uint32 TableSize )
	{
		for(uint32 y = 0; y < TableSize; ++y)
		{
			for(uint32 x = 0; x < TableSize; ++x)
			{
				OutTable2D[x + y * TableSize] = InTable1D[y] * InTable1D[x];
			}
		}
	}

	// at max we support MaxKernelExtend x MaxKernelExtend kernels
	const static uint32 MaxKernelExtend = 12;
	// 0 if no kernel was setup yet
	uint32 FilterTableSize;
	// normalized, means the sum of it should be 1.0f
	float KernelWeights[MaxKernelExtend * MaxKernelExtend];
};

template <EMipGenAddressMode AddressMode>
static FVector4 ComputeAlphaCoverage(const FVector4& Thresholds, const FVector4& Scales, const FImageView2D& SourceImageData)
{
	FVector4 Coverage(0, 0, 0, 0);

	for (int32 y = 0; y < SourceImageData.SizeY; ++y)
	{
		for (int32 x = 0; x < SourceImageData.SizeX; ++x)
		{
			// Sample channel values at pixel neighborhood
			FVector4 PixelValue (LookupSourceMip<AddressMode>(SourceImageData, x, y));

			// Calculate coverage for each channel (if being used as an alpha mask)
			for (int32 i = 0; i < 4; ++i)
			{
				// Skip channel if Threshold is 0
				if (Thresholds[i] == 0)
				{
					continue;
				}

				if (PixelValue[i] * Scales[i] >= Thresholds[i])
				{
					++Coverage[i];
				}
			}
		}
	}

	return Coverage / float(SourceImageData.SizeX * SourceImageData.SizeY);
}

template <EMipGenAddressMode AddressMode>
static FVector4 ComputeAlphaScale(const FVector4& Coverages, const FVector4& AlphaThresholds, const FImageView2D& SourceImageData)
{
	FVector4 MinAlphaScales (0, 0, 0, 0);
	FVector4 MaxAlphaScales (4, 4, 4, 4);
	FVector4 AlphaScales (1, 1, 1, 1);

	//Binary Search to find Alpha Scale
	for (int32 i = 0; i < 8; ++i)
	{
		FVector4 ComputedCoverages = ComputeAlphaCoverage<AddressMode>(AlphaThresholds, AlphaScales, SourceImageData);

		for (int32 j = 0; j < 4; ++j)
		{
			if (AlphaThresholds[j] == 0 || fabs(ComputedCoverages[j] - Coverages[j]) < KINDA_SMALL_NUMBER)
			{
				continue;
			}

			if (ComputedCoverages[j] < Coverages[j])
			{
				MinAlphaScales[j] = AlphaScales[j];
			}
			else if (ComputedCoverages[j] > Coverages[j])
			{
				MaxAlphaScales[j] = AlphaScales[j];
			}

			AlphaScales[j] = (MinAlphaScales[j] + MaxAlphaScales[j]) * 0.5f;
		}

		if (ComputedCoverages.Equals(Coverages))
		{
			break;
		}
	}

	return AlphaScales;
}


/**
* Generates a mip-map for an 2D B8G8R8A8 image using a 4x4 filter with sharpening
* @param SourceImageData - The source image's data.
* @param DestImageData - The destination image's data.
* @param ImageFormat - The format of both the source and destination images.
* @param FilterTable2D - [FilterTableSize * FilterTableSize]
* @param FilterTableSize - >= 2
* @param ScaleFactor 1 / 2:for downsampling
*/
template <EMipGenAddressMode AddressMode>
static void GenerateSharpenedMipB8G8R8A8Templ(
	const FImageView2D& SourceImageData, 
	FImageView2D& DestImageData, 
	bool bDitherMipMapAlpha,
	FVector4 AlphaCoverages,
	FVector4 AlphaThresholds,
	const FImageKernel2D& Kernel,
	uint32 ScaleFactor,
	bool bSharpenWithoutColorShift )
{
	check( SourceImageData.SizeX == ScaleFactor * DestImageData.SizeX || DestImageData.SizeX == 1 );
	check( SourceImageData.SizeY == ScaleFactor * DestImageData.SizeY || DestImageData.SizeY == 1 );
	check( Kernel.GetFilterTableSize() >= 2 );

	const int32 KernelCenter = (int32)Kernel.GetFilterTableSize() / 2 - 1;

	// Set up a random number stream for dithering.
	FRandomStream RandomStream(0);

	FVector4 AlphaScale(1, 1, 1, 1);
	if (AlphaThresholds != FVector4(0,0,0,0))
	{
		AlphaScale = ComputeAlphaScale<AddressMode>(AlphaCoverages, AlphaThresholds, SourceImageData);
	}
	
	for ( int32 DestY = 0;DestY < DestImageData.SizeY; DestY++ )
	{
		for ( int32 DestX = 0;DestX < DestImageData.SizeX; DestX++ )
		{
			const int32 SourceX = DestX * ScaleFactor;
			const int32 SourceY = DestY * ScaleFactor;

			FLinearColor FilteredColor(0, 0, 0, 0);

			if ( bSharpenWithoutColorShift )
			{
				FLinearColor SharpenedColor(0, 0, 0, 0);

				for ( uint32 KernelY = 0; KernelY < Kernel.GetFilterTableSize();  ++KernelY )
				{
					for ( uint32 KernelX = 0; KernelX < Kernel.GetFilterTableSize();  ++KernelX )
					{
						float Weight = Kernel.GetAt( KernelX, KernelY );
						FLinearColor Sample = LookupSourceMip<AddressMode>( SourceImageData, SourceX + KernelX - KernelCenter, SourceY + KernelY - KernelCenter );
						SharpenedColor += Weight * Sample;
					}
				}

				float NewLuminance = SharpenedColor.ComputeLuminance();

				// simple 2x2 kernel to compute the color
				FilteredColor =
					( LookupSourceMip<AddressMode>( SourceImageData, SourceX + 0, SourceY + 0 )
					+ LookupSourceMip<AddressMode>( SourceImageData, SourceX + 1, SourceY + 0 )
					+ LookupSourceMip<AddressMode>( SourceImageData, SourceX + 0, SourceY + 1 )
					+ LookupSourceMip<AddressMode>( SourceImageData, SourceX + 1, SourceY + 1 ) ) * 0.25f;

				float OldLuminance = FilteredColor.ComputeLuminance();

				if ( OldLuminance > 0.001f )
				{
					float Factor = NewLuminance / OldLuminance;
					FilteredColor.R *= Factor;
					FilteredColor.G *= Factor;
					FilteredColor.B *= Factor;
				}

				// We also want to sharpen the alpha channel (was missing before)
				FilteredColor.A = SharpenedColor.A;
			}
			else
			{
				for ( uint32 KernelY = 0; KernelY < Kernel.GetFilterTableSize();  ++KernelY )
				{
					for ( uint32 KernelX = 0; KernelX < Kernel.GetFilterTableSize();  ++KernelX )
					{
						float Weight = Kernel.GetAt( KernelX, KernelY );
						FLinearColor Sample = LookupSourceMip<AddressMode>( SourceImageData, SourceX + KernelX - KernelCenter, SourceY + KernelY - KernelCenter );
						FilteredColor += Weight	* Sample;
					}
				}
			}

			// Apply computed alpha scales to each channel		
			FilteredColor.R *= AlphaScale.X;
			FilteredColor.G *= AlphaScale.Y;
			FilteredColor.B *= AlphaScale.Z;
			FilteredColor.A *= AlphaScale.W;


			if ( bDitherMipMapAlpha )
			{
				// Dither the alpha of any pixel which passes an alpha threshold test.
				const int32 DitherAlphaThreshold = 5.0f / 255.0f;
				const float MinRandomAlpha = 85.0f;
				const float MaxRandomAlpha = 255.0f;

				if ( FilteredColor.A > DitherAlphaThreshold)
				{
					FilteredColor.A = FMath::TruncToInt( FMath::Lerp( MinRandomAlpha, MaxRandomAlpha, RandomStream.GetFraction() ) );
				}
			}

			// Set the destination pixel.
			//FLinearColor& DestColor = *(DestImageData.AsRGBA32F() + DestX + DestY * DestImageData.SizeX);
			FLinearColor& DestColor = DestImageData.Access(DestX, DestY);
			DestColor = FilteredColor;
		}
	}
}

// to switch conveniently between different texture wrapping modes for the mip map generation
// the template can optimize the inner loop using a constant AddressMode
static void GenerateSharpenedMipB8G8R8A8(
	const FImageView2D& SourceImageData, 
	FImageView2D& DestImageData, 
	EMipGenAddressMode AddressMode, 
	bool bDitherMipMapAlpha,
	FVector4 AlphaCoverages,
	FVector4 AlphaThresholds,
	const FImageKernel2D &Kernel,
	uint32 ScaleFactor,
	bool bSharpenWithoutColorShift
	)
{
	switch(AddressMode)
	{
	case MGTAM_Wrap:
		GenerateSharpenedMipB8G8R8A8Templ<MGTAM_Wrap>(SourceImageData, DestImageData, bDitherMipMapAlpha, AlphaCoverages, AlphaThresholds, Kernel, ScaleFactor, bSharpenWithoutColorShift);
		break;
	case MGTAM_Clamp:
		GenerateSharpenedMipB8G8R8A8Templ<MGTAM_Clamp>(SourceImageData, DestImageData, bDitherMipMapAlpha, AlphaCoverages, AlphaThresholds, Kernel, ScaleFactor, bSharpenWithoutColorShift);
		break;
	case MGTAM_BorderBlack:
		GenerateSharpenedMipB8G8R8A8Templ<MGTAM_BorderBlack>(SourceImageData, DestImageData, bDitherMipMapAlpha, AlphaCoverages, AlphaThresholds, Kernel, ScaleFactor, bSharpenWithoutColorShift);
		break;
	default:
		check(0);
	}
}

// Update border texels after normal mip map generation to preserve the colors there (useful for particles and decals).
static void GenerateMipBorder(
	const FImageView2D& SrcImageData, 
	FImageView2D& DestImageData
	)
{
	check( SrcImageData.SizeX == 2 * DestImageData.SizeX || DestImageData.SizeX == 1 );
	check( SrcImageData.SizeY == 2 * DestImageData.SizeY || DestImageData.SizeY == 1 );

	for ( int32 DestY = 0; DestY < DestImageData.SizeY; DestY++ )
	{
		for ( int32 DestX = 0; DestX < DestImageData.SizeX; )
		{
			FLinearColor FilteredColor(0, 0, 0, 0);
			{
				float WeightSum = 0.0f;
				for ( int32 KernelY = 0; KernelY < 2;  ++KernelY )
				{
					for ( int32 KernelX = 0; KernelX < 2;  ++KernelX )
					{
						const int32 SourceX = DestX * 2 + KernelX;
						const int32 SourceY = DestY * 2 + KernelY;

						// only average the source border
						if ( SourceX == 0 ||
							SourceX == SrcImageData.SizeX - 1 ||
							SourceY == 0 ||
							SourceY == SrcImageData.SizeY - 1 )
						{
							FLinearColor Sample = LookupSourceMip<MGTAM_Wrap>( SrcImageData, SourceX, SourceY );
							FilteredColor += Sample;
							WeightSum += 1.0f;
						}
					}
				}
				FilteredColor /= WeightSum;
			}

			// Set the destination pixel.
			//FLinearColor& DestColor = *(DestImageData.AsRGBA32F() + DestX + DestY * DestImageData.SizeX);
			FLinearColor& DestColor = DestImageData.Access(DestX, DestY);
			DestColor = FilteredColor;

			++DestX;

			if ( DestY > 0 &&
				DestY < DestImageData.SizeY - 1 &&
				DestX > 0 &&
				DestX < DestImageData.SizeX - 1 )
			{
				// jump over the non border area
				DestX += FMath::Max( 1, DestImageData.SizeX - 2 );
			}
		}
	}
}

// how should be treat lookups outside of the image
static EMipGenAddressMode ComputeAdressMode(const FTextureBuildSettings& Settings)
{
	EMipGenAddressMode AddressMode = MGTAM_Wrap;

	if(Settings.bPreserveBorder)
	{
		AddressMode = Settings.bBorderColorBlack ? MGTAM_BorderBlack : MGTAM_Clamp;
	}

	return AddressMode;
}

static void GenerateTopMip(const FImage& SrcImage, FImage& DestImage, const FTextureBuildSettings& Settings)
{
	EMipGenAddressMode AddressMode = ComputeAdressMode(Settings);

	FImageKernel2D KernelDownsample;
	// /2 as input resolution is same as output resolution and the settings assumed the output is half resolution
	KernelDownsample.BuildSeparatableGaussWithSharpen( FMath::Max( 2u, Settings.SharpenMipKernelSize / 2 ), Settings.MipSharpening );
	
	DestImage.Init(SrcImage.SizeX, SrcImage.SizeY, SrcImage.NumSlices, SrcImage.Format, SrcImage.GammaSpace);

	for (int32 SliceIndex = 0; SliceIndex < SrcImage.NumSlices; ++SliceIndex)
	{
		FImageView2D SrcView((FImage&)SrcImage, SliceIndex);
		FImageView2D DestView(DestImage, SliceIndex);

		// generate DestImage: down sample with sharpening
		GenerateSharpenedMipB8G8R8A8(
			SrcView, 
			DestView,
			AddressMode,
			Settings.bDitherMipMapAlpha,
			FVector4(0, 0, 0, 0),
			FVector4(0, 0, 0, 0),
			KernelDownsample,
			1,
			Settings.bSharpenWithoutColorShift
			);
	}
}

/**
 * Generate a full mip chain. The input mip chain must have one or more mips.
 * @param Settings - Preprocess settings.
 * @param BaseImage - An image that will serve as the source for the generation of the mip chain.
 * @param OutMipChain - An array that will contain the resultant mip images. Generated mip levels are appended to the array.
 * @param MipChainDepth - number of mip images to produce. Mips chain is finished when either a 1x1 mip is produced or 'MipChainDepth' images have been produced.
 */
static void GenerateMipChain(
	const FTextureBuildSettings& Settings,
	const FImage& BaseImage,
	TArray<FImage> &OutMipChain,
	uint32 MipChainDepth = MAX_uint32
	)
{
	check(BaseImage.Format == ERawImageFormat::RGBA32F);

	const FImage& BaseMip = BaseImage;
	const int32 SrcWidth = BaseMip.SizeX;
	const int32 SrcHeight= BaseMip.SizeY;
	const int32 SrcNumSlices = BaseMip.NumSlices;
	const ERawImageFormat::Type ImageFormat = ERawImageFormat::RGBA32F;
	FVector4 AlphaScales(1, 1, 1, 1);
	FVector4 AlphaCoverages(0, 0, 0, 0);

	// space for one source mip and one destination mip
	FImage IntermediateSrc(SrcWidth, SrcHeight, SrcNumSlices, ImageFormat);
	FImage IntermediateDst(FMath::Max<uint32>( 1, SrcWidth >> 1 ), FMath::Max<uint32>( 1, SrcHeight >> 1 ), SrcNumSlices, ImageFormat);

	// copy base mip
	BaseMip.CopyTo(IntermediateSrc, ERawImageFormat::RGBA32F, EGammaSpace::Linear);

	// Filtering kernels.
	FImageKernel2D KernelSimpleAverage;
	FImageKernel2D KernelDownsample;
	KernelSimpleAverage.BuildSeparatableGaussWithSharpen( 2 );
	KernelDownsample.BuildSeparatableGaussWithSharpen( Settings.SharpenMipKernelSize, Settings.MipSharpening );

	EMipGenAddressMode AddressMode = ComputeAdressMode(Settings);
	bool bReDrawBorder = false;
	if( Settings.bPreserveBorder )
	{
		bReDrawBorder = !Settings.bBorderColorBlack;
	}

	// Calculate alpha coverage value to preserve along mip chain
	if (Settings.AlphaCoverageThresholds != FVector4(0,0,0,0))
	{
		FImageView2D IntermediateSrcView(IntermediateSrc, 0);
		switch (AddressMode)
		{
		case MGTAM_Wrap:
			AlphaCoverages = ComputeAlphaCoverage<MGTAM_Wrap>(Settings.AlphaCoverageThresholds, AlphaScales, IntermediateSrcView);
			break;
		case MGTAM_Clamp:
			AlphaCoverages = ComputeAlphaCoverage<MGTAM_Clamp>(Settings.AlphaCoverageThresholds, AlphaScales, IntermediateSrcView);
			break;
		case MGTAM_BorderBlack:
			AlphaCoverages = ComputeAlphaCoverage<MGTAM_BorderBlack>(Settings.AlphaCoverageThresholds, AlphaScales, IntermediateSrcView);
			break;
		default:
			check(0);
		}		
	}

	// Generate mips
	for (; MipChainDepth != 0 ; --MipChainDepth)
	{
		FImage& DestImage = *new(OutMipChain) FImage(IntermediateDst.SizeX, IntermediateDst.SizeY, SrcNumSlices, ImageFormat);
		
		for (int32 SliceIndex = 0; SliceIndex < SrcNumSlices; ++SliceIndex)
		{
			FImageView2D IntermediateSrcView(IntermediateSrc, SliceIndex);
			FImageView2D DestView(DestImage, SliceIndex);
			FImageView2D IntermediateDstView(IntermediateDst, SliceIndex);

			// generate DestImage: down sample with sharpening
			GenerateSharpenedMipB8G8R8A8(
				IntermediateSrcView, 
				DestView,
				AddressMode,
				Settings.bDitherMipMapAlpha,
				AlphaCoverages,
				Settings.AlphaCoverageThresholds,
				KernelDownsample,
				2,
				Settings.bSharpenWithoutColorShift
				);

			// generate IntermediateDstImage:
			if ( Settings.bDownsampleWithAverage )
			{
				// down sample without sharpening for the next iteration
				GenerateSharpenedMipB8G8R8A8(
					IntermediateSrcView,
					IntermediateDstView,
					AddressMode,
					Settings.bDitherMipMapAlpha,
					AlphaCoverages,
					Settings.AlphaCoverageThresholds,
					KernelSimpleAverage,
					2,
					Settings.bSharpenWithoutColorShift
					);
			}
		}

		if ( Settings.bDownsampleWithAverage == false )
		{
			FMemory::Memcpy( IntermediateDst.AsRGBA32F(), DestImage.AsRGBA32F(),
				IntermediateDst.SizeX * IntermediateDst.SizeY * SrcNumSlices * sizeof(FLinearColor) );
		}

		if ( bReDrawBorder )
		{
			for (int32 SliceIndex = 0; SliceIndex < SrcNumSlices; ++SliceIndex)
			{
				FImageView2D IntermediateSrcView(IntermediateSrc, SliceIndex);
				FImageView2D DestView(DestImage, SliceIndex);
				FImageView2D IntermediateDstView(IntermediateDst, SliceIndex);
				GenerateMipBorder( IntermediateSrcView, DestView );
				GenerateMipBorder( IntermediateSrcView, IntermediateDstView );
			}
		}

		// Once we've created mip-maps down to 1x1, we're done.
		if ( IntermediateDst.SizeX == 1 && IntermediateDst.SizeY == 1 )
		{
			break;
		}

		// last destination becomes next source
		FMemory::Memcpy(IntermediateSrc.AsRGBA32F(), IntermediateDst.AsRGBA32F(),
			IntermediateDst.SizeX * IntermediateDst.SizeY * SrcNumSlices * sizeof(FLinearColor));

		// Sizes for the next iteration.
		IntermediateSrc.SizeX = FMath::Max<uint32>( 1, IntermediateSrc.SizeX >> 1 );
		IntermediateSrc.SizeY = FMath::Max<uint32>( 1, IntermediateSrc.SizeY >> 1 );
		IntermediateDst.SizeX = FMath::Max<uint32>( 1, IntermediateDst.SizeX >> 1 );
		IntermediateDst.SizeY = FMath::Max<uint32>( 1, IntermediateDst.SizeY >> 1 );
	}
}

/*------------------------------------------------------------------------------
	Angular Filtering for HDR Cubemaps.
------------------------------------------------------------------------------*/

/**
 * View in to an image that allows access by converting a direction to longitude and latitude.
 */
struct FImageViewLongLat
{
	/** Image colors. */
	FLinearColor* ImageColors;
	/** Width of the image. */
	int32 SizeX;
	/** Height of the image. */
	int32 SizeY;

	/** Initialization constructor. */
	explicit FImageViewLongLat(FImage& Image)
	{
		ImageColors = Image.AsRGBA32F();
		SizeX = Image.SizeX;
		SizeY = Image.SizeY;
	}

	/** Wraps X around W. */
	static void WrapTo(int32& X, int32 W)
	{
		X = X % W;

		if(X < 0)
		{
			X += W;
		}
	}

	/** Const access to a texel. */
	FLinearColor Access(int32 X, int32 Y) const
	{
		return ImageColors[X + Y * SizeX];
	}

	/** Makes a filtered lookup. */
	FLinearColor LookupFiltered(float X, float Y) const
	{
		int32 X0 = (int32)floor(X);
		int32 Y0 = (int32)floor(Y);

		float FracX = X - X0;
		float FracY = Y - Y0;

		int32 X1 = X0 + 1;
		int32 Y1 = Y0 + 1;

		WrapTo(X0, SizeX);
		WrapTo(X1, SizeX);
		Y0 = FMath::Clamp(Y0, 0, (int32)(SizeY - 1));
		Y1 = FMath::Clamp(Y1, 0, (int32)(SizeY - 1));

		FLinearColor CornerRGB00 = Access(X0, Y0);
		FLinearColor CornerRGB10 = Access(X1, Y0);
		FLinearColor CornerRGB01 = Access(X0, Y1);
		FLinearColor CornerRGB11 = Access(X1, Y1);

		FLinearColor CornerRGB0 = FMath::Lerp(CornerRGB00, CornerRGB10, FracX);
		FLinearColor CornerRGB1 = FMath::Lerp(CornerRGB01, CornerRGB11, FracX);

		return FMath::Lerp(CornerRGB0, CornerRGB1, FracY);
	}

	/** Makes a filtered lookup using a direction. */
	FLinearColor LookupLongLat(FVector NormalizedDirection) const
	{
		// see http://gl.ict.usc.edu/Data/HighResProbes
		// latitude-longitude panoramic format = equirectangular mapping

		float X = (1 + atan2(NormalizedDirection.X, - NormalizedDirection.Z) / PI) / 2 * SizeX;
		float Y = acos(NormalizedDirection.Y) / PI * SizeY;

		return LookupFiltered(X, Y);
	}
};

// transform world space vector to a space relative to the face
static FVector TransformSideToWorldSpace(uint32 CubemapFace, FVector InDirection)
{
	float x = InDirection.X, y = InDirection.Y, z = InDirection.Z;

	FVector Ret = FVector(0, 0, 0);

	// see http://msdn.microsoft.com/en-us/library/bb204881(v=vs.85).aspx
	switch(CubemapFace)
	{
		case 0: Ret = FVector(+z, -y, -x); break;
		case 1: Ret = FVector(-z, -y, +x); break;
		case 2: Ret = FVector(+x, +z, +y); break;
		case 3: Ret = FVector(+x, -z, -y); break;
		case 4: Ret = FVector(+x, -y, +z); break;
		case 5: Ret = FVector(-x, -y, -z); break;
		default:
			checkSlow(0);
	}

	// this makes it with the Unreal way (z and y are flipped)
	return FVector(Ret.X, Ret.Z, Ret.Y);
}

// transform vector relative to the face to world space
static FVector TransformWorldToSideSpace(uint32 CubemapFace, FVector InDirection)
{
	// undo Unreal way (z and y are flipped)
	float x = InDirection.X, y = InDirection.Z, z = InDirection.Y;

	FVector Ret = FVector(0, 0, 0); 

	// see http://msdn.microsoft.com/en-us/library/bb204881(v=vs.85).aspx
	switch(CubemapFace)
	{
		case 0: Ret = FVector(-z, -y, +x); break;
		case 1: Ret = FVector(+z, -y, -x); break;
		case 2: Ret = FVector(+x, +z, +y); break;
		case 3: Ret = FVector(+x, -z, -y); break;
		case 4: Ret = FVector(+x, -y, +z); break;
		case 5: Ret = FVector(-x, -y, -z); break;
		default:
			checkSlow(0);
	}

	return Ret;
}

FVector ComputeSSCubeDirectionAtTexelCenter(uint32 x, uint32 y, float InvSideExtent)
{
	// center of the texels
	FVector DirectionSS((x + 0.5f) * InvSideExtent * 2 - 1, (y + 0.5f) * InvSideExtent * 2 - 1, 1);
	DirectionSS.Normalize();
	return DirectionSS;
}

static FVector ComputeWSCubeDirectionAtTexelCenter(uint32 CubemapFace, uint32 x, uint32 y, float InvSideExtent)
{
	FVector DirectionSS = ComputeSSCubeDirectionAtTexelCenter(x, y, InvSideExtent);
	FVector DirectionWS = TransformSideToWorldSpace(CubemapFace, DirectionSS);
	return DirectionWS;
}

static int32 ComputeLongLatCubemapExtents(const FImage& SrcImage, const int32 MaxCubemapTextureResolution)
{
	return FMath::Clamp(1 << FMath::FloorLog2(SrcImage.SizeX / 2), 32, MaxCubemapTextureResolution);
}

/**
 * Generates the base cubemap mip from a longitude-latitude 2D image.
 * @param OutMip - The output mip.
 * @param SrcImage - The source longlat image.
 */
static void GenerateBaseCubeMipFromLongitudeLatitude2D(FImage* OutMip, const FImage& SrcImage, const int32 MaxCubemapTextureResolution)
{
	FImage LongLatImage;
	SrcImage.CopyTo(LongLatImage, ERawImageFormat::RGBA32F, EGammaSpace::Linear);
	FImageViewLongLat LongLatView(LongLatImage);

	// TODO_TEXTURE: Expose target size to user.
	int32 Extent = ComputeLongLatCubemapExtents(LongLatImage, MaxCubemapTextureResolution);
	float InvExtent = 1.0f / Extent;
	OutMip->Init(Extent, Extent, 6, ERawImageFormat::RGBA32F, EGammaSpace::Linear);

	for(uint32 Face = 0; Face < 6; ++Face)
	{
		FImageView2D MipView(*OutMip, Face);
		for(int32 y = 0; y < Extent; ++y)
		{
			for(int32 x = 0; x < Extent; ++x)
			{
				FVector DirectionWS = ComputeWSCubeDirectionAtTexelCenter(Face, x, y, InvExtent);
				MipView.Access(x, y) = LongLatView.LookupLongLat(DirectionWS);
			}
		}
	}
}

class FTexelProcessor
{
public:
	// @param InConeAxisSS - normalized, in side space
	// @param TexelAreaArray - precomputed area of each texel for correct weighting
	FTexelProcessor(const FVector& InConeAxisSS, float ConeAngle, const FLinearColor* InSideData, const float* InTexelAreaArray, uint32 InFullExtent)
		: ConeAxisSS(InConeAxisSS)
		, AccumulatedColor(0, 0, 0, 0)
		, SideData(InSideData)
		, TexelAreaArray(InTexelAreaArray)
		, FullExtent(InFullExtent)
	{
		ConeAngleSin = sinf(ConeAngle);
		ConeAngleCos = cosf(ConeAngle);

		// *2 as the position is from -1 to 1
		// / InFullExtent as x and y is in the range 0..InFullExtent-1
		PositionToWorldScale = 2.0f / InFullExtent;
		InvFullExtent = 1.0f / FullExtent;

		// examples: 0 to diffuse convolution, 0.95f for glossy
		DirDot = FMath::Min(FMath::Cos(ConeAngle), 0.9999f);

		InvDirOneMinusDot = 1.0f / (1.0f - DirDot);

		// precomputed sqrt(2.0f * 2.0f + 2.0f * 2.0f)
		float Sqrt8 = 2.8284271f;
		RadiusToWorldScale = Sqrt8 / (float)InFullExtent;
	}

	// @return true: yes, traverse deeper, false: not relevant
	bool TestIfRelevant(uint32 x, uint32 y, uint32 LocalExtent) const
	{
		float HalfExtent = LocalExtent * 0.5f; 
		float U = (x + HalfExtent) * PositionToWorldScale - 1.0f;
		float V = (y + HalfExtent) * PositionToWorldScale - 1.0f;

		float SphereRadius = RadiusToWorldScale * LocalExtent;

		FVector SpherePos(U, V, 1);

		return FMath::SphereConeIntersection(SpherePos, SphereRadius, ConeAxisSS, ConeAngleSin, ConeAngleCos);
	}

	void Process(uint32 x, uint32 y)
	{
		const FLinearColor* In = &SideData[x + y * FullExtent];
		
		FVector DirectionSS = ComputeSSCubeDirectionAtTexelCenter(x, y, InvFullExtent);

		float DotValue = ConeAxisSS | DirectionSS;

		if(DotValue > DirDot)
		{
			// 0..1, 0=at kernel border..1=at kernel center
			float KernelWeight = 1.0f - (1.0f - DotValue) * InvDirOneMinusDot;

			// apply smoothstep function (softer, less linear result)
			KernelWeight = KernelWeight * KernelWeight * (3 - 2 * KernelWeight);

			float AreaCompensation = TexelAreaArray[x + y * FullExtent];
			// AreaCompensation would be need for correctness but seems it has a but
			// as it looks much better (no seam) without, the effect is minor so it's deactivated for now.
//			float Weight = KernelWeight * AreaCompensation;
			float Weight = KernelWeight;

			AccumulatedColor.R += Weight * In->R;
			AccumulatedColor.G += Weight * In->G;
			AccumulatedColor.B += Weight * In->B;
			AccumulatedColor.A += Weight;
		}
	}

	// normalized, in side space
	FVector ConeAxisSS;

	FLinearColor AccumulatedColor;

	// cached for better performance
	float ConeAngleSin;
	float ConeAngleCos;
	float PositionToWorldScale;
	float RadiusToWorldScale;
	float InvFullExtent;
	// 0 to diffuse convolution, 0.95f for glossy
	float DirDot;
	float InvDirOneMinusDot;

	/** [x + y * FullExtent] */
	const FLinearColor* SideData;
	const float* TexelAreaArray;
	uint32 FullExtent;
};

template <class TVisitor>
void TCubemapSideRasterizer(TVisitor &TexelProcessor, int32 x, uint32 y, uint32 Extent)
{
	if(Extent > 1)
	{
		if(!TexelProcessor.TestIfRelevant(x, y, Extent))
		{
			return;
		}
		Extent /= 2;

		TCubemapSideRasterizer(TexelProcessor, x, y, Extent);
		TCubemapSideRasterizer(TexelProcessor, x + Extent, y, Extent);
		TCubemapSideRasterizer(TexelProcessor, x, y + Extent, Extent);
		TCubemapSideRasterizer(TexelProcessor, x + Extent, y + Extent, Extent);
	}
	else
	{
		TexelProcessor.Process(x, y);
	}
}

static FLinearColor IntegrateAngularArea(FImage& Image, FVector FilterDirectionWS, float ConeAngle, const float* TexelAreaArray)
{
	// Alpha channel is used to renormalize later
	FLinearColor ret(0, 0, 0, 0);
	int32 Extent = Image.SizeX;

	for(uint32 Face = 0; Face < 6; ++Face)
	{
		FImageView2D ImageView(Image, Face);
		FVector FilterDirectionSS = TransformWorldToSideSpace(Face, FilterDirectionWS);
		FTexelProcessor Processor(FilterDirectionSS, ConeAngle, &ImageView.Access(0,0), TexelAreaArray, Extent);

		// recursively split the (0,0)-(Extent-1,Extent-1), tests for intersection and processes only colors inside
		TCubemapSideRasterizer(Processor, 0, 0, Extent);
		ret += Processor.AccumulatedColor;
	}
	
	if(ret.A != 0)
	{
		float Inv = 1.0f / ret.A;

		ret.R *= Inv;
		ret.G *= Inv;
		ret.B *= Inv;
	}
	else
	{
		// should not happen
//		checkSlow(0);
	}

	ret.A = 0;

	return ret;
}

// @return 2 * computed triangle area 
static inline float TriangleArea2_3D(FVector A, FVector B, FVector C)
{
	return ((A-B) ^ (C-B)).Size();
}

static inline float ComputeTexelArea(uint32 x, uint32 y, float InvSideExtentMul2)
{
	float fU = x * InvSideExtentMul2 - 1;
	float fV = y * InvSideExtentMul2 - 1;

	FVector CornerA = FVector(fU, fV, 1);
	FVector CornerB = FVector(fU + InvSideExtentMul2, fV, 1);
	FVector CornerC = FVector(fU, fV + InvSideExtentMul2, 1);
	FVector CornerD = FVector(fU + InvSideExtentMul2, fV + InvSideExtentMul2, 1);

	CornerA.Normalize();
	CornerB.Normalize();
	CornerC.Normalize();
	CornerD.Normalize();

	return TriangleArea2_3D(CornerA, CornerB, CornerC) + TriangleArea2_3D(CornerC, CornerB, CornerD) * 0.5f;
}

/**
 * Generate a mip using angular filtering.
 * @param DestMip - The filtered mip.
 * @param SrcMip - The source mip which will be filtered.
 * @param ConeAngle - The cone angle with which to filter.
 */
static void GenerateAngularFilteredMip(FImage* DestMip, FImage& SrcMip, float ConeAngle)
{
	int32 MipExtent = DestMip->SizeX;
	float MipInvSideExtent = 1.0f / MipExtent;

	TArray<float> TexelAreaArray;
	TexelAreaArray.AddUninitialized(SrcMip.SizeX * SrcMip.SizeY);

	// precompute the area size for one face (is the same for each face)
	for(int32 y = 0; y < SrcMip.SizeY; ++y)
	{
		for(int32 x = 0; x < SrcMip.SizeX; ++x)
		{
			TexelAreaArray[x + y * SrcMip.SizeX] = ComputeTexelArea(x, y, MipInvSideExtent * 2);
		}
	}

	// We start getting gains running threaded upwards of sizes >= 128
	if (SrcMip.SizeX >= 128)
	{
		// Quick workaround: Do a thread per mip
		struct FAsyncGenerateMipsPerFaceWorker : public FNonAbandonableTask
		{
			int32 Face;
			FImage* DestMip;
			int32 Extent;
			float ConeAngle;
			const float* TexelAreaArray;
			FImage* SrcMip;
			FAsyncGenerateMipsPerFaceWorker(int32 InFace, FImage* InDestMip, int32 InExtent, float InConeAngle, const float* InTexelAreaArray, FImage* InSrcMip) :
				Face(InFace),
				DestMip(InDestMip),
				Extent(InExtent),
				ConeAngle(InConeAngle),
				TexelAreaArray(InTexelAreaArray),
				SrcMip(InSrcMip)
			{
			}

			void DoWork()
			{
				const float InvSideExtent = 1.0f / Extent;
				FImageView2D DestMipView(*DestMip, Face);
				for (int32 y = 0; y < Extent; ++y)
				{
					for (int32 x = 0; x < Extent; ++x)
					{
						FVector DirectionWS = ComputeWSCubeDirectionAtTexelCenter(Face, x, y, InvSideExtent);
						DestMipView.Access(x, y) = IntegrateAngularArea(*SrcMip, DirectionWS, ConeAngle, TexelAreaArray);
					}
				}
			}

			FORCEINLINE TStatId GetStatId() const
			{
				RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncGenerateMipsPerFaceWorker, STATGROUP_ThreadPoolAsyncTasks);
			}
		};

		typedef FAsyncTask<FAsyncGenerateMipsPerFaceWorker> FAsyncGenerateMipsPerFaceTask;
		TIndirectArray<FAsyncGenerateMipsPerFaceTask> AsyncTasks;

		for (int32 Face = 0; Face < 6; ++Face)
		{
			auto* AsyncTask = new(AsyncTasks) FAsyncGenerateMipsPerFaceTask(Face, DestMip, MipExtent, ConeAngle, TexelAreaArray.GetData(), &SrcMip);
			AsyncTask->StartBackgroundTask();
		}

		for (int32 TaskIndex = 0; TaskIndex < AsyncTasks.Num(); ++TaskIndex)
		{
			auto& AsyncTask = AsyncTasks[TaskIndex];
			AsyncTask.EnsureCompletion();
		}
	}
	else
	{
		for (int32 Face = 0; Face < 6; ++Face)
		{
			FImageView2D DestMipView(*DestMip, Face);
			for (int32 y = 0; y < MipExtent; ++y)
			{
				for (int32 x = 0; x < MipExtent; ++x)
				{
					FVector DirectionWS = ComputeWSCubeDirectionAtTexelCenter(Face, x, y, MipInvSideExtent);
					DestMipView.Access(x, y) = IntegrateAngularArea(SrcMip, DirectionWS, ConeAngle, TexelAreaArray.GetData());
				}
			}
		}
	}
}

/**
 * Generates angularly filtered mips.
 * @param InOutMipChain - The mip chain to angularly filter.
 * @param NumMips - The number of mips the chain should have.
 * @param DiffuseConvolveMipLevel - The mip level that contains the diffuse convolution.
 */
static void GenerateAngularFilteredMips(TArray<FImage>& InOutMipChain, int32 NumMips, uint32 DiffuseConvolveMipLevel)
{
	TArray<FImage> SrcMipChain;
	Exchange(SrcMipChain, InOutMipChain);
	InOutMipChain.Empty(NumMips);

	// Generate simple averaged mips to accelerate angular filtering.
	for (int32 MipIndex = SrcMipChain.Num(); MipIndex < NumMips; ++MipIndex)
	{
		FImage& BaseMip = SrcMipChain[MipIndex - 1];
		int32 BaseExtent = BaseMip.SizeX;
		int32 MipExtent = FMath::Max(BaseExtent >> 1, 1);
		FImage* Mip = new(SrcMipChain) FImage(MipExtent, MipExtent, BaseMip.NumSlices, BaseMip.Format);

		for(int32 Face = 0; Face < 6; ++Face)
		{
			FImageView2D BaseMipView(BaseMip, Face);
			FImageView2D MipView(*Mip, Face);

			for(int32 y = 0; y < MipExtent; ++y)
			{
				for(int32 x = 0; x < MipExtent; ++x)
				{		
					FLinearColor Sum = (
						BaseMipView.Access(x*2, y*2) +
						BaseMipView.Access(x*2+1, y*2) +
						BaseMipView.Access(x*2, y*2+1) +
						BaseMipView.Access(x*2+1, y*2+1)
						) * 0.25f;
					MipView.Access(x,y) = Sum;
				}
			}
		}
	}

	int32 Extent = 1 << (NumMips - 1);
	int32 BaseExtent = Extent;
	for (int32 i = 0; i < NumMips; ++i)
	{
		// 0:top mip 1:lowest mip = diffuse convolve
		float NormalizedMipLevel = i / (float)(NumMips - DiffuseConvolveMipLevel);
		float AdjustedMipLevel = NormalizedMipLevel * NumMips;
		float NormalizedWidth = BaseExtent * FMath::Pow(2.0f, -AdjustedMipLevel);
		float TexelSize = 1.0f / NormalizedWidth;

		// 0.001f:sharp  .. PI/2: diffuse convolve
		// all lower mips are used for diffuse convolve
		// above that the angle blends from sharp to diffuse convolved version
		float ConeAngle = PI / 2.0f * TexelSize;

		// restrict to reasonable range
		ConeAngle = FMath::Clamp(ConeAngle, 0.002f, (float)PI / 2.0f);

		UE_LOG(LogTextureCompressor, Verbose, TEXT("GenerateAngularFilteredMips  %f %f %f %f %f"), NormalizedMipLevel, AdjustedMipLevel, NormalizedWidth, TexelSize, ConeAngle * 180 / PI);

		// 0:normal, -1:4x faster, +1:4 times slower but more precise, -2, 2 ...
		float QualityBias = 3.0f;

		// defined to result in a area of 1.0f (NormalizedArea)
		// optimized = 0.5f * FMath::Sqrt(1.0f / PI);
		float SphereRadius = 0.28209478f;
		float SegmentHeight = SphereRadius * (1.0f - FMath::Cos(ConeAngle));
		// compute SphereSegmentArea
		float AreaCoveredInNormalizedArea = 2 * PI * SphereRadius * SegmentHeight;
		checkSlow(AreaCoveredInNormalizedArea <= 0.5f);

		// unoptimized
		//	float FloatInputMip = FMath::Log2(FMath::Sqrt(AreaCoveredInNormalizedArea)) + InputMipCount - QualityBias;
		// optimized
		float FloatInputMip = 0.5f * FMath::Log2(AreaCoveredInNormalizedArea) + NumMips - QualityBias;
		uint32 InputMip = FMath::Clamp(FMath::TruncToInt(FloatInputMip), 0, NumMips - 1);

		FImage* Mip = new(InOutMipChain) FImage(Extent, Extent, 6, ERawImageFormat::RGBA32F);
		GenerateAngularFilteredMip(Mip, SrcMipChain[InputMip], ConeAngle);
		Extent = FMath::Max(Extent >> 1, 1);
	}
}

/*------------------------------------------------------------------------------
	Image Processing.
------------------------------------------------------------------------------*/

/**
 * Adjusts the colors of the image using the specified settings
 *
 * @param	Image			Image to adjust
 * @param	InBuildSettings	Image build settings
 */
static void AdjustImageColors( FImage& Image, const FTextureBuildSettings& InBuildSettings )
{
	const FColorAdjustmentParameters& InParams = InBuildSettings.ColorAdjustment;
	check( Image.SizeX > 0 && Image.SizeY > 0 );

	if( !FMath::IsNearlyEqual( InParams.AdjustBrightness, 1.0f, (float)KINDA_SMALL_NUMBER ) ||
		!FMath::IsNearlyEqual( InParams.AdjustBrightnessCurve, 1.0f, (float)KINDA_SMALL_NUMBER ) ||
		!FMath::IsNearlyEqual( InParams.AdjustSaturation, 1.0f, (float)KINDA_SMALL_NUMBER ) ||
		!FMath::IsNearlyEqual( InParams.AdjustVibrance, 0.0f, (float)KINDA_SMALL_NUMBER ) ||
		!FMath::IsNearlyEqual( InParams.AdjustRGBCurve, 1.0f, (float)KINDA_SMALL_NUMBER ) ||
		!FMath::IsNearlyEqual( InParams.AdjustHue, 0.0f, (float)KINDA_SMALL_NUMBER ) ||
		!FMath::IsNearlyEqual( InParams.AdjustMinAlpha, 0.0f, (float)KINDA_SMALL_NUMBER ) ||
		!FMath::IsNearlyEqual( InParams.AdjustMaxAlpha, 1.0f, (float)KINDA_SMALL_NUMBER ) ||
		InBuildSettings.bChromaKeyTexture )
	{
		const FLinearColor ChromaKeyTarget = InBuildSettings.ChromaKeyColor;
		const float ChromaKeyThreshold = InBuildSettings.ChromaKeyThreshold + SMALL_NUMBER;
		const int32 NumPixels = Image.SizeX * Image.SizeY * Image.NumSlices;
		FLinearColor* ImageColors = Image.AsRGBA32F();

		for( int32 CurPixelIndex = 0; CurPixelIndex < NumPixels; ++CurPixelIndex )
		{
			const FLinearColor OriginalColorRaw = ImageColors[ CurPixelIndex ];

			FLinearColor OriginalColor = OriginalColorRaw;
			if (InBuildSettings.bChromaKeyTexture && (OriginalColor.Equals(ChromaKeyTarget, ChromaKeyThreshold)))
			{
				OriginalColor = FLinearColor::Transparent;
			}

			// Convert to HSV
			FLinearColor HSVColor = OriginalColor.LinearRGBToHSV();
			float& PixelHue = HSVColor.R;
			float& PixelSaturation = HSVColor.G;
			float& PixelValue = HSVColor.B;

			// Apply brightness adjustment
			PixelValue *= InParams.AdjustBrightness;

			// Apply brightness power adjustment
			if( !FMath::IsNearlyEqual( InParams.AdjustBrightnessCurve, 1.0f, (float)KINDA_SMALL_NUMBER ) && InParams.AdjustBrightnessCurve != 0.0f )
			{
				// Raise HSV.V to the specified power
				PixelValue = FMath::Pow( PixelValue, InParams.AdjustBrightnessCurve );
			}

			// Apply "vibrance" adjustment
			if( !FMath::IsNearlyZero( InParams.AdjustVibrance, (float)KINDA_SMALL_NUMBER ) )
			{
				const float SatRaisePow = 5.0f;
				const float InvSatRaised = FMath::Pow( 1.0f - PixelSaturation, SatRaisePow );

				const float ClampedVibrance = FMath::Clamp( InParams.AdjustVibrance, 0.0f, 1.0f );
				const float HalfVibrance = ClampedVibrance * 0.5f;

				const float SatProduct = HalfVibrance * InvSatRaised;

				PixelSaturation += SatProduct;
			}

			// Apply saturation adjustment
			PixelSaturation *= InParams.AdjustSaturation;

			// Apply hue adjustment
			PixelHue += InParams.AdjustHue;

			// Clamp HSV values
			{
				PixelHue = FMath::Fmod( PixelHue, 360.0f );
				if( PixelHue < 0.0f )
				{
					// Keep the hue value positive as HSVToLinearRGB prefers that
					PixelHue += 360.0f;
				}
				PixelSaturation = FMath::Clamp( PixelSaturation, 0.0f, 1.0f );
				PixelValue = FMath::Clamp( PixelValue, 0.0f, 1.0f );
			}

			// Convert back to a linear color
			FLinearColor LinearColor = HSVColor.HSVToLinearRGB();

			// Apply RGB curve adjustment (linear space)
			if( !FMath::IsNearlyEqual( InParams.AdjustRGBCurve, 1.0f, (float)KINDA_SMALL_NUMBER ) && InParams.AdjustRGBCurve != 0.0f )
			{
				LinearColor.R = FMath::Pow( LinearColor.R, InParams.AdjustRGBCurve );
				LinearColor.G = FMath::Pow( LinearColor.G, InParams.AdjustRGBCurve );
				LinearColor.B = FMath::Pow( LinearColor.B, InParams.AdjustRGBCurve );
			}

			// Remap the alpha channel
			LinearColor.A = FMath::Lerp(InParams.AdjustMinAlpha, InParams.AdjustMaxAlpha, OriginalColor.A);
			ImageColors[ CurPixelIndex ] = LinearColor;
		}
	}
}

/**
 * Compute the alpha channel how BokehDOF needs it setup
 *
 * @param	Image	Image to adjust
 */
static void ComputeBokehAlpha(FImage& Image)
{
	check( Image.SizeX > 0 && Image.SizeY > 0 );

	const int32 NumPixels = Image.SizeX * Image.SizeY * Image.NumSlices;
	FLinearColor* ImageColors = Image.AsRGBA32F();

	// compute LinearAverage
	FLinearColor LinearAverage;
	{
		FLinearColor LinearSum(0, 0, 0, 0);
		for( int32 CurPixelIndex = 0; CurPixelIndex < NumPixels; ++CurPixelIndex )
		{
			LinearSum += ImageColors[ CurPixelIndex ];
		}
		LinearAverage = LinearSum / (float)NumPixels;
	}

	FLinearColor Scale(1, 1, 1, 1);

	// we want to normalize the image to have 0.5 as average luminance, this is assuming clamping doesn't happen (can happen when using a very small Bokeh shape)
	{
		float RGBLum = (LinearAverage.R + LinearAverage.G + LinearAverage.B) / 3.0f;

		// ideally this would be 1 but then some pixels would need to be >1 which is not supported for the textureformat we want to use.
		// The value affects the occlusion computation of the BokehDOF
		const float LumGoal = 0.25f;

		// clamp to avoid division by 0
		Scale *= LumGoal / FMath::Max(RGBLum, 0.001f);
	}

	{
		for( int32 CurPixelIndex = 0; CurPixelIndex < NumPixels; ++CurPixelIndex )
		{
			const FLinearColor OriginalColor = ImageColors[ CurPixelIndex ];

			// Convert to a linear color
			FLinearColor LinearColor = OriginalColor * Scale;
			float RGBLum = (LinearColor.R + LinearColor.G + LinearColor.B) / 3.0f;
			LinearColor.A = FMath::Clamp(RGBLum, 0.0f, 1.0f);
			ImageColors[ CurPixelIndex ] = LinearColor;
		}
	}
}

/**
 * Replicates the contents of the red channel to the green, blue, and alpha channels.
 */
static void ReplicateRedChannel( TArray<FImage>& InOutMipChain )
{
	const uint32 MipCount = InOutMipChain.Num();
	for ( uint32 MipIndex = 0; MipIndex < MipCount; ++MipIndex )
	{
		FImage& SrcMip = InOutMipChain[MipIndex];
		FLinearColor* FirstColor = SrcMip.AsRGBA32F();
		FLinearColor* LastColor = FirstColor + (SrcMip.SizeX * SrcMip.SizeY * SrcMip.NumSlices);
		for ( FLinearColor* Color = FirstColor; Color < LastColor; ++Color )
		{
			*Color = FLinearColor( Color->R, Color->R, Color->R, Color->R );
		}
	}
}

/**
 * Replicates the contents of the alpha channel to the red, green, and blue channels.
 */
static void ReplicateAlphaChannel( TArray<FImage>& InOutMipChain )
{
	const uint32 MipCount = InOutMipChain.Num();
	for ( uint32 MipIndex = 0; MipIndex < MipCount; ++MipIndex )
	{
		FImage& SrcMip = InOutMipChain[MipIndex];
		FLinearColor* FirstColor = SrcMip.AsRGBA32F();
		FLinearColor* LastColor = FirstColor + (SrcMip.SizeX * SrcMip.SizeY * SrcMip.NumSlices);
		for ( FLinearColor* Color = FirstColor; Color < LastColor; ++Color )
		{
			*Color = FLinearColor( Color->A, Color->A, Color->A, Color->A );
		}
	}
}

/**
 * Flips the contents of the green channel.
 * @param InOutMipChain - The mip chain on which the green channel shall be flipped.
 */
static void FlipGreenChannel( FImage& Image )
{
	FLinearColor* FirstColor = Image.AsRGBA32F();
	FLinearColor* LastColor = FirstColor + (Image.SizeX * Image.SizeY * Image.NumSlices);
	for ( FLinearColor* Color = FirstColor; Color < LastColor; ++Color )
	{
		Color->G = 1.0f - FMath::Clamp(Color->G, 0.0f, 1.0f);
	}
}

/**
 * Detects whether or not the image contains an alpha channel where at least one texel is != 255.
 */
static bool DetectAlphaChannel(const FImage& InImage)
{
	// Uncompressed data is required to check for an alpha channel.
	const FLinearColor* SrcColors = InImage.AsRGBA32F();
	const FLinearColor* LastColor = SrcColors + (InImage.SizeX * InImage.SizeY * InImage.NumSlices);
	while (SrcColors < LastColor)
	{
		if (SrcColors->A < (1.0f - SMALL_NUMBER))
		{
			return true;
		}
		++SrcColors;
	}
	return false;
}

float RoughnessToSpecularPower(float Roughness)
{
	float Div = FMath::Pow(Roughness, 4);

	// Roughness of 0 should result in a high specular power
	float MaxSpecPower = 10000000000.0f;
	Div = FMath::Max(Div, 2.0f / (MaxSpecPower + 2.0f));

	return 2.0f / Div - 2.0f;
}

float SpecularPowerToRoughness(float SpecularPower)
{
	float Out = FMath::Pow( SpecularPower * 0.5f + 1.0f, -0.25f );

	return Out;
}

// @param CompositeTextureMode original type ECompositeTextureMode
void ApplyCompositeTexture(FImage& RoughnessSourceMips, const FImage& NormalSourceMips, uint8 CompositeTextureMode, float CompositePower)
{
	check(RoughnessSourceMips.SizeX == NormalSourceMips.SizeX);
	check(RoughnessSourceMips.SizeY == NormalSourceMips.SizeY);

	FLinearColor* FirstColor = RoughnessSourceMips.AsRGBA32F();
	const FLinearColor* NormalColors = NormalSourceMips.AsRGBA32F();

	FLinearColor* LastColor = FirstColor + (RoughnessSourceMips.SizeX * RoughnessSourceMips.SizeY * RoughnessSourceMips.NumSlices);
	for ( FLinearColor* Color = FirstColor; Color < LastColor; ++Color, ++NormalColors )
	{
		FVector Normal = FVector(NormalColors->R * 2.0f - 1.0f, NormalColors->G * 2.0f - 1.0f, NormalColors->B * 2.0f - 1.0f);

		// to prevent crash for unknown CompositeTextureMode
		float Dummy;
		float* RefValue = &Dummy;

		switch((ECompositeTextureMode)CompositeTextureMode)
		{
			case CTM_NormalRoughnessToRed:
				RefValue = &Color->R;
				break;
			case CTM_NormalRoughnessToGreen:
				RefValue = &Color->G;
				break;
			case CTM_NormalRoughnessToBlue:
				RefValue = &Color->B;
				break;
			case CTM_NormalRoughnessToAlpha:
				RefValue = &Color->A;
				break;
			default:
				checkSlow(0);
		}
		
		// Toksvig estimation of variance
		float LengthN = FMath::Min( Normal.Size(), 1.0f );
		float Variance = ( 1.0f - LengthN ) / LengthN;
		Variance = FMath::Max( 0.0f, Variance - 0.00004f );

		Variance *= CompositePower;
		
		float Roughness = *RefValue;

#if 0
		float Power = RoughnessToSpecularPower( Roughness );
		Power = Power / ( 1.0f + Variance * Power );
		Roughness = SpecularPowerToRoughness( Power );
#else
		// Refactored above to avoid divide by zero
		float a = Roughness * Roughness;
		float a2 = a * a;
		float B = 2.0f * Variance * (a2 - 1.0f);
		a2 = ( B - a2 ) / ( B - 1.0f );
		Roughness = FMath::Pow( a2, 0.25f );
#endif

		*RefValue = Roughness;
	}
}

/*------------------------------------------------------------------------------
	Image Compression.
------------------------------------------------------------------------------*/

/**
 * Asynchronous compression, used for compressing mips simultaneously.
 */
class FAsyncCompressionWorker : public FNonAbandonableTask 
{
public:
	/**
	 * Initializes the data and creates the async compression task.
	 */
	FAsyncCompressionWorker(const ITextureFormat* InTextureFormat, const FImage* InImage, const FTextureBuildSettings& InBuildSettings, bool bInImageHasAlphaChannel)
		: TextureFormat(*InTextureFormat)
		, SourceImage(*InImage)
		, BuildSettings(InBuildSettings)
		, bImageHasAlphaChannel(bInImageHasAlphaChannel)
		, bCompressionResults(false)
	{
	}

	/**
	 * Compresses the texture
	 */
	void DoWork()
	{
		bCompressionResults = TextureFormat.CompressImage(
			SourceImage,
			BuildSettings,
			bImageHasAlphaChannel,
			CompressedImage
			);
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncCompressionWorker, STATGROUP_ThreadPoolAsyncTasks);
	}

	bool GetCompressionResults(FCompressedImage2D& OutCompressedImage) const
	{
		OutCompressedImage = CompressedImage;
		return bCompressionResults;
	}

private:

	/** Texture format interface with which to compress. */
	const ITextureFormat& TextureFormat;
	/** The image to compress. */
	const FImage& SourceImage;
	/** The resulting compressed image. */
	FCompressedImage2D CompressedImage;
	/** Build settings. */
	FTextureBuildSettings BuildSettings;
	/** true if the image has a non-white alpha channel. */
	bool bImageHasAlphaChannel;
	/** true if compression was successful. */
	bool bCompressionResults;
};
typedef FAsyncTask<FAsyncCompressionWorker> FAsyncCompressionTask;

FTextureFormatCompressorCaps GetTextureFormatCaps(const FTextureBuildSettings& Settings)
{
	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	if (TPM)
	{
		const ITextureFormat* TextureFormat = TPM->FindTextureFormat(Settings.TextureFormatName);
		if (TextureFormat != nullptr)
		{
			return TextureFormat->GetFormatCapabilities();
		}
	}
	
	return FTextureFormatCompressorCaps();
}

// compress mip-maps in InMipChain and add mips to Texture, might alter the source content
static bool CompressMipChain(
	const TArray<FImage>& MipChain,
	const FTextureBuildSettings& Settings,
	TArray<FCompressedImage2D>& OutMips
	)
{
	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	if (TPM)
	{
		const ITextureFormat* TextureFormat = TPM->FindTextureFormat(Settings.TextureFormatName);

		if (TextureFormat)
		{
			TIndirectArray<FAsyncCompressionTask> AsyncCompressionTasks;
			const int32 MipCount = MipChain.Num();
			const bool bImageHasAlphaChannel = DetectAlphaChannel(MipChain[0]);
			const int32 MinAsyncCompressionSize = 128;
			const bool bAllowParallelBuild = TextureFormat->AllowParallelBuild();
			bool bCompressionSucceeded = true;
			uint32 StartCycles = FPlatformTime::Cycles();

			OutMips.Empty(MipCount);
			for (int32 MipIndex = 0; MipIndex < MipCount; ++MipIndex)
			{
				const FImage& SrcMip = MipChain[MipIndex];
				FCompressedImage2D& DestMip = *new(OutMips) FCompressedImage2D;
				if (bAllowParallelBuild && FMath::Min(SrcMip.SizeX, SrcMip.SizeY) >= MinAsyncCompressionSize)
				{
					FAsyncCompressionTask* AsyncTask = new(AsyncCompressionTasks) FAsyncCompressionTask(
						TextureFormat,
						&SrcMip,
						Settings,
						bImageHasAlphaChannel
						);
#if WITH_EDITOR
					AsyncTask->StartBackgroundTask(GLargeThreadPool);
#else
					AsyncTask->StartBackgroundTask();
#endif
				}
				else
				{
					bCompressionSucceeded = bCompressionSucceeded && TextureFormat->CompressImage(
						SrcMip,
						Settings,
						bImageHasAlphaChannel,
						DestMip
						);
				}
			}

			for (int32 TaskIndex = 0; TaskIndex < AsyncCompressionTasks.Num(); ++TaskIndex)
			{
				FAsyncCompressionTask& AsynTask = AsyncCompressionTasks[TaskIndex];
				AsynTask.EnsureCompletion();
				FCompressedImage2D& DestMip = OutMips[TaskIndex];
				bCompressionSucceeded = bCompressionSucceeded && AsynTask.GetTask().GetCompressionResults(DestMip);
			}

			if (!bCompressionSucceeded)
			{
				OutMips.Empty();
			}

			uint32 EndCycles = FPlatformTime::Cycles();
			UE_LOG(LogTextureCompressor,Verbose,TEXT("Compressed %dx%dx%d %s in %fms"),
				MipChain[0].SizeX,
				MipChain[0].SizeY,
				MipChain[0].NumSlices,
				*Settings.TextureFormatName.ToString(),
				FPlatformTime::ToMilliseconds( EndCycles-StartCycles )
				);

			return bCompressionSucceeded;
		}
		else
		{
			UE_LOG(LogTextureCompressor, Warning,
				TEXT("Failed to find compressor for texture format '%s'."),
				*Settings.TextureFormatName.ToString()
				);
			return false;
		}
	}

	UE_LOG(LogTextureCompressor, Warning,
		TEXT("Failed to load target platform manager module. Unable to compress textures.")
		);
	return false;
}

// only useful for normal maps, fixed bad input (denormalized normals) and improved quality (quantization artifacts)
static void NormalizeMip(FImage& InOutMip)
{
	const uint32 NumPixels = InOutMip.SizeX * InOutMip.SizeY * InOutMip.NumSlices;
	FLinearColor* ImageColors = InOutMip.AsRGBA32F();
	for(uint32 CurPixelIndex = 0; CurPixelIndex < NumPixels; ++CurPixelIndex)
	{
		FLinearColor& Color = ImageColors[CurPixelIndex];

		FVector Normal = FVector(Color.R * 2.0f - 1.0f, Color.G * 2.0f - 1.0f, Color.B * 2.0f - 1.0f);

		Normal = Normal.GetSafeNormal();

		Color = FLinearColor(Normal.X * 0.5f + 0.5f, Normal.Y * 0.5f + 0.5f, Normal.Z * 0.5f + 0.5f, Color.A);
	}
}

/**
 * Texture compression module
 */
class FTextureCompressorModule : public ITextureCompressorModule
{
public:
	FTextureCompressorModule()
#if PLATFORM_WINDOWS
		:	nvTextureToolsHandle(0)
#endif	//PLATFORM_WINDOWS
	{
	}

	virtual bool BuildTexture(
		const TArray<FImage>& SourceMips,
		const TArray<FImage>& AssociatedNormalSourceMips,
		const FTextureBuildSettings& BuildSettings,
		TArray<FCompressedImage2D>& OutTextureMips
		)
	{
		TArray<FImage> IntermediateMipChain;

		if(!BuildTextureMips(SourceMips, BuildSettings, IntermediateMipChain))
		{
			return false;
		}

		// apply roughness adjustment depending on normal map variation
		if(AssociatedNormalSourceMips.Num())
		{
//			check AssociatedNormalSourceMips.Format; 

			TArray<FImage> IntermediateAssociatedNormalSourceMipChain;

			FTextureBuildSettings DefaultSettings;

			// helps to reduce aliasing further
			DefaultSettings.MipSharpening = -4.0f;
			DefaultSettings.SharpenMipKernelSize = 4;
			DefaultSettings.bApplyKernelToTopMip = true;
			// important to make accurate computation with normal length
			DefaultSettings.bRenormalizeTopMip = true;

			if(!BuildTextureMips(AssociatedNormalSourceMips, DefaultSettings, IntermediateAssociatedNormalSourceMipChain))
			{
				UE_LOG(LogTexture, Warning, TEXT("Failed to generate texture mips for composite texture"));
			}

			if(!ApplyCompositeTexture(IntermediateMipChain, IntermediateAssociatedNormalSourceMipChain, BuildSettings.CompositeTextureMode, BuildSettings.CompositePower))
			{
				UE_LOG(LogTexture, Warning, TEXT("Failed to apply composite texture"));
			}
		}

		// Set the correct biased texture size so that the compressor understands the original source image size
		// This is requires for platforms that may need to tile based on the original source texture size
		BuildSettings.TopMipSize.X = IntermediateMipChain[0].SizeX;
		BuildSettings.TopMipSize.Y = IntermediateMipChain[0].SizeY;

		return CompressMipChain(IntermediateMipChain, BuildSettings, OutTextureMips);
	}

	// IModuleInterface implementation.
	void StartupModule()
	{
#if PLATFORM_WINDOWS
	#if PLATFORM_64BITS
		nvTextureToolsHandle = FPlatformProcess::GetDllHandle(TEXT("../../../Engine/Binaries/ThirdParty/nvTextureTools/Win64/nvtt_64.dll"));
	#else	//32-bit platform
		nvTextureToolsHandle = FPlatformProcess::GetDllHandle(TEXT("../../../Engine/Binaries/ThirdParty/nvTextureTools/Win32/nvtt_.dll"));
	#endif
#endif	//PLATFORM_WINDOWS
	}

	void ShutdownModule()
	{
#if PLATFORM_WINDOWS
		FPlatformProcess::FreeDllHandle(nvTextureToolsHandle);
		nvTextureToolsHandle = 0;
#endif
	}

private:
#if PLATFORM_WINDOWS
	// Handle to the nvtt dll
	void* nvTextureToolsHandle;
#endif	//PLATFORM_WINDOWS

	bool BuildTextureMips(
		const TArray<FImage>& InSourceMips,
		const FTextureBuildSettings& BuildSettings,
		TArray<FImage>& OutMipChain)
	{
		check(InSourceMips.Num());
		check(InSourceMips[0].SizeX > 0 && InSourceMips[0].SizeY > 0 && InSourceMips[0].NumSlices > 0);
		const FTextureFormatCompressorCaps CompressorCaps = GetTextureFormatCaps(BuildSettings);

		// Identify long-lat cubemaps.
		bool bLongLatCubemap = BuildSettings.bCubemap && InSourceMips[0].NumSlices == 1;

		if (BuildSettings.bCubemap && InSourceMips[0].NumSlices != 6 && !bLongLatCubemap)
		{
			return false;
		}

		// Determine the maximum possible mip counts for source and dest.
		const int32 MaxSourceMipCount = bLongLatCubemap ?
			1 + FMath::CeilLogTwo(ComputeLongLatCubemapExtents(InSourceMips[0], BuildSettings.MaxTextureResolution)) :
			1 + FMath::CeilLogTwo(FMath::Max(InSourceMips[0].SizeX, InSourceMips[0].SizeY));
		const int32 MaxDestMipCount = 1 + FMath::CeilLogTwo(FMath::Min(CompressorCaps.MaxTextureDimension, BuildSettings.MaxTextureResolution));

		// Determine the number of mips required by BuildSettings.
		int32 NumOutputMips = (BuildSettings.MipGenSettings == TMGS_NoMipmaps) ? 1 : MaxSourceMipCount;
		NumOutputMips = FMath::Min(NumOutputMips, MaxDestMipCount);

		int32 NumSourceMips = InSourceMips.Num();

		if (BuildSettings.MipGenSettings != TMGS_LeaveExistingMips ||
			bLongLatCubemap)
		{
			NumSourceMips = 1;
		}

		TArray<FImage> PaddedSourceMips;

		{
			const FImage& FirstSourceMipImage = InSourceMips[0];
			int32 TargetTextureSizeX = FirstSourceMipImage.SizeX;
			int32 TargetTextureSizeY = FirstSourceMipImage.SizeY;
			bool bPadOrStretchTexture = false;

			const int32 PowerOfTwoTextureSizeX = FMath::RoundUpToPowerOfTwo(TargetTextureSizeX);
			const int32 PowerOfTwoTextureSizeY = FMath::RoundUpToPowerOfTwo(TargetTextureSizeY);
			switch (static_cast<const ETexturePowerOfTwoSetting::Type>(BuildSettings.PowerOfTwoMode))
			{
			case ETexturePowerOfTwoSetting::None:
				break;

			case ETexturePowerOfTwoSetting::PadToPowerOfTwo:
				bPadOrStretchTexture = true;
				TargetTextureSizeX = PowerOfTwoTextureSizeX;
				TargetTextureSizeY = PowerOfTwoTextureSizeY;
				break;

			case ETexturePowerOfTwoSetting::PadToSquarePowerOfTwo:
				bPadOrStretchTexture = true;
				TargetTextureSizeX = TargetTextureSizeY = FMath::Max<int32>(PowerOfTwoTextureSizeX, PowerOfTwoTextureSizeY);
				break;

			default:
				checkf(false, TEXT("Unknown entry in ETexturePowerOfTwoSetting::Type"));
				break;
			}

			if (bPadOrStretchTexture)
			{
				// Want to stretch or pad the texture
				bool bSuitableFormat = FirstSourceMipImage.Format == ERawImageFormat::RGBA32F;

				FImage Temp;
				if (!bSuitableFormat)
				{
					// convert to RGBA32F
					FirstSourceMipImage.CopyTo(Temp, ERawImageFormat::RGBA32F, EGammaSpace::Linear);
				}

				// space for one source mip and one destination mip
				const FImage& SourceImage = bSuitableFormat ? FirstSourceMipImage : Temp;
				FImage& TargetImage = *new (PaddedSourceMips) FImage(TargetTextureSizeX, TargetTextureSizeY, SourceImage.NumSlices, SourceImage.Format);
				FLinearColor FillColor = BuildSettings.PaddingColor;

				FLinearColor* TargetPtr = (FLinearColor*)TargetImage.RawData.GetData();
				FLinearColor* SourcePtr = (FLinearColor*)SourceImage.RawData.GetData();
				check(SourceImage.GetBytesPerPixel() == sizeof(FLinearColor));
				check(TargetImage.GetBytesPerPixel() == sizeof(FLinearColor));

				const int32 SourceBytesPerLine = SourceImage.SizeX * SourceImage.GetBytesPerPixel();
				const int32 DestBytesPerLine = TargetImage.SizeX * TargetImage.GetBytesPerPixel();
				for (int32 SliceIndex = 0; SliceIndex < SourceImage.NumSlices; ++SliceIndex)
				{
					for (int32 Y = 0; Y < TargetTextureSizeY; ++Y)
					{
						int32 XStart = 0;
						if (Y < SourceImage.SizeY)
						{
							XStart = SourceImage.SizeX;
							FMemory::Memcpy(TargetPtr, SourcePtr, SourceImage.SizeX * sizeof(FLinearColor));
							SourcePtr += SourceImage.SizeX;
							TargetPtr += SourceImage.SizeX;
						}

						for (int32 XPad = XStart; XPad < TargetImage.SizeX; ++XPad)
						{
							*TargetPtr++ = FillColor;
						}
					}
				}
			}
		}

		const TArray<FImage>& PostOptionalUpscaleSourceMips = (PaddedSourceMips.Num() > 0) ? PaddedSourceMips : InSourceMips;

		// See if the smallest provided mip image is still too large for the current compressor.
		int32 LevelsToUsableSource = FMath::Max(0, MaxSourceMipCount - MaxDestMipCount);
		int32 StartMip = FMath::Max(0, LevelsToUsableSource);
		bool bBuildSourceImage = StartMip > (NumSourceMips - 1);

		TArray<FImage> GeneratedSourceMips;
		if (bBuildSourceImage)
		{			
			// the source is larger than the compressor allows and no mip image exists to act as a smaller source.
			// We must generate a suitable source image:
			bool bSuitableFormat = PostOptionalUpscaleSourceMips.Last().Format == ERawImageFormat::RGBA32F;
			const FImage& BaseImage = PostOptionalUpscaleSourceMips.Last();

			if (BaseImage.SizeX != FMath::RoundUpToPowerOfTwo(BaseImage.SizeX) || BaseImage.SizeY != FMath::RoundUpToPowerOfTwo(BaseImage.SizeY))
			{
				UE_LOG(LogTextureCompressor, Warning,
					TEXT("Source image %dx%d (npot) prevents resizing and is too large for compressors max dimension (%d)."),
					BaseImage.SizeX,
					BaseImage.SizeY,
					CompressorCaps.MaxTextureDimension
					);
				return false;
			}

			FImage Temp;
			if (!bSuitableFormat)
			{
				// convert to RGBA32F
				BaseImage.CopyTo(Temp, ERawImageFormat::RGBA32F, EGammaSpace::Linear);
			}

			UE_LOG(LogTextureCompressor, Verbose,
				TEXT("Source image %dx%d too large for compressors max dimension (%d). Resizing."),
				BaseImage.SizeX,
				BaseImage.SizeY,
				CompressorCaps.MaxTextureDimension
				);
			GenerateMipChain(BuildSettings, bSuitableFormat ? BaseImage : Temp, GeneratedSourceMips, LevelsToUsableSource);

			check(GeneratedSourceMips.Num() != 0);
			// Note: The newly generated mip chain does not include the original top level mip.
			StartMip--;
		}

		const TArray<FImage>& SourceMips = bBuildSourceImage ? GeneratedSourceMips : PostOptionalUpscaleSourceMips;

		OutMipChain.Empty(NumOutputMips);
		// Copy over base mips.
		check(StartMip < SourceMips.Num());
		int32 CopyCount = SourceMips.Num() - StartMip;

		for (int32 MipIndex = StartMip; MipIndex < StartMip + CopyCount; ++MipIndex)
		{
			const FImage& Image = SourceMips[MipIndex];
			const int32 SrcWidth = Image.SizeX;
			const int32 SrcHeight = Image.SizeY;
			ERawImageFormat::Type MipFormat = ERawImageFormat::RGBA32F;

			// create base for the mip chain
			FImage* Mip = new(OutMipChain) FImage();

			if (bLongLatCubemap)
			{
				// Generate the base mip from the long-lat source image.
				GenerateBaseCubeMipFromLongitudeLatitude2D(Mip, Image, BuildSettings.MaxTextureResolution);
			}
			else
			{
				// copy base source content to the base of the mip chain
				if(BuildSettings.bApplyKernelToTopMip)
				{
					FImage Temp;
					
					Image.CopyTo(Temp, MipFormat, EGammaSpace::Linear);

					if(BuildSettings.bRenormalizeTopMip)
					{
						NormalizeMip(Temp);
					}

					GenerateTopMip(Temp, *Mip, BuildSettings);
				}
				else
				{
					Image.CopyTo(*Mip, MipFormat, EGammaSpace::Linear);

					if(BuildSettings.bRenormalizeTopMip)
					{
						NormalizeMip(*Mip);
					}
				}				
			}

			// Apply color adjustments
			AdjustImageColors(*Mip, BuildSettings);
			if (BuildSettings.bComputeBokehAlpha)
			{
				// To get the occlusion in the BokehDOF shader working for all Bokeh textures.
				ComputeBokehAlpha(*Mip);
			}
			if (BuildSettings.bFlipGreenChannel)
			{
				FlipGreenChannel(*Mip);
			}
		}

		// Generate any missing mips in the chain.
		if (NumOutputMips > OutMipChain.Num())
		{
			// Do angular filtering of cubemaps if requested.
			if (BuildSettings.bCubemap)
			{
				GenerateAngularFilteredMips(OutMipChain, NumOutputMips, BuildSettings.DiffuseConvolveMipLevel);
			}
			else
			{
				GenerateMipChain(BuildSettings, OutMipChain.Last(), OutMipChain);
			}
		}
		check(OutMipChain.Num() == NumOutputMips);

		// Apply post-mip generation adjustments.
		if (BuildSettings.bReplicateRed)
		{
			ReplicateRedChannel(OutMipChain);
		}
		else if (BuildSettings.bReplicateAlpha)
		{
			ReplicateAlphaChannel(OutMipChain);
		}

		return true;
	}

	// @param CompositeTextureMode original type ECompositeTextureMode
	// @return true on success, false on failure. Can fail due to bad mismatched dimensions of incomplete mip chains.
	bool ApplyCompositeTexture(TArray<FImage>& RoughnessSourceMips, const TArray<FImage>& NormalSourceMips, uint8 CompositeTextureMode, float CompositePower)
	{
		uint32 MinLevel = FMath::Min(RoughnessSourceMips.Num(), NormalSourceMips.Num());

		if( RoughnessSourceMips[RoughnessSourceMips.Num() - MinLevel].SizeX != NormalSourceMips[NormalSourceMips.Num() - MinLevel].SizeX || 
			RoughnessSourceMips[RoughnessSourceMips.Num() - MinLevel].SizeY != NormalSourceMips[NormalSourceMips.Num() - MinLevel].SizeY )
		{
			//incomplete mip chain or mismatched dimensions so bail
			return false;
		}

		for(uint32 Level = 0; Level < MinLevel; ++Level)
		{
			::ApplyCompositeTexture(RoughnessSourceMips[RoughnessSourceMips.Num() - 1 - Level], NormalSourceMips[NormalSourceMips.Num() - 1 - Level], CompositeTextureMode, CompositePower);
		}

		return true;
	}
};

IMPLEMENT_MODULE(FTextureCompressorModule, TextureCompressor)
