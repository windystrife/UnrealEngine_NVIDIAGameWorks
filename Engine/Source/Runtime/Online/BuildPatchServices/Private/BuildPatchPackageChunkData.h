// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class FBuildPackageChunkData
{
public:
	static bool PackageChunkData(const FString& ManifestFilePath, const FString& OutputFile, const FString& CloudDir, uint64 MaxOutputFileSize);
};
