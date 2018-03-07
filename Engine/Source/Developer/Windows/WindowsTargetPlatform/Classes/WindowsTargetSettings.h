// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WindowsTargetSettings.h: Declares the UWindowsTargetSettings class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WindowsTargetSettings.generated.h"

UENUM()
enum class EMinimumSupportedOS : uint8
{
	MSOS_Vista = 0 UMETA(DisplayName = "Windows Vista"),
};

UENUM()
enum class ECompilerVersion : uint8
{
	Default = 0,
	VisualStudio2015 = 1 UMETA(DisplayName = "Visual Studio 2015"),
	VisualStudio2017 = 2 UMETA(DisplayName = "Visual Studio 2017"),
};

/**
 * Implements the settings for the Windows target platform. The first instance of this class is initialized in
 * WindowsTargetPlatform, really early during the startup sequence before the CDO has been constructed, so its config 
 * settings are read manually from there.
 */
UCLASS(config=Engine, defaultconfig)
class WINDOWSTARGETPLATFORM_API UWindowsTargetSettings
	: public UObject
{
public:

	GENERATED_UCLASS_BODY()

	/** The compiler version to use for this project. May be different to the chosen IDE. */
	UPROPERTY(EditAnywhere, config, Category="Toolchain", Meta=(DisplayName="Compiler Version"))
	ECompilerVersion Compiler;

	/** 
	 * The collection of RHI's we want to support on this platform.
	 * This is not always the full list of RHI we can support.
	 */
	UPROPERTY(EditAnywhere, config, Category=Rendering)
	TArray<FString> TargetedRHIs;

	/**
	 * Determine the minimum supported 
	 */
	UPROPERTY(EditAnywhere, config, Category="OS Info", Meta=(DisplayName = "Minimum OS Version"))
	EMinimumSupportedOS MinimumOSVersion;

	/** The audio device name to use if not the default windows audio device. Leave blank to use default audio device. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString AudioDevice;

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
