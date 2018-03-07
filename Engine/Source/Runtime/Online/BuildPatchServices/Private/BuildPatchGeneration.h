// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "BuildPatchSettings.h"

/**
 * A class that controls the process of generating manifests and chunk data from a build image.
 */
class FBuildDataGenerator
{
public:
	/**
	 * Processes a Build Image to determine new chunks and produce a chunk based manifest, all saved to the cloud.
	 * NOTE: This function is blocking and will not return until finished. Don't run on main thread.
	 * @param Configuration      Specifies the settings for the operation.  See BuildPatchServices::FGenerationConfiguration comments.
	 * @return True if no errors occurred.
	 */
	static bool GenerateChunksManifestFromDirectory(const BuildPatchServices::FGenerationConfiguration& Configuration);
};
