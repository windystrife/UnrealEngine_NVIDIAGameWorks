// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LeapGesture.h"
#include "LeapFrame.h"
#include "LeapHandList.h"
#include "LeapPointableList.h"

class FPrivateGesture
{
public:
	Leap::Gesture Gesture;
};

ULeapGesture::ULeapGesture(const FObjectInitializer &ObjectInitializer) : UObject(ObjectInitializer), Private(new FPrivateGesture())
{
}

ULeapGesture::~ULeapGesture()
{
	delete Private;
}

ULeapFrame* ULeapGesture::Frame()
{
	if (PFrame == nullptr)
	{
		PFrame = NewObject<ULeapFrame>(this);
	}
	PFrame->SetFrame(Private->Gesture.frame());
	return (PFrame);
}

ULeapHandList* ULeapGesture::Hands()
{
	if (PHands == nullptr)
	{
		PHands = NewObject<ULeapHandList>(this);
	}
	PHands->SetHandList(Private->Gesture.hands());
	return (PHands);
}

ULeapPointableList* ULeapGesture::Pointables()
{
	if (PPointables == nullptr)
	{
		PPointables = NewObject<ULeapPointableList>(this);
	}
	PPointables->SetPointableList(Private->Gesture.pointables());
	return (PPointables);
}

LeapGestureState gestureState(Leap::Gesture::State State)
{
	switch (State)
	{
	case Leap::Gesture::STATE_START:
		return (GESTURE_STATE_START);
	case Leap::Gesture::STATE_UPDATE:
		return (GESTURE_STATE_UPDATE);
	case Leap::Gesture::STATE_STOP:
		return (GESTURE_STATE_STOP);
	default:
		return (GESTURE_STATE_INVALID);
	}
}

LeapGestureType gestureType(Leap::Gesture::Type Type)
{
	switch (Type)
	{
	case Leap::Gesture::TYPE_CIRCLE:
		return (GESTURE_TYPE_CIRCLE);
	case Leap::Gesture::TYPE_KEY_TAP:
		return (GESTURE_TYPE_KEY_TAP);
	case Leap::Gesture::TYPE_SCREEN_TAP:
		return (GESTURE_TYPE_SCREEN_TAP);
	case Leap::Gesture::TYPE_SWIPE:
		return (GESTURE_TYPE_SWIPE);
	default:
		return (GESTURE_TYPE_INVALID);
	}
}

void ULeapGesture::SetGesture(const Leap::Gesture &Gesture)
{
	Private->Gesture = Gesture;

	Duration = Private->Gesture.duration();
	DurationSeconds = Private->Gesture.durationSeconds();
	Id = Private->Gesture.id();
	IsValid = Private->Gesture.isValid();
	State = gestureState(Private->Gesture.state());
	Type = gestureType(Private->Gesture.type());
}