// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CookerSettings.h: Declares the UCookerSettings class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/DeveloperSettings.h"
#include "CookerSettings.generated.h"

struct FPropertyChangedEvent;

/**
 * Various cooker settings.
 */
UCLASS(config=Engine, defaultconfig, meta=(DisplayName="Cooker"))
class UNREALED_API UCookerSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

	virtual void PostInitProperties() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

public:

	UPROPERTY(GlobalConfig, EditAnywhere, Category = Cooker, meta = (DisplayName = "Enable cooking via network in the background of the editor, launch on uses this setting, requires device to have network access to editor", ConfigRestartRequired = true))
	bool bEnableCookOnTheSide;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = Cooker, meta = (DisplayName = "Generate DDC data in background for desired launch on platform (speeds up launch on)"))
	bool bEnableBuildDDCInBackground;

	/** Enable -iterate for launch on */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Cooker, meta = (DisplayName = "Iterative cooking for builds launched from the editor (launch on)"))
	bool bIterativeCookingForLaunchOn;

	/** Enable -iterate for launch on */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Cooker, meta = (DisplayName = "Iterative cooking for the File->Cook Content menu item"))
	bool bIterativeCookingForFileCookContent;

	/** Ignore ini changes when doing iterative cooking, either in editor or out of editor */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Cooker, AdvancedDisplay)
	bool bIgnoreIniSettingsOutOfDateForIteration;

	/** Ignore native header file changes when doing iterative cooking, either in editor or out of editor */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Cooker, AdvancedDisplay)
	bool bIgnoreScriptPackagesOutOfDateForIteration;

	/** Whether or not to compile Blueprints in development mode when cooking. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Cooker, AdvancedDisplay)
	bool bCompileBlueprintsInDevelopmentMode;
	
	/** Whether or not to cook Blueprint Component data for faster instancing at runtime. This assumes that the Component templates do not get modified at runtime. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Cooker, AdvancedDisplay, meta = (DisplayName = "Cook Blueprint Component data for faster instancing at runtime"))
	bool bCookBlueprintComponentTemplateData;

	/** List of class names to exclude when cooking for dedicated server */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Cooker, AdvancedDisplay, meta = (DisplayName = "Classes excluded when cooking for dedicated server"))
	TArray<FString> ClassesExcludedOnDedicatedServer;

	/** List of module names to exclude when cooking for dedicated server */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Cooker, AdvancedDisplay, meta = (DisplayName = "Modules excluded when cooking for dedicated server"))
	TArray<FString> ModulesExcludedOnDedicatedServer;

	/** List of class names to exclude when cooking for dedicated client */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Cooker, AdvancedDisplay, meta = (DisplayName = "Classes excluded when cooking for dedicated client"))
	TArray<FString> ClassesExcludedOnDedicatedClient;

	/** List of module names to exclude when cooking for dedicated client */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Cooker, AdvancedDisplay, meta = (DisplayName = "Modules excluded when cooking for dedicated client"))
	TArray<FString> ModulesExcludedOnDedicatedClient;

	/** Quality of 0 means fastest, 4 means best quality */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Textures, meta = (DisplayName = "PVRTC Compression Quality (0-4, 0 is fastest)"))
	int32 DefaultPVRTCQuality;

	/** Quality of 0 means fastest, 4 means best quality */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Textures, meta = (DisplayName = "ASTC Compression Quality vs Speed (0-4, 0 is fastest)"))
	int32 DefaultASTCQualityBySpeed;

	/** Quality of 0 means smallest (12x12 block size), 4 means best (4x4 block size) */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Textures, meta = (DisplayName = "ASTC Compression Quality vs Size (0-4, 0 is smallest)"))
	int32 DefaultASTCQualityBySize;
};
