// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "LeapEnums.h"
#include "Leap_NoPI.h"
#include "LeapBone.generated.h"

/**
* The Bone class represents a tracked bone.
* All fingers contain 4 bones that make up the anatomy of the finger. 
* Get valid Bone objects from a Finger object.
*
* Leap API reference: https://developer.leapmotion.com/documentation/cpp/api/Leap.Bone.html
*/
UCLASS(BlueprintType)
class LEAPMOTION_API ULeapBone : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	~ULeapBone();

	/**
	* The orthonormal basis vectors for this Bone as a Matrix.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Bone")
	FMatrix Basis;

	/**
	* The midpoint of the bone.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Bone")
	FVector Center;

	/**
	* The normalized direction of the bone from base to tip.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Bone")
	FVector Direction;

	/**
	* Convenience method, requires knowledge of the hand this bone belongs to in order to 
	* give a correct orientation (left hand basis is different from right).
	*
	* @param HandType left or right
	*/
	UFUNCTION(BlueprintCallable, Category = "Leap Bone")
	FRotator GetOrientation(LeapHandType handType);

	/**
	* Reports whether this is a valid Bone object.
	* @return True, if this Bone object contains valid tracking data.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Bone")
	bool IsValid;

	/**
	* The estimated length of the bone in centimeters.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Bone")
	float Length;

	/**
	* The end of the bone, closest to the finger tip.
    * In anatomical terms, this is the distal end of the bone.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Bone")
	FVector NextJoint;

	/**
	* The base of the bone, closest to the wrist.
	* In anatomical terms, this is the proximal end of the bone.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Bone")
	FVector PrevJoint;

	/**
	* The name of this bone.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Bone")
	TEnumAsByte<LeapBoneType> Type;

	/**
	* The average width of the flesh around the bone in centimeters.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Bone")
	float Width;


	/**
	* Compare Bone object inequality.
	*
	* @return False if and only if both Bone objects represent the exact same physical bone in the same frame and both Bone objects are valid.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "different", CompactNodeTitle = "!=", Keywords = "different operator"), Category = "Leap Bone")
	bool Different(const ULeapBone *other) const;

	/**
	* Compare Bone object equality.
	*
	* @return True if and only if both Bone objects represent the exact same physical bone in the same frame and both Bone objects are valid.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "equal", CompactNodeTitle = "==", Keywords = "equal operator"), Category = "Leap Bone")
	bool Equal(const ULeapBone *other) const;

	void SetBone(const class Leap::Bone &bone);

private:
	class FPrivateBone* Private;
};