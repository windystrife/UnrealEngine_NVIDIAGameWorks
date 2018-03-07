// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Math/Color.h"


// General identifiers for potential force feedback channels. These will be mapped according to the
// platform specific implementation.
// For example, the PS4 only listens to the XXX_LARGE channels and ignores the rest, while the XBox One could
// map the XXX_LARGE to the handle motors and XXX_SMALL to the trigger motors. And iOS can map LEFT_SMALL to
// its single motor.
enum class FForceFeedbackChannelType
{
	LEFT_LARGE,
	LEFT_SMALL,
	RIGHT_LARGE,
	RIGHT_SMALL
};


struct FForceFeedbackValues
{
	float LeftLarge;
	float LeftSmall;
	float RightLarge;
	float RightSmall;

	FForceFeedbackValues()
		: LeftLarge(0.f)
		, LeftSmall(0.f)
		, RightLarge(0.f)
		, RightSmall(0.f)
	{ }
};

struct FHapticFeedbackBuffer
{
	TArray<uint8> RawData;
	uint32 CurrentPtr;
	int BufferLength;
	int SamplesSent;
	bool bFinishedPlaying;
	int SamplingRate;
	float ScaleFactor;

	FHapticFeedbackBuffer()
		: CurrentPtr(0)
		, BufferLength(0)
		, SamplesSent(0)
		, bFinishedPlaying(false)
		, SamplingRate(0)
	{
	}

	bool NeedsUpdate() const
	{
		return !bFinishedPlaying;
	}
};

struct FHapticFeedbackValues
{
	float Frequency;
	float Amplitude;

	FHapticFeedbackBuffer* HapticBuffer;

	FHapticFeedbackValues()
		: Frequency(0.f)
		, Amplitude(0.f)
		, HapticBuffer(NULL)
	{
	}

	FHapticFeedbackValues(const float InFrequency, const float InAmplitude)
	{
		// can't use FMath::Clamp here due to header files dependencies
		Frequency = (InFrequency < 0.f) ? 0.f : ((InFrequency > 1.f) ? 1.f : InFrequency);
		Amplitude = (InAmplitude < 0.f) ? 0.f : ((InAmplitude > 1.f) ? 1.f : InAmplitude);
		HapticBuffer = NULL;
	}
};


/**
 * Interface for the input interface.
 */
class IInputInterface
{
public:

	/** Virtual destructor. */
	virtual ~IInputInterface() { };

	/**
	* Sets the strength/speed of the given channel for the given controller id.
	* NOTE: If the channel is not supported, the call will silently fail
	*
	* @param ControllerId the id of the controller whose value is to be set
	* @param ChannelType the type of channel whose value should be set
	* @param Value strength or speed of feedback, 0.0f to 1.0f. 0.0f will disable
	*/
	virtual void SetForceFeedbackChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) = 0;

	/**
	* Sets the strength/speed of all the channels for the given controller id.
	* NOTE: Unsupported channels are silently ignored
	*
	* @param ControllerId the id of the controller whose value is to be set
	* @param FForceFeedbackChannelValues strength or speed of feedback for all channels
	*/
	virtual void SetForceFeedbackChannelValues(int32 ControllerId, const FForceFeedbackValues &Values) = 0;

	/**
	* Sets the frequency and amplitude of haptic feedback channels for a given controller id.
	* Some devices / platforms may support just haptics, or just force feedback.
	*
	* @param ControllerId	ID of the controller to issue haptic feedback for
	* @param HandId			Which hand id (e.g. left or right) to issue the feedback for.  These usually correspond to EControllerHands
	* @param Values			Frequency and amplitude to haptics at
	*/
	virtual void SetHapticFeedbackValues(int32 ControllerId, int32 Hand, const FHapticFeedbackValues& Values) {}

	/*
	 * Sets the controller for the given controller.  Ignored if controller does not support a color.
	 */
	virtual void SetLightColor(int32 ControllerId, FColor Color) = 0;
};
