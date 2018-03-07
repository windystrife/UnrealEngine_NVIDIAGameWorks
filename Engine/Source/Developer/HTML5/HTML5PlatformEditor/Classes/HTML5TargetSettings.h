// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HTML5TargetSettings.h: Declares the UHTML5TargetSettings class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "HTML5TargetSettings.generated.h"


USTRUCT()
struct FHTML5LevelTransitions
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = HTML5_LevelTransitions, Meta = (DisplayName = "From Map"))
	FFilePath MapFrom;

	UPROPERTY(EditAnywhere, Category = HTML5_LevelTransitions, Meta = (DisplayName = "To Map"))
	FFilePath MapTo;
};


/**
 * Implements the settings for the HTML5 target platform.
 */
UCLASS(config=Engine, defaultconfig)
class HTML5PLATFORMEDITOR_API UHTML5TargetSettings
	: public UObject
{
public:

	GENERATED_UCLASS_BODY()

	// ------------------------------------------------------------

	/**
	 * Target WebGL1 builds
	 * NOTE: WebGL1 target will be going away soon...
	 */
	UPROPERTY(GlobalConfig, EditAnywhere, Category=Emscripten, Meta = (DisplayName = "WebGL1 Build (else build WebGL2)"))
	bool TargetWebGL1;

	/**
	 * Use IndexedDB storage
	 */
	UPROPERTY(GlobalConfig, EditAnywhere, Category=Emscripten, Meta = (DisplayName = "IndexedDB storage"))
	bool EnableIndexedDB;

	/**
	 * Use Fixed TimeStep
	 */
	UPROPERTY(GlobalConfig, EditAnywhere, Category=Emscripten, Meta = (DisplayName = "Fixed TimeStep (i.e. use requestAnimationFrame)"))
	bool UseFixedTimeStep; // need to make a note of: answerhub 409629

//	TODO: re-enable these when they become supported in WASM
//	/**
//	 * Enable SIMD
//	 * NOTE 1: this does not currently work with WASM - it will be forced false in this case.
//	 * NOTE 2: SIMD will be supported during WASM builds in a future emscripten release.
//	 */
//	UPROPERTY(GlobalConfig, EditAnywhere, Category=Emscripten, Meta = (DisplayName = "SIMD support"))
//	bool EnableSIMD;

//	TODO: re-enable these when they become supported in WASM
//	/**
//	 * Enable Multithreading
//	 * NOTE 1: this is not supported currently in WASM - it will be forced false in this case.
//	 * NOTE 2: Multithreading will be supported during WASM builds in a future emscripten release.
//	 */
//	UPROPERTY(GlobalConfig, EditAnywhere, Category=Emscripten, Meta = (DisplayName = "Multithreading support"))
//	bool EnableMultithreading;

	/**
	 * Enable Tracing (trace.h)
	 */
	UPROPERTY(GlobalConfig, EditAnywhere, Category=Emscripten, Meta = (DisplayName = "Tracing support"))
	bool EnableTracing;

	// ------------------------------------------------------------

	/**
	 * Compress Files
	 * NOTE 1: it is also recommended to NOT enable PAK file packaging - this is currently redundant
	 * NOTE 2: future emscripten version will allow separate (asset) files in a new FileSystem feature - which will make use of this (as well as PAK file) option again
	 */
	UPROPERTY(GlobalConfig, EditAnywhere, Category=Packaging, Meta = (DisplayName = "Compress files during shipping packaging"))
	bool Compressed;

	// ------------------------------------------------------------

	/**
	 * Port to use when deploying game from the editor
	 */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Launch, Meta = (DisplayName = "Port to use when deploying game from the editor", ClampMin="49152", ClampMax="65535"))
	int32 DeployServerPort;

	/**
	 * Generate Delta Pak files for these level transitions.
	 */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Launch, Meta = (DisplayName = "Level transitions for delta paks [experimental,depends on download maps]"))
	TArray<FHTML5LevelTransitions> LevelTransitions;

	// ------------------------------------------------------------

	UPROPERTY(GlobalConfig, EditAnywhere, Category = Amazon_S3, Meta = (DisplayName = "Upload builds to Amazon S3 when packaging"))
	bool UploadToS3;

	/**
	* Required
	*/
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Amazon_S3, Meta = (DisplayName = "Amazon S3 Region", EditCondition = "UploadToS3"))
	FString S3Region;
	/**
	* Required
	*/
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Amazon_S3, Meta = (DisplayName = "Amazon S3 Key ID", EditCondition = "UploadToS3"))
	FString S3KeyID;
	/**
	* Required
	*/
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Amazon_S3, Meta = (DisplayName = "Amazon S3 Secret Access Key", EditCondition = "UploadToS3"))
	FString S3SecretAccessKey;
	/**
	* Required
	*/
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Amazon_S3, Meta = (DisplayName = "Amazon S3 Bucket Name", EditCondition = "UploadToS3"))
	FString S3BucketName;
	/**
	* Provide another level of nesting beyond the bucket. Can be left empty, defaults to game name. DO NOT LEAVE A TRAILING SLASH!
	*/
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Amazon_S3, Meta = (DisplayName = "Nested Folder Name", EditCondition = "UploadToS3"))
	FString S3FolderName;

	/** Which of the currently enabled spatialization plugins to use on Windows. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString SpatializationPlugin;

	/** Which of the currently enabled reverb plugins to use on Windows. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString ReverbPlugin;

	/** Which of the currently enabled occlusion plugins to use on Windows. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString OcclusionPlugin;
};

