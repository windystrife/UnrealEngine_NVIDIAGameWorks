// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AndroidDeviceProfileMatchingRules.generated.h"

UENUM()
enum ESourceType
{
	SRC_PreviousRegexMatch,
	SRC_GpuFamily,
	SRC_GlVersion,
	SRC_AndroidVersion,
	SRC_DeviceMake,
	SRC_DeviceModel,
	SRC_VulkanVersion,
	SRC_UsingHoudini,
	SRC_MAX,
};

UENUM()
enum ECompareType
{
	CMP_Equal,
	CMP_Less,
	CMP_LessEqual,
	CMP_Greater,
	CMP_GreaterEqual,
	CMP_NotEqual,
	CMP_Regex,
	CMP_MAX,
};

USTRUCT()
struct FProfileMatchItem
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TEnumAsByte<ESourceType> SourceType;

	UPROPERTY()
	TEnumAsByte<ECompareType> CompareType;

	UPROPERTY()
	FString MatchString;
};

USTRUCT()
struct FProfileMatch
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString Profile;
	
	UPROPERTY()
	TArray<FProfileMatchItem> Match;
};

UCLASS(config = DeviceProfiles)
class UAndroidDeviceProfileMatchingRules : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Array of rules to match */
	UPROPERTY(EditAnywhere, config, Category = "Matching Rules")
	TArray<FProfileMatch> MatchProfile;
};
