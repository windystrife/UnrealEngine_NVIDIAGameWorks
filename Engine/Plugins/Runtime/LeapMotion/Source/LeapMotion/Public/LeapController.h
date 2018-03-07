// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LeapGesture.h"
#include "LeapController.generated.h"

/**
* Leap Controller class wrapped into an Actor Component.
*
* The Controller class is your main interface to the Leap Motion Controller.
* Create an instance of this Controller class to access frames of tracking data
* and configuration information. Frame data can be polled at any time using the
* Frame() function. Call Frame() or Frame(0) to get the most recent 
* frame. Set the history parameter to a positive integer to access previous frames. 
* A controller stores up to 60 frames in its frame history.
*
* Leap API reference: https://developer.leapmotion.com/documentation/cpp/api/Leap.Controller.html
*/
UCLASS(ClassGroup=Input, meta=(BlueprintSpawnableComponent))
class LEAPMOTION_API ULeapController : public UActorComponent
{
	GENERATED_UCLASS_BODY()

public:
	~ULeapController();
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	/**
	* Reports whether this Controller is connected to the Leap Motion service and the Leap Motion hardware is plugged in.
	*/
	UFUNCTION(BlueprintPure, meta = (Keywords = "is connected"), Category = "Leap Controller")
	bool IsConnected() const;

	/**
	* Returns a frame of tracking data from the Leap Motion software.
	* Call frame() or frame(0) to access the most recent frame; call frame(1)
	* to access the previous frame, and so on. If you use a history value greater 
	* than the number of stored frames, then the controller returns an invalid frame.
	*
	* @param optional history parameter to specify which frame to retrieve. 
	* @return The specified frame; or, if no history parameter is specified, the newest frame. If a frame is not available at the specified history position, an invalid Frame is returned.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get Frame", Keywords = "get frame"), Category = "Leap Controller")
	class ULeapFrame* Frame(int32 History);

	/**
	* Reports whether this application is the focused, foreground application.
	*
	* @return True, if application has focus; false otherwise.
	*/
	UFUNCTION(BlueprintCallable, meta = (Keywords = "has Focus"), Category = "Leap Controller")
	bool HasFocus() const;

	/**
	* Reports whether this Controller is connected to the Leap Motion service and the Leap Motion hardware is plugged in.
	*
	* @return True, if connected; false otherwise.
	*/
	UFUNCTION(BlueprintPure, meta = (Keywords = "is service connected"), Category = "Leap Controller")
	bool IsServiceConnected() const;

	/**
	* Set Flags and tracking for the plugin to use tracking expecting leap mounted on HMD.
	* Optionally auto-rotate and auto-shift values by the movement of the hmd (useful pre-4.11)
	*/
	UFUNCTION(BlueprintCallable, meta = (Keywords = "optimize hmd facing top set policy"), Category = "Leap Controller")
	void OptimizeForHMD(bool UseTopdown = false, bool AutoRotate = true, bool AutoShift = true);

	/**
	* Enable image streaming by the leap motion. Optionally emit raw image events and adjust images by standard gamma correction.
	*
	* @param AllowImages enable image support at minimum for polling
	* @param EmitImageEvents whether to emit raw image event whenever they're ready
	* @param UseGammaCorrection true if you wish to use image gamma correction
	*/
	UFUNCTION(BlueprintCallable, meta = (Keywords = "use allow images set policy"), Category = "Leap Controller")
	void EnableImageSupport(bool AllowImages = true, bool EmitImageEvents = true, bool UseGammaCorrection = false);

	/**
	*  Requests that your application receives frames when it is not the foreground application for user input.
	*
	* @param TrackInBackground toggle to enable or disable
	*/
	UFUNCTION(BlueprintCallable, meta = (Keywords = "enable background tracking"), Category = "Leap Controller")
	void EnableBackgroundTracking(bool TrackInBackground = false);

	/**
	* Enables or disables reporting of a specified gesture type.
	*
	* @param GestureType category of gesture you wish to enable or disable
	* @param Enable whether the gesture detection should be enabled
	*/
	UFUNCTION(BlueprintCallable, meta = (Keywords = "enable gesture"), Category = "Leap Controller")
	void EnableGesture(enum LeapGestureType GestureType, bool Enable = true);

	/**
	* Specify a custom leap to eye offset. Given in UE coordinate system (XForward). 
	*
	* @param Offset offset vector, defaults to DK2 value (8cm forward)
	*/
	UFUNCTION(BlueprintCallable, Category = "Leap Controller")
	void SetLeapMountToHMDOffset(FVector Offset = FVector(8,0,0));	//default to the leap dk2 offset

	//Leap Event Interface forwarding, automatically set since 0.6.2, available for event redirection
	UFUNCTION(BlueprintCallable, meta = (Keywords = "set delegate self"), Category = "Leap Interface")
	void SetInterfaceDelegate(UObject* NewDelegate);

private:
	class FLeapControllerPrivate* Private;

	//Private UProperties
	UPROPERTY()
	ULeapFrame* PFrame = nullptr;
	UPROPERTY()
	class ULeapHand* PEventHand = nullptr;
	UPROPERTY()
	class ULeapFinger* PEventFinger = nullptr;
	UPROPERTY()
	class ULeapGesture* PEventGesture = nullptr;
	UPROPERTY()
	class ULeapCircleGesture* PEventCircleGesture = nullptr;
	UPROPERTY()
	class ULeapKeyTapGesture* PEventKeyTapGesture = nullptr;
	UPROPERTY()
	class ULeapScreenTapGesture* PEventScreenTapGesture = nullptr;
	UPROPERTY()
	class ULeapSwipeGesture* PEventSwipeGesture = nullptr;
	UPROPERTY()
	class ULeapImage* PEventImage1 = nullptr;
	UPROPERTY()
	class ULeapImage* PEventImage2 = nullptr;

	void InterfaceEventTick(float DeltaTime);
};