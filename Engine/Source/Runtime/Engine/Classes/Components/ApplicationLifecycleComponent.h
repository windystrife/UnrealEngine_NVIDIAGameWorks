// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
// ApplicationLifecycleComponent.:  See FCoreDelegates for details

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/ActorComponent.h"
#include "ApplicationLifecycleComponent.generated.h"

/** Component to handle receiving notifications from the OS about application state (activated, suspended, termination, etc). */
UCLASS(ClassGroup=Utility, HideCategories=(Activation, "Components|Activation", Collision), meta=(BlueprintSpawnableComponent))
class ENGINE_API UApplicationLifecycleComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FApplicationLifetimeDelegate);

	// This is called when the application is about to be deactivated (e.g., due to a phone call or SMS or the sleep button). 
	// The game should be paused if possible, etc... 
	UPROPERTY(BlueprintAssignable)
	FApplicationLifetimeDelegate ApplicationWillDeactivateDelegate;  
	
	// Called when the application has been reactivated (reverse any processing done in the Deactivate delegate)
	UPROPERTY(BlueprintAssignable)
	FApplicationLifetimeDelegate ApplicationHasReactivatedDelegate; 
	
	// This is called when the application is being backgrounded (e.g., due to switching  
	// to another app or closing it via the home button)  
	// The game should release shared resources, save state, etc..., since it can be  
	// terminated from the background state without any further warning.  
	UPROPERTY(BlueprintAssignable)	
	FApplicationLifetimeDelegate ApplicationWillEnterBackgroundDelegate; // for instance, hitting the home button
	
	// Called when the application is returning to the foreground (reverse any processing done in the EnterBackground delegate)
	UPROPERTY(BlueprintAssignable)
	FApplicationLifetimeDelegate ApplicationHasEnteredForegroundDelegate; 
	
	// This *may* be called when the application is getting terminated by the OS.  
	// There is no guarantee that this will ever be called on a mobile device,  
	// save state when ApplicationWillEnterBackgroundDelegate is called instead.  
	UPROPERTY(BlueprintAssignable)
	FApplicationLifetimeDelegate ApplicationWillTerminateDelegate;

public:
	void OnRegister() override;
	void OnUnregister() override;

private:
	/** Native handlers that get registered with the actual FCoreDelegates, and then proceed to broadcast to the delegates above */
	void ApplicationWillDeactivateDelegate_Handler() { ApplicationWillDeactivateDelegate.Broadcast(); }
	void ApplicationHasReactivatedDelegate_Handler() { ApplicationHasReactivatedDelegate.Broadcast(); }
	void ApplicationWillEnterBackgroundDelegate_Handler() { ApplicationWillEnterBackgroundDelegate.Broadcast(); }
	void ApplicationHasEnteredForegroundDelegate_Handler() { ApplicationHasEnteredForegroundDelegate.Broadcast(); }
	void ApplicationWillTerminateDelegate_Handler() { ApplicationWillTerminateDelegate.Broadcast(); }
};



