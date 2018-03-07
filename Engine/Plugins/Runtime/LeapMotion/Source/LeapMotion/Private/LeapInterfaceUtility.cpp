// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LeapInterfaceUtility.h"
#include "IHeadMountedDisplay.h"
#include "LeapGesture.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "IXRTrackingSystem.h"

DEFINE_LOG_CATEGORY(LeapPluginLog);

#define LEAP_TO_UE_SCALE 0.1f
#define UE_TO_LEAP_SCALE 10.f
//#define LEAP_MOUNT_OFFSET 8.f	//in cm (UE units) forming FVector(LEAP_MOUNT_OFFSET,0,0) forward vector, deprecated

FVector LeapMountOffset = FVector(8.f, 0, 0);

//NB: localized variable for transforms - defaults
bool LeapShouldAdjustForFacing = false;
bool LeapShouldAdjustRotationForHMD = false;
bool LeapShouldAdjustPositionForHMD = false;
bool LeapShouldAdjustForMountOffset = true;

FRotator CombineRotators(FRotator A, FRotator B)
{
	FQuat AQuat = FQuat(A);
	FQuat BQuat = FQuat(B);

	return FRotator(BQuat*AQuat);
}

FVector AdjustForLeapFacing(FVector In)
{
	FRotator Rotation = FRotator(90.f, 0.f, 180.f);
	return FQuat(Rotation).RotateVector(In);
}

FVector AdjustForHMD(FVector In)
{
	if (GEngine->XRSystem.IsValid())
	{
		FQuat OrientationQuat;
		FVector Position;
		GEngine->XRSystem->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, OrientationQuat, Position);
		FVector Out = OrientationQuat.RotateVector(In);
		if (LeapShouldAdjustPositionForHMD)
		{
			if (LeapShouldAdjustForMountOffset)
			{
				Position += OrientationQuat.RotateVector(LeapMountOffset);
			}
			Out += Position;
		}
		return Out;
	}
	else
	{
		return In;
	}
}

FVector adjustForHMDOrientation(FVector In)
{
	if (GEngine->XRSystem.IsValid())
	{
		FQuat OrientationQuat;
		FVector Position;
		GEngine->XRSystem->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, OrientationQuat, Position);
		FVector Out = OrientationQuat.RotateVector(In);
		return Out;
	}
	else
		return In;

}

//Conversion - use find and replace to change behavior, no scaling version is typically used for orientations
FVector ConvertLeapToUE(Leap::Vector LeapVector)
{
	//Convert Axis
	FVector ConvertedVector = FVector(-LeapVector.z, LeapVector.x, LeapVector.y);

	//Hmd orientation adjustment
	if (LeapShouldAdjustForFacing)
	{
		FRotator Rotation = FRotator(90.f, 0.f, 180.f);
		ConvertedVector = FQuat(Rotation).RotateVector(ConvertedVector);
		
		if (LeapShouldAdjustRotationForHMD)
		{
			if (GEngine->XRSystem.IsValid())
			{
				FQuat orientationQuat;
				FVector position;
				GEngine->XRSystem->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, orientationQuat, position);
				ConvertedVector = orientationQuat.RotateVector(ConvertedVector);
			}
		}
	}

	return ConvertedVector;
}

FVector ConvertAndScaleLeapToUE(Leap::Vector LeapVector)
{
	//Scale from mm to cm (ue default)
	FVector ConvertedVector = FVector(-LeapVector.z * LEAP_TO_UE_SCALE, LeapVector.x * LEAP_TO_UE_SCALE, LeapVector.y * LEAP_TO_UE_SCALE);

	//Front facing leap adjustments
	if (LeapShouldAdjustForFacing)
	{
		ConvertedVector = AdjustForLeapFacing(ConvertedVector);
		if (LeapShouldAdjustRotationForHMD)
		{
			ConvertedVector = AdjustForHMD(ConvertedVector);
		}
	}
	return ConvertedVector;
}

FMatrix ConvertLeapBasisMatrix(Leap::Matrix LeapMatrix)
{
	/*
	Leap Basis depends on hand type with -z, x, y as general format. This then needs to be inverted to point correctly (x = forward), which yields the matrix below.
	*/
	FVector InX, InY, InZ, InW;
	InX.Set(LeapMatrix.zBasis.z, -LeapMatrix.zBasis.x, -LeapMatrix.zBasis.y);
	InY.Set(-LeapMatrix.xBasis.z, LeapMatrix.xBasis.x, LeapMatrix.xBasis.y);
	InZ.Set(-LeapMatrix.yBasis.z, LeapMatrix.yBasis.x, LeapMatrix.yBasis.y);
	InW.Set(-LeapMatrix.origin.z, LeapMatrix.origin.x, LeapMatrix.origin.y);

	if (LeapShouldAdjustForFacing)
	{
		InX = AdjustForLeapFacing(InX);
		InY = AdjustForLeapFacing(InY);
		InZ = AdjustForLeapFacing(InZ);
		InW = AdjustForLeapFacing(InW);

		if (LeapShouldAdjustRotationForHMD)
		{
			InX = adjustForHMDOrientation(InX);
			InY = adjustForHMDOrientation(InY);
			InZ = adjustForHMDOrientation(InZ);
			InW = adjustForHMDOrientation(InW);
		}
	}

	return (FMatrix(InX, InY, InZ, InW));
}
FMatrix SwapLeftHandRuleForRight(const FMatrix& UEMatrix)
{
	FMatrix Matrix = UEMatrix;
	//Convenience method to swap the axis correctly, already in UE format to swap Y instead of leap Z
	FVector InverseVector = -Matrix.GetUnitAxis(EAxis::Y);
	Matrix.SetAxes(NULL, &InverseVector, NULL, NULL);
	return Matrix;
}

Leap::Vector ConvertUEToLeap(FVector UEVector)
{
	return Leap::Vector(UEVector.Y, UEVector.Z, -UEVector.X);
}

Leap::Vector ConvertAndScaleUEToLeap(FVector UEVector)
{
	return Leap::Vector(UEVector.Y * UE_TO_LEAP_SCALE, UEVector.Z * UE_TO_LEAP_SCALE, -UEVector.X * UE_TO_LEAP_SCALE);
}

float ScaleLeapToUE(float LeapFloat)
{
	return LeapFloat * LEAP_TO_UE_SCALE;	//mm->cm
}

float ScaleUEToLeap(float UEFloat)
{
	return UEFloat * UE_TO_LEAP_SCALE;	//mm->cm
}


void LeapSetMountToHMDOffset(FVector Offset)
{
	LeapMountOffset = Offset;
}

void LeapSetShouldAdjustForFacing(bool ShouldRotate)
{
	LeapShouldAdjustForFacing = ShouldRotate;
}

void LeapSetShouldAdjustForHMD(bool ShouldRotate, bool ShouldOffset)
{
	LeapShouldAdjustRotationForHMD = ShouldRotate;
	LeapShouldAdjustPositionForHMD = ShouldOffset;
}

void LeapSetShouldAdjustForMountOffset(bool ShouldAddOffset)
{
	LeapShouldAdjustForMountOffset = ShouldAddOffset;
}

LeapBasicDirection LeapBasicVectorDirection(FVector Direction)
{
	float x = FMath::Abs(Direction.X);
	float y = FMath::Abs(Direction.Y);
	float z = FMath::Abs(Direction.Z);

	//is basic in x?
	if (x >= y && x >= z)
	{
		if (Direction.X < -0.5)
			return LeapBasicDirection::DIRECTION_TOWARD;
		else if (Direction.X > 0.5)
			return LeapBasicDirection::DIRECTION_AWAY;
	}
	//is basic in y?
	else if (y >= x && y >= z)
	{
		if (Direction.Y < -0.5)
			return LeapBasicDirection::DIRECTION_LEFT;
		else if (Direction.Y > 0.5)
			return LeapBasicDirection::DIRECTION_RIGHT;
	}
	//is basic in z?
	else if (z >= x && z >= y)
	{
		if (Direction.Z < -0.5)
			return LeapBasicDirection::DIRECTION_DOWN;
		else if (Direction.Z > 0.5)
			return LeapBasicDirection::DIRECTION_UP;
	}
	
	//If we haven't returned by now, the direction is none
	return LeapBasicDirection::DIRECTION_NONE;
}

//Utility function used to debug address allocation - helped find the cdcdcdcd allocation error
void UtilityDebugAddress(void* Pointer)
{
	const void * address = static_cast<const void*>(Pointer);
	std::stringstream ss;
	ss << address;
	std::string name = ss.str();
	FString string(name.c_str());

	UE_LOG(LeapPluginLog, Warning, TEXT("Address: %s"), *string);
}

//Utility function to handle uncaught GC pointer releases (which will typically pass null checks)
//This is hacky, but guarantees certain GC releases will be caught, make a pull request if you know a better way to handle these cases
bool UtilityPointerIsValid(void* Pointer)
{
#if PLATFORM_32BITS
	return (Pointer != nullptr && Pointer != (void*)0xdddddddd);
#else
	return (Pointer != nullptr && Pointer != (void*)0xdddddddddddddddd);
#endif
}
