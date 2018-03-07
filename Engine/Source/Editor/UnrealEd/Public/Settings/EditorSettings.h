// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "Scalability.h"
#include "Engine/EngineTypes.h"
#include "EditorSettings.generated.h"

UCLASS(config=EditorSettings)
class UNREALED_API UEditorSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	/** When checked, the most recently loaded project will be auto-loaded at editor startup if no other project was specified on the command line */
	UPROPERTY()
	bool bLoadTheMostRecentlyLoadedProjectAtStartup; // Note that this property is NOT config since it is not necessary to save the value to ini. It is determined at startup in UEditorEngine::InitEditor().

	/** Can the editor report usage analytics (types of assets being spawned, etc...) back to Epic in order for us to improve the editor user experience?  Note: The editor must be restarted for changes to take effect. */
	UPROPERTY()
	bool bEditorAnalyticsEnabled_DEPRECATED;

	/** Sets the path to be used for caching derived data (native textures, compiled shaders, etc...). The editor must be restarted for changes to take effect. */
	UPROPERTY(EditAnywhere, config, Category=DerivedData, meta = (ConfigRestartRequired = true))
	FDirectoryPath LocalDerivedDataCache;

	// =====================================================================
	// The following options are NOT exposed in the preferences Editor
	// (usually because there is a different way to set them interactively!)

	/** Game project files that were recently opened in the editor */
	UPROPERTY(config)
	TArray<FString> RecentlyOpenedProjectFiles;

	/** The paths of projects created with the new project wizard. This is used to populate the "Path" field of the new project dialog. */
	UPROPERTY(config)
	TArray<FString> CreatedProjectPaths;

	UPROPERTY(config)
	bool bCopyStarterContentPreference;

	/** The id's of the surveys completed */
	UPROPERTY(config)
	TArray<FGuid> CompletedSurveys;

	/** The id's of the surveys currently in-progress */
	UPROPERTY(config)
	TArray<FGuid> InProgressSurveys;

	UPROPERTY(config)
	float AutoScalabilityWorkScaleAmount;

	/** Engine scalability benchmark results */
	Scalability::FQualityLevels EngineBenchmarkResult;

	/** Load the engine scalability benchmark results. Performs a benchmark if not yet valid. */
	void LoadScalabilityBenchmark();

	/** Auto detects and applies the scalability benchmark */
	void AutoApplyScalabilityBenchmark();

	/** @return true if the scalability benchmark is valid */
	bool IsScalabilityBenchmarkValid() const;

	//~ Begin UObject Interface
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface
};
