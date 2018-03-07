// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "LeapEnums.h"
#include "Leap_NoPI.h"
#include "LeapArm.generated.h"

/**
* The Arm class represents the forearm.
*
* Leap API reference: https://developer.leapmotion.com/documentation/cpp/api/Leap.Arm.html
*/
UCLASS(BlueprintType)
class LEAPMOTION_API ULeapArm : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	~ULeapArm();

	/**
	* The orthonormal basis vectors for the Arm bone as a Matrix.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Arm")
	FMatrix Basis;

	/**
	* The center of the forearm.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Arm")
	FVector Center;

	/**
	* The normalized direction in which the arm is pointing (from elbow to wrist).
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Arm")
	FVector Direction;

	/**
	* The position of the elbow.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Arm")
	FVector ElbowPosition;

	/**
	* Basis matrix in rotation form for the arm given the hand
	* @param HandType left or right
	* @return Orientation basis
	*/
	UFUNCTION(BlueprintCallable, Category = "Leap Arm")
	FRotator GetOrientation(LeapHandType handType);

	/**
	* Reports whether this is a valid Hand object.
	* @return True, if this Hand object contains valid tracking data.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Arm")
	bool IsValid;


	/**
	* The estimated width of the palm when the hand is in a flat position.
	* @return Width
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Arm")
	float Width;

	/**
	* The position of the wrist of this hand.
	* @return A vector containing the coordinates of the wrist position in centimeters.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Arm")
	FVector WristPosition;

	bool operator!=(const ULeapArm &) const;

	bool operator==(const ULeapArm &) const; 

	void SetArm(const class Leap::Arm &arm);

private:
	class FPrivateArm* Private;
};