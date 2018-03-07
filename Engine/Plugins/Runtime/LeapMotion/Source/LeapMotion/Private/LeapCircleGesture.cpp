// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LeapCircleGesture.h"
#include "LeapGesture.h"
#include "LeapPointable.h"
#include "LeapInterfaceUtility.h"
#include "Math.h"

class FPrivateCircleGesture
{
public:
	~FPrivateCircleGesture()
	{
	}
	Leap::CircleGesture Gesture;
};

ULeapCircleGesture::ULeapCircleGesture(const FObjectInitializer &ObjectInitializer) : ULeapGesture(ObjectInitializer), Private(new FPrivateCircleGesture)
{
}

ULeapCircleGesture::~ULeapCircleGesture()
{
	delete Private;
}

ULeapPointable* ULeapCircleGesture::Pointable()
{
	if (PPointable == nullptr)
	{
		PPointable = NewObject<ULeapPointable>(this);
	}
	PPointable->SetPointable(Private->Gesture.pointable());
	return (PPointable);
}

void ULeapCircleGesture::SetGesture(const Leap::CircleGesture &Gesture)
{
	//Super
	ULeapGesture::SetGesture(Gesture);

	Private->Gesture = Gesture;

	Center = ConvertAndScaleLeapToUE(Private->Gesture.center());
	Normal = ConvertLeapToUE(Private->Gesture.normal());
	Progress = Private->Gesture.progress();
	Radius = ScaleLeapToUE(Private->Gesture.radius());	//scale
}