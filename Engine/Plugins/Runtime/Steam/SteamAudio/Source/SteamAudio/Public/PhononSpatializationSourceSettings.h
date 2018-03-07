//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "IAudioExtensionPlugin.h"
#include "PhononCommon.h"
#include "PhononSpatializationSourceSettings.generated.h"

UCLASS()
class STEAMAUDIO_API UPhononSpatializationSourceSettings : public USpatializationPluginSourceSettingsBase
{
	GENERATED_BODY()

public:
	UPhononSpatializationSourceSettings();
		
#if WITH_EDITOR
	virtual bool CanEditChange(const UProperty* InProperty) const override;
#endif

	UPROPERTY(GlobalConfig, EditAnywhere, Category = SpatializationSettings)
	EIplSpatializationMethod SpatializationMethod;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = SpatializationSettings, meta = (DisplayName = "HRTF Interpolation Method"))
	EIplHrtfInterpolationMethod HrtfInterpolationMethod;
};

