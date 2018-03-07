// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

/**
* 
*/

class PIEPREVIEWDEVICEPROFILESELECTOR_API FPIEPreviewDeviceContainerCategory
{
public:
	FPIEPreviewDeviceContainerCategory(const FString& InSubDirectoryPath, FText DisplayName) : 
		CategoryDisplayName(DisplayName),
		SubDirectoryPath(InSubDirectoryPath)
	{
	}

	FName GetCategoryName();
	FText GetCategoryToolTip();
	const FText& GetCategoryDisplayName() const { return CategoryDisplayName; }
	const FString& GetSubDirectoryPath() const { return SubDirectoryPath; }
	const TArray<TSharedPtr<FPIEPreviewDeviceContainerCategory>>& GetSubCategories() const {return SubCategories;}
	int GetDeviceStartIndex() const { return DeviceStartIndex; }
	int GetDeviceCount() const { return DeviceCount; }
protected:
	int32 DeviceStartIndex;
	int32 DeviceCount;
	FText CategoryDisplayName;
	FString SubDirectoryPath;
	TArray<TSharedPtr<FPIEPreviewDeviceContainerCategory>> SubCategories;
	friend class FPIEPreviewDeviceContainer;
};

class PIEPREVIEWDEVICEPROFILESELECTOR_API FPIEPreviewDeviceContainer
{
public:
	// Recursively iterate through 'RootDir' for all device json files.
	// Sub directories are recorded as categories.
	void EnumerateDeviceSpecifications(const FString& RootDir);

	const TSharedPtr<FPIEPreviewDeviceContainerCategory> GetRootCategory() const { return RootCategory; }

	const TArray<FString>& GetDeviceSpecifications() const { return DeviceSpecifications; }


	// return the category that contains DeviceIndex.
	const TSharedPtr<FPIEPreviewDeviceContainerCategory> FindDeviceContainingCategory(int32 DeviceIndex) const;

private:
	FString DeviceSpecificationRootDir;
	FString GetDeviceSpecificationRootDir() const { return DeviceSpecificationRootDir; }
	TSharedPtr<FPIEPreviewDeviceContainerCategory> RootCategory;

	// All device specifications found.
	TArray<FString> DeviceSpecifications;
	void EnumerateDeviceSpecifications(TSharedPtr<FPIEPreviewDeviceContainerCategory> SubCategory);
};