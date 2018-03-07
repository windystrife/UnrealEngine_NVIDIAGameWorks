// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "Leap_NoPI.h"
#include "LeapFrame.generated.h"

/**
* The Frame class represents a set of hand and finger tracking data detected 
* in a single frame.
* 
* The Leap Motion software detects hands, fingers and tools within the tracking
* area, reporting their positions, orientations, gestures, and motions in frames
* at the Leap Motion frame rate.
*
* Leap API reference: https://developer.leapmotion.com/documentation/cpp/api/Leap.Frame.html
*/
UCLASS(BlueprintType)
class LEAPMOTION_API ULeapFrame : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	~ULeapFrame();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Frame")
	float CurrentFPS;

	/**
	* Whether this Frame instance is valid.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Frame")
	bool IsValid;

	/**
	* The Finger object with the specified ID in this frame.
	* 
	* @param	Id The ID value of a Finger object from a previous frame.
	* @return	The Finger object with the matching ID if one exists in this frame; otherwise, an invalid Finger object is returned.
	*/
	UFUNCTION(BlueprintCallable, Category = "Leap Frame")
	class ULeapFinger* Finger(int32 Id);

	/**
	* The list of Finger objects detected in this frame, given in arbitrary order.
	*
	* @return	The FingerList containing all Finger objects detected in this frame.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "fingers", CompactNodeTitle = "", Keywords = "get fingers"), Category = "Leap Frame")
	class ULeapFingerList* Fingers();

	/**
	* The Gesture object with the specified ID in this frame.
	*
	* @param		Id The ID of an Gesture object from a previous frame.
	* @return The Gesture object in the frame with the specified ID if one exists; Otherwise, an Invalid Gesture object.
	*/
	UFUNCTION(BlueprintCallable, Category = "Leap Frame")
	class ULeapGesture* Gesture(int32 Id);

	/**
	* The gestures recognized or continuing in this frame.
	*
	* @return the list of gestures.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "gestures", CompactNodeTitle = "", Keywords = "get gestures"), Category = "Leap Frame")
	class ULeapGestureList* Gestures();

	/**
	* Returns a GestureList containing all gestures that have occurred since the specified frame.
	*
	* @param		Frame An earlier Frame object. The starting frame must still be in the frame history cache, which has a default length of 60 frames.
	* @return GestureList The list of the Gesture objects that have occurred since the specified frame.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "gestures", CompactNodeTitle = "", Keywords = "get gestures"), Category = "Leap Frame")
	class ULeapGestureList* GesturesSinceFrame(class ULeapFrame* frame);

	/**
	* The Hand object with the specified ID in this frame.
	*
	* @param		Id The ID value of a Hand object from a previous frame.
	* @return The Hand object with the matching ID if one exists in this frame; otherwise, an invalid Hand object is returned.
	*/
	UFUNCTION(BlueprintCallable, Category = "Leap Frame")
	class ULeapHand* Hand(int32 Id);

	/**
	* The list of Hand objects detected in this frame, given in arbitrary order.
	*
	* @return The HandList containing all Hand objects detected in this frame.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "getHands", CompactNodeTitle = "", Keywords = "get hands"), Category = "Leap Frame")
	class ULeapHandList* Hands();

	/**
	* The list of images from the Leap Motion cameras.
	*
	* @return An ImageList object containing the camera images analyzed to create this Frame.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "images", CompactNodeTitle = "", Keywords = "get images"), Category = "Leap Frame")
	class ULeapImageList* Images();

	/**
	* The current InteractionBox for the frame.
	*
	* @return The current InteractionBox object.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "interactionBox", CompactNodeTitle = "", Keywords = "get interaction box"), Category = "Leap Frame")
	class ULeapInteractionBox* InteractionBox();

	/**
	* The Pointable object with the specified ID in this frame.
	*
	* @param		Id The ID value of a Pointable object from a previous frame.
	* @return The Pointable object with the matching ID if one exists in this frame; otherwise, an invalid Pointable object is returned.
	*/
	UFUNCTION(BlueprintCallable, Category = "Leap Frame")
	class ULeapPointable* Pointable(int32 Id);

	/**
	* The list of Pointable objects (fingers and tools) detected in this frame, given in arbitrary order.
	*
	* @return The PointableList containing all Pointable objects detected in this frame.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "pointables", CompactNodeTitle = "", Keywords = "get pointables"), Category = "Leap Frame")
	class ULeapPointableList* Pointables();

	/**
	* The angle of rotation around the rotation axis derived from the overall rotational motion between the current frame and the specified frame.
	*
	* @param Frame	The starting frame for computing the relative rotation.
	* @return A positive value containing the heuristically determined rotational change between the current frame and that specified in the sinceFrame parameter.
	*/
	UFUNCTION(BlueprintCallable, Category = "Leap Frame")
	float RotationAngle(class ULeapFrame* Frame);

	/**
	* The angle of rotation around the specified axis derived from the overall rotational motion between the current frame and the specified frame.
	*
	* @param Frame	The starting frame for computing the relative rotation.
	* @param Axis	The axis to measure rotation around.
	* @return A value containing the heuristically determined rotational change between the current frame and that specified in the sinceFrame parameter around the given axis.
	*/
	UFUNCTION(BlueprintCallable, Category = "Leap Frame")
	float RotationAngleAroundAxis(class ULeapFrame* Frame, FVector Axis);

	/**
	* The axis of rotation derived from the overall rotational motion between the current frame and the specified frame.
	*
	* @param Frame	The starting frame for computing the relative rotation.
	* @return A normalized direction Vector representing the axis of the heuristically determined rotational change between the current frame and that specified in the sinceFrame parameter.
	*/
	UFUNCTION(BlueprintCallable, Category = "Leap Frame")
	FVector RotationAxis(class ULeapFrame* Frame);

	/**
	* The estimated probability that the overall motion between the current frame and the specified frame is intended to be a rotating motion.
	*
	* @param Frame	The starting frame for computing the relative rotation.
	* @return A value between 0 and 1 representing the estimated probability that the overall motion between the current frame and the specified frame is intended to be a rotating motion.
	*/
	UFUNCTION(BlueprintCallable, Category = "Leap Frame")
	float RotationProbability(class ULeapFrame* Frame);

	/**
	* The scale factor derived from the overall motion between the current frame and the specified frame.
	*
	* @param Frame	The starting frame for computing the relative scaling.
	* @return A positive value representing the heuristically determined scaling change ratio between the current frame and that specified in the sinceFrame parameter.
	*/
	UFUNCTION(BlueprintCallable, Category = "Leap Frame")
	float ScaleFactor(class ULeapFrame* Frame);

	/**
	* The estimated probability that the overall motion between the current frame and the specified frame is intended to be a scaling motion.
	*
	* @param Frame	The starting frame for computing the relative scaling.
	* @return A value between 0 and 1 representing the estimated probability that the overall motion between the current frame and the specified frame is intended to be a scaling motion.
	*/
	UFUNCTION(BlueprintCallable, Category = "Leap Frame")
	float ScaleProbability(class ULeapFrame* Frame);

	/**
	* The Tool object with the specified ID in this frame.
	*
	* @param Id		The ID value of a Tool object from a previous frame.
	* @return The Tool object with the matching ID if one exists in this frame; otherwise, an invalid Tool object is returned.
	*/
	UFUNCTION(BlueprintCallable, Category = "Leap Frame")
	class ULeapTool* Tool(int32 Id);

	/**
	* The list of Tool objects detected in this frame, given in arbitrary order.
	*
	* @return The ToolList containing all Tool objects detected in this frame.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "pointables", CompactNodeTitle = "", Keywords = "get pointables"), Category = "Leap Frame")
	class ULeapToolList* Tools();

	/**
	* The change of position derived from the overall linear motion between the current frame and the specified frame.
	*
	* @param Frame	The starting frame for computing the relative translation.
	* @return A Vector representing the heuristically determined change in position of all objects between the current frame and that specified in the frame parameter.
	*/
	UFUNCTION(BlueprintCallable, Category = "Leap Frame")
	FVector Translation(class ULeapFrame* Frame);

	/**
	* The estimated probability that the overall motion between the current frame and the specified frame is intended to be a translating motion.
	*
	* @param Frame	The starting frame for computing the translation.
	* @return A value between 0 and 1 representing the estimated probability that the overall motion between the current frame and the specified frame is intended to be a translating motion.
	*/
	UFUNCTION(BlueprintCallable, Category = "Leap Frame")
	float TranslationProbability(class ULeapFrame* Frame);

	void SetFrame(Leap::Controller &Leap, int32 History = 0);
	void SetFrame(const class Leap::Frame &Frame);
	const Leap::Frame &GetFrame() const;

private:
	class FPrivateLeapFrame* Private;

	UPROPERTY()
	ULeapFinger* PFinger = nullptr;
	UPROPERTY()
	ULeapFingerList* PFingers = nullptr;
	UPROPERTY()
	ULeapGesture* PGesture = nullptr;
	UPROPERTY()
	ULeapGestureList* PGestures = nullptr;
	UPROPERTY()
	ULeapHand* PHand = nullptr;
	UPROPERTY()
	ULeapHandList* PHands = nullptr;
	UPROPERTY()
	ULeapImageList* PImages = nullptr;
	UPROPERTY()
	ULeapInteractionBox* PInteractionBox = nullptr;
	UPROPERTY()
	ULeapPointable* PPointable = nullptr;
	UPROPERTY()
	ULeapPointableList* PPointables = nullptr;
	UPROPERTY()
	ULeapTool* PTool = nullptr;
	UPROPERTY()
	ULeapToolList* PTools = nullptr;
};