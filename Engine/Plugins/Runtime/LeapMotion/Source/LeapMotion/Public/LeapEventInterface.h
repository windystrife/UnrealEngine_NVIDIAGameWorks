// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LeapHand.h"
#include "UObject/Interface.h"
#include "LeapEventInterface.generated.h"

UINTERFACE(MinimalAPI)
class ULeapEventInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

/**
* Event driven interface you can attach to any blueprint to receive leap 
* events as they are processed. Events are called after attaching 
* LeapControllerComponent to your blueprint and it will automatically call
* the events if the interface is also set.
* 
* Leap API reference: https://github.com/getnamo/leap-ue4/
*/
class LEAPMOTION_API ILeapEventInterface
{
	GENERATED_IINTERFACE_BODY()

public:

	//Hands

	/**
	* Event triggered for each hand that is visible and tracked.
	*
	* @param Hand		reference to LeapHand object containing data for the tracked hand
	*/
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "HandMoved", CompactNodeTitle = "", Keywords = "hand moved"), Category = "Leap Interface Event")
	void LeapHandMoved(ULeapHand* hand);

	/**
	* Event triggered for when the left hand is visible and tracked.
	*
	* @param Hand		reference to LeapHand object containing data for the tracked hand
	*/
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "LeftHandMoved", CompactNodeTitle = "", Keywords = "left hand moved"), Category = "Leap Interface Event")
	void LeapLeftHandMoved(ULeapHand* hand);

	/**
	* Event triggered for when the right hand that is visible and tracked.
	*
	* @param Hand		reference to LeapHand object containing data for the tracked hand
	*/
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "RightHandMoved", CompactNodeTitle = "", Keywords = "right hand moved"), Category = "Leap Interface Event")
	void LeapRightHandMoved(ULeapHand* hand);

	/**
	* Event triggered when the given hand has grabbed stronger than the grab trigger (grab strength>=0.5)
	*
	* @param Strength	grabbing strength from 0-1.0
	* @param Hand		reference to LeapHand object containing data for the tracked hand
	*/
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "HandGrabbed", CompactNodeTitle = "", Keywords = "hand grab"), Category = "Leap Interface Event")
	void LeapHandGrabbed(float strength, ULeapHand* hand);

	/**
	* Event triggered continuously when the given hand has a grab strength greater than 0.5
	*
	* @param Strength	grabbing strength from 0-1.0
	* @param Hand		reference to LeapHand object containing data for the tracked hand
	*/
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "HandGrabbing", CompactNodeTitle = "", Keywords = "hand grab"), Category = "Leap Interface Event")
	void LeapHandGrabbing(float strength, ULeapHand* hand);

	/**
	* Event triggered when the given hand's grab strength has fallen below the grab trigger (grab strength<0.5) after having grabbed earlier.
	*
	* @param Strength	grabbing strength from 0-1.0
	* @param Hand		reference to LeapHand object containing data for the tracked hand
	*/
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "HandReleased", CompactNodeTitle = "", Keywords = "hand released ungrab"), Category = "Leap Interface Event")
	void LeapHandReleased(float strength, ULeapHand* hand);

	/**
	* Event triggered when the given hand has pinched stronger than the pinch trigger (pinch strength>=0.5)
	*
	* @param Strength	pinch strength from 0-1.0
	* @param Hand		reference to LeapHand object containing data for the tracked hand
	*/
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "HandPinched", CompactNodeTitle = "", Keywords = "hand pinch"), Category = "Leap Interface Event")
	void LeapHandPinched(float strength, ULeapHand* hand);

	/**
	* Event triggered continuously when the given hand has a pinch strength greater than 0.5
	*
	* @param Strength	pinch strength from 0-1.0
	* @param Hand		reference to LeapHand object containing data for the tracked hand
	*/
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "HandPinching", CompactNodeTitle = "", Keywords = "hand grab"), Category = "Leap Interface Event")
	void LeapHandPinching(float strength, ULeapHand* hand);

	/**
	* Event triggered when the given hand's pinch strength has fallen below the pinch trigger (pinch strength<0.5) after having pinched earlier.
	*
	* @param Strength	pinch strength from 0-1.0
	* @param Hand		reference to LeapHand object containing data for the tracked hand
	*/
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "HandUnpinched", CompactNodeTitle = "", Keywords = "hand unpinch"), Category = "Leap Interface Event")
	void LeapHandUnpinched(float strength, ULeapHand* hand);

	/**
	* Event triggered when number of hands changed from a previous frame.
	*
	* @param Count		Number of hands visible
	*/
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "HandCountChanged", CompactNodeTitle = "", Keywords = "hand count"), Category = "Leap Interface Event")
	void HandCountChanged(int32 count);

	//Fingers

	/**
	* Event triggered for each finger that is visible and tracked.
	*
	* @param Finger		reference to LeapFinger object containing data for the tracked finger
	*/
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "FingerMoved", CompactNodeTitle = "", Keywords = "finger moved"), Category = "Leap Interface Event")
	void LeapFingerMoved(ULeapFinger* finger);

	/**
	* Event triggered for the finger that is frontmost from the perspective of the leap (X Positive) for each visible hand
	*
	* @param Finger		reference to LeapFinger object containing data for the tracked finger
	*/
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "FrontFingerMoved", CompactNodeTitle = "", Keywords = "finger front most moved"), Category = "Leap Interface Event")
	void LeapFrontMostFingerMoved(ULeapFinger* finger);

	/**
	* Event triggered for the finger that is rightmost from the perspective of the leap (Y Positive) for each visible hand
	*
	* @param Finger		reference to LeapFinger object containing data for the tracked finger
	*/
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "LeftFingerMoved", CompactNodeTitle = "", Keywords = "finger left most moved"), Category = "Leap Interface Event")
	void LeapRightMostFingerMoved(ULeapFinger* finger);

	/**
	* Event triggered for the finger that is leftmost from the perspective of the leap (YNegative)
	*
	* @param Finger		reference to LeapFinger object containing data for the tracked finger
	*/
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "RightFingerMoved", CompactNodeTitle = "", Keywords = "finger right most moved"), Category = "Leap Interface Event")
	void LeapLeftMostFingerMoved(ULeapFinger* finger);

	/**
	* Event triggered only for the frontmost finger when it is touching, defined as
	* TouchDistance < 0, a value proportional to the distance between this Pointable object and the adaptive touch plane.
	*
	* @param Finger		reference to LeapFinger object containing data for the tracked finger
	*/
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "LeapFrontFingerTouch", CompactNodeTitle = "", Keywords = "finger touched"), Category = "Leap Interface Event")
	void LeapFrontFingerTouch(ULeapFinger* finger);		//emitted only for frontmost finger, typically best use case

	/**
	* Event triggered when number of fingers changed from a previous frame.
	*
	* @param Count		Number of fingers visible
	*/
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "FingerCountChanged", CompactNodeTitle = "", Keywords = "finger count"), Category = "Leap Interface Event")
	void FingerCountChanged(int32 count);

	//Gestures - Only emits if enabled

	/**
	* Event triggered when any gesture is detected. Gestures need to be enabled in the LeapController to receive their events.
	*
	* @param Gesture		gesture object detected, can be cast to a sub-class for specific methods
	*/
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "GestureDetected", CompactNodeTitle = "", Keywords = "gesture detect"), Category = "Leap Interface Event")
	void GestureDetected(ULeapGesture* gesture);

	/**
	* Event triggered when a circle gesture is detected.  Gestures need to be enabled in the LeapController to receive their events.
	*
	* @param Gesture		circle gesture object detected, can be queried to get details of the event such as radius
	*/
	UFUNCTION(BlueprintImplementableEvent, meta = (Keywords = "gesture circle detect"), Category = "Leap Interface Event")
	void CircleGestureDetected(ULeapCircleGesture* gesture);

	/**
	* Event triggered when a key tap gesture is detected.  Gestures need to be enabled in the LeapController to receive their events.
	*
	* @param Gesture		key tap gesture object detected, can be queried to get details of the event
	*/
	UFUNCTION(BlueprintImplementableEvent, meta = (Keywords = "gesture key tap detect"), Category = "Leap Interface Event")
	void KeyTapGestureDetected(ULeapKeyTapGesture* gesture);

	/**
	* Event triggered when a screen tap gesture is detected.  Gestures need to be enabled in the LeapController to receive their events.
	*
	* @param Gesture		screen tap gesture object detected, can be queried to get details of the event
	*/
	UFUNCTION(BlueprintImplementableEvent, meta = (Keywords = "gesture screen tap detect"), Category = "Leap Interface Event")
	void ScreenTapGestureDetected(ULeapScreenTapGesture* gesture);

	/**
	* Event triggered when a swipe gesture is detected.  Gestures need to be enabled in the LeapController to receive their events.
	*
	* @param Gesture		swipe gesture object detected, can be queried to get details of the event
	*/
	UFUNCTION(BlueprintImplementableEvent, meta = (Keywords = "gesture swipe detect"), Category = "Leap Interface Event")
	void SwipeGestureDetected(ULeapSwipeGesture* gesture);

	//Images - Only emits if enabled

	/**
	* Event triggered when a raw image has been processed.  Raw images need to be enabled in the LeapController to receive their events.
	*
	* @param Texture		UTexture2D representation of the raw leap image
	* @param Image			image object containing details relevant to the image such as distorion
	*/
	UFUNCTION(BlueprintImplementableEvent, Category = "Leap Interface Event")
	void RawImageReceived(UTexture2D* texture, ULeapImage* image);

	//Help identifying
	virtual FString ToString();
};