// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "InputCoreTypes.h"
#include "EditorExperimentalSettings.generated.h"

/**
 * Implements Editor settings for experimental features.
 */
UCLASS(config=EditorPerProjectUserSettings)
class UNREALED_API UEditorExperimentalSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Allows usage of the procedural foliage system */
	UPROPERTY(EditAnywhere, config, Category = Foliage, meta = (DisplayName = "Procedural Foliage"))
	bool bProceduralFoliage;

	/** Allows usage of the Localization Dashboard */
	UPROPERTY(EditAnywhere, config, Category = Tools, meta = (DisplayName = "Localization Dashboard"))
	bool bEnableLocalizationDashboard;

	/** Allows usage of the Translation Picker */
	UPROPERTY(EditAnywhere, config, Category = Tools, meta = (DisplayName = "Translation Picker"))
	bool bEnableTranslationPicker;

	/** The Blutility shelf holds editor utility Blueprints. Summon from the Workspace menu. */
	UPROPERTY(EditAnywhere, config, Category=Tools, meta=(DisplayName="Editor Utility Blueprints (Blutility)"))
	bool bEnableEditorUtilityBlueprints;

	/** When enabled, all details panels will be able to have properties marked as favorite that show in a top most category.  
	 * NOTE: Some customizations are not supported yet
	 */
	UPROPERTY(EditAnywhere, config, Category = Tools, meta = (DisplayName = "Enable Details Panel Favorites"))
	bool bEnableFavoriteSystem;


	/** Device output log window (currently implemented for Android only)*/
	UPROPERTY(EditAnywhere, config, Category = Tools, meta = (DisplayName = "Device Output Log"))
	bool bDeviceOutputLog;

	/** Specify which console-specific nomenclature to use for gamepad label text */
	UPROPERTY(EditAnywhere, config, Category=UserInterface, meta=(DisplayName="Console for Gamepad Labels"))
	TEnumAsByte<EConsoleForGamepadLabels::Type> ConsoleForGamepadLabels;

	/** Allows for customization of toolbars and menus throughout the editor */
	UPROPERTY(config)
	bool bToolbarCustomization;

	/** Break on Exceptions allows you to trap Access Nones and other exceptional events in Blueprints. */
	UPROPERTY(EditAnywhere, config, Category=Blueprints, meta=(DisplayName="Blueprint Break on Exceptions"))
	bool bBreakOnExceptions;

	/** Enables "Find and Replace All" tool in the MyBlueprint window for variables */
	UPROPERTY(EditAnywhere, config, Category = Blueprints, meta = (DisplayName = "Find and Replace All References Tool"))
	bool bEnableFindAndReplaceReferences;

	/** Should arrows indicating data/execution flow be drawn halfway along wires? */
	UPROPERTY(/*EditAnywhere - deprecated (moved into UBlueprintEditorSettings), */config/*, Category=Blueprints, meta=(DisplayName="Draw midpoint arrows in Blueprints")*/)
	bool bDrawMidpointArrowsInBlueprints;

	/** Allows ChunkIDs to be assigned to assets to via the content browser context menu. */
	UPROPERTY(EditAnywhere,config,Category=UserInterface,meta=(DisplayName="Allow ChunkID Assignments"))
	bool bContextMenuChunkAssignments;

	/** Disable cook in the editor */
	UPROPERTY(EditAnywhere, config, Category = Cooking, meta = (DisplayName = "Disable Cook In The Editor feature (cooks from launch on will be run in a separate process if disabled)", ConfigRestartRequired=true))
	bool bDisableCookInEditor;

	UPROPERTY(EditAnywhere, config, Category = Cooking, meta = (DisplayName = "Use shared cooked builds in launch on", ConfigRestartRequired = true))
	bool bSharedCookedBuilds;

	UPROPERTY(EditAnywhere, config, Category = Cooking, meta = (DisplayName = "Use multiple processes when cooking (only affects File -> Package)"))
	int32 MultiProcessCooking;

	/** Enables Environment Queries editor */
	UPROPERTY(EditAnywhere, config, Category = AI, meta = (DisplayName = "Environment Querying System"))
	bool bEQSEditor;

	/** Enable late joining in PIE */
	UPROPERTY(EditAnywhere, config, Category = PIE, meta = (DisplayName = "Allow late joining"))
	bool bAllowLateJoinInPIE;

	/** Allow Vulkan Preview */
	UPROPERTY(EditAnywhere, config, Category = PIE, meta = (DisplayName = "Allow Vulkan Mobile Preview"))
	bool bAllowVulkanPreview;

	/** Enable multithreaded lightmap encoding (decreases time taken to encode lightmaps) */
	UPROPERTY(EditAnywhere, config, Category = LightingBuilds, meta = (DisplayName = "Enable Multithreaded lightmap encoding"))
	bool bEnableMultithreadedLightmapEncoding;

	/** Enable multithreaded shadow map encoding (decreases time taken to encode shadow maps) */
	UPROPERTY(EditAnywhere, config, Category = LightingBuilds, meta = (DisplayName = "Enable Multithreaded shadowmap encoding"))
	bool bEnableMultithreadedShadowmapEncoding;
	
	/** Whether to use OpenCL to accelerate convex hull decomposition (uses GPU to decrease time taken to decompose meshes, currently only available on Mac OS X) */
	UPROPERTY(EditAnywhere, config, Category = Tools, meta = (DisplayName = "Use OpenCL for convex hull decomposition"))
	bool bUseOpenCLForConvexHullDecomp;

	/** Allows editing of potentially unsafe properties during PIE. Advanced use only - use with caution. */
	UPROPERTY(EditAnywhere, config, Category = Tools, meta = (DisplayName = "Allow editing of potentially unsafe properties."))
	bool bAllowPotentiallyUnsafePropertyEditing;

	/** Enable experimental bulk facial animation importer (found in Developer Tools menu, requires editor restart) */
	UPROPERTY(EditAnywhere, config, Category = Tools, meta = (ConfigRestartRequired = true))
	bool bFacialAnimationImporter;

	/** Allow animation blueprints to be recompiled while a PIE session is running */
	UPROPERTY(EditAnywhere, config, Category = Tools)
	bool bEnableLiveRecompilationOfAnimationBlueprints;

	/** Enable experimental PIE preview device launch */
	UPROPERTY(EditAnywhere, config, Category = PIE, meta = (DisplayName = "Enable mobile PIE with preview device launch options."))
	bool bMobilePIEPreviewDeviceLaunch;

	/** Enables the ability to bake materials inside of the Static Mesh and Skeletal Mesh Editor, and for static mesh instances. */
	UPROPERTY(EditAnywhere, config, Category = Tools, meta = (ConfigRestartRequired = true))
	bool bAssetMaterialBaking;
	
	/**
	 * Returns an event delegate that is executed when a setting has changed.
	 *
	 * @return The delegate.
	 */
	DECLARE_EVENT_OneParam(UEditorExperimentalSettings, FSettingChangedEvent, FName /*PropertyName*/);
	FSettingChangedEvent& OnSettingChanged( )
	{
		return SettingChangedEvent;
	}

protected:

	// UObject overrides

	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent ) override;

private:

	// Holds an event delegate that is executed when a setting has changed.
	FSettingChangedEvent SettingChangedEvent;
};
