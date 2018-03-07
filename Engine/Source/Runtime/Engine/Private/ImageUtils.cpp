// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
ImageUtils.cpp: Image utility functions.
=============================================================================*/

#include "ImageUtils.h"
#include "Engine/Texture2D.h"
#include "Misc/ObjectThumbnail.h"
#include "Engine/TextureRenderTarget2D.h"
#include "CubemapUnwrapUtils.h"
#include "Logging/MessageLog.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogImageUtils, Log, All);

#define LOCTEXT_NAMESPACE "ImageUtils"

static bool GetRawData(UTextureRenderTarget2D* TexRT, TArray<uint8>& RawData)
{
	FRenderTarget* RenderTarget = TexRT->GameThread_GetRenderTargetResource();
	EPixelFormat Format = TexRT->GetFormat();

	int32 ImageBytes = CalculateImageBytes(TexRT->SizeX, TexRT->SizeY, 0, Format);
	RawData.AddUninitialized(ImageBytes);
	bool bReadSuccess = false;
	switch (Format)
	{
	case PF_FloatRGBA:
	{
		TArray<FFloat16Color> FloatColors;
		bReadSuccess = RenderTarget->ReadFloat16Pixels(FloatColors);
		FMemory::Memcpy(RawData.GetData(), FloatColors.GetData(), ImageBytes);
	}
	break;
	case PF_B8G8R8A8:
		bReadSuccess = RenderTarget->ReadPixelsPtr((FColor*)RawData.GetData());
		break;
	}
	if (bReadSuccess == false)
	{
		RawData.Empty();
	}
	return bReadSuccess;
}

/**
 * Resizes the given image using a simple average filter and stores it in the destination array.
 *
 * @param SrcWidth		Source image width.
 * @param SrcHeight		Source image height.
 * @param SrcData		Source image data.
 * @param DstWidth		Destination image width.
 * @param DstHeight		Destination image height.
 * @param DstData		Destination image data.
 * @param bLinearSpace	If true, convert colors into linear space before interpolating (slower but more accurate)
 */
void FImageUtils::ImageResize(int32 SrcWidth, int32 SrcHeight, const TArray<FColor> &SrcData, int32 DstWidth, int32 DstHeight, TArray<FColor> &DstData, bool bLinearSpace )
{
	DstData.Empty(DstWidth*DstHeight);
	DstData.AddZeroed(DstWidth*DstHeight);

	float SrcX = 0;
	float SrcY = 0;

	const float StepSizeX = SrcWidth / (float)DstWidth;
	const float StepSizeY = SrcHeight / (float)DstHeight;

	for(int32 Y=0; Y<DstHeight;Y++)
	{
		int32 PixelPos = Y * DstWidth;
		SrcX = 0.0f;	
	
		for(int32 X=0; X<DstWidth; X++)
		{
			int32 PixelCount = 0;
			float EndX = SrcX + StepSizeX;
			float EndY = SrcY + StepSizeY;
			
			// Generate a rectangular region of pixels and then find the average color of the region.
			int32 PosY = FMath::TruncToInt(SrcY+0.5f);
			PosY = FMath::Clamp<int32>(PosY, 0, (SrcHeight - 1));

			int32 PosX = FMath::TruncToInt(SrcX+0.5f);
			PosX = FMath::Clamp<int32>(PosX, 0, (SrcWidth - 1));

			int32 EndPosY = FMath::TruncToInt(EndY+0.5f);
			EndPosY = FMath::Clamp<int32>(EndPosY, 0, (SrcHeight - 1));

			int32 EndPosX = FMath::TruncToInt(EndX+0.5f);
			EndPosX = FMath::Clamp<int32>(EndPosX, 0, (SrcWidth - 1));

			FColor FinalColor;
			if(bLinearSpace)
			{
				FLinearColor LinearStepColor(0.0f,0.0f,0.0f,0.0f);
				for(int32 PixelX = PosX; PixelX <= EndPosX; PixelX++)
				{
					for(int32 PixelY = PosY; PixelY <= EndPosY; PixelY++)
					{
						int32 StartPixel =  PixelX + PixelY * SrcWidth;

						// Convert from gamma space to linear space before the addition.
						LinearStepColor += SrcData[StartPixel];
						PixelCount++;
					}
				}
				LinearStepColor /= (float)PixelCount;

				// Convert back from linear space to gamma space.
				FinalColor = LinearStepColor.ToFColor(true);
			}
			else
			{
				FVector StepColor(0,0,0);
				for(int32 PixelX = PosX; PixelX <= EndPosX; PixelX++)
				{
					for(int32 PixelY = PosY; PixelY <= EndPosY; PixelY++)
					{
						int32 StartPixel =  PixelX + PixelY * SrcWidth;
						StepColor.X += (float)SrcData[StartPixel].R;
						StepColor.Y += (float)SrcData[StartPixel].G;
						StepColor.Z += (float)SrcData[StartPixel].B;
						PixelCount++;
					}
				}
				StepColor /= (float)PixelCount;
				uint8 FinalR = FMath::Clamp(FMath::TruncToInt(StepColor.X), 0, 255);
				uint8 FinalG = FMath::Clamp(FMath::TruncToInt(StepColor.Y), 0, 255);
				uint8 FinalB = FMath::Clamp(FMath::TruncToInt(StepColor.Z), 0, 255);
				FinalColor = FColor(FinalR, FinalG, FinalB);
			}

			// Store the final averaged pixel color value.
			FinalColor.A = 255;
			DstData[PixelPos] = FinalColor;

			SrcX = EndX;	
			PixelPos++;
		}

		SrcY += StepSizeY;
	}
}

/**
 * Creates a 2D texture from a array of raw color data.
 *
 * @param SrcWidth		Source image width.
 * @param SrcHeight		Source image height.
 * @param SrcData		Source image data.
 * @param Outer			Outer for the texture object.
 * @param Name			Name for the texture object.
 * @param Flags			Object flags for the texture object.
 * @param InParams		Params about how to set up the texture.
 * @return				Returns a pointer to the constructed 2D texture object.
 *
 */
UTexture2D* FImageUtils::CreateTexture2D(int32 SrcWidth, int32 SrcHeight, const TArray<FColor> &SrcData, UObject* Outer, const FString& Name, const EObjectFlags &Flags, const FCreateTexture2DParameters& InParams)
{
#if WITH_EDITOR
	UTexture2D* Tex2D;

	Tex2D = NewObject<UTexture2D>(Outer, FName(*Name), Flags);
	Tex2D->Source.Init(SrcWidth, SrcHeight, /*NumSlices=*/ 1, /*NumMips=*/ 1, TSF_BGRA8);
	
	// Create base mip for the texture we created.
	uint8* MipData = Tex2D->Source.LockMip(0);
	for( int32 y=0; y<SrcHeight; y++ )
	{
		uint8* DestPtr = &MipData[(SrcHeight - 1 - y) * SrcWidth * sizeof(FColor)];
		FColor* SrcPtr = const_cast<FColor*>(&SrcData[(SrcHeight - 1 - y) * SrcWidth]);
		for( int32 x=0; x<SrcWidth; x++ )
		{
			*DestPtr++ = SrcPtr->B;
			*DestPtr++ = SrcPtr->G;
			*DestPtr++ = SrcPtr->R;
			if( InParams.bUseAlpha )
			{
				*DestPtr++ = SrcPtr->A;
			}
			else
			{
				*DestPtr++ = 0xFF;
			}
			SrcPtr++;
		}
	}
	Tex2D->Source.UnlockMip(0);

	// Set the Source Guid/Hash if specified
	if (InParams.SourceGuidHash.IsValid())
	{
		Tex2D->Source.SetId(InParams.SourceGuidHash, true);
	}

	// Set compression options.
	Tex2D->SRGB = InParams.bSRGB;
	Tex2D->CompressionSettings	= InParams.CompressionSettings;
	if( !InParams.bUseAlpha )
	{
		Tex2D->CompressionNoAlpha = true;
	}
	Tex2D->DeferCompression	= InParams.bDeferCompression;

	Tex2D->PostEditChange();
	return Tex2D;
#else
	UE_LOG(LogImageUtils, Fatal,TEXT("ConstructTexture2D not supported on console."));
	return NULL;
#endif
}

void FImageUtils::CropAndScaleImage( int32 SrcWidth, int32 SrcHeight, int32 DesiredWidth, int32 DesiredHeight, const TArray<FColor> &SrcData, TArray<FColor> &DstData  )
{
	// Get the aspect ratio, and calculate the dimension of the image to crop
	float DesiredAspectRatio = (float)DesiredWidth/(float)DesiredHeight;

	float MaxHeight = (float)SrcWidth / DesiredAspectRatio;
	float MaxWidth = (float)SrcWidth;

	if ( MaxHeight > (float)SrcHeight)
	{
		MaxHeight = (float)SrcHeight;
		MaxWidth = MaxHeight * DesiredAspectRatio;
	}

	// Store crop width and height as ints for convenience
	int32 CropWidth = FMath::FloorToInt(MaxWidth);
	int32 CropHeight = FMath::FloorToInt(MaxHeight);

	// Array holding the cropped image
	TArray<FColor> CroppedData;
	CroppedData.AddUninitialized(CropWidth*CropHeight);

	int32 CroppedSrcTop  = 0;
	int32 CroppedSrcLeft = 0;

	// Set top pixel if we are cropping height
	if ( CropHeight < SrcHeight )
	{
		CroppedSrcTop = ( SrcHeight - CropHeight ) / 2;
	}

	// Set width pixel if cropping width
	if ( CropWidth < SrcWidth )
	{
		CroppedSrcLeft = ( SrcWidth - CropWidth ) / 2;
	}

	const int32 DataSize = sizeof(FColor);

	//Crop the image
	for (int32 Row = 0; Row < CropHeight; Row++)
	{
	 	//Row*Side of a row*byte per color
	 	int32 SrcPixelIndex = (CroppedSrcTop+Row)*SrcWidth + CroppedSrcLeft;
	 	const void* SrcPtr = &(SrcData[SrcPixelIndex]);
	 	void* DstPtr = &(CroppedData[Row*CropWidth]);
	 	FMemory::Memcpy(DstPtr, SrcPtr, CropWidth*DataSize);
	}

	// Scale the image
	DstData.AddUninitialized(DesiredWidth*DesiredHeight);

	// Resize the image
	FImageUtils::ImageResize( MaxWidth, MaxHeight, CroppedData, DesiredWidth, DesiredHeight, DstData, true );
}

void FImageUtils::CompressImageArray( int32 ImageWidth, int32 ImageHeight, const TArray<FColor> &SrcData, TArray<uint8> &DstData )
{
	TArray<FColor> MutableSrcData = SrcData;

	// PNGs are saved as RGBA but FColors are stored as BGRA. An option to swap the order upon compression may be added at 
	// some point. At the moment, manually swapping Red and Blue 
	for ( int32 Index = 0; Index < ImageWidth*ImageHeight; Index++ )
	{
		uint8 TempRed = MutableSrcData[Index].R;
		MutableSrcData[Index].R = MutableSrcData[Index].B;
		MutableSrcData[Index].B = TempRed;
	}

	FObjectThumbnail TempThumbnail;
	TempThumbnail.SetImageSize( ImageWidth, ImageHeight );
	TArray<uint8>& ThumbnailByteArray = TempThumbnail.AccessImageData();

	// Copy scaled image into destination thumb
	int32 MemorySize = ImageWidth*ImageHeight*sizeof(FColor);
	ThumbnailByteArray.AddUninitialized(MemorySize);
	FMemory::Memcpy(ThumbnailByteArray.GetData(), MutableSrcData.GetData(), MemorySize);

	// Compress data - convert into a .png
	TempThumbnail.CompressImageData();
	TArray<uint8>& CompressedByteArray = TempThumbnail.AccessCompressedImageData();
	DstData = TempThumbnail.AccessCompressedImageData();
}

UTexture2D* FImageUtils::CreateCheckerboardTexture(FColor ColorOne, FColor ColorTwo, int32 CheckerSize)
{
	CheckerSize = FMath::Min<uint32>( FMath::RoundUpToPowerOfTwo(CheckerSize), 4096 );
	const int32 HalfPixelNum = CheckerSize >> 1;

	// Create the texture
	UTexture2D* CheckerboardTexture = UTexture2D::CreateTransient(CheckerSize, CheckerSize, PF_B8G8R8A8);

	// Lock the checkerboard texture so it can be modified
	FColor* MipData = static_cast<FColor*>( CheckerboardTexture->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE) );

	// Fill in the colors in a checkerboard pattern
	for ( int32 RowNum = 0; RowNum < CheckerSize; ++RowNum )
	{
		for ( int32 ColNum = 0; ColNum < CheckerSize; ++ColNum )
		{
			FColor& CurColor = MipData[( ColNum + ( RowNum * CheckerSize ) )];

			if ( ColNum < HalfPixelNum )
			{
				CurColor = ( RowNum < HalfPixelNum ) ? ColorOne : ColorTwo;
			}
			else
			{
				CurColor = ( RowNum < HalfPixelNum ) ? ColorTwo : ColorOne;
			}
		}
	}

	// Unlock the texture
	CheckerboardTexture->PlatformData->Mips[0].BulkData.Unlock();
	CheckerboardTexture->UpdateResource();

	return CheckerboardTexture;
}

/*------------------------------------------------------------------------------
HDR file format helper.
------------------------------------------------------------------------------*/
class FHDRExportHelper
{
public:
	/**
	* Writes HDR format image to an FArchive
	* @param TexRT - A 2D source render target to read from.
	* @param Ar - Archive object to write HDR data to.
	* @return true on successful export.
	*/
	bool ExportHDR(UTextureRenderTarget2D* TexRT, FArchive& Ar)
	{
		check(TexRT != nullptr);
		FRenderTarget* RenderTarget = TexRT->GameThread_GetRenderTargetResource();
		Size = RenderTarget->GetSizeXY();
		Format = TexRT->GetFormat();

		TArray<uint8> RawData;
		bool bReadSuccess = GetRawData(TexRT, RawData);
		if (bReadSuccess)
		{
			WriteHDRImage(RawData, Ar);
			return true;
		}
		return false;
	}

	/**
	* Writes HDR format image to an FArchive
	* @param TexRT - A 2D source render target to read from.
	* @param Ar - Archive object to write HDR data to.
	* @return true on successful export.
	*/
	bool ExportHDR(UTexture2D* Texture, FArchive& Ar)
	{
		check(Texture != nullptr);
		bool bReadSuccess = true;
		TArray<uint8> RawData;

#if WITH_EDITORONLY_DATA
		Size = FIntPoint(Texture->Source.GetSizeX(), Texture->Source.GetSizeY());
		bReadSuccess = Texture->Source.GetMipData(RawData, 0);
		const ETextureSourceFormat NewFormat = Texture->Source.GetFormat();

		if (NewFormat == TSF_BGRA8)
		{
			Format = PF_B8G8R8A8;
		}
		else if (NewFormat == TSF_RGBA16F)
		{
			Format = PF_FloatRGBA;
		}
		else
		{
			bReadSuccess = false;			
			FMessageLog("ImageUtils").Warning(LOCTEXT("ExportHDRUnsupportedSourceTextureFormat", "Unsupported source texture format provided."));
		}
#else
		TArray<uint8*> RawData2;
		Size = Texture->GetImportedSize();
		RawData2.AddZeroed(Texture->GetNumMips());
		Texture->GetMipData(0, (void**)RawData2.GetData());
		const EPixelFormat NewFormat = Texture->GetPixelFormat();

		if (Texture->PlatformData->Mips.Num() == 0)
		{
			bReadSuccess = false;
			FMessageLog("ImageUtils").Warning(FText::Format(LOCTEXT("ExportHDRFailedToReadMipData", "Failed to read Mip Data in: '{0}'"), FText::FromString(Texture->GetName())));
		}

		if (NewFormat == PF_B8G8R8A8)
		{
			Format = PF_B8G8R8A8;
		}
		else if (NewFormat == PF_FloatRGBA)
		{
			Format = PF_FloatRGBA;
		}
		else
		{
			bReadSuccess = false;
			FMessageLog("ImageUtils").Warning(LOCTEXT("ExportHDRUnsupportedTextureFormat", "Unsupported texture format provided."));
		}

		//Put first mip data into usable array
		if (bReadSuccess)
		{
			uint32 TotalSize = Texture->PlatformData->Mips[0].BulkData.GetElementCount();
			RawData.AddZeroed(TotalSize);
			FMemory::Memcpy(RawData.GetData(), RawData2[0], TotalSize);
		}

		//Deallocate the mip data
		for (auto MipData : RawData2)
		{
			FMemory::Free(MipData);
		}

#endif // WITH_EDITORONLY_DATA

		if (bReadSuccess)
		{
			WriteHDRImage(RawData, Ar);
			return true;
		}

		return false;
	}

	/**
	* Writes HDR format image to an FArchive
	* This function unwraps the cube image on to a 2D surface.
	* @param TexCube - A cube source cube texture to read from.
	* @param Ar - Archive object to write HDR data to.
	* @return true on successful export.
	*/
	bool ExportHDR(UTextureCube* TexCube, FArchive& Ar)
	{
		check(TexCube != nullptr);

		// Generate 2D image.
		TArray<uint8> RawData;
		bool bUnwrapSuccess = CubemapHelpers::GenerateLongLatUnwrap(TexCube, RawData, Size, Format);
		bool bAcceptableFormat = (Format == PF_B8G8R8A8 || Format == PF_FloatRGBA);
		if (bUnwrapSuccess == false || bAcceptableFormat == false)
		{
			return false;
		}

		WriteHDRImage(RawData, Ar);

		return true;
	}

	/**
	* Writes HDR format image to an FArchive
	* This function unwraps the cube image on to a 2D surface.
	* @param TexCube - A cube source render target cube texture to read from.
	* @param Ar - Archive object to write HDR data to.
	* @return true on successful export.
	*/
	bool ExportHDR(UTextureRenderTargetCube* TexCube, FArchive& Ar)
	{
		check(TexCube != nullptr);

		// Generate 2D image.
		TArray<uint8> RawData;
		bool bUnwrapSuccess = CubemapHelpers::GenerateLongLatUnwrap(TexCube, RawData, Size, Format);
		bool bAcceptableFormat = (Format == PF_B8G8R8A8 || Format == PF_FloatRGBA);
		if (bUnwrapSuccess == false || bAcceptableFormat == false)
		{
			return false;
		}

		WriteHDRImage(RawData, Ar);

		return true;
	}

private:
	void WriteScanLine(FArchive& Ar, const TArray<uint8>& ScanLine)
	{
		const uint8* LineEnd = ScanLine.GetData() + ScanLine.Num();
		const uint8* LineSource = ScanLine.GetData();
		TArray<uint8> Output;
		Output.Reserve(ScanLine.Num() * 2);
		while (LineSource < LineEnd)
		{
			int32 CurrentPos = 0;
			int32 NextPos = 0;
			int32 CurrentRunLength = 0;
			while (CurrentRunLength <= 4 && NextPos < 128 && LineSource + NextPos < LineEnd)
			{
				CurrentPos = NextPos;
				CurrentRunLength = 0;
				while (CurrentRunLength < 127 && CurrentPos + CurrentRunLength < 128 && LineSource + NextPos < LineEnd && LineSource[CurrentPos] == LineSource[NextPos])
				{
					NextPos++;
					CurrentRunLength++;
				}
			}

			if (CurrentRunLength > 4)
			{
				// write a non run: LineSource[0] to LineSource[CurrentPos]
				if (CurrentPos > 0)
				{
					Output.Add(CurrentPos);
					for (int32 i = 0; i < CurrentPos; i++)
					{
						Output.Add(LineSource[i]);
					}
				}
				Output.Add((uint8)(128 + CurrentRunLength));
				Output.Add(LineSource[CurrentPos]);
			}
			else
			{
				// write a non run: LineSource[0] to LineSource[NextPos]
				Output.Add((uint8)(NextPos));
				for (int32 i = 0; i < NextPos; i++)
				{
					Output.Add((uint8)(LineSource[i]));
				}
			}
			LineSource += NextPos;
		}
		Ar.Serialize(Output.GetData(), Output.Num());
	}

	template<typename TSourceColorType>
	void WriteHDRBits(FArchive& Ar, TSourceColorType* SourceTexels)
	{
		const FRandomStream RandomStream(0xA1A1);
		const int32 NumChannels = 4;
		const int32 SizeX = Size.X;
		const int32 SizeY = Size.Y;
		TArray<uint8> ScanLine[NumChannels];
		for (int32 Channel = 0; Channel < NumChannels; Channel++)
		{
			ScanLine[Channel].Reserve(SizeX);
		}

		for (int32 y = 0; y < SizeY; y++)
		{
			// write RLE header
			uint8 RLEheader[4];
			RLEheader[0] = 2;
			RLEheader[1] = 2;
			RLEheader[2] = SizeX >> 8;
			RLEheader[3] = SizeX & 0xFF;
			Ar.Serialize(&RLEheader[0], sizeof(RLEheader));

			for (int32 Channel = 0; Channel < NumChannels; Channel++)
			{
				ScanLine[Channel].Reset();
			}

			for (int32 x = 0; x < SizeX; x++)
			{
				FLinearColor LinearColor(*SourceTexels);
				FColor RGBEColor = ToRGBEDithered(LinearColor, RandomStream);

				FLinearColor lintest = RGBEColor.FromRGBE();
				ScanLine[0].Add(RGBEColor.R);
				ScanLine[1].Add(RGBEColor.G);
				ScanLine[2].Add(RGBEColor.B);
				ScanLine[3].Add(RGBEColor.A);
				SourceTexels++;
			}

			for (int32 Channel = 0; Channel < NumChannels; Channel++)
			{
				WriteScanLine(Ar, ScanLine[Channel]);
			}
		}
	}

	void WriteHDRHeader(FArchive& Ar)
	{
		const int32 MaxHeaderSize = 256;
		char Header[MAX_SPRINTF];
		FCStringAnsi::Sprintf(Header, "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y %d +X %d\n", Size.Y, Size.X);
		Header[MaxHeaderSize - 1] = 0;
		int32 Len = FMath::Min(FCStringAnsi::Strlen(Header), MaxHeaderSize);
		Ar.Serialize(Header, Len);
	}

	void WriteHDRImage(const TArray<uint8>& RawData, FArchive& Ar)
	{
		WriteHDRHeader(Ar);
		if (Format == PF_FloatRGBA)
		{
			WriteHDRBits(Ar, (FFloat16Color*)RawData.GetData());
		}
		else
		{
			WriteHDRBits(Ar, (FColor*)RawData.GetData());
		}
	}

	/**
	* Returns data containing the pixmap of the passed in rendertarget.
	* @param TexRT - The 2D rendertarget from which to read pixmap data.
	* @param RawData - an array to be filled with pixel data.
	* @return true if RawData has been successfully filled.
	*/
	

	static FColor ToRGBEDithered(const FLinearColor& ColorIN, const FRandomStream& Rand)
	{
		const float R = ColorIN.R;
		const float G = ColorIN.G;
		const float B = ColorIN.B;
		const float Primary = FMath::Max3(R, G, B);
		FColor	ReturnColor;

		if (Primary < 1E-32)
		{
			ReturnColor = FColor(0, 0, 0, 0);
		}
		else
		{
			int32 Exponent;
			const float Scale = frexp(Primary, &Exponent) / Primary * 255.f;

			ReturnColor.R = FMath::Clamp(FMath::TruncToInt((R* Scale) + Rand.GetFraction()), 0, 255);
			ReturnColor.G = FMath::Clamp(FMath::TruncToInt((G* Scale) + Rand.GetFraction()), 0, 255);
			ReturnColor.B = FMath::Clamp(FMath::TruncToInt((B* Scale) + Rand.GetFraction()), 0, 255);
			ReturnColor.A = FMath::Clamp(FMath::TruncToInt(Exponent), -128, 127) + 128;
		}

		return ReturnColor;
	}

	FIntPoint Size;
	EPixelFormat Format;
};

bool FImageUtils::ExportRenderTarget2DAsHDR(UTextureRenderTarget2D* TexRT, FArchive& Ar)
{
	FHDRExportHelper Exporter;
	return Exporter.ExportHDR(TexRT, Ar);
}

bool FImageUtils::ExportRenderTarget2DAsPNG(UTextureRenderTarget2D* TexRT, FArchive& Ar)
{
	bool bSuccess = false;
	if(TexRT->GetFormat() == PF_B8G8R8A8)
	{
		check(TexRT != nullptr);
		FRenderTarget* RenderTarget = TexRT->GameThread_GetRenderTargetResource();
		FIntPoint Size = RenderTarget->GetSizeXY();

		TArray<uint8> RawData;
		bSuccess = GetRawData(TexRT, RawData);

		IImageWrapperModule& ImageWrapperModule = FModuleManager::Get().LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));

		TSharedPtr<IImageWrapper> PNGImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

		PNGImageWrapper->SetRaw(RawData.GetData(), RawData.GetAllocatedSize(), Size.X, Size.Y, ERGBFormat::BGRA, 8);

		const TArray<uint8>& PNGData = PNGImageWrapper->GetCompressed(100);

		Ar.Serialize((void*)PNGData.GetData(), PNGData.GetAllocatedSize());
	}

	return bSuccess;
}

bool FImageUtils::ExportTexture2DAsHDR(UTexture2D* TexRT, FArchive& Ar)
{
	FHDRExportHelper Exporter;
	return Exporter.ExportHDR(TexRT, Ar);
}

bool FImageUtils::ExportRenderTargetCubeAsHDR(UTextureRenderTargetCube* TexRT, FArchive& Ar)
{
	FHDRExportHelper Exporter;
	return Exporter.ExportHDR(TexRT, Ar);
}

bool FImageUtils::ExportTextureCubeAsHDR(UTextureCube* TexRT, FArchive& Ar)
{
	FHDRExportHelper Exporter;
	return Exporter.ExportHDR(TexRT, Ar);
}

#undef LOCTEXT_NAMESPACE
