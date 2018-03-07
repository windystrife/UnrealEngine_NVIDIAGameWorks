// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Process exit codes */
enum class EMovieSceneCaptureExitCode : uint32
{
	Base = 0xAC701000,
	AssetNotFound,
	WorldNotFound,
};
