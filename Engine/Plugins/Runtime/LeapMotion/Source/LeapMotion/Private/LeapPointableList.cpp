// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LeapPointableList.h"
#include "LeapPointable.h"
#include "LeapToolList.h"
#include "LeapFingerList.h"

class FPrivatePointableList
{
public:

	Leap::PointableList Pointables;
};

ULeapPointableList::ULeapPointableList(const FObjectInitializer &ObjectInitializer) : UObject(ObjectInitializer), Private(new FPrivatePointableList())
{
}

ULeapPointableList::~ULeapPointableList()
{
	delete Private;
}

ULeapPointableList *ULeapPointableList::Append(ULeapPointableList *List)
{
	if (PAppendedList == nullptr)
	{
		PAppendedList = NewObject<ULeapPointableList>(this, ULeapPointableList::StaticClass());
	}
	PAppendedList->SetPointableList(Private->Pointables.append(List->Private->Pointables));
	return (PAppendedList);
}

ULeapPointableList *ULeapPointableList::AppendTools(ULeapToolList *List)
{
	if (PAppendedList == nullptr)
	{
		PAppendedList = NewObject<ULeapPointableList>(this, ULeapPointableList::StaticClass());
	}
	PAppendedList->SetPointableList(this->Private->Pointables.append(*List->ToolList()));
	return (PAppendedList);
}

ULeapPointableList *ULeapPointableList::AppendFingers(ULeapFingerList *List)
{
	if (PAppendedList == nullptr)
	{
		PAppendedList = NewObject<ULeapPointableList>(this, ULeapPointableList::StaticClass());
	}
	PAppendedList->SetPointableList(this->Private->Pointables.append(*List->FingerList()));
	return (PAppendedList);
}

ULeapPointableList *ULeapPointableList::Extended()
{
	if (PExtendedList == nullptr)
	{
		PExtendedList = NewObject<ULeapPointableList>(this, ULeapPointableList::StaticClass());
	}
	PExtendedList->SetPointableList(Private->Pointables.extended());
	return (PExtendedList);
}

ULeapPointable *ULeapPointableList::Leftmost()
{
	if (PLeftmost == nullptr)
	{
		PLeftmost = NewObject<ULeapPointable>(this);
	}
	PLeftmost->SetPointable(Private->Pointables.leftmost());
	return (PLeftmost);
}

ULeapPointable *ULeapPointableList::Rightmost()
{
	if (PRightmost == nullptr)
	{
		PRightmost = NewObject<ULeapPointable>(this);
	}
	PRightmost->SetPointable(Private->Pointables.rightmost());
	return (PRightmost);
}


ULeapPointable *ULeapPointableList::Frontmost()
{
	if (PFrontmost == nullptr)
	{
		PFrontmost = NewObject<ULeapPointable>(this);
	}
	PFrontmost->SetPointable(Private->Pointables.frontmost());
	return (PFrontmost);
}

ULeapPointable *ULeapPointableList::GetPointableByIndex(int32 Index)
{
	if (PPointableByIndex == nullptr)
	{
		PPointableByIndex = NewObject<ULeapPointable>(this);
	}
	PPointableByIndex->SetPointable(Private->Pointables[Index]);
	return (PPointableByIndex);
}

void ULeapPointableList::SetPointableList(const Leap::PointableList &Pointables)
{
	Private->Pointables = Pointables;
	Count = Private->Pointables.count();
	IsEmpty = Private->Pointables.isEmpty();
}