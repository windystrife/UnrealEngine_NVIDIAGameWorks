// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LeapHand.h"
#include "LeapFrame.h"
#include "LeapFingerList.h"
#include "LeapInterfaceUtility.h"
#include "LeapArm.h"
#include "Leap_NoPI.h"

class FPrivateHand
{
public:
	Leap::Hand Hand;
};

ULeapHand::ULeapHand(const FObjectInitializer &ObjectInitializer) : UObject(ObjectInitializer), Private(new FPrivateHand())
{
}

ULeapHand::~ULeapHand()
{
	delete Private;
}

ULeapFrame *ULeapHand::Frame()
{
	if (PFrame == nullptr)
	{
		PFrame = NewObject<ULeapFrame>(this);
	}
	PFrame->SetFrame(Private->Hand.frame());
	return (PFrame);
}

ULeapFingerList* ULeapHand::Fingers()
{
	if (PFingers == nullptr)
	{
		PFingers = NewObject<ULeapFingerList>(this);
	}
	PFingers->SetFingerList(Private->Hand.fingers());
	return (PFingers);
}

float ULeapHand::RotationAngle(ULeapFrame *Frame)
{
	Leap::Frame RFrame;

	RFrame = Frame->GetFrame();
	return (Private->Hand.rotationAngle(RFrame));
}

float ULeapHand::RotationAngleWithAxis(class ULeapFrame *Frame, const FVector &Axis)
{
	Leap::Frame RFrame;
	Leap::Vector Vector;

	RFrame = Frame->GetFrame();
	Vector.x = Axis.X;
	Vector.y = Axis.Y;
	Vector.z = Axis.Z;
	return (Private->Hand.rotationAngle(RFrame, Vector));
}

FMatrix ULeapHand::RotationMatrix(const ULeapFrame *Frame)
{
	Leap::Frame RFrame;

	RFrame = Frame->GetFrame();
	return ConvertLeapBasisMatrix(Private->Hand.rotationMatrix(RFrame));
}


FVector ULeapHand::RotationAxis(const ULeapFrame *Frame)
{
	Leap::Frame RFrame;
	Leap::Vector Vector;

	RFrame = Frame->GetFrame();
	Vector = Private->Hand.rotationAxis(RFrame);
	return (ConvertAndScaleLeapToUE(Vector));
}

float ULeapHand::RotationProbability(const ULeapFrame *Frame)
{
	Leap::Frame RFrame;

	RFrame = Frame->GetFrame();
	return (Private->Hand.rotationProbability(RFrame));
}

float ULeapHand::ScaleFactor(const ULeapFrame *Frame)
{
	Leap::Frame RFrame;

	RFrame = Frame->GetFrame();
	return (Private->Hand.scaleFactor(RFrame));
}

float ULeapHand::ScaleProbability(const ULeapFrame *Frame)
{
	Leap::Frame RFrame;

	RFrame = Frame->GetFrame();
	return (Private->Hand.scaleProbability(RFrame));
}

FVector ULeapHand::Translation(const ULeapFrame *Frame)
{
	Leap::Frame RFrame;
	Leap::Vector Vector;

	RFrame = Frame->GetFrame();
	Vector = Private->Hand.translation(RFrame);
	return (ConvertAndScaleLeapToUE(Vector));
}

float ULeapHand::TranslationProbability(const ULeapFrame *Frame)
{
	Leap::Frame RFrame;

	RFrame = Frame->GetFrame();
	return (Private->Hand.translationProbability(RFrame));
}

bool ULeapHand::operator!=(const ULeapHand &Hand) const
{
	return (Hand.Private->Hand != this->Private->Hand);
}

bool ULeapHand::operator==(const ULeapHand &Hand) const
{
	return (Hand.Private->Hand == this->Private->Hand);
}


void ULeapHand::SetHand(const Leap::Hand &Hand)
{
	Private->Hand = Hand;

	//Set Properties
	if (Arm == nullptr || !Arm->IsValidLowLevel())
		Arm = NewObject<ULeapArm>(this, ULeapArm::StaticClass());
	Arm->SetArm(Private->Hand.arm());

	Confidence = Private->Hand.confidence();
	Direction = ConvertLeapToUE(Private->Hand.direction());
	GrabStrength = Private->Hand.grabStrength();
	IsLeft = Private->Hand.isLeft();
	IsRight = Private->Hand.isRight();
	PalmWidth = ScaleLeapToUE(Private->Hand.palmWidth());
	PinchStrength = Private->Hand.pinchStrength();
	SphereCenter = ConvertAndScaleLeapToUE(Private->Hand.sphereCenter());
	SphereRadius = ScaleLeapToUE(Private->Hand.sphereRadius());
	StabilizedPalmPosition = ConvertAndScaleLeapToUE(Private->Hand.stabilizedPalmPosition());
	TimeVisible = Private->Hand.timeVisible();

	PalmNormal = ConvertLeapToUE(Private->Hand.palmNormal());
	PalmPosition = ConvertAndScaleLeapToUE(Private->Hand.palmPosition());
	PalmVelocity = ConvertAndScaleLeapToUE(Private->Hand.palmVelocity());

	PalmOrientation = FRotationMatrix::MakeFromZX(PalmNormal*-1.f, Direction).Rotator();

	WristPosition = ConvertLeapToUE(Private->Hand.wristPosition());

	Basis = ConvertLeapBasisMatrix(Private->Hand.basis());

	//Convenience Setting, allows for easy branching in blueprint
	if (IsLeft)
	{
		HandType = LeapHandType::HAND_LEFT;
	}
	else if (IsRight)
	{
		HandType = LeapHandType::HAND_RIGHT;
	}
	else
	{
		HandType = LeapHandType::HAND_UNKNOWN;
	}

	Id = Private->Hand.id();
}
