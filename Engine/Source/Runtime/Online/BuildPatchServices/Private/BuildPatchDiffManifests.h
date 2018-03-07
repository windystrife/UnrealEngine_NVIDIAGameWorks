// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class FBuildDiffManifests
{
public:
	static bool DiffManifests(const FString& ManifestFilePathA, const TSet<FString>& TagSetA, const FString& ManifestFilePathB, const TSet<FString>& TagSetB, const FString& OutputFilePath);
};
