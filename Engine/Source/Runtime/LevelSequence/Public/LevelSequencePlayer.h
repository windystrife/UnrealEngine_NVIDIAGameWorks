// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "IMovieScenePlayer.h"
#include "Evaluation/MovieScenePlayback.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Evaluation/PersistentEvaluationData.h"
#include "MovieSceneSequencePlayer.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.generated.h"

class AActor;
class ALevelSequenceActor;
class FLevelSequenceSpawnRegister;
class FViewportClient;
class UCameraComponent;

struct DEPRECATED(4.15, "Please use FMovieSceneSequencePlaybackSettings.") FLevelSequencePlaybackSettings
	: public FMovieSceneSequencePlaybackSettings
{};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLevelSequencePlayerCameraCutEvent, UCameraComponent*, CameraComponent);

USTRUCT(BlueprintType)
struct FLevelSequenceSnapshotSettings
{
	GENERATED_BODY()

	FLevelSequenceSnapshotSettings()
		: ZeroPadAmount(4), FrameRate(30)
	{}

	FLevelSequenceSnapshotSettings(int32 InZeroPadAmount, float InFrameRate)
		: ZeroPadAmount(InZeroPadAmount), FrameRate(InFrameRate)
	{}

	/** Zero pad frames */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	uint8 ZeroPadAmount;

	/** Playback framerate */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	float FrameRate;
};

/**
 * Frame snapshot information for a level sequence
 */
USTRUCT(BlueprintType)
struct FLevelSequencePlayerSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	FText MasterName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	float MasterTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	FText CurrentShotName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	float CurrentShotLocalTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	UCameraComponent* CameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="General")
	FLevelSequenceSnapshotSettings Settings;

	UPROPERTY()
	FMovieSceneSequenceID ShotID;
};

/**
 * ULevelSequencePlayer is used to actually "play" an level sequence asset at runtime.
 *
 * This class keeps track of playback state and provides functions for manipulating
 * an level sequence while its playing.
 */
UCLASS(BlueprintType)
class LEVELSEQUENCE_API ULevelSequencePlayer
	: public UMovieSceneSequencePlayer
{
public:
	ULevelSequencePlayer(const FObjectInitializer&);

	GENERATED_BODY()

	/**
	 * Initialize the player.
	 *
	 * @param InLevelSequence The level sequence to play.
	 * @param InWorld The world that the animation is played in.
	 * @param Settings The desired playback settings
	 */
	void Initialize(ULevelSequence* InLevelSequence, UWorld* InWorld, const FMovieSceneSequencePlaybackSettings& Settings);

public:

	/**
	 * Create a new level sequence player.
	 *
	 * @param WorldContextObject Context object from which to retrieve a UWorld.
	 * @param LevelSequence The level sequence to play.
	 * @param Settings The desired playback settings
	 * @param OutActor The level sequence actor created to play this sequence.
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic", meta=(WorldContext="WorldContextObject", DynamicOutputParam="OutActor"))
	static ULevelSequencePlayer* CreateLevelSequencePlayer(UObject* WorldContextObject, ULevelSequence* LevelSequence, FMovieSceneSequencePlaybackSettings Settings, ALevelSequenceActor*& OutActor);

	/** Set the settings used to capture snapshots with */
	void SetSnapshotSettings(const FLevelSequenceSnapshotSettings& InSettings) { SnapshotSettings = InSettings; }

	/** Event triggered when there is a camera cut */
	UPROPERTY(BlueprintAssignable, Category="Game|Cinematic")
	FOnLevelSequencePlayerCameraCutEvent OnCameraCut;

public:

	/**
	 * Access the level sequence this player is playing
	 * @return the level sequence currently assigned to this player
	 */
	DEPRECATED(4.15, "Please use GetSequence instead.")
	ULevelSequence* GetLevelSequence() const { return Cast<ULevelSequence>(Sequence); }

protected:

	// IMovieScenePlayer interface
	virtual void UpdateCameraCut(UObject* CameraObject, UObject* UnlockIfCameraObject, bool bJumpCut) override;
	virtual void NotifyBindingUpdate(const FGuid& InGuid, FMovieSceneSequenceIDRef InSequenceID, TArrayView<TWeakObjectPtr<>> Objects) override;
	virtual UObject* GetPlaybackContext() const override;
	virtual TArray<UObject*> GetEventContexts() const override;

	//~ UMovieSceneSequencePlayer interface
	virtual bool CanPlay() const override;
	virtual void OnStartedPlaying() override;
	virtual void OnStopped() override;

public:

	/** Populate the specified array with any given event contexts for the specified world */
	static void GetEventContexts(UWorld& InWorld, TArray<UObject*>& OutContexts);

	/**
	 * Set an array of additional actors that will receive events triggerd from this sequence player
	 *
	 * @param AdditionalReceivers An array of actors to receive events
	 */
	void SetEventReceivers(TArray<UObject*>&& AdditionalReceivers) { AdditionalEventReceivers = MoveTemp(AdditionalReceivers); }

	/** Take a snapshot of the current state of this player */
	void TakeFrameSnapshot(FLevelSequencePlayerSnapshot& OutSnapshot) const;

	/** Set the offset time for the snapshot */
	void SetSnapshotOffsetTime(float InTime) {SnapshotOffsetTime = TOptional<float>(InTime); }

private:

	void EnableCinematicMode(bool bEnable);

private:

	/** The world this player will spawn actors in, if needed */
	TWeakObjectPtr<UWorld> World;

	/** The last view target to reset to when updating camera cuts to null */
	TWeakObjectPtr<AActor> LastViewTarget;

	/** The last aspect ratio axis constraint to reset to when the camera cut is null */
	EAspectRatioAxisConstraint LastAspectRatioAxisConstraint;

protected:

	/** How to take snapshots */
	FLevelSequenceSnapshotSettings SnapshotSettings;

	TOptional<float> SnapshotOffsetTime;

	TWeakObjectPtr<UCameraComponent> CachedCameraComponent;

	/** Array of additional event receivers */
	UPROPERTY(transient)
	TArray<UObject*> AdditionalEventReceivers;

	/** Set of actors that have been added as tick prerequisites to the parent actor */
	TSet<FObjectKey> PrerequisiteActors;
};
