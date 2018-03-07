// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MotionTrackedDeviceFunctionLibrary.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Features/IModularFeatures.h"
#include "IMotionController.h"
#include "IMotionTrackingSystemManagement.h"
#include "MotionControllerComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogMotionTracking, Log, All);

UMotionTrackedDeviceFunctionLibrary::UMotionTrackedDeviceFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


bool UMotionTrackedDeviceFunctionLibrary::IsMotionTrackedDeviceCountManagementNecessary()
{
	return IModularFeatures::Get().IsModularFeatureAvailable(IMotionTrackingSystemManagement::GetModularFeatureName());
}

void UMotionTrackedDeviceFunctionLibrary::SetIsControllerMotionTrackingEnabledByDefault(bool Enable)
{
	if (IModularFeatures::Get().IsModularFeatureAvailable(IMotionTrackingSystemManagement::GetModularFeatureName()))
	{
		IMotionTrackingSystemManagement& MotionTrackingSystemManagement = IModularFeatures::Get().GetModularFeature<IMotionTrackingSystemManagement>(IMotionTrackingSystemManagement::GetModularFeatureName());
		return MotionTrackingSystemManagement.SetIsControllerMotionTrackingEnabledByDefault(Enable);
	}
}

int32 UMotionTrackedDeviceFunctionLibrary::GetMaximumMotionTrackedControllerCount()
{
	if (IModularFeatures::Get().IsModularFeatureAvailable(IMotionTrackingSystemManagement::GetModularFeatureName()))
	{
		IMotionTrackingSystemManagement& MotionTrackingSystemManagement = IModularFeatures::Get().GetModularFeature<IMotionTrackingSystemManagement>(IMotionTrackingSystemManagement::GetModularFeatureName());
		return MotionTrackingSystemManagement.GetMaximumMotionTrackedControllerCount();
	} 
	else
	{
		return -1;
	}
}


int32 UMotionTrackedDeviceFunctionLibrary::GetMotionTrackingEnabledControllerCount()
{
	if (IModularFeatures::Get().IsModularFeatureAvailable(IMotionTrackingSystemManagement::GetModularFeatureName()))
	{
		IMotionTrackingSystemManagement& MotionTrackingSystemManagement = IModularFeatures::Get().GetModularFeature<IMotionTrackingSystemManagement>(IMotionTrackingSystemManagement::GetModularFeatureName());
		return MotionTrackingSystemManagement.GetMotionTrackingEnabledControllerCount();
	}
	else
	{
		return -1;
	}
}

bool UMotionTrackedDeviceFunctionLibrary::EnableMotionTrackingOfDevice(int32 PlayerIndex, EControllerHand Hand)
{
	if (IModularFeatures::Get().IsModularFeatureAvailable(IMotionTrackingSystemManagement::GetModularFeatureName()))
	{
		IMotionTrackingSystemManagement& MotionTrackingSystemManagement = IModularFeatures::Get().GetModularFeature<IMotionTrackingSystemManagement>(IMotionTrackingSystemManagement::GetModularFeatureName());
		return MotionTrackingSystemManagement.EnableMotionTrackingOfDevice(PlayerIndex, Hand);
	}
	else
	{
		// Return true if TrackingManagement is not available.  
		// It isn't available because we don't need to manage it like this, because everything is always tracked.
		return true;
	}
}
bool UMotionTrackedDeviceFunctionLibrary::EnableMotionTrackingForComponent(UMotionControllerComponent* MotionControllerComponent)
{
	if (MotionControllerComponent == nullptr)
	{
		return false;
	}

	int32 PlayerIndex = MotionControllerComponent->PlayerIndex;
	EControllerHand Hand = MotionControllerComponent->Hand;
	return EnableMotionTrackingOfDevice(PlayerIndex, Hand);
}

void UMotionTrackedDeviceFunctionLibrary::DisableMotionTrackingOfDevice(int32 PlayerIndex, EControllerHand Hand)
{
	if (IModularFeatures::Get().IsModularFeatureAvailable(IMotionTrackingSystemManagement::GetModularFeatureName()))
	{
		IMotionTrackingSystemManagement& MotionTrackingSystemManagement = IModularFeatures::Get().GetModularFeature<IMotionTrackingSystemManagement>(IMotionTrackingSystemManagement::GetModularFeatureName());
		MotionTrackingSystemManagement.DisableMotionTrackingOfDevice(PlayerIndex, Hand);
	}
}
void UMotionTrackedDeviceFunctionLibrary::DisableMotionTrackingForComponent(const UMotionControllerComponent* MotionControllerComponent)
{
	if (MotionControllerComponent == nullptr)
	{
		return;
	}

	int32 PlayerIndex = MotionControllerComponent->PlayerIndex;
	EControllerHand Hand = MotionControllerComponent->Hand;
	DisableMotionTrackingOfDevice(PlayerIndex, Hand);
}

bool UMotionTrackedDeviceFunctionLibrary::IsMotionTrackingEnabledForDevice(int32 PlayerIndex, EControllerHand Hand)
{
	if (IModularFeatures::Get().IsModularFeatureAvailable(IMotionTrackingSystemManagement::GetModularFeatureName()))
	{
		IMotionTrackingSystemManagement& MotionTrackingSystemManagement = IModularFeatures::Get().GetModularFeature<IMotionTrackingSystemManagement>(IMotionTrackingSystemManagement::GetModularFeatureName());
		return MotionTrackingSystemManagement.IsMotionTrackingEnabledForDevice(PlayerIndex, Hand);
	}
	else
	{
		// Return true if TrackingManagement is not available.  
		// It isn't available because we don't need to manage it like this, because everything is always tracked.
		return true;
	}
}
bool UMotionTrackedDeviceFunctionLibrary::IsMotionTrackingEnabledForComponent(const UMotionControllerComponent* MotionControllerComponent)
{
	if (MotionControllerComponent == nullptr)
	{
		return false;
	}

	int32 PlayerIndex = MotionControllerComponent->PlayerIndex;
	EControllerHand Hand = MotionControllerComponent->Hand;
	return IsMotionTrackingEnabledForDevice(PlayerIndex, Hand);
}

void UMotionTrackedDeviceFunctionLibrary::DisableMotionTrackingOfAllControllers()
{
	if (IModularFeatures::Get().IsModularFeatureAvailable(IMotionTrackingSystemManagement::GetModularFeatureName()))
	{
		IMotionTrackingSystemManagement& MotionTrackingSystemManagement = IModularFeatures::Get().GetModularFeature<IMotionTrackingSystemManagement>(IMotionTrackingSystemManagement::GetModularFeatureName());
		return MotionTrackingSystemManagement.DisableMotionTrackingOfAllControllers();
	}
}

void UMotionTrackedDeviceFunctionLibrary::DisableMotionTrackingOfControllersForPlayer(int32 PlayerIndex)
{
	if (IModularFeatures::Get().IsModularFeatureAvailable(IMotionTrackingSystemManagement::GetModularFeatureName()))
	{
		IMotionTrackingSystemManagement& MotionTrackingSystemManagement = IModularFeatures::Get().GetModularFeature<IMotionTrackingSystemManagement>(IMotionTrackingSystemManagement::GetModularFeatureName());
		return MotionTrackingSystemManagement.DisableMotionTrackingOfControllersForPlayer(PlayerIndex);
	}
}

