// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LeapGesture.h"
#include "LeapCircleGesture.generated.h"

/**
* The CircleGesture classes represents a circular finger movement.
* A circle movement is recognized when the tip of a finger draws a circle within
* the Leap Motion Controller field of view.
*
* Leap API reference: https://developer.leapmotion.com/documentation/cpp/api/Leap.CircleGesture.html
*/
UCLASS(BlueprintType)
class LEAPMOTION_API ULeapCircleGesture : public ULeapGesture
{
	GENERATED_UCLASS_BODY()
public:
	~ULeapCircleGesture();
	
	/**
	* The center point of the circle within the Leap Motion frame of reference.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Circle Gesture")
	FVector Center;

	/**
	* Returns the normal vector for the circle being traced.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Circle Gesture")
	FVector Normal;

	/**
	* @return The finger performing the circle gesture.
	*/
	UFUNCTION(BlueprintCallable, Category = "Leap Circle Gesture")
	class ULeapPointable* Pointable();

	/**
	* The number of times the finger tip has traversed the circle.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Circle Gesture")
	float Progress;

	/**
	* The radius of the circle.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Circle Gesture")
	float Radius;

	void SetGesture(const class Leap::CircleGesture &Gesture);

private:
	class FPrivateCircleGesture* Private;

	UPROPERTY()
	class ULeapPointable* PPointable = nullptr;
};