// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

// Custom serialization version for changes made in Dev-Rendering stream
struct CORE_API FLoadTimesObjectVersion
{
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded = 0,

		// Allows uncompressed reflection captures for cooked builds
		UncompressedReflectionCapturesForCookedBuilds,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FLoadTimesObjectVersion() {}
};
