// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FSceneViewport;
struct FCaptureProtocolInitSettings;
struct FFrameMetrics;
struct ICaptureProtocolHost;

/** A capture protocol responsible for dealing with captured frames using some custom method (writing out to disk, streaming, etc) */
struct IMovieSceneCaptureProtocol
{
	/**
	 * Initialize this capture protocol
	 *
	 * @param InSettings		The initial initialization settings to use for the capture
	 * @param Host				The client that is initializing this protocol
	 *
	 * @return true if initialization was successful, false otherwise
	 */
	virtual bool Initialize(const FCaptureProtocolInitSettings& InSettings, const ICaptureProtocolHost& Host) = 0;

	/**
	 * Instruct this protocol to capture a frame relating to the specified metrics
	 *
	 * @param FrameMetrics		Frame metrics relating to the current frame
	 * @param Host				The client that is initializing this protocol
	 */
	virtual void CaptureFrame(const FFrameMetrics& FrameMetrics, const ICaptureProtocolHost& Host) = 0;

	/**
	 * Check whether this protocol has any processing left to do, or whether it should be finalized.
	 * Only called when the capture has been asked to end.
	 */
	virtual bool HasFinishedProcessing() const { return true; }

	/**
	 * Called on the main thread to do any additional processing
	 */
	virtual void Tick() {}

	/**
	 * Called when we have finished capturing
	 */
	virtual void Finalize() {}

	/**
	 * Called when generating formatting filename to add any additional format mappings
	 *
	 * @param FormatMappings	Map to add additional format rules to
	 */
	virtual void AddFormatMappings(TMap<FString, FStringFormatArg>& FormatMappings) const {}

	/**
	 * Test whether this capture protocol thinks the file should be written to. Only called when we're not overwriting existing files.
	 * By default, we simply test for the file's existence, however this can be overridden to afford complex behaviour like
	 * writing out multiple video files for different file names
	 * @param InFilename 			The filename to test
	 * @param bOverwriteExisting	Whether we are allowed to overwrite existing files
	 * @return Whether we should deem this file writable or not
	 */
	virtual MOVIESCENECAPTURE_API bool CanWriteToFile(const TCHAR* InFilename, bool bOverwriteExisting) const;

	virtual ~IMovieSceneCaptureProtocol() {}
};



/** Metrics that correspond to a particular frame */
struct FFrameMetrics
{
	/** Default construction */
	FFrameMetrics() : TotalElapsedTime(0), FrameDelta(0), FrameNumber(0), NumDroppedFrames(0) {}

	FFrameMetrics(float InTotalElapsedTime, float InFrameDelta, uint32 InFrameNumber, uint32 InNumDroppedFrames)
		: TotalElapsedTime(InTotalElapsedTime), FrameDelta(InFrameDelta), FrameNumber(InFrameNumber), NumDroppedFrames(InNumDroppedFrames)
	{
	}

	/** The total amount of time, in seconds, since the capture started */
	float TotalElapsedTime;
	/** The total amount of time, in seconds, that this specific frame took to render (not accounting for dropped frames) */
	float FrameDelta;
	/** The index of this frame from the start of the capture, including dropped frames */
	uint32 FrameNumber;
	/** The number of frames we dropped in-between this frame, and the last one we captured */
	uint32 NumDroppedFrames;
};


/** Interface that defines when to capture or drop frames */
struct ICaptureStrategy
{
	virtual ~ICaptureStrategy(){}
	
	virtual void OnWarmup() = 0;
	virtual void OnStart() = 0;
	virtual void OnStop() = 0;
	virtual void OnPresent(double CurrentTimeSeconds, uint32 FrameIndex) = 0;
	virtual bool ShouldSynchronizeFrames() const { return true; }

	virtual bool ShouldPresent(double CurrentTimeSeconds, uint32 FrameIndex) const = 0;
	virtual int32 GetDroppedFrames(double CurrentTimeSeconds, uint32 FrameIndex) const = 0;
};


/** Interface to be implemented by any class using an IMovieSceneCaptureProtocol instance*/
struct ICaptureProtocolHost
{
	/** Generate a filename for the specified frame metrics. How often this is called is determined by the protocol itself */
	virtual FString GenerateFilename(const FFrameMetrics& FrameMetrics, const TCHAR* Extension) const = 0;

	/** Ensure that the specified file is writable, potentially deleting an existing file if settings allow */
	virtual void EnsureFileWritable(const FString& File) const = 0;

	/** Get the capture frequency */
	virtual float GetCaptureFrequency() const = 0;

	/** Access the host's capture strategy */
	virtual const ICaptureStrategy& GetCaptureStrategy() const = 0;
};


/** Structure used to initialize a capture protocol */
struct FCaptureProtocolInitSettings
{
	/**~ @todo: add ability to capture a sub-rectangle */

	/** Capture from a slate viewport, using the specified custom protocol settings */
	MOVIESCENECAPTURE_API static FCaptureProtocolInitSettings FromSlateViewport(TSharedRef<FSceneViewport> InSceneViewport, UObject* InProtocolSettings);
	
	/** The slate viewport we should capture from */
	TSharedPtr<FSceneViewport> SceneViewport;
	/** The desired size of the captured frames */
	FIntPoint DesiredSize;
	/** Protocol settings specific to the protocol we will be initializing */
	UObject* ProtocolSettings;

private:
	/** Private construction to ensure use of static init methods */
	FCaptureProtocolInitSettings() : ProtocolSettings(nullptr) {}
};
