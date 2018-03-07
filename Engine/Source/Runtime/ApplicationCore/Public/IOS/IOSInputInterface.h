// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IInputInterface.h"
#include "IForceFeedbackSystem.h"
#if !PLATFORM_TVOS
#import <CoreMotion/CoreMotion.h>
#endif
#import <GameController/GameController.h>
#include "Misc/CoreMisc.h"
#include "Math/Quat.h"
#include "Math/Vector.h"
#include "Math/Color.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h"


#define KEYCODE_ENTER 1000
#define KEYCODE_BACKSPACE 1001
#define KEYCODE_ESCAPE 1002


enum TouchType
{
	TouchBegan,
	TouchMoved,
	TouchEnded,
};

struct TouchInput
{
	int Handle;
	TouchType Type;
	FVector2D LastPosition;
	FVector2D Position;
};

/**
 * Interface class for IOS input devices
 */
class FIOSInputInterface : public IForceFeedbackSystem, FSelfRegisteringExec
{
public:

	static TSharedRef< FIOSInputInterface > Create(  const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler );
	static TSharedPtr< FIOSInputInterface > Get();

public:

	virtual ~FIOSInputInterface() {}

	void SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler );

	/** Tick the interface (i.e check for new controllers) */
	void Tick( float DeltaTime );

	/**
	 * Poll for controller state and send events if needed
	 */
	void SendControllerEvents();

	static void QueueTouchInput(const TArray<TouchInput>& InTouchEvents);
	static void QueueKeyInput(int32 Key, int32 Char);

	//~ Begin Exec Interface
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	//~ End Exec Interface

	/**
	 * IForceFeedbackSystem implementation
	 */
	virtual void SetForceFeedbackChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override;
	virtual void SetForceFeedbackChannelValues(int32 ControllerId, const FForceFeedbackValues &values) override;
	virtual void SetLightColor(int32 ControllerId, FColor Color) override {}

	bool IsControllerAssignedToGamepad(int32 ControllerId);

private:

	FIOSInputInterface( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler );

	// handle disconnect and connect events
	void HandleConnection(GCController* Controller);
	void HandleDisconnect(GCController* Controller);
	
	/**
	 * Get the current Movement data from the device
	 *
	 * @param Attitude The current Roll/Pitch/Yaw of the device
	 * @param RotationRate The current rate of change of the attitude
	 * @param Gravity A vector that describes the direction of gravity for the device
	 * @param Acceleration returns the current acceleration of the device
	 */
	void GetMovementData(FVector& Attitude, FVector& RotationRate, FVector& Gravity, FVector& Acceleration);

	/**
	 * Calibrate the devices motion
	 */
	void CalibrateMotion(uint32 PlayerIndex);

private:
	void ProcessTouchesAndKeys(uint32 ControllerId);


	static TArray<TouchInput> TouchInputStack;
	static TArray<int32> KeyInputStack;

	// protects the input stack used on 2 threads
	static FCriticalSection CriticalSection;

	TSharedRef< FGenericApplicationMessageHandler > MessageHandler;


	/** Game controller objects (per user)*/
	struct FUserController
	{
		GCGamepadSnapshot* PreviousGamepad;
		GCExtendedGamepadSnapshot* PreviousExtendedGamepad;
#if PLATFORM_TVOS
		GCMicroGamepadSnapshot* PreviousMicroGamepad;
#endif
		FQuat ReferenceAttitude;
		bool bNeedsReferenceAttitude;
		bool bHasReferenceAttitude;
		bool bIsGamepadConnected;
		bool bIsRemoteConnected;
		bool bPauseWasPressed;
	};
	// there is a hardcoded limit of 4 controllers in the API
	FUserController Controllers[4];

	// can the remote be rotated to landscape
	bool bAllowRemoteRotation;

	// is the remote treated as a separate controller?
	bool bTreatRemoteAsSeparateController;

	// should the remote be used as virtual joystick vs touch events
	bool bUseRemoteAsVirtualJoystick;

	// should the tracking use the pad center as the virtual joystick center?
	bool bUseRemoteAbsoluteDpadValues;
	
#if !PLATFORM_TVOS
	/** Access to the ios devices motion */
	CMMotionManager* MotionManager;

	/** Access to the ios devices tilt information */
	CMAttitude* ReferenceAttitude;
#endif
	

	/** Last frames roll, for calculating rate */
	float LastRoll;

	/** Last frames pitch, for calculating rate */
	float LastPitch;

	/** True if a calibration is requested */
	bool bIsCalibrationRequested;

	/** The center roll value for tilt calibration */
	float CenterRoll;

	/** The center pitch value for tilt calibration */
	float CenterPitch;

	/** When using just acceleration (without full motion) we store a frame of accel data to filter by */
	FVector FilteredAccelerometer;
};
