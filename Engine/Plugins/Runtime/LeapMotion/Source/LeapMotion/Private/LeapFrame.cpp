// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LeapFrame.h"
#include "LeapTool.h"
#include "LeapFinger.h"
#include "LeapGesture.h"
#include "LeapGestureList.h"
#include "LeapHandList.h"
#include "LeapHand.h"
#include "LeapInteractionBox.h"
#include "LeapPointableList.h"
#include "LeapFingerList.h"
#include "LeapToolList.h"
#include "LeapImageList.h"
#include "LeapInterfaceUtility.h"

class FPrivateLeapFrame
{
public:

	Leap::Frame Frame;
};

ULeapFrame::ULeapFrame(const FObjectInitializer &ObjectInitializer) : UObject(ObjectInitializer), Private(new FPrivateLeapFrame())
{
}

ULeapFrame::~ULeapFrame()
{
	delete Private;
}

ULeapFinger* ULeapFrame::Finger(int32 Id)
{
	if (PFinger == nullptr)
	{
		PFinger = NewObject<ULeapFinger>(this);
	}
	PFinger->SetFinger(Private->Frame.finger(Id));
	return PFinger;
}

ULeapFingerList* ULeapFrame::Fingers()
{
	if (PFingers == nullptr)
	{
		PFingers = NewObject<ULeapFingerList>(this);
	}
	PFingers->SetFingerList(Private->Frame.fingers());
	return (PFingers);
}

ULeapGesture* ULeapFrame::Gesture(int32 id)
{
	if (PGesture == nullptr)
	{
		PGesture = NewObject<ULeapGesture>(this);
	}
	PGesture->SetGesture(Private->Frame.gesture(id));
	return PGesture;
}

ULeapGestureList* ULeapFrame::Gestures()
{
	if (PGestures == nullptr)
	{
		PGestures = NewObject<ULeapGestureList>(this);
	}
	PGestures->SetGestureList(Private->Frame.gestures());
	return (PGestures);
}

ULeapGestureList* ULeapFrame::GesturesSinceFrame(class ULeapFrame* Frame)
{
	if (PGestures == nullptr)
	{
		PGestures = NewObject<ULeapGestureList>(this);
	}
	PGestures->SetGestureList(Private->Frame.gestures(Frame->GetFrame()));
	return (PGestures);
}

ULeapHand* ULeapFrame::Hand(int32 Id)
{
	if (PHand == nullptr)
	{
		PHand = NewObject<ULeapHand>(this);
	}
	PHand->SetHand(Private->Frame.hand(Id));
	return PHand;
}

ULeapHandList* ULeapFrame::Hands()
{
	if (PHands == nullptr)
	{
		PHands = NewObject<ULeapHandList>(this);
	}
	PHands->SetHandList(Private->Frame.hands());
	return (PHands);
}


ULeapImageList* ULeapFrame::Images()
{
	if (PImages == nullptr)
	{
		PImages = NewObject<ULeapImageList>(this);
	}
	PImages->SetLeapImageList(Private->Frame.images());
	return (PImages);
}

ULeapInteractionBox* ULeapFrame::InteractionBox()
{
	if (PInteractionBox == nullptr)
	{
		PInteractionBox = NewObject<ULeapInteractionBox>(this);
	}
	PInteractionBox->SetInteractionBox(Private->Frame.interactionBox());
	return (PInteractionBox);
}

ULeapPointable* ULeapFrame::Pointable(int32 Id)
{
	if (PPointable == nullptr)
	{
		PPointable = NewObject<ULeapPointable>(this);
	}
	PPointable->SetPointable(Private->Frame.pointable(Id));
	return PPointable;
}

ULeapPointableList* ULeapFrame::Pointables()
{
	if (PPointables == nullptr)
	{
		PPointables = NewObject<ULeapPointableList>(this);
	}
	PPointables->SetPointableList(Private->Frame.pointables());
	return (PPointables);
}

float ULeapFrame::RotationAngle(class ULeapFrame* Frame)
{
	return Private->Frame.rotationAngle(Frame->GetFrame());
}

float ULeapFrame::RotationAngleAroundAxis(class ULeapFrame* Frame, FVector Axis)
{
	return Private->Frame.rotationAngle(Frame->GetFrame(), ConvertUEToLeap(Axis));
}

FVector ULeapFrame::RotationAxis(class ULeapFrame* Frame)
{
	return ConvertLeapToUE(Private->Frame.rotationAxis(Frame->GetFrame()));
}

float ULeapFrame::RotationProbability(class ULeapFrame* Frame)
{
	return Private->Frame.rotationProbability(Frame->GetFrame());
}

float ULeapFrame::ScaleFactor(class ULeapFrame* Frame)
{
	return Private->Frame.scaleFactor(Frame->GetFrame());
}

float ULeapFrame::ScaleProbability(class ULeapFrame* Frame)
{
	return Private->Frame.scaleProbability(Frame->GetFrame());
}

ULeapTool* ULeapFrame::Tool(int32 Id)
{
	if (PTool == nullptr)
	{
		PTool = NewObject<ULeapTool>(this);
	}
	PTool->SetTool(Private->Frame.tool(Id));
	return PTool;
}

ULeapToolList* ULeapFrame::Tools()
{
	if (PTools == nullptr)
	{
		PTools = NewObject<ULeapToolList>(this);
	}
	PTools->SetToolList(Private->Frame.tools());
	return (PTools);
}

FVector ULeapFrame::Translation(class ULeapFrame* Frame)
{
	return ConvertLeapToUE(Private->Frame.translation(Frame->GetFrame()));
}

float ULeapFrame::TranslationProbability(class ULeapFrame* Frame)
{
	return Private->Frame.translationProbability(Frame->GetFrame());
}

const Leap::Frame &ULeapFrame::GetFrame() const
{
	return (Private->Frame);
}

void ULeapFrame::SetFrame(Leap::Controller &Leap, int History)
{
	Leap::Frame frame = Leap.frame(History);

	this->SetFrame(frame);
}

void ULeapFrame::SetFrame(const Leap::Frame &Frame)
{
	Private->Frame = Frame;
	IsValid = Private->Frame.isValid();
	CurrentFPS = Private->Frame.currentFramesPerSecond();
}

