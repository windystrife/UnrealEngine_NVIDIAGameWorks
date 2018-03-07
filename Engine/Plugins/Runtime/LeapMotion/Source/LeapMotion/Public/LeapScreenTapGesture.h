// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LeapGesture.h"
#include "LeapScreenTapGesture.generated.h"

/**
* The ScreenTapGesture class represents a tapping gesture by a finger or tool.
* A screen tap gesture is recognized when the tip of a finger pokes forward and
* then springs back to approximately the original position, as if tapping a vertical
* screen. The tapping finger must pause briefly before beginning the tap.
*
* Leap API reference: https://developer.leapmotion.com/documentation/cpp/api/Leap.ScreenTapGesture.html
*/
UCLASS(BlueprintType)
class LEAPMOTION_API ULeapScreenTapGesture : public ULeapGesture
{
	GENERATED_UCLASS_BODY()
public:
	~ULeapScreenTapGesture();

	/**
	* The direction of finger tip motion in basic enum form, useful for switching 
	* through common directions checks (Up/Down, Left/Right, In/Out)
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Screen Tap  Gesture")
	TEnumAsByte<LeapBasicDirection> BasicDirection;

	/**
	* The direction of finger tip motion.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Screen Tap Gesture")
	FVector Direction;

	/**
	* The finger performing the screen tap gesture.
	*
	* @return	A Pointable object representing the tapping finger.
	*/
	UFUNCTION(BlueprintCallable, Category = "Leap Screen Tap Gesture")
	class ULeapPointable* Pointable();

	/**
	* The position where the screen tap is registered.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Screen Tap Gesture")
	FVector Position;

	/**
	* The progress value is always 1.0 for a screen tap gesture.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Screen Tap Gesture")
	float Progress;

	void SetGesture(const class Leap::ScreenTapGesture &Gesture);

private:
	class FPrivateScreenTapGesture* Private;

	UPROPERTY()
	ULeapPointable* PPointable = nullptr;
};