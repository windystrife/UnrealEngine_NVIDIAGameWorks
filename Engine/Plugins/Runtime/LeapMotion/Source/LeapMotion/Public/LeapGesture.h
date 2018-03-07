// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "LeapEnums.h"
#include "Leap_NoPI.h"
#include "LeapGesture.generated.h"

/**
* The Gesture class represents a recognized movement by the user.
* The Leap Motion Controller watches the activity within its field of view for certain 
* movement patterns typical of a user gesture or command. For example, a movement from 
* side to side with the hand can indicate a swipe gesture, while a finger poking forward
* can indicate a screen tap gesture.
*
* Leap API reference: https://developer.leapmotion.com/documentation/cpp/api/Leap.Gesture.html
*/
UCLASS(BlueprintType)
class LEAPMOTION_API ULeapGesture : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	~ULeapGesture();
	
	/**
	* The elapsed duration of the recognized movement up to the frame containing this
	* Gesture object, in microseconds.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Gesture")
	float Duration;

	/**
	* The elapsed duration in seconds.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Gesture")
	float DurationSeconds;

	/**
	* The Frame containing this Gesture instance.
	* 
	* @return The parent Frame object.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "frame", CompactNodeTitle = "", Keywords = "frame"), Category = "Leap Gesture")
	class ULeapFrame* Frame();

	/**
	* The list of hands associated with this Gesture, if any.
	*
	* @return the list of related Hand objects.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "hands", CompactNodeTitle = "", Keywords = "hands"), Category = "Leap Gesture")
	class ULeapHandList* Hands();

	/**
	* The gesture ID.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Gesture")
	int32 Id;

	/**
	* Reports whether this Gesture instance represents a valid Gesture.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Gesture")
	bool IsValid;

	/**
	* The list of fingers and tools associated with this Gesture, if any.
	*
	* @return the list of related Pointable objects.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "pointables", CompactNodeTitle = "", Keywords = "pointables"), Category = "Leap Gesture")
	class ULeapPointableList* Pointables();

	/**
	* The gesture state.
	* Recognized movements occur over time and have a beginning, a middle, and an end.
	* The ‘state‘ attribute reports where in that sequence this Gesture object falls.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Gesture")
	TEnumAsByte<LeapGestureState> State;

	/**
	* The gesture type.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Gesture")
	TEnumAsByte<LeapGestureType> Type;

	bool operator!=(const ULeapGesture &rhs) const;
	bool operator==(const ULeapGesture &rhs) const;

	void SetGesture(const class Leap::Gesture &Gesture);

private:
	class FPrivateGesture* Private;

	UPROPERTY()
	ULeapFrame* PFrame = nullptr;
	UPROPERTY()
	ULeapHandList* PHands = nullptr;
	UPROPERTY()
	ULeapPointableList* PPointables = nullptr;
};