// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameCircleRuntimeSettings.h"

//////////////////////////////////////////////////////////////////////////
// UGameCircleRuntimeSettings
UGameCircleRuntimeSettings::UGameCircleRuntimeSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bEnableAmazonGameCircleSupport(false)
	, bSupportsInAppPurchasing(false)
	, bEnableFireTVSupport(false)
	, DebugAPIKeyFile(TEXT("Build/GameCircle/Debug_api_key.txt"))
	, DistributionAPIKeyFile(TEXT("Build/GameCircle/Distro_api_key.txt"))
{

}

#if WITH_EDITOR
void UGameCircleRuntimeSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UGameCircleRuntimeSettings::PostInitProperties()
{
	Super::PostInitProperties();
}
#endif
