// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimHand.h"
#include "LeapArm.h"
#include "LeapFinger.h"
#include "LeapFingerList.h"

UAnimHand::UAnimHand(const class FObjectInitializer& Init)
	: Super(Init)
{
	Thumb = Init.CreateDefaultSubobject<UAnimFinger>(this, TEXT("Thumb"));
	Index = Init.CreateDefaultSubobject<UAnimFinger>(this, TEXT("Index"));
	Middle = Init.CreateDefaultSubobject<UAnimFinger>(this, TEXT("Middle"));
	Ring = Init.CreateDefaultSubobject<UAnimFinger>(this, TEXT("Ring"));
	Pinky = Init.CreateDefaultSubobject<UAnimFinger>(this, TEXT("Pinky"));

	Wrist = Init.CreateDefaultSubobject<UAnimBone>(this, TEXT("Wrist"));
	LowerArm = Init.CreateDefaultSubobject<UAnimBone>(this, TEXT("LowerArm"));
	Palm = Init.CreateDefaultSubobject<UAnimBone>(this, TEXT("Palm"));
}

bool UAnimHand::Enabled()
{
	return Alpha == 1.f;
}

void UAnimHand::SetEnabled(bool Enable)
{
	if (Enable)
	{
		Alpha = 1.f;
	}
	else
	{
		Alpha = 0.f;
	}

	//Forward to every finger
	Thumb->SetEnabled(Enable);
	Index->SetEnabled(Enable);
	Middle->SetEnabled(Enable);
	Ring->SetEnabled(Enable);
	Pinky->SetEnabled(Enable);

	//Arm/Wrist
	Wrist->SetEnabled(Enable);
	LowerArm->SetEnabled(Enable);
	Palm->SetEnabled(Enable);
}

void UAnimHand::TranslateHand(FVector Shift)
{
	//Shift all fingers
	Thumb->TranslateFinger(Shift);
	Index->TranslateFinger(Shift);
	Middle->TranslateFinger(Shift);
	Ring->TranslateFinger(Shift);
	Pinky->TranslateFinger(Shift);

	//Arm/Wrist
	Wrist->TranslateBone(Shift);
	LowerArm->TranslateBone(Shift);
}

void UAnimHand::ChangeBasis(FRotator PreBase, FRotator PostBase, bool AdjustVectors)
{
	//Change Basis for all fingers
	Thumb->ChangeBasis(PreBase, PostBase, AdjustVectors);
	Index->ChangeBasis(PreBase, PostBase, AdjustVectors);
	Middle->ChangeBasis(PreBase, PostBase, AdjustVectors);
	Ring->ChangeBasis(PreBase, PostBase, AdjustVectors);
	Pinky->ChangeBasis(PreBase, PostBase, AdjustVectors);

	//Arm/Wrist
	Wrist->ChangeBasis(PreBase, PostBase, AdjustVectors);
	LowerArm->ChangeBasis(PreBase, PostBase, AdjustVectors);
}

void UAnimHand::SetFromLeapHand(ULeapHand* LeapHand)
{
	//Confidence value
	Confidence = LeapHand->Confidence;

	//Wrist/Arm
	Wrist->Orientation = LeapHand->PalmOrientation;
	Wrist->Position = LeapHand->Arm->WristPosition;

	Palm->Orientation = LeapHand->PalmOrientation; //this works because we derive it from two vectors
	Palm->Position = LeapHand->PalmPosition;
	Palm->Scale = FVector(LeapHand->PalmWidth / 8.5f, LeapHand->PalmWidth / 8.5f, LeapHand->PalmWidth / 8.5f);

	LeapHandType HandType = LeapHand->HandType;

	//Equivalent to elbow in leap
	LowerArm->Position = LeapHand->Arm->ElbowPosition;
	LowerArm->Orientation = LeapHand->Arm->GetOrientation(HandType);

	//Fingers
	ULeapFingerList* Fingers = LeapHand->Fingers();


	for (int i = 0; i < Fingers->Count; i++)
	{
		ULeapFinger* Finger = Fingers->GetPointableById(i);
		switch (Finger->Type)
		{
		case FINGER_TYPE_THUMB:
			Thumb->SetFromLeapFinger(Finger, HandType);
			break;
		case FINGER_TYPE_INDEX:
			Index->SetFromLeapFinger(Finger, HandType);
			break;
		case FINGER_TYPE_MIDDLE:
			Middle->SetFromLeapFinger(Finger, HandType);
			break;
		case FINGER_TYPE_RING:
			Ring->SetFromLeapFinger(Finger, HandType);
			break;
		case FINGER_TYPE_PINKY:
			Pinky->SetFromLeapFinger(Finger, HandType);
			break;
		default:
			break;
		}
	}
}