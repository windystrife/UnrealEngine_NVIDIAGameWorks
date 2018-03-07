// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimFinger.h"
#include "LeapHand.h"
#include "AnimHand.generated.h"

UCLASS(BlueprintType)
class UAnimHand : public UObject
{
	GENERATED_UCLASS_BODY()

	//Hand Properties
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anim Hand")
	float Alpha;

	UPROPERTY(BlueprintReadOnly, Category = "Anim Hand Hand")
	float Confidence;

	//Fingers
	UPROPERTY(BlueprintReadOnly, Category = "Anim Hand Finger")
	UAnimFinger* Thumb;

	UPROPERTY(BlueprintReadOnly, Category = "Anim Hand Finger")
	UAnimFinger* Index;
	
	UPROPERTY(BlueprintReadOnly, Category = "Anim Hand Finger")
	UAnimFinger* Middle;

	UPROPERTY(BlueprintReadOnly, Category = "Anim Hand Finger")
	UAnimFinger* Ring;

	UPROPERTY(BlueprintReadOnly, Category = "Anim Hand Finger")
	UAnimFinger* Pinky;
	
	//Arm and Wrist, NB: Consider moving this up the skeletal tree, for leap this is sufficient
	UPROPERTY(BlueprintReadOnly, Category = "Anim Hand")
	UAnimBone* Wrist;

	UPROPERTY(BlueprintReadOnly, Category = "Anim Hand")
	UAnimBone* Palm;

	UPROPERTY(BlueprintReadOnly, Category = "Anim Hand")
	UAnimBone* LowerArm;

	UFUNCTION(BlueprintCallable, Category = "Anim Hand")
	bool Enabled();

	UFUNCTION(BlueprintCallable, Category = "Anim Hand")
	void SetEnabled(bool Enable = true);

	UFUNCTION(BlueprintCallable, Category = "Anim Hand")
	void TranslateHand(FVector Shift);

	UFUNCTION(BlueprintCallable, Category = "Anim Hand")
	void ChangeBasis(FRotator PreBase, FRotator PostBase, bool AdjustVectors = true);


	//These will turn into interface events for the general case later on
	UFUNCTION(BlueprintCallable, Category = "Anim Hand")
	void SetFromLeapHand(class ULeapHand* LeapHand);
};