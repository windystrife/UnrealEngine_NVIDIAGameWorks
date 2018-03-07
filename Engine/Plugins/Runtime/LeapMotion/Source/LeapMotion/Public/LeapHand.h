// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "LeapEnums.h"
#include "LeapForwardDeclaration.h"
#include "LeapHand.generated.h"

/**
* The Hand class reports the physical characteristics of a detected hand.
* Hand tracking data includes a palm position and velocity; vectors for 
* the palm normal and direction to the fingers; properties of a sphere 
* fit to the hand; and lists of the attached fingers.
*
* Leap API reference: https://developer.leapmotion.com/documentation/cpp/api/Leap.Hand.html
*/
UCLASS(BlueprintType)
class LEAPMOTION_API ULeapHand : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	~ULeapHand();
	
	/**
	* The arm to which this hand is attached.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Hand")
	class ULeapArm* Arm;

	/**
	* The orientation of the hand as a basis matrix.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Hand")
	FMatrix Basis;

	/**
	* How confident we are with a given hand pose.
	* The confidence level ranges between 0.0 and 1.0 inclusive.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Hand")
	float Confidence;

	/**
	* The direction from the palm position toward the fingers.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Hand")
	FVector Direction;

	/**
	* The list of Finger objects detected in this frame that are attached to this hand,
	* given in order from thumb to pinky.
	*
	* @return	The FingerList containing all Finger objects attached to this hand.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Fingers"), Category = "Leap Hand")
	class ULeapFingerList* Fingers();

	/**
	* The Frame associated with this Hand.
	*
	* @return	The associated Frame object, if available; otherwise, an invalid Frame object is returned.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Frame"), Category = "Leap Hand")
	class ULeapFrame* Frame();

	/**
	* The strength of a grab hand pose as a float value in the [0..1] range representing 
	* the holding strength of the pose.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Hand")
	float GrabStrength;

	/**
	* Identifies whether this hand is Left, Right, or Unknown
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Hand")
	TEnumAsByte<LeapHandType> HandType;

	/*
	* A unique ID assigned to this Hand object, whose value remains the same across consecutive 
	* frames while the tracked hand remains visible.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Hand")
	int32 Id;

	/**
	* Identifies whether this Hand is a left hand.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Hand")
	bool IsLeft;

	/**
	* Whether this is a right hand.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Hand")
	bool IsRight;

	/**
	* Reports whether this is a valid Hand object.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Hand")
	bool IsValid;

	/**
	* The normal vector to the palm. If your hand is flat, this vector will point downward,
	* or “out” of the front surface of your palm.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Hand")
	FVector PalmNormal;

	/**
	* Custom API, Origin is a flat palm facing down.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Hand")
	FRotator PalmOrientation;

	/**
	* The center position of the palm in centimeters from the Leap Motion Controller origin.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Hand")
	FVector PalmPosition;

	/**
	* The rate of change of the palm position in centimeters/second.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Hand")
	FVector PalmVelocity;

	/**
	* The estimated width of the palm when the hand is in a flat position.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Hand")
	float PalmWidth;

	/**
	* The holding strength of a pinch hand pose. The strength is zero for an open hand, 
	* and blends to 1.0 when a pinching hand pose is recognized. Pinching can be done 
	* between the thumb and any other finger of the same hand.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Hand")
	float PinchStrength;

	/**
	* The angle of rotation around the rotation axis derived from the change in orientation 
	* of this hand, and any associated fingers, between the current frame and the specified frame.
	*
	* @param	OtherFrame	The starting frame for computing the relative rotation.
	* @return	A positive value representing the heuristically determined rotational change of the hand between the current frame and that specified in the sinceFrame parameter.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "RotationAngle"), Category = "Leap Hand")
	float RotationAngle(class ULeapFrame *OtherFrame);

	/**
	* The angle of rotation around the specified axis derived from the change in orientation 
	* of this hand, and any associated fingers, between the current frame and the specified frame.
	*
	* @param	OtherFrame	The starting frame for computing the relative rotation.
	* @param	Axis		The axis to measure rotation around.
	* @return	A value representing the heuristically determined rotational change of the hand between the current frame and that specified in the sinceFrame parameter around the specified axis.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "RotationAngleWithAxis"), Category = "Leap Hand")
	float RotationAngleWithAxis(class ULeapFrame *OtherFrame, const FVector &Axis);

	/**
	* The axis of rotation derived from the change in orientation of this hand, and any associated 
	* fingers, between the current frame and the specified frame.
	*
	* @param	OtherFrame	The starting frame for computing the relative rotation.
	* @return	A normalized direction Vector representing the heuristically determined axis of rotational change of the hand between the current frame and that specified in the sinceFrame parameter.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "RotationAxis"), Category = "Leap Hand")
	FVector RotationAxis(const class ULeapFrame *OtherFrame);

	/**
	* The transform matrix expressing the rotation derived from the change in orientation of this 
	* hand, and any associated fingers, between the current frame and the specified frame.
	*
	* @param	OtherFrame	The starting frame for computing the relative rotation.
	* @return	A transformation Matrix representing the heuristically determined rotational change of the hand between the current frame and that specified in the sinceFrame parameter.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "RotationMatrix"), Category = "Leap Hand")
	FMatrix RotationMatrix(const class ULeapFrame *OtherFrame);

	/**
	* The estimated probability that the hand motion between the current frame and the specified 
	* frame is intended to be a rotating motion.
	*
	* @param	OtherFrame	The starting frame for computing the relative rotation.
	* @return	A value between 0 and 1 representing the estimated probability that the hand motion between the current frame and the specified frame is intended to be a rotating motion.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "RotationProbability"), Category = "Leap Hand")
	float RotationProbability(const class ULeapFrame *OtherFrame);

	/**
	* The scale factor derived from this hand’s motion between the current frame and the specified frame.
	*
	* @param	OtherFrame	The starting frame for computing the relative scaling.
	* @return	A positive value representing the heuristically determined scaling change ratio of the hand between the current frame and that specified in the sinceFrame parameter.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ScaleFactor"), Category = "Leap Hand")
	float ScaleFactor(const class ULeapFrame *OtherFrame);

	/**
	* The estimated probability that the hand motion between the current frame and the specified 
	* frame is intended to be a scaling motion.
	*
	* @param	OtherFrame	The starting frame for computing the relative scaling.
	* @return	A value between 0 and 1 representing the estimated probability that the hand motion between the current frame and the specified frame is intended to be a scaling motion.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ScaleProbability"), Category = "Leap Hand")
	float ScaleProbability(const class ULeapFrame *OtherFrame);

	/**
	* The center of a sphere fit to the curvature of this hand. This sphere is placed roughly as 
	* if the hand were holding a ball.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Hand")
	FVector SphereCenter;

	/**
	* The radius of a sphere fit to the curvature of this hand. This sphere is placed roughly as
	* if the hand were holding a ball.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Hand")
	float SphereRadius;

	/**
	* The stabilized palm position of this Hand.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Hand")
	FVector StabilizedPalmPosition;

	/**
	* The duration of time this Hand has been visible to the Leap Motion Controller.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Hand")
	float TimeVisible;

	/**
	* The change of position of this hand between the current frame and the specified frame.
	*
	* @param	OtherFrame	The starting frame for computing the translation.
	* @return	A Vector representing the heuristically determined change in hand position between the current frame and that specified in the sinceFrame parameter.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Translation"), Category = "Leap Hand")
	FVector Translation(const class ULeapFrame *OtherFrame);

	/**
	* The estimated probability that the hand motion between the current frame and the specified 
	* frame is intended to be a translating motion.
	*
	* @param	OtherFrame	The starting frame for computing the translation.
	* @return	A value between 0 and 1 representing the estimated probability that the hand motion between the current frame and the specified frame is intended to be a translating motion.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "TranslationProbability"), Category = "Leap Hand")
	float TranslationProbability(const class ULeapFrame *OtherFrame);

	/**
	* The position of the wrist of this hand.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Hand")
	FVector WristPosition;

	bool operator!=(const ULeapHand &) const;

	bool operator==(const ULeapHand &) const;

	void SetHand(const class Leap::Hand &Hand);

private:
	class FPrivateHand* Private;

	UPROPERTY()
	ULeapFrame* PFrame = nullptr;
	UPROPERTY()
	ULeapFingerList* PFingers = nullptr;
};