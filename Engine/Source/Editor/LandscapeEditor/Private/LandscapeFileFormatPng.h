// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "LandscapeFileFormatInterface.h"

// Implement .png file format
class FLandscapeHeightmapFileFormat_Png : public ILandscapeHeightmapFileFormat
{
private:
	FLandscapeFileTypeInfo FileTypeInfo;

public:
	FLandscapeHeightmapFileFormat_Png();

	virtual const FLandscapeFileTypeInfo& GetInfo() const override
	{
		return FileTypeInfo;
	}

	virtual FLandscapeHeightmapInfo Validate(const TCHAR* HeightmapFilename) const override;
	virtual FLandscapeHeightmapImportData Import(const TCHAR* HeightmapFilename, FLandscapeFileResolution ExpectedResolution) const override;
	virtual void Export(const TCHAR* HeightmapFilename, TArrayView<const uint16> Data, FLandscapeFileResolution DataResolution, FVector Scale) const override;
};

//////////////////////////////////////////////////////////////////////////

class FLandscapeWeightmapFileFormat_Png : public ILandscapeWeightmapFileFormat
{
private:
	FLandscapeFileTypeInfo FileTypeInfo;

public:
	FLandscapeWeightmapFileFormat_Png();

	virtual const FLandscapeFileTypeInfo& GetInfo() const override
	{
		return FileTypeInfo;
	}

	virtual FLandscapeWeightmapInfo Validate(const TCHAR* WeightmapFilename, FName LayerName) const override;
	virtual FLandscapeWeightmapImportData Import(const TCHAR* WeightmapFilename, FName LayerName, FLandscapeFileResolution ExpectedResolution) const override;
	virtual void Export(const TCHAR* WeightmapFilename, FName LayerName, TArrayView<const uint8> Data, FLandscapeFileResolution DataResolution) const override;
};
