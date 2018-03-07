// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Containers/ArrayView.h"
#include "Misc/Paths.h"

class Error;

struct FLandscapeFileTypeInfo
{
	// Description of file type for the file selector
	FText Description;

	// Extensions for this type, with leading dot, e.g. ".png"
	TArray<FString, TInlineAllocator<2>> Extensions;

	// Whether this file type supports exporting from the editor back to file
	// (All file types must support *importing*, but exporting is optional)
	bool bSupportsExport = false;
};

UENUM()
enum class ELandscapeImportResult : uint8
{
	Success = 0,
	Warning,
	Error,
};

struct FLandscapeFileResolution
{
	uint32 Width;
	uint32 Height;
};

FORCEINLINE bool operator==(const FLandscapeFileResolution& Lhs, const FLandscapeFileResolution& Rhs)
{
	return Lhs.Width == Rhs.Width && Lhs.Height == Rhs.Height;
}

FORCEINLINE bool operator!=(const FLandscapeFileResolution& Lhs, const FLandscapeFileResolution& Rhs)
{
	return !(Lhs == Rhs);
}

struct FLandscapeHeightmapInfo
{
	// Whether the heightmap is usable or has errors/warnings
	ELandscapeImportResult ResultCode = ELandscapeImportResult::Success;

	// Message to show as the warning/error result
	FText ErrorMessage;

	// Normally contains a single resolution, but .raw is awful
	TArray<FLandscapeFileResolution, TInlineAllocator<1>> PossibleResolutions;

	// The inherent scale of the data format, if it has one, in centimeters
	// The default for data with no inherent scale is 100,100,0.78125 (100.0/128, shown as 100 in the editor UI)
	TOptional<FVector> DataScale;
};

struct FLandscapeWeightmapInfo
{
	// Whether the weightmap is usable or has errors/warnings
	ELandscapeImportResult ResultCode = ELandscapeImportResult::Success;

	// Message to show as the warning/error result
	FText ErrorMessage;

	// Normally contains a single resolution, but .raw is awful
	TArray<FLandscapeFileResolution, TInlineAllocator<1>> PossibleResolutions;
};

struct FLandscapeHeightmapImportData
{
	// Whether the heightmap is usable or has errors/warnings
	ELandscapeImportResult ResultCode = ELandscapeImportResult::Success;

	// Message to show as the warning/error result
	FText ErrorMessage;

	// The height data!
	// A value of 32768 is the 0 level (e.g. sea level), lower values are below and higher values are above
	TArray<uint16> Data;
};

struct FLandscapeWeightmapImportData
{
	// Whether the weightmap is usable or has errors/warnings
	ELandscapeImportResult ResultCode = ELandscapeImportResult::Success;

	// Message to show as the warning/error result
	FText ErrorMessage;

	// The weight data!
	// 255 is fully painted and 0 is unpainted
	TArray<uint8> Data;
};

// Interface
class ILandscapeHeightmapFileFormat
{
public:
	/** Gets info about this format
	 * @return information about the file types supported by this file format plugin
	 */
	virtual const FLandscapeFileTypeInfo& GetInfo() const = 0;

	/** Validate a file for Import
	 * Gives the file format the opportunity to reject a file or return warnings
	 * as well as return information about the file for the import UI (e.g. resolution and scale)
	 * @param HeightmapFilename path to the file to validate for import
	 * @return information about the file and (optional) error message
	 */
	virtual FLandscapeHeightmapInfo Validate(const TCHAR* HeightmapFilename) const = 0;

	/** Import a file
	 * @param HeightmapFilename path to the file to import
	 * @param ExpectedResolution resolution selected in the import UI (mostly for the benefit of .raw)
	 * @return imported data and (optional) error message
	 */
	virtual FLandscapeHeightmapImportData Import(const TCHAR* HeightmapFilename, FLandscapeFileResolution ExpectedResolution) const = 0;

	/** Export a file (if supported)
	 * @param HeightmapFilename path to the file to export to
	 * @param Data raw data to export
	 * @param DataResolution resolution of Data
	 * @param Scale scale of the landscape data, in centimeters
	 */
	virtual void Export(const TCHAR* HeightmapFilename, TArrayView<const uint16> Data, FLandscapeFileResolution DataResolution, FVector Scale) const
	{
		checkf(0, TEXT("File type hasn't implemented support for heightmap export - %s"), *FPaths::GetExtension(HeightmapFilename, true));
	}

	/**
	 * Note: Even though this is an interface class we need a virtual destructor as derived objects are deleted via a pointer to this interface
	 */
	virtual ~ILandscapeHeightmapFileFormat() {}
};

class ILandscapeWeightmapFileFormat
{
public:
	/** Gets info about this format
	 * @return information about the file types supported by this file format plugin
	 */
	virtual const FLandscapeFileTypeInfo& GetInfo() const = 0;

	/** Validate a file for Import
	 * Gives the file format the opportunity to reject a file or return warnings
	 * as well as return information about the file for the import UI (e.g. resolution and scale)
	 * @param HeightmapFilename path to the file to validate for import
	 * @param LayerName name of layer that is being imported
	 * @return information about the file and (optional) error message
	 */
	virtual FLandscapeWeightmapInfo Validate(const TCHAR* WeightmapFilename, FName LayerName) const = 0;

	/** Import a file
	 * @param HeightmapFilename path to the file to import
	 * @param LayerName name of layer that is being imported
	 * @param ExpectedResolution resolution selected in the import UI (mostly for the benefit of .raw)
	 * @return imported data and (optional) error message
	 */
	virtual FLandscapeWeightmapImportData Import(const TCHAR* WeightmapFilename, FName LayerName, FLandscapeFileResolution ExpectedResolution) const = 0;

	/** Export a file (if supported)
	 * @param HeightmapFilename path to the file to export to
	 * @param LayerName name of layer that is being exported
	 * @param Data raw data to export
	 * @param DataResolution resolution of Data
	 * @param Scale scale of the landscape data, in centimeters
	 */
	virtual void Export(const TCHAR* WeightmapFilename, FName LayerName, TArrayView<const uint8> Data, FLandscapeFileResolution DataResolution) const
	{
		checkf(0, TEXT("File type hasn't implemented support for weightmap export - %s"), *FPaths::GetExtension(WeightmapFilename, true));
	}

	/**
 	 * Note: Even though this is an interface class we need a virtual destructor as derived objects are deleted via a pointer to this interface
	 */
	virtual ~ILandscapeWeightmapFileFormat() {}
};
