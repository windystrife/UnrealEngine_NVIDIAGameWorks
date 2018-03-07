// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HardwareTargetingSettings.h"


UHardwareTargetingSettings::UHardwareTargetingSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TargetedHardwareClass(EHardwareClass::Unspecified)
	, AppliedTargetedHardwareClass(EHardwareClass::Unspecified)
	, DefaultGraphicsPerformance(EGraphicsPreset::Unspecified)
	, AppliedDefaultGraphicsPerformance(EGraphicsPreset::Unspecified)
{ }


bool UHardwareTargetingSettings::HasPendingChanges() const
{
	if (TargetedHardwareClass == EHardwareClass::Unspecified || DefaultGraphicsPerformance == EGraphicsPreset::Unspecified)
	{
		return false;
	}

	return AppliedTargetedHardwareClass != TargetedHardwareClass || AppliedDefaultGraphicsPerformance != DefaultGraphicsPerformance;
}


void UHardwareTargetingSettings::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent )
{
	SettingChangedEvent.Broadcast();
}
