// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LeapPointable.h"
#include "LeapFrame.h"
#include "LeapHand.h"
#include "LeapInterfaceUtility.h"

class FPrivatePointable
{
public:
	Leap::Pointable Pointable;
	ULeapFrame* PFrame = nullptr;
	ULeapHand* PHand = nullptr;
};

ULeapPointable::ULeapPointable(const FObjectInitializer &ObjectInitializer) : UObject(ObjectInitializer), Private(new FPrivatePointable())
{
}

ULeapPointable::~ULeapPointable()
{
	delete Private;
}

ULeapFrame *ULeapPointable::Frame()
{
	if (!PFrame)
	{
		PFrame = NewObject<ULeapFrame>(this);
	}
	PFrame->SetFrame(Private->Pointable.frame());
	return (PFrame);
}

ULeapHand *ULeapPointable::Hand()
{
	if (!PHand)
	{
		PHand = NewObject<ULeapHand>(this);
	}
	PHand->SetHand(Private->Pointable.hand());
	return (PHand);
}

bool ULeapPointable::Equal(const ULeapPointable *Other)
{
	return (Private->Pointable == Other->Private->Pointable);
}

bool ULeapPointable::Different(const ULeapPointable *Other)
{
	return (Private->Pointable != Other->Private->Pointable);
}

LeapZone ConvertTouchZone(Leap::Pointable::Zone Zone)
{
	switch (Zone)
	{
	case Leap::Pointable::Zone::ZONE_NONE:
		return (ZONE_NONE);
	case Leap::Pointable::Zone::ZONE_HOVERING:
		return (ZONE_HOVERING);
	case Leap::Pointable::Zone::ZONE_TOUCHING:
		return (ZONE_TOUCHING);
	default:
		return (ZONE_ERROR);
	}
}

void ULeapPointable::SetPointable(const Leap::Pointable &Pointable)
{
	Private->Pointable = Pointable;

	Direction = ConvertLeapToUE(Private->Pointable.direction());
	Id = Private->Pointable.id();
	IsExtended = Private->Pointable.isExtended();
	IsFinger = Private->Pointable.isFinger();
	IsTool = Private->Pointable.isTool();
	IsValid = Private->Pointable.isValid();
	Length = ScaleLeapToUE(Private->Pointable.length());
	StabilizedTipPosition = ConvertAndScaleLeapToUE(Private->Pointable.stabilizedTipPosition());
	TimeVisible = Private->Pointable.timeVisible();
	TipPosition = ConvertAndScaleLeapToUE(Private->Pointable.tipPosition());
	TipVelocity = ConvertAndScaleLeapToUE(Private->Pointable.tipVelocity());
	TouchDistance = Private->Pointable.touchDistance();
	TouchZone = ConvertTouchZone(Private->Pointable.touchZone());
	Width = ScaleLeapToUE(Private->Pointable.width());
}

const Leap::Pointable &ULeapPointable::GetPointable() const
{
	return (Private->Pointable);
}