// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LeapPointable.h"
#include "LeapFinger.generated.h"

/**
* The Finger class represents a tracked finger.
*
* Fingers are Pointable objects that the Leap Motion software has classified as a 
* finger. Get valid Finger objects from a Frame or a Hand object.
*
* Leap API reference: https://developer.leapmotion.com/documentation/cpp/api/Leap.Finger.html
*/
UCLASS(BlueprintType)
class LEAPMOTION_API ULeapFinger : public ULeapPointable
{
	GENERATED_UCLASS_BODY()
public:
	~ULeapFinger();

	/**
	* The bone at a given bone type index on this finger.
	*
	* @param	Type value from the LeapBoneType enumeration identifying the bone of interest.
	* @return	The Bone that has the specified bone type.
	*/
	UFUNCTION(BlueprintCallable, meta = (Category = "Leap Finger"))
	class ULeapBone *Bone(enum LeapBoneType Type);

	/**
	* The Metacarpal bone of this finger.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Finger")
	class ULeapBone *Metacarpal;

	/**
	* The Proximal bone of this finger.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Finger")
	class ULeapBone *Proximal;

	/**
	* The Intermediate bone of this finger.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Finger")
	class ULeapBone *Intermediate;
	
	/**
	* The Distal bone of this finger.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Finger")
	class ULeapBone *Distal;

	/**
	* Type of finger as enum (see LeapFingerType enum)
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Finger")
	TEnumAsByte<LeapFingerType> Type;

	void SetFinger(const class Leap::Finger &Pointable);

private:
	class FPrivateFinger* Private;
};