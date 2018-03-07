// Copyright 2017 Google Inc.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "GoogleVRControllerEventManager.generated.h"

UENUM(BlueprintType)
enum class EGoogleVRControllerState : uint8
{
	Disconnected = 0,
	Scanning = 1,
	Connecting = 2,
	Connected = 3,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGoogleVRControllerRecenterDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGoogleVRControllerStateChangeDelegate, EGoogleVRControllerState, NewControllerState);

/**
 * GoogleVRController Extensions Function Library
 */
UCLASS()
class GOOGLEVRCONTROLLER_API UGoogleVRControllerEventManager : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** DEPRECATED:  Please use VRNotificationsComponent's VRControllerRecentered delegate instead! */
	UPROPERTY(BlueprintAssignable)
	FGoogleVRControllerRecenterDelegate OnControllerRecenteredDelegate_DEPRECATED;

	UPROPERTY(BlueprintAssignable)
	FGoogleVRControllerStateChangeDelegate OnControllerStateChangedDelegate;

public:
	static UGoogleVRControllerEventManager* GetInstance();
};