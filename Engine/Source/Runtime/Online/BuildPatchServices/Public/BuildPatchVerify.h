// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

namespace BuildPatchServices
{
	/**
	 * An enum defining the verification mode that should be used.
	 */
	enum class EVerifyMode : uint32
	{
		// Fully SHA checks all files in the build.
		ShaVerifyAllFiles,

		// Fully SHA checks only files touched by the install/patch process.
		ShaVerifyTouchedFiles,

		// Checks just the existence and file size of all files in the build.
		FileSizeCheckAllFiles,

		// Checks just the existence and file size of only files touched by the install/patch process.
		FileSizeCheckTouchedFiles
	};
}
