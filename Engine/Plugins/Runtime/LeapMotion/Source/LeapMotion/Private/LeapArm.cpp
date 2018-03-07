// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LeapArm.h"
#include "LeapInterfaceUtility.h"

class FPrivateArm
{
public:
	Leap::Arm Arm;
};

ULeapArm::ULeapArm(const FObjectInitializer &ObjectInitializer) : UObject(ObjectInitializer), Private(new FPrivateArm())
{
}

ULeapArm::~ULeapArm()
{
	delete Private;
}

FRotator ULeapArm::GetOrientation(LeapHandType HandType)
{
	if (HandType == LeapHandType::HAND_LEFT)
	{
		return SwapLeftHandRuleForRight(Basis).Rotator();
	}
	else
	{
		return Basis.Rotator();
	}
}


bool ULeapArm::operator!=(const ULeapArm &Arm) const
{
	return (Arm.Private->Arm != this->Private->Arm);
}

bool ULeapArm::operator==(const ULeapArm &Arm) const
{
	return (Arm.Private->Arm == this->Private->Arm);
}

void ULeapArm::SetArm(const Leap::Arm &Arm)
{
	Private->Arm = Arm;

	Basis = ConvertLeapBasisMatrix(Private->Arm.basis());
	Center = ConvertAndScaleLeapToUE(Private->Arm.center());
	Direction = ConvertLeapToUE(Private->Arm.direction());
	ElbowPosition = ConvertAndScaleLeapToUE(Private->Arm.elbowPosition());
	IsValid = Private->Arm.isValid();
	Width = Private->Arm.width();
	WristPosition = ConvertAndScaleLeapToUE(Private->Arm.wristPosition());
}