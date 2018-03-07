// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneCaptureProtocolSettings.h"
#include "HAL/Runnable.h"
#include "HAL/ThreadSafeBool.h"

#if WITH_EDITOR
	#include "IImageWrapper.h"
#endif

#include "FrameGrabber.h"
#include "Protocols/FrameGrabberProtocol.h"
#include "ImageSequenceProtocol.generated.h"

struct FMovieSceneCaptureSettings;

UCLASS(config=EditorPerProjectUserSettings, DisplayName="Image Encoding")
class MOVIESCENECAPTURE_API UBmpImageCaptureSettings : public UMovieSceneCaptureProtocolSettings
{
public:
	UBmpImageCaptureSettings(const FObjectInitializer& Init) : UMovieSceneCaptureProtocolSettings(Init) {}

	GENERATED_BODY()

	/**~ UMovieSceneCaptureProtocolSettings implementation */
	virtual void OnReleaseConfig(FMovieSceneCaptureSettings& InSettings) override;
	virtual void OnLoadConfig(FMovieSceneCaptureSettings& InSettings) override;
};

UCLASS(config=EditorPerProjectUserSettings, DisplayName="Image Encoding")
class MOVIESCENECAPTURE_API UImageCaptureSettings : public UFrameGrabberProtocolSettings
{
public:
	UImageCaptureSettings(const FObjectInitializer& Init) : UFrameGrabberProtocolSettings(Init), CompressionQuality(100) {}

	GENERATED_BODY()

	/**~ UMovieSceneCaptureProtocolSettings implementation */
	virtual void OnReleaseConfig(FMovieSceneCaptureSettings& InSettings) override;
	virtual void OnLoadConfig(FMovieSceneCaptureSettings& InSettings) override;

	/** Level of compression to apply to the image, between 1 (worst quality, best compression) and 100 (best quality, worst compression)*/
	UPROPERTY(config, EditAnywhere, Category=ImageSettings, meta=(ClampMin=1, ClampMax=100))
	int32 CompressionQuality;
};

#if WITH_EDITOR

/** Single runnable thread used to dispatch captured frames */
struct FImageCaptureThread : public FRunnable
{
	FImageCaptureThread(EImageFormat InFormat, int32 InCompressionQuality);
	~FImageCaptureThread();

	void Add(FCapturedFrameData Frame);
	uint32 GetNumOutstandingFrames() const;
	void Close();

	virtual uint32 Run();
	virtual void Stop() override;

private:

	void WriteFrameToDisk(FCapturedFrameData& Frame, TSharedPtr<IImageWrapper> ImageWrapper) const;

private:
	/** The thread itself */
	FRunnableThread* Thread;
	/** Command stack on which commands are pushed */
	mutable FCriticalSection CommandMutex;
	TArray<FCapturedFrameData> CapturedFrames;
	/** Event that is triggered when we have something to do */
	FEvent* WorkToDoEvent;
	/** Event that is triggered when we've finished processing outstanding frames */
	FEvent* ThreadEmptyEvent;
	/** Set to false when the thread should terminate */
	FThreadSafeBool bRunning;
	/** The format we are writing out */
	EImageFormat Format;
	/** Level of compression to apply to the image, between 1 (worst quality, best compression) and 100 (best quality, worst compression) */
	int32 CompressionQuality;
	/** array of image writer pointers for async writing */
	TArray<TSharedPtr<IImageWrapper>> ImageWrappers;
};

struct MOVIESCENECAPTURE_API FImageSequenceProtocol : FFrameGrabberProtocol
{
	FImageSequenceProtocol(EImageFormat InFormat);

	/** ~FFrameGrabberProtocol implementation */
	virtual bool Initialize(const FCaptureProtocolInitSettings& InSettings, const ICaptureProtocolHost& Host) override;
	virtual FFramePayloadPtr GetFramePayload(const FFrameMetrics& FrameMetrics, const ICaptureProtocolHost& Host);
	virtual void ProcessFrame(FCapturedFrameData Frame);
	virtual void AddFormatMappings(TMap<FString, FStringFormatArg>& FormatMappings) const override;
	virtual void Finalize() override;
	virtual bool HasFinishedProcessing() const override;
	/** ~End FFrameGrabberProtocol implementation */

private:

	/** Custom string format arguments for filenames */
	TMap<FString, FStringFormatArg> StringFormatMap;
	/** Level of compression to apply to the image, between 1 (worst quality, best compression) and 100 (best quality, worst compression)*/
	int32 CompressionQuality;
	/** The format of the image to write out */
	EImageFormat Format;
	/** Thread responsible for writing out frames to disk */
	TUniquePtr<FImageCaptureThread> CaptureThread;
};

#endif
