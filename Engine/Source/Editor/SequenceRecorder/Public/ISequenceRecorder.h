// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Modules/ModuleInterface.h"
#include "Containers/ArrayView.h"

class AActor;
class ISequenceAudioRecorder;

DECLARE_DELEGATE_OneParam(FOnRecordingStarted, class UMovieSceneSequence* /*Sequence*/);

DECLARE_DELEGATE_OneParam(FOnRecordingFinished, class UMovieSceneSequence* /*Sequence*/);

class ISequenceRecorder : public IModuleInterface
{
public:
	/** 
	 * Start recording the passed-in actors.
	 * @param	World			The world we use to record actors
	 * @param	ActorFilter		Actor filter to gather actors spawned in this world for recording
	 * @return true if recording was successfully started
	 */
	virtual bool StartRecording(UWorld* World, const struct FSequenceRecorderActorFilter& ActorFilter) = 0;

	/** Stop recording current sequence, if any */
	virtual void StopRecording() = 0;

	/** Are we currently recording a sequence */
	virtual bool IsRecording() = 0;

	/** How long is the currently recording sequence */
	virtual float GetCurrentRecordingLength() = 0;

	/**
	 * Start a recording, possibly with some delay (specified by the sequence recording settings).
	 * @param	ActorsToRecord		Actors to record.
	 * @param	OnRecordingStarted	Delegate fired when recording has commenced.
	 * @param	OnRecordingFinished	Delegate fired when recording has finished.
	 * @param	PathToRecordTo		Optional path to a sequence to record to. If none is specified we use the defaults in the settings.
	 * @param	SequenceName		Optional name of a sequence to record to. If none is specified we use the defaults in the settings.
	 * @return true if recording was successfully started
	*/
	virtual bool StartRecording(TArrayView<AActor* const> ActorsToRecord, const FOnRecordingStarted& OnRecordingStarted, const FOnRecordingFinished& OnRecordingFinished, const FString& PathToRecordTo = FString(), const FString& SequenceName = FString()) = 0;

	/**
	 * Start a recording, possibly with some delay (specified by the sequence recording settings).
	 * @param	ActorToRecord		Actor to record.
	 * @param	OnRecordingStarted	Delegate fired when recording has commenced.
	 * @param	OnRecordingFinished	Delegate fired when recording has finished.
	 * @param	PathToRecordTo		Optional path to a sequence to record to. If none is specified we use the defaults in the settings.
	 * @param	SequenceName		Optional name of a sequence to record to. If none is specified we use the defaults in the settings.
	 * @return true if recording was successfully started
	*/
	bool StartRecording(AActor* ActorToRecord, const FOnRecordingStarted& OnRecordingStarted, const FOnRecordingFinished& OnRecordingFinished, const FString& PathToRecordTo = FString(), const FString& SequenceName = FString())
	{
		return StartRecording(MakeArrayView(&ActorToRecord, 1), OnRecordingStarted, OnRecordingFinished, PathToRecordTo, SequenceName);
	}

	/**
	 * Notify we should start recording an actor - usually used for 'actor pooling' implementations
	 * to simulate spawning. Has no effect when recording is not in progress.
	 * @param	Actor	The actor that was 'spawned'
	 */
	virtual void NotifyActorStartRecording(AActor* Actor) = 0;

	/**
	 * Notify we should stop recording an actor - usually used for 'actor pooling' implementations
	 * to simulate de-spawning. Has no effect when recording is not in progress.
	 * @param	Actor	The actor that was 'de-spawned'
	 */
	virtual void NotifyActorStopRecording(AActor* Actor) = 0;

	/**
	 * Get the spawnable Guid in the currently recording movie scene for the specified actor.
	 * @param	Actor	The Actor to check
	 * @return the spawnable Guid
	 */
	virtual FGuid GetRecordingGuid(AActor* Actor) const = 0;

	/**
	 * Register a function that will return a new audio capturer for the specified parameters
	 * @param	FactoryFunction		Function used to generate a new audio recorder
	 * @return A handle to be passed to UnregisterAudioRecorder to unregister the recorder
	 */
	virtual FDelegateHandle RegisterAudioRecorder(const TFunction<TUniquePtr<ISequenceAudioRecorder>()>& FactoryFunction) = 0;

	/**
	 * Unregister a previously registered audio recorder factory function
	 * @param	RegisteredHandle	The handle returned from RegisterAudioRecorder
	 */
	virtual void UnregisterAudioRecorder(FDelegateHandle RegisteredHandle) = 0;

	/**
	 * Check whether we have an audio recorder registered or not
	 * @return true if we have an audio recorder registered, false otherwise
	 */
	virtual bool HasAudioRecorder() const = 0;

	/**
	 * Attempt to create an audio recorder
	 * @param	Settings	Settings for the audio recorder
	 * @return A valid ptr to an audio recorder or null
	 */
	virtual TUniquePtr<ISequenceAudioRecorder> CreateAudioRecorder() const = 0;
};
