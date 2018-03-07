// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
// VRNotificationsComponent.cpp: Component to handle receiving notifications from VR HMD

#include "VRNotificationsComponent.h"
#include "Misc/CoreDelegates.h"

UVRNotificationsComponent::UVRNotificationsComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UVRNotificationsComponent::OnRegister()
{
	Super::OnRegister();

	FCoreDelegates::VRHeadsetTrackingInitializingAndNeedsHMDToBeTrackedDelegate.AddUObject(this, &UVRNotificationsComponent::HMDTrackingInitializingAndNeedsHMDToBeTrackedDelegate_Handler);
	FCoreDelegates::VRHeadsetTrackingInitializedDelegate.AddUObject(this, &UVRNotificationsComponent::HMDTrackingInitializedDelegate_Handler);
	FCoreDelegates::VRHeadsetRecenter.AddUObject(this, &UVRNotificationsComponent::HMDRecenteredDelegate_Handler);
	FCoreDelegates::VRHeadsetLost.AddUObject(this, &UVRNotificationsComponent::HMDLostDelegate_Handler);
	FCoreDelegates::VRHeadsetReconnected.AddUObject(this, &UVRNotificationsComponent::HMDReconnectedDelegate_Handler);
	FCoreDelegates::VRHeadsetConnectCanceled.AddUObject(this, &UVRNotificationsComponent::HMDConnectCanceledDelegate_Handler);
	FCoreDelegates::VRHeadsetPutOnHead.AddUObject(this, &UVRNotificationsComponent::HMDPutOnHeadDelegate_Handler);
	FCoreDelegates::VRHeadsetRemovedFromHead.AddUObject(this, &UVRNotificationsComponent::HMDRemovedFromHeadDelegate_Handler);
	FCoreDelegates::VRControllerRecentered.AddUObject(this, &UVRNotificationsComponent::VRControllerRecentered_Handler);
}

void UVRNotificationsComponent::OnUnregister()
{
	Super::OnUnregister();
	
	FCoreDelegates::VRHeadsetTrackingInitializingAndNeedsHMDToBeTrackedDelegate.RemoveAll(this);
	FCoreDelegates::VRHeadsetTrackingInitializedDelegate.RemoveAll(this);
	FCoreDelegates::VRHeadsetRecenter.RemoveAll(this);
	FCoreDelegates::VRHeadsetLost.RemoveAll(this);
	FCoreDelegates::VRHeadsetReconnected.RemoveAll(this);
	FCoreDelegates::VRHeadsetConnectCanceled.RemoveAll(this);
	FCoreDelegates::VRHeadsetPutOnHead.RemoveAll(this);
	FCoreDelegates::VRHeadsetRemovedFromHead.RemoveAll(this);
	FCoreDelegates::VRControllerRecentered.RemoveAll(this);
}
