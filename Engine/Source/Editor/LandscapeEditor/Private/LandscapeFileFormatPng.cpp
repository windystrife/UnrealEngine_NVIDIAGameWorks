// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LandscapeFileFormatPng.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Containers/Algo/Transform.h"


#define LOCTEXT_NAMESPACE "LandscapeEditor.NewLandscape"


FLandscapeHeightmapFileFormat_Png::FLandscapeHeightmapFileFormat_Png()
{
	FileTypeInfo.Description = LOCTEXT("FileFormatPng_HeightmapDesc", "Heightmap .png files");
	FileTypeInfo.Extensions.Add(".png");
	FileTypeInfo.bSupportsExport = true;
}

FLandscapeHeightmapInfo FLandscapeHeightmapFileFormat_Png::Validate(const TCHAR* HeightmapFilename) const
{
	FLandscapeHeightmapInfo Result;

	TArray<uint8> ImportData;
	if (!FFileHelper::LoadFileToArray(ImportData, HeightmapFilename, FILEREAD_Silent))
	{
		Result.ResultCode = ELandscapeImportResult::Error;
		Result.ErrorMessage = LOCTEXT("Import_HeightmapFileReadError", "Error reading heightmap file");
	}
	else
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

		if (!ImageWrapper->SetCompressed(ImportData.GetData(), ImportData.Num()) || ImageWrapper->GetWidth() <= 0 || ImageWrapper->GetHeight() <= 0)
		{
			Result.ResultCode = ELandscapeImportResult::Error;
			Result.ErrorMessage = LOCTEXT("Import_HeightmapFileCorruptPng", "The heightmap file cannot be read (corrupt png?)");
		}
		else
		{
			if (ImageWrapper->GetFormat() != ERGBFormat::Gray)
			{
				Result.ResultCode = ELandscapeImportResult::Warning;
				Result.ErrorMessage = LOCTEXT("Import_HeightmapFileColorPng", "The heightmap file appears to be a color png, grayscale is expected. The import *can* continue, but the result may not be what you expect...");
			}
			else if (ImageWrapper->GetBitDepth() != 16)
			{
				Result.ResultCode = ELandscapeImportResult::Warning;
				Result.ErrorMessage = LOCTEXT("Import_HeightmapFileLowBitDepth", "The heightmap file appears to be an 8-bit png, 16-bit is preferred. The import *can* continue, but the result may be lower quality than desired.");
			}
			FLandscapeFileResolution ImportResolution;
			ImportResolution.Width = ImageWrapper->GetWidth();
			ImportResolution.Height = ImageWrapper->GetHeight();
			Result.PossibleResolutions.Add(ImportResolution);
		}
	}

	// possible todo: support sCAL (XY scale) and pCAL (Z scale) png chunks for filling out Result.DataScale
	// I don't know if any heightmap generation software uses these or not
	// if we support their import we should make the exporter write them too

	return Result;
}

FLandscapeHeightmapImportData FLandscapeHeightmapFileFormat_Png::Import(const TCHAR* HeightmapFilename, FLandscapeFileResolution ExpectedResolution) const
{
	FLandscapeHeightmapImportData Result;

	TArray<uint8> TempData;
	if (!FFileHelper::LoadFileToArray(TempData, HeightmapFilename, FILEREAD_Silent))
	{
		Result.ResultCode = ELandscapeImportResult::Error;
		Result.ErrorMessage = LOCTEXT("Import_HeightmapFileReadError", "Error reading heightmap file");
	}
	else
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

		if (!ImageWrapper->SetCompressed(TempData.GetData(), TempData.Num()))
		{
			Result.ResultCode = ELandscapeImportResult::Error;
			Result.ErrorMessage = LOCTEXT("Import_HeightmapFileCorruptPng", "The heightmap file cannot be read (corrupt png?)");
		}
		else if (ImageWrapper->GetWidth() != ExpectedResolution.Width || ImageWrapper->GetHeight() != ExpectedResolution.Height)
		{
			Result.ResultCode = ELandscapeImportResult::Error;
			Result.ErrorMessage = LOCTEXT("Import_HeightmapResolutionMismatch", "The heightmap file's resolution does not match the requested resolution");
		}
		else
		{
			if (ImageWrapper->GetFormat() != ERGBFormat::Gray)
			{
				Result.ResultCode = ELandscapeImportResult::Warning;
				Result.ErrorMessage = LOCTEXT("Import_HeightmapFileColorPng", "The heightmap file appears to be a color png, grayscale is expected. The import *can* continue, but the result may not be what you expect...");
			}
			else if (ImageWrapper->GetBitDepth() != 16)
			{
				Result.ResultCode = ELandscapeImportResult::Warning;
				Result.ErrorMessage = LOCTEXT("Import_HeightmapFileLowBitDepth", "The heightmap file appears to be an 8-bit png, 16-bit is preferred. The import *can* continue, but the result may be lower quality than desired.");
			}

			const TArray<uint8>* RawData = nullptr;
			if (ImageWrapper->GetBitDepth() <= 8)
			{
				if (!ImageWrapper->GetRaw(ERGBFormat::Gray, 8, RawData))
				{
					Result.ResultCode = ELandscapeImportResult::Error;
					Result.ErrorMessage = LOCTEXT("Import_HeightmapFileCorruptPng", "The heightmap file cannot be read (corrupt png?)");
				}
				else
				{
					Result.Data.Empty(ExpectedResolution.Width * ExpectedResolution.Height);
					Algo::Transform(*RawData, Result.Data, [](uint8 Value) { return Value * 0x101; }); // Expand to 16-bit
				}
			}
			else
			{
				if (!ImageWrapper->GetRaw(ERGBFormat::Gray, 16, RawData))
				{
					Result.ResultCode = ELandscapeImportResult::Error;
					Result.ErrorMessage = LOCTEXT("Import_HeightmapFileCorruptPng", "The heightmap file cannot be read (corrupt png?)");
				}
				else
				{
					Result.Data.Empty(ExpectedResolution.Width * ExpectedResolution.Height);
					Result.Data.AddUninitialized(ExpectedResolution.Width * ExpectedResolution.Height);
					FMemory::Memcpy(Result.Data.GetData(), RawData->GetData(), ExpectedResolution.Width * ExpectedResolution.Height * 2);
				}
			}
		}
	}

	return Result;
}

void FLandscapeHeightmapFileFormat_Png::Export(const TCHAR* HeightmapFilename, TArrayView<const uint16> Data, FLandscapeFileResolution DataResolution, FVector Scale) const
{
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

	if (ImageWrapper->SetRaw(Data.GetData(), Data.Num() * 2, DataResolution.Width, DataResolution.Height, ERGBFormat::Gray, 16))
	{
		const TArray<uint8>& TempData = ImageWrapper->GetCompressed();
		FFileHelper::SaveArrayToFile(TempData, HeightmapFilename);
	}
}

//////////////////////////////////////////////////////////////////////////

FLandscapeWeightmapFileFormat_Png::FLandscapeWeightmapFileFormat_Png()
{
	FileTypeInfo.Description = LOCTEXT("FileFormatPng_WeightmapDesc", "Layer .png files");
	FileTypeInfo.Extensions.Add(".png");
	FileTypeInfo.bSupportsExport = true;
}

FLandscapeWeightmapInfo FLandscapeWeightmapFileFormat_Png::Validate(const TCHAR* WeightmapFilename, FName LayerName) const
{
	FLandscapeWeightmapInfo Result;

	TArray<uint8> ImportData;
	if (!FFileHelper::LoadFileToArray(ImportData, WeightmapFilename, FILEREAD_Silent))
	{
		Result.ResultCode = ELandscapeImportResult::Error;
		Result.ErrorMessage = LOCTEXT("Import_LayerFileReadError", "Error reading layer file");
	}
	else
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

		if (!ImageWrapper->SetCompressed(ImportData.GetData(), ImportData.Num()))
		{
			Result.ResultCode = ELandscapeImportResult::Error;
			Result.ErrorMessage = LOCTEXT("Import_LayerCorruptPng", "The layer file cannot be read (corrupt png?)");
		}
		else
		{
			if (ImageWrapper->GetFormat() != ERGBFormat::Gray)
			{
				Result.ResultCode = ELandscapeImportResult::Warning;
				Result.ErrorMessage = LOCTEXT("Import_LayerColorPng", "The layer file appears to be a color png, grayscale is expected. The import *can* continue, but the result may not be what you expect...");
			}
			FLandscapeFileResolution ImportResolution;
			ImportResolution.Width = ImageWrapper->GetWidth();
			ImportResolution.Height = ImageWrapper->GetHeight();
			Result.PossibleResolutions.Add(ImportResolution);
		}
	}

	return Result;
}

FLandscapeWeightmapImportData FLandscapeWeightmapFileFormat_Png::Import(const TCHAR* WeightmapFilename, FName LayerName, FLandscapeFileResolution ExpectedResolution) const
{
	FLandscapeWeightmapImportData Result;

	TArray<uint8> TempData;
	if (!FFileHelper::LoadFileToArray(TempData, WeightmapFilename, FILEREAD_Silent))
	{
		Result.ResultCode = ELandscapeImportResult::Error;
		Result.ErrorMessage = LOCTEXT("Import_LayerFileReadError", "Error reading layer file");
	}
	else
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

		const TArray<uint8>* RawData = nullptr;
		if (!ImageWrapper->SetCompressed(TempData.GetData(), TempData.Num()))
		{
			Result.ResultCode = ELandscapeImportResult::Error;
			Result.ErrorMessage = LOCTEXT("Import_LayerCorruptPng", "The layer file cannot be read (corrupt png?)");
		}
		else if (ImageWrapper->GetWidth() != ExpectedResolution.Width || ImageWrapper->GetHeight() != ExpectedResolution.Height)
		{
			Result.ResultCode = ELandscapeImportResult::Error;
			Result.ErrorMessage = LOCTEXT("Import_LayerResolutionMismatch", "The layer file's resolution does not match the requested resolution");
		}
		else if (!ImageWrapper->GetRaw(ERGBFormat::Gray, 8, RawData))
		{
			Result.ResultCode = ELandscapeImportResult::Error;
			Result.ErrorMessage = LOCTEXT("Import_LayerCorruptPng", "The layer file cannot be read (corrupt png?)");
		}
		else
		{
			if (ImageWrapper->GetFormat() != ERGBFormat::Gray)
			{
				Result.ResultCode = ELandscapeImportResult::Warning;
				Result.ErrorMessage = LOCTEXT("Import_LayerColorPng", "The layer file appears to be a color png, grayscale is expected. The import *can* continue, but the result may not be what you expect...");
			}

			Result.Data = *RawData; // agh I want to use MoveTemp() here
		}
	}

	return Result;
}

void FLandscapeWeightmapFileFormat_Png::Export(const TCHAR* WeightmapFilename, FName LayerName, TArrayView<const uint8> Data, FLandscapeFileResolution DataResolution) const
{
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

	if (ImageWrapper->SetRaw(Data.GetData(), Data.Num(), DataResolution.Width, DataResolution.Height, ERGBFormat::Gray, 8))
	{
		const TArray<uint8>& TempData = ImageWrapper->GetCompressed();
		FFileHelper::SaveArrayToFile(TempData, WeightmapFilename);
	}
}

#undef LOCTEXT_NAMESPACE
