//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "PhononMaterial.h"
#include "PhononCommon.h"
#include "SteamAudioSettings.generated.h"

UCLASS(config = Engine, defaultconfig)
class STEAMAUDIO_API USteamAudioSettings : public UObject
{
	GENERATED_BODY()

public:

	USteamAudioSettings();

	IPLMaterial GetDefaultStaticMeshMaterial() const;
	IPLMaterial GetDefaultBSPMaterial() const;
	IPLMaterial GetDefaultLandscapeMaterial() const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange(const UProperty* InProperty) const override;
#endif

	// Which audio engine to use.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = General)
	EIplAudioEngine AudioEngine;

	//==============================================================================================================================================

	// Whether or not to export BSP geometry.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export", meta = (DisplayName = "Export BSP Geometry"))
	bool ExportBSPGeometry;

	// Whether or not to export Landscape geometry.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export", meta = (DisplayName = "Export Landscape Geometry"))
	bool ExportLandscapeGeometry;

	//==============================================================================================================================================

	// Preset material settings for Static Mesh actors.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Static Mesh Material", meta = (DisplayName = "Material Preset"))
	EPhononMaterial StaticMeshMaterialPreset;

	// How much this material absorbs low frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Static Mesh Material", meta = (ClampMin = "0.0", ClampMax = "1.0", 
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Low Frequency Absorption"))
	float StaticMeshLowFreqAbsorption;

	// How much this material absorbs mid frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Static Mesh Material", meta = (ClampMin = "0.0", ClampMax = "1.0", 
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Mid Frequency Absorption"))
	float StaticMeshMidFreqAbsorption;

	// How much this material absorbs high frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Static Mesh Material", meta = (ClampMin = "0.0", ClampMax = "1.0", 
		UIMin = "0.0", UIMax = "1.0", DisplayName = "High Frequency Absorption"))
	float StaticMeshHighFreqAbsorption; 

	// How much this material transmits low frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Static Mesh Material", meta = (ClampMin = "0.0", ClampMax = "1.0",
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Low Frequency Transmission"))
	float StaticMeshLowFreqTransmission;

	// How much this material transmits mid frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Static Mesh Material", meta = (ClampMin = "0.0", ClampMax = "1.0",
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Mid Frequency Transmission"))
	float StaticMeshMidFreqTransmission;

	// How much this material transmits high frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Static Mesh Material", meta = (ClampMin = "0.0", ClampMax = "1.0",
		UIMin = "0.0", UIMax = "1.0", DisplayName = "High Frequency Transmission"))
	float StaticMeshHighFreqTransmission;

	// Specifies how "rough" the surface is. Surfaces with a high scattering value randomly reflect sound in all directions;
	// surfaces with a low scattering value reflect sound in a mirror-like manner.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Static Mesh Material", meta = (ClampMin = "0.0", ClampMax = "1.0", 
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Scattering"))
	float StaticMeshScattering;

	//==============================================================================================================================================

	// Preset material settings for BSP geometry.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default BSP Material", meta = (DisplayName = "Material Preset"))
	EPhononMaterial BSPMaterialPreset;

	// How much this material absorbs low frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default BSP Material", meta = (ClampMin = "0.0", ClampMax = "1.0", 
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Low Frequency Absorption"))
	float BSPLowFreqAbsorption;

	// How much this material absorbs mid frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default BSP Material", meta = (ClampMin = "0.0", ClampMax = "1.0", 
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Mid Frequency Absorption"))
	float BSPMidFreqAbsorption;

	// How much this material absorbs high frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default BSP Material", meta = (ClampMin = "0.0", ClampMax = "1.0", 
		UIMin = "0.0", UIMax = "1.0", DisplayName = "High Frequency Absorption"))
	float BSPHighFreqAbsorption;

	// How much this material transmits low frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default BSP Material", meta = (ClampMin = "0.0", ClampMax = "1.0",
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Low Frequency Transmission"))
	float BSPLowFreqTransmission;

	// How much this material transmits mid frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default BSP Material", meta = (ClampMin = "0.0", ClampMax = "1.0",
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Mid Frequency Transmission"))
	float BSPMidFreqTransmission;

	// How much this material transmits high frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default BSP Material", meta = (ClampMin = "0.0", ClampMax = "1.0",
		UIMin = "0.0", UIMax = "1.0", DisplayName = "High Frequency Transmission"))
	float BSPHighFreqTransmission;

	// Specifies how "rough" the surface is. Surfaces with a high scattering value randomly reflect sound in all directions;
	// surfaces with a low scattering value reflect sound in a mirror-like manner.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default BSP Material", meta = (ClampMin = "0.0", ClampMax = "1.0", 
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Scattering"))
	float BSPScattering;

	//==============================================================================================================================================

	// Preset material settings for landscape actors.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Landscape Material", meta = (DisplayName = "Material Preset"))
	EPhononMaterial LandscapeMaterialPreset;

	// How much this material absorbs low frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Landscape Material", meta = (ClampMin = "0.0", ClampMax = "1.0", 
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Low Frequency Absorption"))
	float LandscapeLowFreqAbsorption;

	// How much this material absorbs mid frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Landscape Material", meta = (ClampMin = "0.0", ClampMax = "1.0", 
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Mid Frequency Absorption"))
	float LandscapeMidFreqAbsorption;

	// How much this material absorbs high frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Landscape Material", meta = (ClampMin = "0.0", ClampMax = "1.0", 
		UIMin = "0.0", UIMax = "1.0", DisplayName = "High Frequency Absorption"))
	float LandscapeHighFreqAbsorption;

	// How much this material transmits low frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Landscape Material", meta = (ClampMin = "0.0", ClampMax = "1.0",
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Low Frequency Transmission"))
	float LandscapeLowFreqTransmission;

	// How much this material transmits mid frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Landscape Material", meta = (ClampMin = "0.0", ClampMax = "1.0",
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Mid Frequency Transmission"))
	float LandscapeMidFreqTransmission;

	// How much this material transmits high frequency sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Landscape Material", meta = (ClampMin = "0.0", ClampMax = "1.0",
		UIMin = "0.0", UIMax = "1.0", DisplayName = "High Frequency Transmission"))
	float LandscapeHighFreqTransmission;

	// Specifies how "rough" the surface is. Surfaces with a high scattering value randomly reflect sound in all directions;
	// surfaces with a low scattering value reflect sound in a mirror-like manner.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Scene Export|Default Landscape Material", meta = (ClampMin = "0.0", ClampMax = "1.0", 
		UIMin = "0.0", UIMax = "1.0", DisplayName = "Scattering"))
	float LandscapeScattering;

	//==============================================================================================================================================

	// Output of indirect propagation is stored in ambisonics of this order.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = IndirectSound, meta = (ClampMin = "0", ClampMax = "4", UIMin = "0", UIMax = "4",
		DisplayName = "Ambisonics Order"))
	int32 IndirectImpulseResponseOrder;

	// Length of impulse response to compute for each sound source.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = IndirectSound, meta = (ClampMin = "0.1", ClampMax = "5.0", UIMin = "0.1", UIMax = "5.0",
		DisplayName = "Impulse Response Duration"))
	float IndirectImpulseResponseDuration;

	// How to spatialize indirect sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = IndirectSound)
	EIplSpatializationMethod IndirectSpatializationMethod;

	// How to simulate listener-centric reverb.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = IndirectSound)
	EIplSimulationType ReverbSimulationType;

	// Scaling applied to indirect sound.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = IndirectSound, meta = (ClampMin = "0.0", ClampMax = "10.0", UIMin = "0.0", UIMax = "10.0"))
	float IndirectContribution;

	// Maximum number of supported sources.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = IndirectSound, meta = (ClampMin = "1", ClampMax = "128", UIMin = "1", UIMax = "128"))
	uint32 MaxSources;

	//==============================================================================================================================================

	// Preset quality settings for realtime simulation.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "IndirectSound|Real-Time Quality Settings", meta = (DisplayName = "Quality Preset"))
	EQualitySettings RealtimeQualityPreset;

	// Number of bounces for realtime simulation.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "IndirectSound|Real-Time Quality Settings", meta = (ClampMin = "1", ClampMax = "32", 
		UIMin = "1", UIMax = "32", DisplayName = "Bounces"))
	int32 RealtimeBounces;

	// Number of rays to trace for realtime simulation.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "IndirectSound|Real-Time Quality Settings", meta = (ClampMin = "512", ClampMax = "16384", 
		UIMin = "512", UIMax = "16384", DisplayName = "Rays"))
	int32 RealtimeRays;

	// Number of secondary rays to trace for realtime simulation.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "IndirectSound|Real-Time Quality Settings", meta = (ClampMin = "128", ClampMax = "4096", 
		UIMin = "128", UIMax = "4096", DisplayName = "Secondary Rays"))
	int32 RealtimeSecondaryRays;

	//==============================================================================================================================================

	// Preset quality settings for baked simulation.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "IndirectSound|Baked Quality Settings", meta = (DisplayName = "Quality Preset"))
	EQualitySettings BakedQualityPreset;

	// Number of bounces for baked simulation.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "IndirectSound|Baked Quality Settings", meta = (ClampMin = "16", ClampMax = "256", 
		UIMin = "16", UIMax = "256", DisplayName = "Bounces"))
	int32 BakedBounces;

	// Number of rays to shoot for baked simulation.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "IndirectSound|Baked Quality Settings", meta = (ClampMin = "8192", ClampMax = "65536", 
		UIMin = "8192", UIMax = "65536", DisplayName = "Rays"))
	int32 BakedRays;

	// Number of secondary rays to shoot for baked simulation.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "IndirectSound|Baked Quality Settings", meta = (ClampMin = "1024", ClampMax = "16384", 
		UIMin = "1024", UIMax = "16384", DisplayName = "Secondary Rays"))
	int32 BakedSecondaryRays;
};