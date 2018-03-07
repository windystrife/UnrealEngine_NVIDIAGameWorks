// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
//
#include "Classes/SteamVRChaperoneComponent.h"
#include "SteamVRPrivate.h"
#include "SteamVRHMD.h"
#include "Engine/Engine.h"

USteamVRChaperoneComponent::USteamVRChaperoneComponent(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;

	bTickInEditor = true;
	bAutoActivate = true;

	bWasInsideBounds = true;
}

void USteamVRChaperoneComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if STEAMVR_SUPPORTED_PLATFORMS
	IXRTrackingSystem* ActiveXRSystem = GEngine->XRSystem.Get();
	// @TODO: hardcoded to match FSteamVRHMD::GetSystemName(), which we should turn into 
	//        a static method so we don't have to litter code with hardcoded values like this
	static FName SteamVRSystemName(TEXT("SteamVR")); 

	if (ActiveXRSystem && ActiveXRSystem->GetSystemName() == SteamVRSystemName)
	{
		FSteamVRHMD* SteamVRHMD = (FSteamVRHMD*)(ActiveXRSystem);
		if (SteamVRHMD->IsStereoEnabled())
		{
			bool bInBounds = SteamVRHMD->IsInsideBounds();

			if (bInBounds != bWasInsideBounds)
			{
				if (bInBounds)
				{
					OnReturnToBounds.Broadcast();
				}
				else
				{
					OnLeaveBounds.Broadcast();
				}
			}

			bWasInsideBounds = bInBounds;
		}
	}
#endif // STEAMVR_SUPPORTED_PLATFORMS
}

TArray<FVector> USteamVRChaperoneComponent::GetBounds() const
{
	TArray<FVector> RetValue;

#if STEAMVR_SUPPORTED_PLATFORMS
	if (GEngine->XRSystem.IsValid())
	{
		static FName SteamHMDName(TEXT("SteamVR"));

		IXRTrackingSystem* XRSystem = GEngine->XRSystem.Get();
		if (XRSystem->GetSystemName() == SteamHMDName)
		{
			FSteamVRHMD* SteamVRHMD = (FSteamVRHMD*)(XRSystem);
			if (SteamVRHMD->IsStereoEnabled())
			{
				RetValue = SteamVRHMD->GetBounds();
			}
		}
	}	
#endif // STEAMVR_SUPPORTED_PLATFORMS

	return RetValue;
}