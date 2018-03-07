// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OculusHMDPrivate.h"
#include "UObject/ObjectMacros.h"
#include "Tickable.h"
#include "OculusSceneCaptureCubemap.generated.h"


//-------------------------------------------------------------------------------------------------
// UOculusSceneCaptureCubemap
//-------------------------------------------------------------------------------------------------

class USceneCaptureComponent2D;

UCLASS()
class UOculusSceneCaptureCubemap : public UObject, public FTickableGameObject
{
	GENERATED_BODY()
public:
	UOculusSceneCaptureCubemap();

	virtual void Tick(float DeltaTime) override;

	virtual bool IsTickable() const override
	{
		return CaptureComponents.Num() != 0 && Stage != None;
	}

	virtual bool IsTickableWhenPaused() const override
	{
		return IsTickable();
	}

	virtual TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(USceneCapturer, STATGROUP_Tickables);
	}

	// init capture params and start
	void StartCapture(UWorld* World, uint32 InCaptureBoxSideRes, EPixelFormat InFormat = EPixelFormat::PF_A16B16G16R16);

	// sets offset for the capture, in UU, relatively to current player 0 location
	void SetOffset(FVector InOffset) { CaptureOffset = InOffset; }

	// overrides player's 0 orientation for the capture.
	void SetInitialOrientation(const FQuat& InOrientation) { OverriddenOrientation = InOrientation; }

	// overrides player's 0 location for the capture.
	void SetInitialLocation(FVector InLocation) { OverriddenLocation = InLocation; }

	bool IsFinished() const { return Stage == Finished; }
	bool IsCapturing() const { return Stage == Capturing || Stage == SettingPos; }

#if !UE_BUILD_SHIPPING
	static void CaptureCubemapCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
#endif // UE_BUILD_SHIPPING

private:

	enum EStage
	{
		None,
		SettingPos,
		Capturing,
		Finished
	} Stage;

	UPROPERTY()
	TArray<USceneCaptureComponent2D*> CaptureComponents;

	uint32 CaptureBoxSideRes;
	EPixelFormat CaptureFormat;

	FString OutputDir;

	FVector OverriddenLocation;		// overridden location of the capture, world coordinates, UU
	FQuat	OverriddenOrientation;	// overridden orientation of the capture. Full orientation is used (not only yaw, like with player's rotation).
	FVector CaptureOffset;			// offset relative to current player's 0 location
};
