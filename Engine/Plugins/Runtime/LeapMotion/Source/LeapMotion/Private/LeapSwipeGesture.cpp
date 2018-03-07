// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LeapSwipeGesture.h"
#include "LeapPointable.h"
#include "LeapInterfaceUtility.h"

class FPrivateSwipeGesture
{
public:
	Leap::SwipeGesture Gesture;
	ULeapPointable* PPointable = nullptr;
};

ULeapSwipeGesture::ULeapSwipeGesture(const FObjectInitializer &ObjectInitializer) : ULeapGesture(ObjectInitializer), Private(new FPrivateSwipeGesture())
{
}

ULeapSwipeGesture::~ULeapSwipeGesture()
{
	delete Private;
}

ULeapPointable* ULeapSwipeGesture::Pointable()
{
	if (PPointable == nullptr)
	{
		PPointable = NewObject<ULeapPointable>(this);
	}
	PPointable->SetPointable(Private->Gesture.pointable());
	return (PPointable);
}

void ULeapSwipeGesture::SetGesture(const Leap::SwipeGesture &Gesture)
{
	//Super
	ULeapGesture::SetGesture(Gesture);

	Private->Gesture = Gesture;

	Direction = ConvertLeapToUE(Private->Gesture.direction());
	Position = ConvertAndScaleLeapToUE(Private->Gesture.position());
	Speed = ScaleLeapToUE(Private->Gesture.speed());
	StartPosition = ConvertAndScaleLeapToUE(Private->Gesture.startPosition());

	//Convenience
	BasicDirection = LeapBasicVectorDirection(Direction);
}