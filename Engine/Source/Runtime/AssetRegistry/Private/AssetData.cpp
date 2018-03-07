// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetData.h"
#include "Serialization/CustomVersion.h"

DEFINE_LOG_CATEGORY(LogAssetData);

// Register Asset Registry version
const FGuid FAssetRegistryVersion::GUID(0x717F9EE7, 0xE9B0493A, 0x88B39132, 0x1B388107);
FCustomVersionRegistration GRegisterAssetRegistryVersion(FAssetRegistryVersion::GUID, FAssetRegistryVersion::LatestVersion, TEXT("AssetRegistry"));

bool FAssetRegistryVersion::SerializeVersion(FArchive& Ar, FAssetRegistryVersion::Type& Version)
{
	FGuid Guid = FAssetRegistryVersion::GUID;

	if (Ar.IsLoading())
	{
		Version = FAssetRegistryVersion::PreVersioning;
	}

	Ar << Guid;

	if (Ar.IsError())
	{
		return false;
	}

	if (Guid == FAssetRegistryVersion::GUID)
	{
		int32 VersionInt = Version;
		Ar << VersionInt;
		Version = (FAssetRegistryVersion::Type)VersionInt;

		Ar.SetCustomVersion(Guid, VersionInt, TEXT("AssetRegistry"));
	}
	else
	{
		return false;
	}

	return !Ar.IsError();
}