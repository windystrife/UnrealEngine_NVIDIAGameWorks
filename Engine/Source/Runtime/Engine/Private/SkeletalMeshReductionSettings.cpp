// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/SkeletalMeshReductionSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogSkeletalMeshReductionSettings, Warning, All)


USkeletalMeshReductionSettings::USkeletalMeshReductionSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bValidSettings(false)
{
}

USkeletalMeshReductionSettings* USkeletalMeshReductionSettings::Get()
{
	static bool bInitialized = false;
	// This is a singleton, use default object
	USkeletalMeshReductionSettings* DefaultSettings = GetMutableDefault<USkeletalMeshReductionSettings>();
	
	if (!bInitialized)
	{
		DefaultSettings->Initialize();
		bInitialized = true;
	}

	return DefaultSettings;
}

void USkeletalMeshReductionSettings::Initialize()
{
	// Check if valid settings were parsed from the .ini file
	bValidSettings = ( Settings.Num() > 0 );
}

const FSkeletalMeshLODGroupSettings& USkeletalMeshReductionSettings::GetDefaultSettingsForLODLevel(const int32 LODIndex) const
{
	if (LODIndex < Settings.Num())
	{
		return Settings[LODIndex];
	}

	// This should not happen as of right now, since the function is only called with 'Default' as name
	checkf(false, TEXT("Invalid Skeletal mesh default settings LOD Level"));

	// Default return value to prevent compile issue
	static FSkeletalMeshLODGroupSettings DefaultReturnValue;
	return DefaultReturnValue;
}


int32 USkeletalMeshReductionSettings::GetNumberOfSettings() const
{
	return Settings.Num();
}

const bool USkeletalMeshReductionSettings::HasValidSettings() const
{
	return bValidSettings;
}

FSkeletalMeshOptimizationSettings FSkeletalMeshLODGroupSettings::GetSettings() const
{
	return OptimizationSettings;
}

const float FSkeletalMeshLODGroupSettings::GetScreenSize() const
{
	return ScreenSize;
}
