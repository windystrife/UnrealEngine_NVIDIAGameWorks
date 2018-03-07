// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "ProjectPackagingSettings.generated.h"

/**
 * Enumerates the available build configurations for project packaging.
 */
UENUM()
enum EProjectPackagingBuildConfigurations
{
	/** Debug configuration. */
	PPBC_DebugGame UMETA(DisplayName="DebugGame"),

	/** Debug Client configuration. */
	PPBC_DebugGameClient UMETA(DisplayName = "DebugGame Client"),

	/** Development configuration. */
	PPBC_Development UMETA(DisplayName="Development"),

	/** Development Client configuration. */
	PPBC_DevelopmentClient UMETA(DisplayName = "Development Client"),

	/** Shipping configuration. */
	PPBC_Shipping UMETA(DisplayName="Shipping"),

	/** Shipping Client configuration. */
	PPBC_ShippingClient UMETA(DisplayName = "Shipping Client")
};

/**
 * Enumerates the available internationalization data presets for project packaging.
 */
UENUM()
enum class EProjectPackagingInternationalizationPresets : uint8
{
	/** English only. */
	English,

	/** English, French, Italian, German, Spanish. */
	EFIGS,

	/** English, French, Italian, German, Spanish, Chinese, Japanese, Korean. */
	EFIGSCJK,

	/** Chinese, Japanese, Korean. */
	CJK,

	/** All known cultures. */
	All
};

/**
 * Determines whether to build the executable when packaging. Note the equivalence between these settings and EPlayOnBuildMode.
 */
UENUM()
enum class EProjectPackagingBuild
{
	/** Always build. */
	Always UMETA(DisplayName="Always"),

	/** Never build. */
	Never UMETA(DisplayName="Never"),

	/** Default (if the Never build. */
	IfProjectHasCode UMETA(DisplayName="If project has code, or running a locally built editor"),

	/** If we're not packaging from a promoted build. */
	IfEditorWasBuiltLocally UMETA(DisplayName="If running a locally built editor")
};

/**
* Enumerates the available methods for Blueprint nativization during project packaging.
*/
UENUM()
enum class EProjectPackagingBlueprintNativizationMethod : uint8
{
	/** Disable Blueprint nativization (default). */
	Disabled,

	/** Enable nativization for all Blueprint assets. */
	Inclusive,

	/** Enable nativization for selected Blueprint assets only. */
	Exclusive
};

/**
 * Implements the Editor's user settings.
 */
UCLASS(config=Game, defaultconfig)
class UNREALED_API UProjectPackagingSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Specifies whether to build the game executable during packaging. */
	UPROPERTY(config, EditAnywhere, Category=Project)
	EProjectPackagingBuild Build;

	/** The build configuration for which the project is packaged. */
	UPROPERTY(config, EditAnywhere, Category=Project)
	TEnumAsByte<EProjectPackagingBuildConfigurations> BuildConfiguration;

	/** The directory to which the packaged project will be copied. */
	UPROPERTY(config, EditAnywhere, Category=Project)
	FDirectoryPath StagingDirectory;

	/**
	 * If enabled, a full rebuild will be enforced each time the project is being packaged.
	 * If disabled, only modified files will be built, which can improve iteration time.
	 * Unless you iterate on packaging, we recommend full rebuilds when packaging.
	 */
	UPROPERTY(config, EditAnywhere, Category=Project)
	bool FullRebuild;

	/**
	 * If enabled, a distribution build will be created and the shipping configuration will be used
	 * If disabled, a development build will be created
	 * Distribution builds are for publishing to the App Store
	 */
	UPROPERTY(config, EditAnywhere, Category=Project)
	bool ForDistribution;

	/** If enabled, debug files will be included in the packaged game */
	UPROPERTY(config, EditAnywhere, Category=Project)
	bool IncludeDebugFiles;

	/** If enabled, then the project's Blueprint assets (including structs and enums) will be intermediately converted into C++ and used in the packaged project (in place of the .uasset files).*/
	UPROPERTY(config, EditAnywhere, Category = Blueprints)
	EProjectPackagingBlueprintNativizationMethod BlueprintNativizationMethod;

	/** List of Blueprints to include for nativization when using the exclusive method. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = Blueprints, meta = (DisplayName = "List of Blueprint assets to nativize", RelativeToGameContentDir, LongPackageName))
	TArray<FFilePath> NativizeBlueprintAssets;

	/** If enabled, the nativized assets code plugin will be added to the Visual Studio solution if it exists when regenerating the game project. Intended primarily to assist with debugging the target platform after cooking with nativization turned on. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = Blueprints)
	bool bIncludeNativizedAssetsInProjectGeneration;

	/** If enabled, all content will be put into a single .pak file instead of many individual files (default = enabled). */
	UPROPERTY(config, EditAnywhere, Category=Packaging)
	bool UsePakFile;

	/** 
	 * If enabled, will generate pak file chunks.  Assets can be assigned to chunks in the editor or via a delegate (See ShooterGameDelegates.cpp). 
	 * Can be used for streaming installs (PS4 Playgo, XboxOne Streaming Install, etc)
	 */
	UPROPERTY(config, EditAnywhere, Category=Packaging)
	bool bGenerateChunks;

	/** 
	 * If enabled, no platform will generate chunks, regardless of settings in platform-specific ini files.
	 */
	UPROPERTY(config, EditAnywhere, Category=Packaging)
	bool bGenerateNoChunks;

	/**
	* Normally during chunk generation all dependencies of a package in a chunk will be pulled into that package's chunk.
	* If this is enabled then only hard dependencies are pulled in. Soft dependencies stay in their original chunk.
	*/
	UPROPERTY(config, EditAnywhere, Category = Packaging)
	bool bChunkHardReferencesOnly;

	/** 
	 * If enabled, will generate data for HTTP Chunk Installer. This data can be hosted on webserver to be installed at runtime. Requires "Generate Chunks" enabled.
	 */
	UPROPERTY(config, EditAnywhere, Category=Packaging)
	bool bBuildHttpChunkInstallData;

	/** 
	 * When "Build HTTP Chunk Install Data" is enabled this is the directory where the data will be build to.
	 */	
	UPROPERTY(config, EditAnywhere, Category = Packaging)
	FDirectoryPath HttpChunkInstallDataDirectory;

	/** 
	 * Version name for HTTP Chunk Install Data.
	 */
	UPROPERTY(config, EditAnywhere, Category = Packaging)
	FString HttpChunkInstallDataVersion;

	/** Specifies whether to include an installer for prerequisites of packaged games, such as redistributable operating system components, on platforms that support it. */
	UPROPERTY(config, EditAnywhere, Category=Prerequisites, meta=(DisplayName="Include prerequisites installer"))
	bool IncludePrerequisites;

	/** Specifies whether to include prerequisites alongside the game executable. */
	UPROPERTY(config, EditAnywhere, Category = Prerequisites, meta = (DisplayName = "Include app-local prerequisites"))
	bool IncludeAppLocalPrerequisites;

	/** 
	 * By default shader code gets saved inline inside material assets, 
	 * enabling this option will store only shader code once as individual files
	 * This will reduce overall package size but might increase loading time
	 */
	UPROPERTY(config, EditAnywhere, Category=Packaging)
	bool bShareMaterialShaderCode;

	/** 
	 * By default shader shader code gets saved into individual platform agnostic files,
	 * enabling this option will use the platform-specific library format if and only if one is available
	 * This will reduce overall package size but might increase loading time
	 */
	UPROPERTY(config, EditAnywhere, Category=Packaging, meta = (EditCondition = "bShareMaterialShaderCode", ConfigRestartRequired = true))
	bool bSharedMaterialNativeLibraries;

	/** A directory containing additional prerequisite packages that should be staged in the executable directory. Can be relative to $(EngineDir) or $(ProjectDir) */
	UPROPERTY(config, EditAnywhere, Category=Prerequisites, AdvancedDisplay)
	FDirectoryPath ApplocalPrerequisitesDirectory;

	/**
	 * Specifies whether to include the crash reporter in the packaged project. 
	 * This is included by default for Blueprint based projects, but can optionally be disabled.
	 */
	UPROPERTY(config, EditAnywhere, Category=Packaging, AdvancedDisplay)
	bool IncludeCrashReporter;

	/** Predefined sets of culture whose internationalization data should be packaged. */
	UPROPERTY(config, EditAnywhere, Category=Packaging, AdvancedDisplay, meta=(DisplayName="Internationalization Support"))
	EProjectPackagingInternationalizationPresets InternationalizationPreset;

	/** Cultures whose data should be cooked, staged, and packaged. */
	UPROPERTY(config, EditAnywhere, Category=Packaging, AdvancedDisplay, meta=(DisplayName="Localizations to Package"))
	TArray<FString> CulturesToStage;

	/**
	 * Cook all things in the project content directory
	 */
	UPROPERTY(config, EditAnywhere, Category=Packaging, AdvancedDisplay, meta=(DisplayName="Cook everything in the project content directory (ignore list of maps below)"))
	bool bCookAll;

	/**
	 * Cook only maps (this only affects the cookall flag)
	 */
	UPROPERTY(config, EditAnywhere, Category=Packaging, AdvancedDisplay, meta=(DisplayName="Cook only maps (this only affects cookall)"))
	bool bCookMapsOnly;


	/**
	 * Create compressed cooked packages (decreased deployment size)
	 */
	UPROPERTY(config, EditAnywhere, Category=Packaging, AdvancedDisplay, meta=(DisplayName="Create compressed cooked packages"))
	bool bCompressed;

	/**
	* Encrypt ini files inside of the pak file
	*/
	UPROPERTY(config, EditAnywhere, Category = Packaging, AdvancedDisplay, meta = (DisplayName = "Encrypt ini files inside pak files"))
	bool bEncryptIniFiles;

	/**
	* Encrypt the pak index 
	*/
	UPROPERTY(config, EditAnywhere, Category = Packaging, AdvancedDisplay, meta = (DisplayName = "Encrypt the pak index, making it unusable without the required key"))
	bool bEncryptPakIndex;
	
	/**
	* Don't include content in any editor folders when cooking.  This can cause issues with missing content in cooked games if the content is being used. 
	*/
	UPROPERTY(config, EditAnywhere, Category = Packaging, AdvancedDisplay, meta = (DisplayName = "Exclude editor content when cooking"))
	bool bSkipEditorContent;

	/**
	 * List of maps to include when no other map list is specified on commandline
	 */
	UPROPERTY(config, EditAnywhere, Category = Packaging, AdvancedDisplay, meta = (DisplayName = "List of maps to include in a packaged build", RelativeToGameContentDir, LongPackageName))
	TArray<FFilePath> MapsToCook;	

	/**
	 * Directories containing .uasset files that should always be cooked regardless of whether they're referenced by anything in your project
	 * Note: These paths are relative to your project Content directory
	 */
	UPROPERTY(config, EditAnywhere, Category=Packaging, AdvancedDisplay, meta=(DisplayName="Additional Asset Directories to Cook", RelativeToGameContentDir))
	TArray<FDirectoryPath> DirectoriesToAlwaysCook;
	

	/**
	* Directories containing .uasset files that should always be cooked regardless of whether they're referenced by anything in your project
	* Note: These paths are relative to your project Content directory
	*/
	UPROPERTY(config, EditAnywhere, Category = Packaging, AdvancedDisplay, meta = (DisplayName = "Directories to never cook", RelativeToGameContentDir))
	TArray<FDirectoryPath> DirectoriesToNeverCook;


	/**
	 * Directories containing files that should always be added to the .pak file (if using a .pak file; otherwise they're copied as individual files)
	 * This is used to stage additional files that you manually load via the UFS (Unreal File System) file IO API
	 * Note: These paths are relative to your project Content directory
	 */
	UPROPERTY(config, EditAnywhere, Category=Packaging, AdvancedDisplay, meta=(DisplayName="Additional Non-Asset Directories to Package", RelativeToGameContentDir))
	TArray<FDirectoryPath> DirectoriesToAlwaysStageAsUFS;

	/**
	 * Directories containing files that should always be copied when packaging your project, but are not supposed to be part of the .pak file
	 * This is used to stage additional files that you manually load without using the UFS (Unreal File System) file IO API, eg, third-party libraries that perform their own internal file IO
	 * Note: These paths are relative to your project Content directory
	 */
	UPROPERTY(config, EditAnywhere, Category=Packaging, AdvancedDisplay, meta=(DisplayName="Additional Non-Asset Directories To Copy", RelativeToGameContentDir))
	TArray<FDirectoryPath> DirectoriesToAlwaysStageAsNonUFS;	

private:
	/** Helper array used to mirror Blueprint asset selections across edits */
	TArray<FFilePath> CachedNativizeBlueprintAssets;

	UPROPERTY(config)
	bool bNativizeBlueprintAssets_DEPRECATED;

	UPROPERTY(config)
	bool bNativizeOnlySelectedBlueprints_DEPRECATED;
	
public:

	// UObject Interface

	virtual void PostInitProperties() override;
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent ) override;
	virtual bool CanEditChange( const UProperty* InProperty ) const override;

	/** Adds the given Blueprint asset to the exclusive nativization list. */
	bool AddBlueprintAssetToNativizationList(const class UBlueprint* InBlueprint);

	/** Removes the given Blueprint asset from the exclusive nativization list. */
	bool RemoveBlueprintAssetFromNativizationList(const class UBlueprint* InBlueprint);

	/** Determines if the specified Blueprint is already saved for exclusive nativization. */
	bool IsBlueprintAssetInNativizationList(const class UBlueprint* InBlueprint) const { return FindBlueprintInNativizationList(InBlueprint) >= 0; }

private:
	/** Returns the index of the specified Blueprint in the exclusive nativization list (otherwise INDEX_NONE) */
	int32 FindBlueprintInNativizationList(const UBlueprint* InBlueprint) const;
};
