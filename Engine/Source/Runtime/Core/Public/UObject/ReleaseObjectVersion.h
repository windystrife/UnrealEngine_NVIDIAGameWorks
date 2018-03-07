// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Misc/Guid.h"

// Custom serialization version for changes made in Dev-Core stream
struct CORE_API FReleaseObjectVersion
{
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded = 0,

		// Static Mesh extended bounds radius fix
		StaticMeshExtendedBoundsFix,

		//Physics asset bodies are either in the sync scene or the async scene, but not both
		NoSyncAsyncPhysAsset,

		// ULevel was using TTransArray incorrectly (serializing the entire array in addition to individual mutations).
		// converted to a TArray:
		LevelTransArrayConvertedToTArray,

		// Add Component node templates now use their own unique naming scheme to ensure more reliable archetype lookups.
		AddComponentNodeTemplateUniqueNames,

		// Fix a serialization issue with static mesh FMeshSectionInfoMap UProperty
		UPropertryForMeshSectionSerialize,

		// Existing HLOD settings screen size to screen area conversion
		ConvertHLODScreenSize,

		// Adding mesh section info data for existing billboard LOD models
		SpeedTreeBillboardSectionInfoFixup,

		// Change FMovieSceneEventParameters::StructType to be a string asset reference from a TWeakObjectPtr<UScriptStruct>
		EventSectionParameterStringAssetRef,

		// Remove serialized irradiance map data from skylight.
		SkyLightRemoveMobileIrradianceMap,

		// rename bNoTwist to bAllowTwist
		RenameNoTwistToAllowTwistInTwoBoneIK,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FReleaseObjectVersion() {}
};
