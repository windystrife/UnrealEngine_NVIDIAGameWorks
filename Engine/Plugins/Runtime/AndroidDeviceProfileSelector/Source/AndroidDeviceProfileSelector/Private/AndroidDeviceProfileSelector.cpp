// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AndroidDeviceProfileSelector.h"
#include "AndroidDeviceProfileMatchingRules.h"
#include "AndroidJavaSurfaceViewDevices.h"
#include "Templates/Casts.h"
#include "Regex.h"

UAndroidDeviceProfileMatchingRules::UAndroidDeviceProfileMatchingRules(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAndroidJavaSurfaceViewDevices::UAndroidJavaSurfaceViewDevices(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

static UAndroidDeviceProfileMatchingRules* GetAndroidDeviceProfileMatchingRules()
{
	// We need to initialize the class early as device profiles need to be evaluated before ProcessNewlyLoadedUObjects can be called.
	extern UClass* Z_Construct_UClass_UAndroidDeviceProfileMatchingRules();
	CreatePackage(nullptr, UAndroidDeviceProfileMatchingRules::StaticPackage());
	Z_Construct_UClass_UAndroidDeviceProfileMatchingRules();

	// Get the default object which will has the values from DeviceProfiles.ini
	UAndroidDeviceProfileMatchingRules* Rules = Cast<UAndroidDeviceProfileMatchingRules>(UAndroidDeviceProfileMatchingRules::StaticClass()->GetDefaultObject());
	check(Rules);
	return Rules;
}

FString FAndroidDeviceProfileSelector::FindMatchingProfile(FString GPUFamily, FString GLVersion, FString AndroidVersion, FString DeviceMake, FString DeviceModel, FString VulkanVersion, FString UsingHoudini, FString ProfileName)
{
	for (const FProfileMatch& Profile : GetAndroidDeviceProfileMatchingRules()->MatchProfile)
	{
		FString PreviousRegexMatch;
		bool bFoundMatch = true;
		for (const FProfileMatchItem& Item : Profile.Match)
		{
			FString* SourceString = nullptr;
			switch (Item.SourceType)
			{
			case SRC_PreviousRegexMatch:
				SourceString = &PreviousRegexMatch;
				break;
			case SRC_GpuFamily:
				SourceString = &GPUFamily;
				break;
			case SRC_GlVersion:
				SourceString = &GLVersion;
				break;
			case SRC_AndroidVersion:
				SourceString = &AndroidVersion;
				break;
			case SRC_DeviceMake:
				SourceString = &DeviceMake;
				break;
			case SRC_DeviceModel:
				SourceString = &DeviceModel;
				break;
			case SRC_VulkanVersion:
				SourceString = &VulkanVersion;
				break;
			case SRC_UsingHoudini:
				SourceString = &UsingHoudini;
				break;
			default:
				continue;
			}

			switch (Item.CompareType)
			{
			case CMP_Equal:
				if (*SourceString != Item.MatchString)
				{
					bFoundMatch = false;
				}
				break;
			case CMP_Less:
				if (FPlatformString::Atoi(**SourceString) >= FPlatformString::Atoi(*Item.MatchString))
				{
					bFoundMatch = false;
				}
				break;
			case CMP_LessEqual:
				if (FPlatformString::Atoi(**SourceString) > FPlatformString::Atoi(*Item.MatchString))
				{
					bFoundMatch = false;
				}
				break;
			case CMP_Greater:
				if (FPlatformString::Atoi(**SourceString) <= FPlatformString::Atoi(*Item.MatchString))
				{
					bFoundMatch = false;
				}
				break;
			case CMP_GreaterEqual:
				if (FPlatformString::Atoi(**SourceString) < FPlatformString::Atoi(*Item.MatchString))
				{
					bFoundMatch = false;
				}
				break;
			case CMP_NotEqual:
				if (*SourceString == Item.MatchString)
				{
					bFoundMatch = false;
				}
				break;
			case CMP_Regex:
			{
				const FRegexPattern RegexPattern(Item.MatchString);
				FRegexMatcher RegexMatcher(RegexPattern, *SourceString);
				if (RegexMatcher.FindNext())
				{
					PreviousRegexMatch = RegexMatcher.GetCaptureGroup(1);
				}
				else
				{
					bFoundMatch = false;
				}
			}
			break;
			default:
				bFoundMatch = false;
			}

			if (!bFoundMatch)
			{
				break;
			}
		}

		if (bFoundMatch)
		{
			ProfileName = Profile.Profile;
			break;
		}
	}
	return ProfileName;
}

int32 FAndroidDeviceProfileSelector::GetNumProfiles()
{
	return GetAndroidDeviceProfileMatchingRules()->MatchProfile.Num();
}