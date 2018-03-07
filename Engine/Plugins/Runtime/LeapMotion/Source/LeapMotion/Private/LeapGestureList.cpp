// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LeapGestureList.h"
#include "LeapGesture.h"

class FPrivateGestureList
{
public:

	Leap::GestureList Gestures;
};

ULeapGestureList::ULeapGestureList(const FObjectInitializer &ObjectInitializer) : UObject(ObjectInitializer), Private(new FPrivateGestureList())
{
}

ULeapGestureList::~ULeapGestureList()
{
	delete Private;
}

ULeapGesture* ULeapGestureList::GetIndex(int32 Index)
{
	if (Gesture == nullptr)
	{
		Gesture = NewObject<ULeapGesture>(this, ULeapGesture::StaticClass());
	}
	Gesture->SetGesture(Private->Gestures[Index]);
	return (Gesture);
}

ULeapGesture* ULeapGestureList::operator[](int Index)
{
	return GetIndex(Index);
}

void ULeapGestureList::SetGestureList(const Leap::GestureList &GestureList)
{
	Private->Gestures = GestureList;

	Count = Private->Gestures.count();
	IsEmpty = Private->Gestures.isEmpty();
}