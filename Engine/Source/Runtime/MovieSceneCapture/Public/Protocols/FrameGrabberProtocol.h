// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "IMovieSceneCaptureProtocol.h"
#include "MovieSceneCaptureProtocolSettings.h"
#include "FrameGrabber.h"

#include "FrameGrabberProtocol.generated.h"

UCLASS()
class MOVIESCENECAPTURE_API UFrameGrabberProtocolSettings : public UMovieSceneCaptureProtocolSettings
{
public:
	UFrameGrabberProtocolSettings(const FObjectInitializer&) : DesiredPixelFormat(PF_B8G8R8A8), RingBufferSize(3) {}

	GENERATED_BODY()

	/** The pixel format we want to capture in */
	EPixelFormat DesiredPixelFormat;

	/** The size of the render-target resolution surface ring-buffer */
	uint32 RingBufferSize;
};

struct MOVIESCENECAPTURE_API FFrameGrabberProtocol : IMovieSceneCaptureProtocol
{
protected:

	/**~ IMovieSceneCaptureProtocol Implementation */
	virtual bool HasFinishedProcessing() const;
	virtual bool Initialize(const FCaptureProtocolInitSettings& InSettings, const ICaptureProtocolHost& Host);
	virtual void CaptureFrame(const FFrameMetrics& FrameMetrics, const ICaptureProtocolHost& Host);
	virtual void Tick();
	virtual void Finalize();
	/**~ End IMovieSceneCaptureProtocol Implementation */

protected:

	/**
	 * Retrieve an arbitrary set of data that relates to the specified frame metrics.
	 * This data will be passed through the capture pipeline, and will be accessible from ProcessFrame
	 *
	 * @param FrameMetrics	Metrics specific to the current frame
	 * @param Host			The host that is managing this protocol

	 * @return Shared pointer to a payload to associate with the frame, or nullptr
	 */
	virtual FFramePayloadPtr GetFramePayload(const FFrameMetrics& FrameMetrics, const ICaptureProtocolHost& Host) = 0;

	/**
	 * Process a captured frame. This may be called on any thread.
	 *
	 * @param Frame			The captured frame data, including any payload retrieved from GetFramePayload
	 */
	virtual void ProcessFrame(FCapturedFrameData Frame) = 0;

private:

	/** The frame grabber, responsible for actually capturing frames */
	TUniquePtr<FFrameGrabber> FrameGrabber;
};
