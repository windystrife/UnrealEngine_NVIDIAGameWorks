// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*===================================================================================
	Scalability.h: Manager class for handling scalability settings
=====================================================================================*/

#pragma once

#include "CoreMinimal.h"

namespace Scalability
{ 
	enum class EQualityLevelBehavior
	{
		EAbsolute,
		ERelativeToMax,
	};

	/**
	 * Structure for holding the state of the engine scalability groups
	 * Actual engine state you can get though GetQualityLevels().
	**/
	struct ENGINE_API FQualityLevels
	{
		float ResolutionQuality;
		int32 ViewDistanceQuality;
		int32 AntiAliasingQuality;
		int32 ShadowQuality;
		int32 PostProcessQuality;
		int32 TextureQuality;
		int32 EffectsQuality;
		int32 FoliageQuality;

		float CPUBenchmarkResults;
		float GPUBenchmarkResults;
		TArray<float> CPUBenchmarkSteps;
		TArray<float> GPUBenchmarkSteps;

		FQualityLevels()
			: CPUBenchmarkResults(-1.0f)
			, GPUBenchmarkResults(-1.0f)
		{
			SetDefaults();
		}
		
		bool operator==(const FQualityLevels& Other ) const
		{
			return FMemory::Memcmp( this, &Other, sizeof(FQualityLevels) ) == 0;
		}

		bool operator!=(const FQualityLevels& Other ) const
		{
			return !(*this == Other);
		}

		/** used for DisplayInternals to quickly identify why a screenshot looks different */
		uint32 GetHash() const
		{
			// Note: this assumes the memory of this class is not containing any uninitialized memory
			return FCrc::MemCrc32(this, sizeof(*this));
		}

		// Sets all other settings based on an overall value
		// @param Value 0:low, 1:medium, 2:high, 3:epic (gets clamped if needed)
		void SetFromSingleQualityLevel(int32 Value);

		// Sets all other settings based on an overall value, but relative to the maximum.
		// @param Value 0: maximum level, 1: maximumlevel -1, etc
		void SetFromSingleQualityLevelRelativeToMax(int32 Value);

		// Returns the overall value if all settings are set to the same thing
		// @param Value -1:custom, 0:low, 1:medium, 2:high, 3:epic
		int32 GetSingleQualityLevel() const;

		void SetBenchmarkFallback();

		void SetDefaults();
	};


	/** This is the only suggested way to set the current state - don't set CVars directly **/
	ENGINE_API void SetQualityLevels(const FQualityLevels& QualityLevels);

	/** This is the only suggested way to get the current state - don't get CVars directly */
	ENGINE_API FQualityLevels GetQualityLevels();

	/** Gets the effects quality directly for the passed thread.
	*
	* @param bGameThread	If true, the game thread value for the CVar is returned, otherwise the render thread value is returned. Useful when accessing the CVar from a game task.
	*/
	ENGINE_API int32 GetEffectsQualityDirect(bool bGameThread);

	/**  */
	ENGINE_API void InitScalabilitySystem();

	/** @param IniName e.g. GEditorPerProjectIni or GGameUserSettingsIni */
	ENGINE_API void LoadState(const FString& IniName);
	
	/** @param IniName e.g. GEditorPerProjectIni or GGameUserSettingsIni */
	ENGINE_API void SaveState(const FString& IniName);

	/**
	 * Sends an analytic event with all quality level data
	 *
	 * @param bAutoApplied	Whether or not the quality levels were auto-applied (true) or applied by the user (false).
	 */
	ENGINE_API void RecordQualityLevelsAnalytics(bool bAutoApplied);

	/** Run synthbenchmark and configure scalability based on results **/
	ENGINE_API FQualityLevels BenchmarkQualityLevels(uint32 WorkScale=10, float CPUMultiplier = 1.0f, float GPUMultiplier = 1.0f);

	/** Process a console command line **/
	ENGINE_API void ProcessCommand(const TCHAR* Cmd, FOutputDevice& Ar);

	/** Returns the number of steps for each quality level */
	ENGINE_API FQualityLevels GetQualityLevelCounts();

	/** Minimum single axis scale for render resolution */
	static const float MinResolutionScale = 10.0f;

	/** Maximum single axis scale for render resolution */
	static const float MaxResolutionScale = 100.0f;

	/** Returns the current screen percentage */
	ENGINE_API float GetResolutionScreenPercentage();
}
