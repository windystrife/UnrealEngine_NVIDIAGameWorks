// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Protocols/FrameGrabberProtocol.h"
#include "Templates/Casts.h"

bool FFrameGrabberProtocol::HasFinishedProcessing() const
{
	return !FrameGrabber->HasOutstandingFrames();
}

bool FFrameGrabberProtocol::Initialize(const FCaptureProtocolInitSettings& InSettings, const ICaptureProtocolHost& Host)
{
	EPixelFormat PixelFormat = PF_B8G8R8A8;
	uint32 RingBufferSize = 3;

	if (UFrameGrabberProtocolSettings* Settings = Cast<UFrameGrabberProtocolSettings>(InSettings.ProtocolSettings))
	{
		PixelFormat = Settings->DesiredPixelFormat;
		RingBufferSize = Settings->RingBufferSize;
	}

	// We'll use our own grabber to capture the entire viewport
	FrameGrabber.Reset(new FFrameGrabber(InSettings.SceneViewport.ToSharedRef(), InSettings.DesiredSize, PixelFormat, RingBufferSize));
	FrameGrabber->StartCapturingFrames();
	return true;
}

void FFrameGrabberProtocol::CaptureFrame(const FFrameMetrics& FrameMetrics, const ICaptureProtocolHost& Host)
{
	FrameGrabber->CaptureThisFrame(GetFramePayload(FrameMetrics, Host));
}

void FFrameGrabberProtocol::Tick()
{
	TArray<FCapturedFrameData> CapturedFrames = FrameGrabber->GetCapturedFrames();

	for (FCapturedFrameData& Frame : CapturedFrames)
	{
		ProcessFrame(MoveTemp(Frame));
	}
}

void FFrameGrabberProtocol::Finalize()
{
	FrameGrabber->Shutdown();
	FrameGrabber.Reset();
}
