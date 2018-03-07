// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetData.h"
#include "UObject/Linker.h"

class FPackageDependencyData : public FLinkerTables
{
public:
	/** The name of the package that dependency data is gathered from */
	FName PackageName;

	/** Asset Package data, gathered at the same time as dependency data */
	FAssetPackageData PackageData;

	/**
	 * Return the package name of the UObject represented by the specified import. 
	 * 
	 * @param	PackageIndex	package index for the resource to get the name for
	 *
	 * @return	the path name of the UObject represented by the resource at PackageIndex, or the empty string if this isn't an import
	 */
	FName GetImportPackageName(int32 ImportIndex);

	/**
	 * Serialize as part of the registry cache. This is not meant to be serialized as part of a package so  it does not handle versions normally
	 * To version this data change FAssetRegistryVersion or CacheSerializationVersion
	 */
	void SerializeForCache(FArchive& Ar)
	{
		Ar << PackageName;
		Ar << ImportMap;
		Ar << SoftPackageReferenceList;
		Ar << SearchableNamesMap;
		
		PackageData.SerializeForCache(Ar);
	}
};
