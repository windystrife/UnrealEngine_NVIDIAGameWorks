// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "XRTrackingSystemBase.h"
#include "DefaultXRCamera.h"

FXRTrackingSystemBase::FXRTrackingSystemBase()
{
}

FXRTrackingSystemBase::~FXRTrackingSystemBase()
{
}

uint32 FXRTrackingSystemBase::CountTrackedDevices(EXRTrackedDeviceType Type /*= EXRTrackedDeviceType::Any*/)
{
	TArray<int32> DeviceIds;
	if (EnumerateTrackedDevices(DeviceIds, Type))
	{
		return DeviceIds.Num();
	}
	else
	{
		return 0;
	}
}

bool FXRTrackingSystemBase::IsTracking(int32 DeviceId)
{
	FQuat Orientation;
	FVector Position;
	return GetCurrentPose(DeviceId, Orientation, Position);
}

TSharedPtr< class IXRCamera, ESPMode::ThreadSafe > FXRTrackingSystemBase::GetXRCamera(int32 DeviceId)
{
	check(DeviceId == HMDDeviceId);

	if (!XRCamera.IsValid())
	{
		XRCamera = FSceneViewExtensions::NewExtension<FDefaultXRCamera>(this, DeviceId);
	}
	return XRCamera;
}

bool FXRTrackingSystemBase::GetRelativeEyePose(int32 DeviceId, EStereoscopicPass Eye, FQuat& OutOrientation, FVector& OutPosition)
{
	OutOrientation = FQuat::Identity;
	OutPosition = FVector::ZeroVector;
	if (DeviceId == IXRTrackingSystem::HMDDeviceId && (Eye == eSSP_LEFT_EYE || Eye == eSSP_RIGHT_EYE))
	{
		OutPosition = FVector(0, (Eye == EStereoscopicPass::eSSP_LEFT_EYE ? .5 : -.5) * 0.064f * GetWorldToMetersScale(), 0);
		return true;
	}
	else
	{
		return false;
	}
}

const int32 IXRTrackingSystem::HMDDeviceId;