// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class ANDROIDDEVICEPROFILESELECTOR_API FAndroidDeviceProfileSelector
{
public:
	static FString FindMatchingProfile(FString GPUFamily, FString GLVersion, FString AndroidVersion, FString DeviceMake, FString DeviceModel, FString VulkanVersion, FString UsingHoudini, FString ProfileName);
	static int32 GetNumProfiles();
};
