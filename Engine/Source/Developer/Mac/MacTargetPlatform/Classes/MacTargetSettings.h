// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MacTargetSettings.h: Declares the UMacTargetSettings class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MacTargetSettings.generated.h"

UENUM()
enum class EMacMetalShaderStandard : uint8
{
    /** Metal Shaders, supporting Tessellation Shaders & Fragment Shader UAVs, Compatible With macOS Sierra 10.12.0 or later (std=osx-metal1.2) */
    MacMetalSLStandard_1_2 = 2 UMETA(DisplayName="Metal v1.2 (10.12.0+)"),
    
    /** Metal Shaders, supporting multiple viewports, Compatible With macOS 10.13.0 or later (std=osx-metal2.0) */
    MacMetalSLStandard_2_0 = 3 UMETA(DisplayName="Metal v2.0 (10.13.0+)"),
};

/**
 * Implements the settings for the Mac target platform.
 */
UCLASS(config=Engine, defaultconfig)
class MACTARGETPLATFORM_API UMacTargetSettings
	: public UObject
{
public:

	GENERATED_UCLASS_BODY()
	
	/**
	 * The collection of RHI's we want to support on this platform.
	 * This is not always the full list of RHI we can support.
	 */
	UPROPERTY(EditAnywhere, config, Category=Rendering)
	TArray<FString> TargetedRHIs;
    
    /**
     * The maximum supported Metal shader langauge version. 
     * This defines what features may be used and OS versions supported.
     */
    UPROPERTY(EditAnywhere, config, Category=Rendering, meta = (DisplayName = "Max. Metal Shader Standard To Target", ConfigRestartRequired = true))
    uint8 MaxShaderLanguageVersion;
    
    /**
     * Whether to use the Metal shading language's "fast" intrinsics.
	 * Fast intrinsics assume that no NaN or INF value will be provided as input, 
	 * so are more efficient. However, they will produce undefined results if NaN/INF 
	 * is present in the argument/s. By default fast-instrinics are disabled so Metal correctly handles NaN/INF arguments.
     */
    UPROPERTY(EditAnywhere, config, Category=Rendering, meta = (DisplayName = "Use Fast-Math intrinsics", ConfigRestartRequired = true))
	bool UseFastIntrinsics;
	
	/**
	 * Whether to use of Metal shader-compiler's -ffast-math optimisations.
	 * Fast-Math performs algebraic-equivalent & reassociative optimisations not permitted by the floating point arithmetic standard (IEEE-754).
	 * These can improve shader performance at some cost to precision and can lead to NaN/INF propagation as they rely on
	 * shader inputs or variables not containing NaN/INF values. By default fast-math is enabled for performance.
	 */
	UPROPERTY(EditAnywhere, config, Category=Rendering, meta = (DisplayName = "Enable Fast-Math optimisations", ConfigRestartRequired = true))
	bool EnableMathOptimisations;

	/** Sample rate to run the audio mixer with. */
	UPROPERTY(config, EditAnywhere, Category = "Audio", Meta = (DisplayName = "Audio Mixer Sample Rate"))
	int32 AudioSampleRate;

	/** The amount of audio to compute each callback block. Lower values decrease latency but may increase CPU cost. */
	UPROPERTY(config, EditAnywhere, Category = "Audio", meta = (ClampMin = "512", ClampMax = "4096", DisplayName = "Callback Buffer Size"))
	int32 AudioCallbackBufferFrameSize;

	/** The number of buffers to keep enqueued. More buffers increases latency, but can compensate for variable compute availability in audio callbacks on some platforms. */
	UPROPERTY(config, EditAnywhere, Category = "Audio", meta = (ClampMin = "1", UIMin = "1", DisplayName = "Number of Buffers To Enqueue"))
	int32 AudioNumBuffersToEnqueue;

	/** The max number of channels (voices) to limit for this platform. The max channels used will be the minimum of this value and the global audio quality settings. A value of 0 will not apply a platform channel count max. */
	UPROPERTY(config, EditAnywhere, Category = "Audio", meta = (ClampMin = "0", UIMin = "0", DisplayName = "Max Channels"))
	int32 AudioMaxChannels;

	/** The number of workers to use to compute source audio. Will only use up to the max number of sources. Will evenly divide sources to each source worker. */
	UPROPERTY(config, EditAnywhere, Category = "Audio", meta = (ClampMin = "0", UIMin = "0", DisplayName = "Number of Source Workers"))
	int32 AudioNumSourceWorkers;

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
