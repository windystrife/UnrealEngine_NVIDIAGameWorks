// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "Leap_NoPI.h"
#include "LeapEnums.h"
#include "LeapPointable.generated.h"

/**
* The Pointable class reports the physical characteristics of a detected finger or tool.
*
* Leap API reference: https://developer.leapmotion.com/documentation/cpp/api/Leap.Pointable.html
*/
UCLASS(BlueprintType)
class LEAPMOTION_API ULeapPointable : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	~ULeapPointable();

	/**
	* The direction in which this finger or tool is pointing. The direction is 
	* expressed as a unit vector pointing in the same direction as the tip.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Pointable")
	FVector Direction;

	/**
	* The Frame associated with this Pointable object.
	*
	* @return	The associated Frame object, if available; otherwise, an invalid Frame object is returned.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "frame", CompactNodeTitle="", Keywords = "frame"), Category = "Leap Pointable")
	virtual class ULeapFrame *Frame();

	/**
	* The Hand associated with a finger.
	*
	* @return	The associated Hand object, if available; otherwise, an invalid Hand object is returned.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "hand", CompactNodeTitle="", Keywords = "hand"), Category = "Leap Pointable")
	virtual class ULeapHand *Hand();

	/**
	* A unique ID assigned to this Pointable object, whose value remains the same 
	* across consecutive frames while the tracked finger or tool remains visible.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Pointable")
	int32 Id;

	/**
	* Whether or not this Pointable is in an extended posture.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Pointable")
	bool IsExtended;

	/**
	* Whether or not this Pointable is classified as a finger.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Pointable")
	bool IsFinger;

	/**
	* Whether or not this Pointable is classified as a tool.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Pointable")
	bool IsTool;

	/**
	* Reports whether this is a valid Pointable object.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Pointable")
	bool IsValid;

	/**
	* The estimated length of the finger or tool in centimeters.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Pointable")
	float Length;

	/**
	* Compare Pointable object equality.
	*
	* @param	Other	pointable to compare to.
	* @return	True if equal.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "equal", CompactNodeTitle="==", Keywords = "equal"), Category = "Leap Pointable")
	virtual bool Equal(const ULeapPointable *Other);

	/**
	* Compare Pointable object inequality.
	*
	* @param	Other	pointable to compare to.
	* @return	True if different.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "different", CompactNodeTitle="!=", Keywords = "different"), Category = "Leap Pointable")
	virtual bool Different(const ULeapPointable *Other);

	/**
	* The stabilized tip position of this Pointable.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Pointable")
	FVector StabilizedTipPosition;

	/**
	* The duration of time this Pointable has been visible to the Leap Motion Controller.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Pointable")
	float TimeVisible;

	/**
	* The tip position in centimeters from the Leap Motion origin.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Pointable")
	FVector TipPosition;
	
	/**
	* The rate of change of the tip position in centimeters/second.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Pointable")
	FVector TipVelocity;

	/**
	* A value proportional to the distance between this Pointable object and the 
	* adaptive touch plane.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Pointable")
	float TouchDistance;

	/**
	* The current touch zone of this Pointable object.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Pointable")
	TEnumAsByte<LeapZone> TouchZone;

	/**
	* The estimated width of the finger or tool in centimeters.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Pointable")
	float Width;

	void SetPointable(const class Leap::Pointable &Pointable);
	const Leap::Pointable &GetPointable() const;

private:
	class FPrivatePointable* Private;

	UPROPERTY()
	ULeapFrame* PFrame = nullptr;
	UPROPERTY()
	ULeapHand* PHand = nullptr;
};