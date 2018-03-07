// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IXRCamera.h"
#include "IXRTrackingSystem.h"
#include "SceneViewExtension.h"
#include "LateUpdateManager.h"

/** 
 * Default base implementation of IXRCamera.
 * Can either be used directly by implementations or extended with platform-specific features.
 */
class HEADMOUNTEDDISPLAY_API  FDefaultXRCamera : public IXRCamera, public FSceneViewExtensionBase
{
public:
	FDefaultXRCamera(const FAutoRegister&, IXRTrackingSystem* InTrackingSystem, int32 InDeviceId);

	virtual ~FDefaultXRCamera()
	{}

	// IXRSystemIdentifier interface
public:
	virtual FName GetSystemName() const override
	{
		return TrackingSystem->GetSystemName();
	}

	// IIdentifiableXRDevice interface:
public:
	virtual int32 GetSystemDeviceId() const override
	{
		return DeviceId;
	}

	// IXRCamera interface
public:

	/**
	 * Set the view offset mode to assume an implied HMD position
	 */
	virtual void UseImplicitHMDPosition(bool bInImplicitHMDPosition) override
	{ 
		bUseImplicitHMDPosition = bInImplicitHMDPosition;
	}

	/**
	 * Optionally called by APlayerController to apply the orientation of the
	 * headset to the PC's rotation. If this is not done then the PC will face
	 * differently than the camera, which might be good (depending on the game).
	 */
	virtual void ApplyHMDRotation(APlayerController* PC, FRotator& ViewRotation) override;

	/**
	 * Apply the orientation and position of the headset to the Camera.
	 */
	virtual bool UpdatePlayerCamera(FQuat& CurrentOrientation, FVector& CurrentPosition) override;
	
	virtual void OverrideFOV(float& InOutFOV) override;

	/** Setup state for applying the render thread late update */
	virtual void SetupLateUpdate(const FTransform& ParentToWorld, USceneComponent* Component) override;

	virtual void CalculateStereoCameraOffset(const enum EStereoscopicPass StereoPassType, FRotator& ViewRotation, FVector& ViewLocation) override;

	// ISceneViewExtension interface:
public:
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override;
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;
	virtual bool IsActiveThisFrame(class FViewport* InViewport) const override;
	

protected:
	IXRTrackingSystem* TrackingSystem;
	const int32 DeviceId;

	FRotator DeltaControlRotation;
	FQuat DeltaControlOrientation;
private:
	FLateUpdateManager LateUpdate;
	bool bUseImplicitHMDPosition;
	mutable bool bCurrentFrameIsStereoRendering;
};
