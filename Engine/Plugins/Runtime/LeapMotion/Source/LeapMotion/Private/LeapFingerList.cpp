// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LeapFingerList.h"
#include "LeapFinger.h"

class FPrivateFingerList{
public:
	Leap::FingerList Fingers;
};

ULeapFingerList::ULeapFingerList(const FObjectInitializer &ObjectInitializer) : UObject(ObjectInitializer), Private(new FPrivateFingerList())
{
}

ULeapFingerList::~ULeapFingerList()
{
	delete Private;
}

ULeapFingerList *ULeapFingerList::Append(const ULeapFingerList *List)
{
	if (PAppendedList == nullptr)
	{
		PAppendedList = NewObject<ULeapFingerList>(this, ULeapFingerList::StaticClass());
	}
	PAppendedList->SetFingerList(Private->Fingers.append(List->Private->Fingers));
	return (PAppendedList);
}

ULeapFingerList *ULeapFingerList::Extended()
{
	if (PExtendedList == nullptr)
	{
		PExtendedList = NewObject<ULeapFingerList>(this, ULeapFingerList::StaticClass());
	}
	PExtendedList->SetFingerList(Private->Fingers.extended());
	return (PExtendedList);
}

ULeapFinger *ULeapFingerList::Leftmost()
{
	if (PLeftmost == nullptr)
	{
		PLeftmost = NewObject<ULeapFinger>(this, ULeapFinger::StaticClass());
	}
	PLeftmost->SetFinger(Private->Fingers.leftmost());
	return (PLeftmost);
}

ULeapFinger *ULeapFingerList::Rightmost()
{
	if (PRightmost == nullptr)
	{
		PRightmost = NewObject<ULeapFinger>(this, ULeapFinger::StaticClass());
	}
	PRightmost->SetFinger(Private->Fingers.rightmost());
	return (PRightmost);
}


ULeapFinger *ULeapFingerList::Frontmost()
{
	if (PFrontmost == nullptr)
	{
		PFrontmost = NewObject<ULeapFinger>(this, ULeapFinger::StaticClass());
	}
	PFrontmost->SetFinger(Private->Fingers.frontmost());
	return (PFrontmost);
}

ULeapFinger *ULeapFingerList::GetPointableById(int32 Id)
{
	if (PPointableById == nullptr)
	{
		PPointableById = NewObject<ULeapFinger>(this, ULeapFinger::StaticClass());
	}
	PPointableById->SetFinger(Private->Fingers[Id]);
	return (PPointableById);
}

void ULeapFingerList::SetFingerList(const Leap::FingerList &Fingers)
{
	Private->Fingers = Fingers;

	Count = Private->Fingers.count();
	IsEmpty = Private->Fingers.isEmpty();
}

Leap::FingerList* ULeapFingerList::FingerList()
{
	return &(Private->Fingers);
}