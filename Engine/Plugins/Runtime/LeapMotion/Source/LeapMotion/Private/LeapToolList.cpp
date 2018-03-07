// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LeapToolList.h"
#include "LeapTool.h"

class FPrivateToolList
{
public:
	Leap::ToolList Tools;
};


ULeapToolList::ULeapToolList(const FObjectInitializer &ObjectInitializer) : UObject(ObjectInitializer), Private(new FPrivateToolList())
{
}

ULeapToolList::~ULeapToolList()
{
	delete Private;
}

ULeapToolList *ULeapToolList::Append(const ULeapToolList *List)
{
	if (PAppendedList == nullptr)
	{
		PAppendedList = NewObject<ULeapToolList>(this, ULeapToolList::StaticClass());
	}
	PAppendedList->SetToolList(this->Private->Tools.append(List->Private->Tools));
	return (PAppendedList);
}

int32 ULeapToolList::Count() const
{
	return (Private->Tools.count());
}

bool ULeapToolList::IsEmpty() const
{
	return (Private->Tools.isEmpty());
}

ULeapTool* ULeapToolList::Leftmost()
{
	if (PLeftmost == nullptr)
	{
		PLeftmost = NewObject<ULeapTool>(this, ULeapTool::StaticClass());
	}
	PLeftmost->SetTool(Private->Tools.leftmost());
	return (PLeftmost);
}

ULeapTool* ULeapToolList::Rightmost()
{
	if (PRightmost == nullptr)
	{
		PRightmost = NewObject<ULeapTool>(this, ULeapTool::StaticClass());
	}
	PRightmost->SetTool(Private->Tools.rightmost());
	return (PRightmost);
}


ULeapTool* ULeapToolList::Frontmost()
{

	if (PFrontmost == nullptr)
	{
		PFrontmost = NewObject<ULeapTool>(this, ULeapTool::StaticClass());
	}
	PFrontmost->SetTool(Private->Tools.frontmost());
	return (PFrontmost);
}

ULeapPointable* ULeapToolList::GetPointableByIndex(int32 Index)
{
	if (PPointableById == nullptr)
	{
		PPointableById = NewObject<ULeapTool>(this, ULeapTool::StaticClass());
	}
	PPointableById->SetPointable(Private->Tools[Index]);
	return (PPointableById);
}

void ULeapToolList::SetToolList(const Leap::ToolList &Pointables)
{
	Private->Tools = Pointables;
}

const Leap::ToolList* ULeapToolList::ToolList()
{
	return &(Private->Tools);
}