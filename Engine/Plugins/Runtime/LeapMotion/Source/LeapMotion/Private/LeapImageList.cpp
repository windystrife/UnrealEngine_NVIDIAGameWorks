// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LeapImageList.h"
#include "LeapImage.h"

class FPrivateLeapImageList
{
public:
	Leap::ImageList LeapImages;
};

ULeapImageList::ULeapImageList(const FObjectInitializer &ObjectInitializer) : UObject(ObjectInitializer), Private(new FPrivateLeapImageList())
{
}

ULeapImageList::~ULeapImageList()
{
	delete Private;
}

ULeapImage* ULeapImageList::GetIndex(int32 Index)
{
	//We need to use two separate objects to ensure we have two separate images
	if (Index == 0)
	{
		if (PIndexImage1 == nullptr)
		{
			PIndexImage1 = NewObject<ULeapImage>(this);
		}
		PIndexImage1->SetLeapImage(Private->LeapImages[Index]);
		return (PIndexImage1);
	}
	else
	{
		if (PIndexImage2 == nullptr)
		{
			PIndexImage2 = NewObject<ULeapImage>(this);
		}
		PIndexImage2->SetLeapImage(Private->LeapImages[Index]);
		return (PIndexImage2);
	}
}

ULeapImage* ULeapImageList::operator[](int Index)
{
	return GetIndex(Index);
}

void ULeapImageList::SetLeapImageList(const Leap::ImageList &LeapImageList)
{
	Private->LeapImages = LeapImageList;

	Count = Private->LeapImages.count();
	IsEmpty = Private->LeapImages.isEmpty();
}