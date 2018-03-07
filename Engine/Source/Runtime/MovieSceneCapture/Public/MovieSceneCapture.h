// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "IMovieSceneCaptureProtocol.h"
#include "MovieSceneCaptureHandle.h"
#include "MovieSceneCaptureSettings.h"
#include "IMovieSceneCapture.h"
#include "MovieSceneCaptureProtocolRegistry.h"
#include "Scalability.h"
#include "MovieSceneCapture.generated.h"

class FJsonObject;
class FSceneViewport;

/** Structure used to cache various metrics for our capture */
struct FCachedMetrics
{
	FCachedMetrics() : Width(0), Height(0), Frame(0), ElapsedSeconds(0.f) {}

	/** The width/Height of the frame */
	int32 Width, Height;
	/** The current frame number */
	int32 Frame;
	/** The number of seconds that have elapsed */
	float ElapsedSeconds;
};

/** Class responsible for capturing scene data */
UCLASS(config=EditorPerProjectUserSettings)
class MOVIESCENECAPTURE_API UMovieSceneCapture : public UObject, public IMovieSceneCaptureInterface, public ICaptureProtocolHost
{
public:
	UMovieSceneCapture(const FObjectInitializer& Initializer);

	GENERATED_BODY()

	virtual void PostInitProperties() override;

public:

	// Begin IMovieSceneCaptureInterface
	virtual void Initialize(TSharedPtr<FSceneViewport> InSceneViewport, int32 PIEInstance = -1) override;
	virtual void StartCapturing() { StartCapture(); }
	virtual void Close() override { Finalize(); }
	virtual FMovieSceneCaptureHandle GetHandle() const override { return Handle; }
	const FMovieSceneCaptureSettings& GetSettings() const override { return Settings; }
	// End IMovieSceneCaptureInterface

	/** Load save from config helpers */
	virtual void LoadFromConfig();
	virtual void SaveToConfig();

	/** Serialize additional json data for this capture */
	void SerializeJson(FJsonObject& Object);

	/** Deserialize additional json data for this capture */
	void DeserializeJson(const FJsonObject& Object);

protected:

	/** Custom, additional json serialization */
	virtual void SerializeAdditionalJson(FJsonObject& Object){}

	/** Custom, additional json deserialization */
	virtual void DeserializeAdditionalJson(const FJsonObject& Object){}

public:

	/** The type of capture protocol to use */
	UPROPERTY(config, EditAnywhere, Category=CaptureSettings, DisplayName="Output Format")
	FCaptureProtocolID CaptureType;

	/** Settings specific to the capture protocol */
	UPROPERTY(EditAnywhere, Category=CaptureSettings)
	UMovieSceneCaptureProtocolSettings* ProtocolSettings;

	/** Settings that define how to capture */
	UPROPERTY(EditAnywhere, config, Category=CaptureSettings, meta=(ShowOnlyInnerProperties))
	FMovieSceneCaptureSettings Settings;

	/** Whether to capture the movie in a separate process or not */
	UPROPERTY(config, EditAnywhere, Category=General, AdvancedDisplay)
	bool bUseSeparateProcess;

	/** When enabled, the editor will shutdown when the capture starts */
	UPROPERTY(EditAnywhere, config, Category=General, AdvancedDisplay, meta=(EditCondition=bUseSeparateProcess))
	bool bCloseEditorWhenCaptureStarts;

	/** Additional command line arguments to pass to the external process when capturing */
	UPROPERTY(EditAnywhere, config, Category=General, AdvancedDisplay, meta=(EditCondition=bUseSeparateProcess))
	FString AdditionalCommandLineArguments;

	/** Command line arguments inherited from this process */
	UPROPERTY(EditAnywhere, transient, Category=General, AdvancedDisplay, meta=(EditCondition=bUseSeparateProcess))
	FString InheritedCommandLineArguments;

	/** Event that is fired after we've finished capturing */
	DECLARE_EVENT( UMovieSceneCapture, FOnCaptureFinished );
	FOnCaptureFinished& OnCaptureFinished() { return OnCaptureFinishedDelegate; }

public:

	/** Access this object's cached metrics */
	const FCachedMetrics& GetMetrics() const { return CachedMetrics; }

	/** Access the capture protocol we are using */
	IMovieSceneCaptureProtocol* GetCaptureProtocol() { return CaptureProtocol.Get(); }

public:

	/** Starts warming up.  May be optionally called before StartCapture().  This can be used to start rendering frames early, before
	    any files are captured or written out */
	void StartWarmup();

	/** Initialize the capture so that it is able to start capturing frames */
	void StartCapture();

	/** Indicate that this frame should be captured - must be called before the movie scene capture is ticked */
	void CaptureThisFrame(float DeltaSeconds);

	/** Automatically finalizes the capture when all currently pending frames are dealt with */
	void FinalizeWhenReady();

	/** Check whether we should automatically finalize this capture */
	bool ShouldFinalize() const { return bFinalizeWhenReady && CaptureProtocol->HasFinishedProcessing(); }

	/** Finalize the capturing process, assumes all frames have been processed. */
	void Finalize();

public:

	/** Called at the end of a frame, before a frame is presented by slate */
	virtual void Tick(float DeltaSeconds) { CaptureThisFrame(DeltaSeconds); }

protected:

	/**~ ICaptureProtocolHost interface */
	virtual FString GenerateFilename(const FFrameMetrics& FrameMetrics, const TCHAR* Extension) const override;
	virtual void EnsureFileWritable(const FString& File) const override;
	virtual float GetCaptureFrequency() const { return Settings.FrameRate; }
	virtual const ICaptureStrategy& GetCaptureStrategy() const { return *CaptureStrategy; }

	/** Add additional format mappings to be used when generating filenames */
	virtual void AddFormatMappings(TMap<FString, FStringFormatArg>& OutFormatMappings, const FFrameMetrics& FrameMetrics) const {}

	/** Resolve the specified format using the user supplied formatting rules. */
	FString ResolveFileFormat(const FString& Format, const FFrameMetrics& FrameMetrics) const;

	/** Initialize the settings structure for the current capture type */
	void InitializeSettings();

protected:

#if WITH_EDITOR
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	/** Capture protocol responsible for actually capturing frame data */
	TSharedPtr<IMovieSceneCaptureProtocol> CaptureProtocol;
	/** Strategy used for capture (real-time/fixed-time-step) */
	TSharedPtr<ICaptureStrategy> CaptureStrategy;
	/** The settings we will use to set up the capture protocol */
	TOptional<FCaptureProtocolInitSettings> InitSettings;
	/** Whether we should automatically attempt to capture frames every tick or not */
	bool bFinalizeWhenReady;
	/** Our unique handle, used for external representation without having to link to the MovieSceneCapture module */
	FMovieSceneCaptureHandle Handle;
	/** Cached metrics for this capture operation */
	FCachedMetrics CachedMetrics;
	/** Format mappings used for generating filenames */
	TMap<FString, FStringFormatArg> FormatMappings;
	/** The number of frames to capture.  If this is zero, we'll capture the entire sequence. */
	int32 FrameCount;
	/** Whether we have started capturing or not */
	bool bCapturing;
	/** Frame number index offset when saving out frames.  This is used to allow the frame numbers on disk to match
	    what they would be in the authoring application, rather than a simple 0-based sequential index */
	int32 FrameNumberOffset;
	/** Event that is triggered when capturing has finished */
	FOnCaptureFinished OnCaptureFinishedDelegate;
	/** Cached quality levels */
	Scalability::FQualityLevels CachedQualityLevels;
};

/** A strategy that employs a fixed frame time-step, and as such never drops a frame. Potentially accelerated. */
struct MOVIESCENECAPTURE_API FFixedTimeStepCaptureStrategy : ICaptureStrategy
{
	FFixedTimeStepCaptureStrategy(uint32 InTargetFPS);

	virtual void OnWarmup() override;
	virtual void OnStart() override;
	virtual void OnStop() override;
	virtual void OnPresent(double CurrentTimeSeconds, uint32 FrameIndex) override;
	virtual bool ShouldPresent(double CurrentTimeSeconds, uint32 FrameIndex) const override;
	virtual int32 GetDroppedFrames(double CurrentTimeSeconds, uint32 FrameIndex) const override;

private:
	uint32 TargetFPS;
};

/** A capture strategy that captures in real-time, potentially dropping frames to maintain a stable constant framerate video. */
struct MOVIESCENECAPTURE_API FRealTimeCaptureStrategy : ICaptureStrategy
{
	FRealTimeCaptureStrategy(uint32 InTargetFPS);

	virtual void OnWarmup() override;
	virtual void OnStart() override;
	virtual void OnStop() override;
	virtual void OnPresent(double CurrentTimeSeconds, uint32 FrameIndex) override;
	virtual bool ShouldSynchronizeFrames() const override { return false; }
	virtual bool ShouldPresent(double CurrentTimeSeconds, uint32 FrameIndex) const override;
	virtual int32 GetDroppedFrames(double CurrentTimeSeconds, uint32 FrameIndex) const override;

private:
	double NextPresentTimeS, FrameLength;
};
