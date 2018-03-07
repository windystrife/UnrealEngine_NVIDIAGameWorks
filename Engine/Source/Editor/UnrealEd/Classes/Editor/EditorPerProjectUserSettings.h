// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "EditorPerProjectUserSettings.generated.h"

UCLASS(minimalapi, autoexpandcategories=(ViewportControls, ViewportLookAndFeel, LevelEditing, SourceControl, Content, Startup), hidecategories=(Object, Options, Grid, RotationGrid), config=EditorPerProjectUserSettings)
class UEditorPerProjectUserSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	// =====================================================================
	// The following options are exposed in the Preferences Editor 

	/** If enabled, any newly opened UI menus, menu bars, and toolbars will show the developer hooks that would accept extensions */
	UPROPERTY(EditAnywhere, config, Category=DeveloperTools)
	uint32 bDisplayUIExtensionPoints:1;

	/** If enabled, tooltips linked to documentation will show the developer the link bound to that UI item */
	UPROPERTY(EditAnywhere, config, Category=DeveloperTools)
	uint32 bDisplayDocumentationLink:1;

	/** If enabled, tooltips on SGraphPaletteItems will show the associated action's string id */
	UPROPERTY(/*EditAnywhere - deprecated (moved into UBlueprintEditorSettings), */config/*, Category=DeveloperTools*/)
	uint32 bDisplayActionListItemRefIds:1;

	/** If enabled, behavior tree debugger will collect its data even when all behavior tree editor windows are closed */
	UPROPERTY(EditAnywhere, config, Category = AI)
	uint32 bAlwaysGatherBehaviorTreeDebuggerData : 1;

	/** When enabled, Engine Version Number is displayed in the ProjectBadge */
	UPROPERTY(EditAnywhere, config, Category = DeveloperTools, meta = (DisplayName = "Display Engine Version Number in Project Badge", ConfigRestartRequired = true))
	bool bDisplayEngineVersionInBadge;

	/** When enabled, use SimplygonSwarm Module / server to create proxies */
	UPROPERTY(EditAnywhere, config, Category = SimplygonSwarm, meta = (DisplayName = "Use Simplygon distributed proxy server"))
	bool bUseSimplygonSwarm;

	/** Server IP for the distributed Simplygon server */
	UPROPERTY(EditAnywhere, config, Category = SimplygonSwarm, meta = (DisplayName = "Simplygon distributed proxy server IP", editcondition = "bUseSimplygonSwarm"))
	FString SimplygonServerIP;
	
	/** Enable swarm debugging features. Temp ssf files are not removed. Detailed message printing */
	UPROPERTY(EditAnywhere, config, Category = SimplygonSwarm, meta = (DisplayName = "Enable Swarm Debugging ", editcondition = "bUseSimplygonSwarm"))
	bool bEnableSwarmDebugging;
	
	/** Time between JSON net requests for Simplygon Swarm */
	UPROPERTY(EditAnywhere, config, Category = SimplygonSwarm, meta = (DisplayName = "Time in (MS). Delay between json request (Default 5000ms)", editcondition = "bUseSimplygonSwarm", ClampMin = "5000", ClampMax = "30000", UIMin = "5000", UIMax = "30000"))
	uint32 SimplygonSwarmDelay;

	/** Number of concurrent swarm jobs to execute. This is independent of the main job queue. */
	UPROPERTY(EditAnywhere, config, Category = SimplygonSwarm, meta = (DisplayName = "Number of concurrent jobs to submit on simplygon grid server", editcondition = "bUseSimplygonSwarm", ClampMin = "4", ClampMax = "512", UIMin = "4", UIMax = "512"))
	uint32 SwarmNumOfConcurrentJobs;

	UPROPERTY(EditAnywhere, config, Category = SimplygonSwarm, meta = (DisplayName = "Max upload size of simplygon swarm zip (MB). File larger than max size will be split into chunks.", editcondition = "bUseSimplygonSwarm", ClampMin = "10", ClampMax = "2000", UIMin = "10", UIMax = "2000"))
	uint32 SwarmMaxUploadChunkSizeInMB;
	
	/** Folder in which Simplygon Swarm will store intermediate texture and mesh data that is uploaded to the Swarm */
	UPROPERTY(EditAnywhere, config, Category = SimplygonSwarm, meta = (DisplayName = "Simplygon Swarm Intermediate Folder", ConfigRestartRequired = true, editcondition = "bUseSimplygonSwarm"))
	FString SwarmIntermediateFolder;

	/** If enabled, any newly added classes will be automatically compiled and trigger a hot-reload of the module they were added to */
	UPROPERTY(EditAnywhere, config, Category=HotReload, meta=(DisplayName="Automatically Compile Newly Added C++ Classes"))
	uint32 bAutomaticallyHotReloadNewClasses:1;

	/** If enabled, the compile message log window will open if there is a compiler error on Hot Reload */
	UPROPERTY(EditAnywhere, config, Category=HotReload)
	uint32 bShowCompilerLogOnCompileError : 1;

	/** If enabled, the fbx option dialog will show when user re-import a fbx */
	UPROPERTY(EditAnywhere, config, Category = Import)
	uint32 bShowImportDialogAtReimport : 1;

	/** Specify a project data source folder to store relative source file path to ease the re-import process*/
	UPROPERTY(EditAnywhere, config, Category = Import)
	FDirectoryPath DataSourceFolder;

	/** If enabled, export level with attachment hierarchy set */
	UPROPERTY(EditAnywhere, config, Category=Export)
	uint32 bKeepAttachHierarchy:1;

	/** If enabled, will compare an animation's sequence length and curves against the old data and inform the user if something changed */
	UPROPERTY(EditAnywhere, config, Category = Import)
	uint32 bAnimationReimportWarnings: 1;

	/** Select to make Distributions use the curves, not the baked lookup tables. */
	UPROPERTY(config)
	uint32 bUseCurvesForDistributions:1; //(GDistributionType == 0)

	/** Controls the minimum value at which the property matrix editor will display a loading bar when pasting values */
	UPROPERTY(config)
	int32 PropertyMatrix_NumberOfPasteOperationsBeforeWarning;

	UPROPERTY(config)
	bool bSCSEditorShowGrid;

	UPROPERTY(config)
	bool bSCSEditorShowFloor;

	/** How fast the SCS viewport camera moves */
	UPROPERTY(config, meta=(UIMin = "1", UIMax = "8", ClampMin="1", ClampMax="8"))
	int32 SCSViewportCameraSpeed;

	// Color curve:
	//   Release->Attack happens instantly
	//   Attack holds for AttackHoldPeriod, then
	//   Decays from Attack to Sustain for DecayPeriod with DecayExponent, then
	//   Sustain holds for SustainHoldPeriod, then
	//   Releases from Sustain to Release for ReleasePeriod with ReleaseExponent
	//
	// The effective time at which an event occurs is it's most recent exec time plus a bonus based on the position in the execution trace


	// =====================================================================
	// The following options are NOT exposed in the preferences Editor
	// (usually because there is a different way to set them interactively!)

	/** Controls whether packages which are checked-out are automatically fully loaded at startup */
	UPROPERTY(config)
	uint32 bAutoloadCheckedOutPackages:1;

	/** If this is true, the user will not be asked to fully load a package before saving or before creating a new object */
	UPROPERTY(config)
	uint32 bSuppressFullyLoadPrompt:1;

	/** True if user should be allowed to select translucent objects in perspective viewports */
	UPROPERTY(config)
	uint32 bAllowSelectTranslucent:1;

	UPROPERTY()
	class UBlueprintPaletteFavorites* BlueprintFavorites;
	
public:
	// Per project user settings for which asset viewer profile should be used
	UPROPERTY()
	int32 AssetViewerProfileIndex;

	UPROPERTY(config)
	FString AssetViewerProfileName;

	UPROPERTY(config)
	int32 MaterialQualityLevel;

public:

	/** Delegate for when a user setting has changed */
	DECLARE_EVENT_OneParam(UEditorPerProjectUserSettings, FUserSettingChangedEvent, FName /*PropertyName*/);
	FUserSettingChangedEvent& OnUserSettingChanged() { return UserSettingChangedEvent; }

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent ) override;
#endif
	virtual void PostInitProperties() override;
	//~ End UObject Interface

private:

	FUserSettingChangedEvent UserSettingChangedEvent;
};

