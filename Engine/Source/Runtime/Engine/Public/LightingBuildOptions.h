// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

class ULevel;

/**
 * A set of parameters specifying how static lighting is rebuilt.
 */
class FLightingBuildOptions
{
public:
	FLightingBuildOptions()
	:	bUseErrorColoring(false)
	,	bDumpBinaryResults(false)
	,	bOnlyBuildSelected(false)
	,	bOnlyBuildCurrentLevel(false)
	,	bOnlyBuildSelectedLevels(false)
	,	bOnlyBuildVisibility(false)
	,	bShowLightingBuildInfo(false)
	,	QualityLevel(Quality_Preview)
	,	NumUnusedLocalCores(1)
	{}

	/**
	 * @return true if the lighting should be built for the level, given the current set of lighting build options.
	 */
	ENGINE_API bool ShouldBuildLightingForLevel(ULevel* Level) const;

	/** Whether to color problem objects (wrapping uvs, etc.)						*/
	bool					bUseErrorColoring;
	/** Whether to dump binary results or not										*/
	bool					bDumpBinaryResults;
	/** Whether to only build lighting for selected actors/brushes/surfaces			*/
	bool					bOnlyBuildSelected;
	/** Whether to only build lighting for current level							*/
	bool					bOnlyBuildCurrentLevel;
	/** Whether to only build lighting for levels selected in the Level Browser.	*/
	bool					bOnlyBuildSelectedLevels;
	/** Whether to only build visibility, and leave lighting untouched.				*/
	bool					bOnlyBuildVisibility;
	/** Whether to display the lighting build info following a build.				*/
	bool					bShowLightingBuildInfo;
	/** The quality level to use for the lighting build. (0-3)						*/
	ELightingBuildQuality	QualityLevel;
	/** The quality level to use for half-resolution lightmaps (not exposed)		*/
	static ELightingBuildQuality	HalfResolutionLightmapQualityLevel;
	/** The number of cores to leave 'unused'										*/
	int32						NumUnusedLocalCores;
	/** The set of levels selected in the Level Browser.							*/
	TArray<ULevel*>			SelectedLevels;
};

