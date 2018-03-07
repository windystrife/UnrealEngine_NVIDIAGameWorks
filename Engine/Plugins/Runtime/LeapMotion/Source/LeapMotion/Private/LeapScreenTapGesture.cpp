// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LeapScreenTapGesture.h"
#include "LeapPointable.h"
#include "LeapInterfaceUtility.h"

class FPrivateScreenTapGesture
{
public:
	Leap::ScreenTapGesture Gesture;
};

ULeapScreenTapGesture::ULeapScreenTapGesture(const FObjectInitializer &ObjectInitializer) : ULeapGesture(ObjectInitializer), Private(new FPrivateScreenTapGesture())
{
}

ULeapScreenTapGesture::~ULeapScreenTapGesture()
{
	delete Private;
}

ULeapPointable* ULeapScreenTapGesture::Pointable()
{
	if (PPointable == nullptr)
	{
		PPointable = NewObject<ULeapPointable>(this);
	}
	PPointable->SetPointable(Private->Gesture.pointable());
	return (PPointable);
}
void ULeapScreenTapGesture::SetGesture(const Leap::ScreenTapGesture &Gesture)
{
	//Super
	ULeapGesture::SetGesture(Gesture);

	Private->Gesture = Gesture;

	Direction = ConvertLeapToUE(Private->Gesture.direction());
	Position = ConvertAndScaleLeapToUE(Private->Gesture.position());
	Progress = Private->Gesture.progress();

	//Convenience
	BasicDirection = LeapBasicVectorDirection(Direction);
}