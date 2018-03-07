// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"

class Error;

/** Represents the state of the video recorder */
enum class EVideoRecordingState
{
	None,
	Starting,
	Recording,
	Pausing,
	Paused,
	Finalizing,
	Error
};

/**
 * Interface for platform video recording functionality
 */
class IVideoRecordingSystem
{
public:
	virtual ~IVideoRecordingSystem() { }

	/**
	 * Enables or disables recording if the platform supports it. Useful to prevent users from sharing spoilers.\
	 *
	 * @param bEnableRecording If true, video recording will be allowed. If false, videos will not be recorded.
	 */
	virtual void EnableRecording(const bool bEnableRecording) = 0;

	/** Returns whether recording is currently enabled. */
	virtual bool IsEnabled() const = 0;

	/**
	 * Initializes a new video recording, but doesn't actually start capturing yet.
	 *
	 * @param DestinationFileName The base name of the resulting video, without a path or extension.
	 * @return True if opening the recording succeeded, false otherwise.
	 */
	virtual bool NewRecording(const TCHAR* DestinationFileName) = 0;

	/** Begins capturing video after a call to NewRecording or PauseRecording. */
	virtual void StartRecording() = 0;

	/** Pauses video recording after a call to StartRecording. Call StartRecording again to resume. */
	virtual void PauseRecording() = 0;

	/**
	 * Stops recording and prepares the final video file for use.
	 *
	 * @param bSaveRecording If true, the recording will be saved. If false, the recording will be discarded.
	 * @param Title The title to use for the final video
	 * @param Comment A comment to store with the final video
	 */
	virtual void FinalizeRecording(const bool bSaveRecording, const FText& Title, const FText& Comment) = 0;

	/** Returns the current state of video recording. */
	virtual EVideoRecordingState GetRecordingState() const = 0;
};

/** A generic implementation of the video recording system, that doesn't support recording */
class FGenericVideoRecordingSystem : public IVideoRecordingSystem
{
public:
	virtual void EnableRecording(const bool bEnableRecording) override {}
	virtual bool IsEnabled() const override { return false; }
	virtual bool NewRecording(const TCHAR* DestinationFileName) override { return false; }
	virtual void StartRecording() override {}
	virtual void PauseRecording() override {}
	virtual void FinalizeRecording(const bool bSaveRecording, const FText& Title, const FText& Comment) override {}

	virtual EVideoRecordingState GetRecordingState() const override { return EVideoRecordingState::None; }
};
