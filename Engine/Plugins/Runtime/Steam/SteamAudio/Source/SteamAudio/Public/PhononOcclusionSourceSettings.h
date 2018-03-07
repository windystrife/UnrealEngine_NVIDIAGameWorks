//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "IAudioExtensionPlugin.h"
#include "PhononCommon.h"
#include "PhononOcclusionSourceSettings.generated.h"

UCLASS()
class STEAMAUDIO_API UPhononOcclusionSourceSettings : public UOcclusionPluginSourceSettingsBase
{
	GENERATED_BODY()

public:
	UPhononOcclusionSourceSettings();

#if WITH_EDITOR
	virtual bool CanEditChange(const UProperty* InProperty) const override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = OcclusionSettings)
	EIplDirectOcclusionMode DirectOcclusionMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = OcclusionSettings)
	EIplDirectOcclusionMethod DirectOcclusionMethod;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = OcclusionSettings)
	float DirectOcclusionSourceRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = OcclusionSettings, meta = (DisplayName = "Physics-based Attenuation"))
	bool DirectAttenuation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = OcclusionSettings)
	bool AirAbsorption;
};

