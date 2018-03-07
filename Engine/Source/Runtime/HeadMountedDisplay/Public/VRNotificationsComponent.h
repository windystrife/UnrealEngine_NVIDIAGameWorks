// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
// VRNotificationsComponent.h: Component to handle receiving notifications from VR HMD

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/ActorComponent.h"
#include "VRNotificationsComponent.generated.h"

UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent), ClassGroup = HeadMountedDisplay)
class HEADMOUNTEDDISPLAY_API UVRNotificationsComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVRNotificationsDelegate);

	// This will be called on Morpheus if the HMD starts up and is not fully initialized (in NOT_STARTED or CALIBRATING states).  
	// The HMD will stay in NOT_STARTED until it is successfully position tracked.  Until it exits NOT_STARTED orientation
	// based reprojection does not happen.  Therefore we do not update rotation at all to avoid user discomfort.
	// Instructions to get the hmd tracked should be shown to the user.
	// Sony may fix this eventually. (PS4 Only) 
	UPROPERTY(BlueprintAssignable)
	FVRNotificationsDelegate HMDTrackingInitializingAndNeedsHMDToBeTrackedDelegate;

	// This will be called on Morpheus when the HMD is done initializing and therefore
	// reprojection will start functioning.
	// The app can continue now. (PS4 Only) 
	UPROPERTY(BlueprintAssignable)
	FVRNotificationsDelegate HMDTrackingInitializedDelegate;

	// This will be called when the application is asked for VR headset recenter.  
	UPROPERTY(BlueprintAssignable)
	FVRNotificationsDelegate HMDRecenteredDelegate;

	// This will be called when connection to HMD is lost.  
	UPROPERTY(BlueprintAssignable)
	FVRNotificationsDelegate HMDLostDelegate;

	// This will be called when connection to HMD is restored.  
	UPROPERTY(BlueprintAssignable)
	FVRNotificationsDelegate HMDReconnectedDelegate;

	// This will be called when the user declines to connect the HMD when prompted to do so by a system dialog. (PS4 Only)  
	UPROPERTY(BlueprintAssignable)
	FVRNotificationsDelegate HMDConnectCanceledDelegate;

	// This will be called when the HMD detects that it has been put on by a player.  
	UPROPERTY(BlueprintAssignable)
	FVRNotificationsDelegate HMDPutOnHeadDelegate;

	// This will be called when the HMD detects that it has been taken off by a player (disconnecting the hmd also causes it to register as taken off).  
	UPROPERTY(BlueprintAssignable)
	FVRNotificationsDelegate HMDRemovedFromHeadDelegate;

	// This will be called when the VR system recenters a controller.  
	UPROPERTY(BlueprintAssignable)
	FVRNotificationsDelegate VRControllerRecenteredDelegate;

public:
	void OnRegister() override;
	void OnUnregister() override;

private:
	/** Native handlers that get registered with the actual FCoreDelegates, and then proceed to broadcast to the delegates above */
	void HMDTrackingInitializingAndNeedsHMDToBeTrackedDelegate_Handler()	{ HMDTrackingInitializingAndNeedsHMDToBeTrackedDelegate.Broadcast(); }
	void HMDTrackingInitializedDelegate_Handler()	{ HMDTrackingInitializedDelegate.Broadcast(); }
	void HMDRecenteredDelegate_Handler()	{ HMDRecenteredDelegate.Broadcast(); }
	void HMDLostDelegate_Handler()			{ HMDLostDelegate.Broadcast(); }
	void HMDReconnectedDelegate_Handler()	{ HMDReconnectedDelegate.Broadcast(); }
	void HMDConnectCanceledDelegate_Handler() { HMDConnectCanceledDelegate.Broadcast(); }
	void HMDPutOnHeadDelegate_Handler() { HMDPutOnHeadDelegate.Broadcast(); }
	void HMDRemovedFromHeadDelegate_Handler() { HMDRemovedFromHeadDelegate.Broadcast(); }
	void VRControllerRecentered_Handler() { VRControllerRecenteredDelegate.Broadcast(); }
};



