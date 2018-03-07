// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sections/MovieSceneAnimationSectionRecorder.h"
#include "LevelSequence.h"
#include "Sections/MovieScene3DTransformSectionRecorder.h"
#include "ISequenceRecorder.h"
#include "Sections/MovieSceneMultiPropertyRecorder.h"

class APlayerController;
class ISequenceAudioRecorder;
class UCanvas;
class UTexture;

struct FSequenceRecorder
{
public:
	static FName MovieScenePropertyRecorderFactoryName;

	/** Singleton accessor */
	static FSequenceRecorder& Get();

	/** Initialize any resources we need */
	void Initialize();

	/** Clear any resources we need */
	void Shutdown();

	/** Starts recording a sequence. */
	bool StartRecording(const FOnRecordingStarted& OnRecordingStarted = FOnRecordingStarted(), const FOnRecordingFinished& OnRecordingFinished = FOnRecordingFinished(), const FString& InPathToRecordTo = FString(), const FString& InSequenceName = FString());

	/** Starts recording a sequence for the specified world. */
	bool StartRecordingForReplay(UWorld* World, const struct FSequenceRecorderActorFilter& ActorFilter);

	/** Stops any currently recording sequence */
	bool StopRecording();

	/** Tick the sequence recorder */
	void Tick(float DeltaSeconds);

	/** Get whether we are currently delaying a recording */
	bool IsDelaying() const;

	/** Get the current delay we are waiting for */
	float GetCurrentDelay() const;

	TWeakObjectPtr<class ULevelSequence> GetCurrentSequence() { return CurrentSequence; }

	bool IsRecordingQueued(AActor* Actor) const;

	class UActorRecording* FindRecording(AActor* Actor) const;

	void StartAllQueuedRecordings();

	void StopAllQueuedRecordings();

	void StopRecordingDeadAnimations();

	class UActorRecording* AddNewQueuedRecording(AActor* Actor = nullptr, UAnimSequence* AnimSequence = nullptr, float Length = 0.0f);

	void RemoveQueuedRecording(AActor* Actor);

	void RemoveQueuedRecording(class UActorRecording* Recording);

	bool HasQueuedRecordings() const;

	void ClearQueuedRecordings();

	const TArray<class UActorRecording*>& GetQueuedRecordings() { return QueuedRecordings; }

	bool AreQueuedRecordingsDirty() const { return bQueuedRecordingsDirty; }

	void ResetQueuedRecordingsDirty() { bQueuedRecordingsDirty = false; }

	bool IsRecording() const { return CurrentSequence.IsValid(); }

	/** Draw the countdown to the screen */
	void DrawDebug(UCanvas* InCanvas, APlayerController* InPlayerController);

	/** Handle actors being spawned */
	static void HandleActorSpawned(AActor* Actor);

	/** Handle actors being de-spawned */
	void HandleActorDespawned(AActor* Actor);

	/** Get the built-in animation factory (as this uses special case handling) */
	const FMovieSceneAnimationSectionRecorderFactory& GetAnimationRecorderFactory() const
	{
		return AnimationSectionRecorderFactory;
	}

	/** Get the built-in transform factory (as this uses special case handling) */
	const FMovieScene3DTransformSectionRecorderFactory& GetTransformRecorderFactory() const
	{
		return TransformSectionRecorderFactory;
	}

	/** Get the name of the next sequence we are targeting */
	const FString& GetNextSequenceName() const { return NextSequenceName; }

	/** Refresh the name of the next sequence we will be recording */
	void RefreshNextSequence();

private:
	/** Starts recording a sequence, possibly delayed */
	bool StartRecordingInternal(UWorld* World);

	/** Check if an actor is valid for recording */
	bool IsActorValidForRecording(AActor* Actor);

	/** Handle exiting cleanly from PIE */
	void HandleEndPIE(bool bSimulating);

	/** Keep sequence range up to date with sections that are being recorded */
	void UpdateSequencePlaybackRange();

private:
	/** Constructor, private - use Get() function */
	FSequenceRecorder();

	/** Currently recording level sequence, if any */
	TWeakObjectPtr<class ULevelSequence> CurrentSequence;

	/** World we are recording a replay for, if any */
	TLazyObjectPtr<class UWorld> CurrentReplayWorld;

	TArray<class UActorRecording*> QueuedRecordings;

	TArray<class UActorRecording*> DeadRecordings;

	TArray<TWeakObjectPtr<AActor>> QueuedSpawnedActors;

	bool bQueuedRecordingsDirty;

	bool bWasImmersive;

	/** The delay we are currently waiting for */
	float CurrentDelay;

	/** Current recording time */
	float CurrentTime;

	/** Delegate handles for FOnActorSpawned events */
	TMap<TWeakObjectPtr<UWorld>, FDelegateHandle> ActorSpawningDelegateHandles;

	/** Texture we use for the countdown */
	TWeakObjectPtr<UTexture> CountdownTexture;

	/** Texture we use for the recording indicator */
	TWeakObjectPtr<UTexture> RecordingIndicatorTexture;

	/** Delegate fired when recording is started */
	FOnRecordingStarted OnRecordingStartedDelegate;

	/** Delegate fired when recording has finished */
	FOnRecordingFinished OnRecordingFinishedDelegate;

	/** Cached sequence name to record to */
	FString SequenceName;

	/** The next sequence we will be targeting. Name can change depending on assets being deleted, moved, renamed etc. */
	FString NextSequenceName;

	/** Cached sequence path to record to */
	FString PathToRecordTo;

	/** Built-in animation recorder factory */
	FMovieSceneAnimationSectionRecorderFactory AnimationSectionRecorderFactory;

	/** Built-in transform recorder factory */
	FMovieScene3DTransformSectionRecorderFactory TransformSectionRecorderFactory;

	/** Audio recorder */
	TUniquePtr<ISequenceAudioRecorder> AudioRecorder;

	/** Built in multi-property recorder */
	FMovieSceneMultiPropertyRecorderFactory MultiPropertySectionRecorder;
};
