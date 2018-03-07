//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include <phonon.h>
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSteamAudio, Log, All);

UENUM(BlueprintType)
enum class EQualitySettings : uint8
{
	LOW				UMETA(DisplayName = "Low"),
	MEDIUM			UMETA(DisplayName = "Medium"),
	HIGH			UMETA(DisplayName = "High"),
	CUSTOM			UMETA(DisplayName = "Custom")
};

UENUM(BlueprintType)
enum class EIplSpatializationMethod : uint8
{
	// Classic 2D panning - fast.
	PANNING			UMETA(DisplayName = "Panning"),
	// Full 3D audio processing with HRTF.
	HRTF			UMETA(DisplayName = "HRTF")
};

UENUM(BlueprintType)
enum class EIplHrtfInterpolationMethod : uint8
{
	// Uses a nearest neighbor lookup - fast.
	NEAREST			UMETA(DisplayName = "Nearest"),
	// Bilinearly interpolates the HRTF before processing. Slower, but can result in a smoother sound as the listener rotates.
	BILINEAR		UMETA(DisplayName = "Bilinear")
};

UENUM(BlueprintType)
enum class EIplDirectOcclusionMethod : uint8
{
	// Binary visible or not test. Adjusts direct volume accordingly.
	RAYCAST			UMETA(DisplayName = "Raycast"),
	// Treats the source as a sphere instead of a point. Smoothly ramps up volume as source becomes visible to listener.
	VOLUMETRIC		UMETA(DisplayName = "Partial")
};

UENUM(BlueprintType)
enum class EIplDirectOcclusionMode : uint8
{
	// Do not perform any occlusion checks.
	NONE										UMETA(DisplayName = "None"),
	// Perform occlusion checks but do not model transmission.
	DIRECTOCCLUSION_NOTRANSMISSION				UMETA(DisplayName = "Direct Occlusion, No Transmission"),
	// Perform occlusion checks and model transmission; occluded sound will be scaled by a frequency-independent attenuation value.
	DIRECTOCCLUSION_TRANSMISSIONBYVOLUME		UMETA(DisplayName = "Direct Occlusion, Frequency-Independent Transmission"),
	// Perform occlusion checks and model transmission; occluded sound will be rendered with a frequency-dependent transmission filter.
	DIRECTOCCLUSION_TRANSMISSIONBYFREQUENCY		UMETA(DisplayName = "Direct Occlusion, Frequency-Dependent Transmission")
};

UENUM(BlueprintType)
enum class EIplSimulationType : uint8
{
	// Simulate indirect sound at run time.
	REALTIME		UMETA(DisplayName = "Real-Time"),
	// Precompute indirect sound.
	BAKED			UMETA(DisplayName = "Baked"),
	// Do not simulate indirect sound.
	DISABLED		UMETA(DisplayName = "Disabled")
};

UENUM(BlueprintType)
enum class EIplAudioEngine : uint8
{
	// Native Unreal audio engine.
	UNREAL			UMETA(DisplayName = "Unreal")
};

namespace SteamAudio
{
	struct FSimulationQualitySettings
	{
		int32 Bounces;
		int32 Rays;
		int32 SecondaryRays;
	};

	// 1 Unreal Unit = 1cm, 1 Phonon Unit = 1m
	const float SCALEFACTOR = 0.01f;

	extern TMap<EQualitySettings, FSimulationQualitySettings> RealtimeSimulationQualityPresets;
	extern TMap<EQualitySettings, FSimulationQualitySettings> BakedSimulationQualityPresets;

	extern const IPLContext STEAMAUDIO_API GlobalContext;

	IPLVector3 STEAMAUDIO_API IPLVector3FromFVector(const FVector& Coords);
	FVector STEAMAUDIO_API FVectorFromIPLVector3(const IPLVector3& Coords);
	FVector STEAMAUDIO_API UnrealToPhononFVector(const FVector& Coords, const bool bScale = true);
	IPLVector3 STEAMAUDIO_API UnrealToPhononIPLVector3(const FVector& Coords, const bool bScale = true);
	FVector STEAMAUDIO_API PhononToUnrealFVector(const FVector& Coords, const bool bScale = true);
	IPLVector3 STEAMAUDIO_API PhononToUnrealIPLVector3(const FVector& Coords, const bool bScale = true);

	void STEAMAUDIO_API GetMatrixForTransform(const FTransform& Transform, float* OutMatrix);

	FText STEAMAUDIO_API GetKBTextFromByte(const int32 NumBytes);

	/** Attempts to loads the specified DLL, performing some basic error checking. Returns handle to DLL or nullptr on error.*/
	void* LoadDll(const FString& DllFile);

	/** Error logs non-successful statuses. */
	void LogSteamAudioStatus(const IPLerror Status);

	/** Converts IPL statuses to a readable FString. */
	FString StatusToFString(const IPLerror Status);
}
