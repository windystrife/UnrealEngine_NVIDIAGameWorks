// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Leap_NoPI.h"

DECLARE_LOG_CATEGORY_EXTERN(LeapPluginLog, Log, All);

//Rotation
FRotator CombineRotators(FRotator A, FRotator B);


//Conversion
//To ue
FVector ConvertLeapToUE(Leap::Vector LeapVector);
FVector ConvertAndScaleLeapToUE(Leap::Vector LeapVector);
FMatrix ConvertLeapBasisMatrix(Leap::Matrix LeapMatrix);
FMatrix SwapLeftHandRuleForRight(const FMatrix &UEMatrix);		//needed for all left hand basis which will be incorrect in ue format

//To leap
Leap::Vector ConvertUEToLeap(FVector UEVector);
Leap::Vector ConvertAndScaleUEToLeap(FVector UEVector);

float ScaleLeapToUE(float LeapFloat);
float ScaleUEToLeap(float UEFloat);

void LeapSetMountToHMDOffset(FVector Offset);

//HMD automation
void LeapSetShouldAdjustForFacing(bool ShouldRotate);
void LeapSetShouldAdjustForHMD(bool ShouldRotate, bool ShouldOffset);
void LeapSetShouldAdjustForMountOffset(bool ShouldAddOffset);

//Convenience gesture determination
enum LeapBasicDirection LeapBasicVectorDirection(FVector Direction);

//Pointer debugging
void UtilityDebugAddress(void* Pointer);
bool UtilityPointerIsValid(void* Pointer);