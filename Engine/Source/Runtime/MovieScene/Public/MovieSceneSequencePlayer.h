// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "IMovieScenePlayer.h"
#include "MovieScene.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "MovieSceneBindingOverridesInterface.h"
#include "MovieSceneSequencePlayer.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMovieSceneSequencePlayerEvent);

/**
 * Settings for the level sequence player actor.
 */
USTRUCT(BlueprintType)
struct FMovieSceneSequencePlaybackSettings
{
	FMovieSceneSequencePlaybackSettings()
		: LoopCount(0)
		, PlayRate(1.f)
		, bRandomStartTime(false)
		, StartTime(0.f)
		, bRestoreState(false)
		, bDisableMovementInput(false)
		, bDisableLookAtInput(false)
		, bHidePlayer(false)
		, bHideHud(false)
	{ }

	GENERATED_BODY()

	/** Number of times to loop playback. -1 for infinite, else the number of times to loop before stopping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playback", meta=(UIMin=1, DisplayName="Loop"))
	int32 LoopCount;

	/** The rate at which to playback the animation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playback", meta=(Units=Multiplier))
	float PlayRate;

	/** Start playback at a random time */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playback")
	bool bRandomStartTime;

	/** Start playback at the specified time */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playback", meta=(Units=s, EditCondition="!bRandomStartTime"))
	float StartTime;

	/** Flag used to specify whether actor states should be restored on stop */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playback")
	bool bRestoreState;

	/** Disable Input from player during play */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	bool bDisableMovementInput;

	/** Disable LookAt Input from player during play */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	bool bDisableLookAtInput;

	/** Hide Player Pawn during play */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	bool bHidePlayer;

	/** Hide HUD during play */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	bool bHideHud;

	/** Interface that defines overridden bindings for this sequence */
	UPROPERTY()
	TScriptInterface<IMovieSceneBindingOverridesInterface> BindingOverrides;

	MOVIESCENE_API bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FArchive& Ar);
};

template<> struct TStructOpsTypeTraits<FMovieSceneSequencePlaybackSettings> : public TStructOpsTypeTraitsBase2<FMovieSceneSequencePlaybackSettings>
{
	enum { WithCopy = true, WithSerializeFromMismatchedTag = true };
};

/**
 * Abstract class that provides consistent player behaviour for various animation players
 */
UCLASS(Abstract, BlueprintType)
class MOVIESCENE_API UMovieSceneSequencePlayer
	: public UObject
	, public IMovieScenePlayer
{
public:
	GENERATED_BODY()

	UMovieSceneSequencePlayer(const FObjectInitializer&);
	virtual ~UMovieSceneSequencePlayer();

	/** Start playback forwards from the current time cursor position, using the current play rate. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void Play();

	/** Reverse playback. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void PlayReverse();

	/** Changes the direction of playback (go in reverse if it was going forward, or vice versa) */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void ChangePlaybackDirection();

	/**
	 * Start playback from the current time cursor position, looping the specified number of times.
	 * @param NumLoops - The number of loops to play. -1 indicates infinite looping.
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void PlayLooping(int32 NumLoops = -1);

	/** Start playback from the current time cursor position, using the current play rate. Does not update the animation until next tick. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void StartPlayingNextTick();
	
	/** Pause playback. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void Pause();
	
	/** Scrub playback. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void Scrub();

	/** Stop playback. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void Stop();

	/** Go to end and stop. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic", meta = (ToolTip = "Go to end of the sequence and stop. Adheres to 'When Finished' section rules."))
	void GoToEndAndStop();

	/** Get the current playback position */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	float GetPlaybackPosition() const;

	/**
	 * Set the current playback position
	 * @param NewPlaybackPosition - The new playback position to set.
	 * If the animation is currently playing, it will continue to do so from the new position
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void SetPlaybackPosition(float NewPlaybackPosition);

	/**
	 * Jump to new playback position
	 * @param NewPlaybackPosition - The new playback position to set.
	 * This can be used to update sequencer repeatedly, as if in a scrubbing state
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void JumpToPosition(float NewPlaybackPosition);

	/** Check whether the sequence is actively playing. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	bool IsPlaying() const;

	/** Check whether the sequence is paused. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	bool IsPaused() const;

	/** Get the playback length of the sequence */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	float GetLength() const;

	/** Get the playback rate of this player. */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	float GetPlayRate() const;

	/**
	 * Set the playback rate of this player. Negative values will play the animation in reverse.
	 * @param PlayRate - The new rate of playback for the animation.
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void SetPlayRate(float PlayRate);

	/**
	 * Sets the range in time to be played back by this player, overriding the default range stored in the asset
	 *
	 * @param	NewStartTime	The new starting time for playback
	 * @param	NewEndTime		The new ending time for playback.  Must be larger than the start time.
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void SetPlaybackRange( const float NewStartTime, const float NewEndTime );

	/** Get the offset within the level sequence to start playing */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	float GetPlaybackStart() const { return StartTime; }

	/** Get the offset within the level sequence to finish playing */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	float GetPlaybackEnd() const { return EndTime; }
	
	/** An event that is broadcast each time this level sequence player is updated */
	DECLARE_EVENT_ThreeParams( UMovieSceneSequencePlayer, FOnMovieSceneSequencePlayerUpdated, const UMovieSceneSequencePlayer&, float /*current time*/, float /*previous time*/ );
	FOnMovieSceneSequencePlayerUpdated& OnSequenceUpdated() const { return OnMovieSceneSequencePlayerUpdate; }

	/** Event triggered when the level sequence player is played */
	UPROPERTY(BlueprintAssignable, Category="Game|Cinematic")
	FOnMovieSceneSequencePlayerEvent OnPlay;

	/** Event triggered when the level sequence player is played in reverse */
	UPROPERTY(BlueprintAssignable, Category="Game|Cinematic")
	FOnMovieSceneSequencePlayerEvent OnPlayReverse;

	/** Event triggered when the level sequence player is stopped */
	UPROPERTY(BlueprintAssignable, Category="Game|Cinematic")
	FOnMovieSceneSequencePlayerEvent OnStop;

	/** Event triggered when the level sequence player is paused */
	UPROPERTY(BlueprintAssignable, Category="Game|Cinematic")
	FOnMovieSceneSequencePlayerEvent OnPause;

	/** Event triggered when the level sequence player finishes naturally (without explicitly calling stop) */
	UPROPERTY(BlueprintAssignable, Category = "Game|Cinematic")
	FOnMovieSceneSequencePlayerEvent OnFinished;


public:

	/** Retrieve all objects currently bound to the specified binding identifier */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	TArray<UObject*> GetBoundObjects(FMovieSceneObjectBindingID ObjectBinding);

public:

	/** Update based on the specified delta seconds */
	void Update(const float DeltaSeconds);

	/** Initialize this player with a sequence and some settings */
	void Initialize(UMovieSceneSequence* InSequence, const FMovieSceneSequencePlaybackSettings& InSettings);

	/** Begin play called */
	virtual void BeginPlay() {};

public:

	/**
	 * Access the sequence this player is playing
	 * @return the sequence currently assigned to this player
	 */
	UMovieSceneSequence* GetSequence() const { return Sequence; }

protected:

	void PlayInternal();

	void UpdateMovieSceneInstance(FMovieSceneEvaluationRange InRange, TOptional<EMovieScenePlayerStatus::Type> OptionalStatus = TOptional<EMovieScenePlayerStatus::Type>(), bool bHasJumped = false);

	void UpdateTimeCursorPosition(float NewPosition, TOptional<EMovieScenePlayerStatus::Type> OptionalStatus = TOptional<EMovieScenePlayerStatus::Type>());

	bool ShouldStopOrLoop(float NewPosition) const;

	FORCEINLINE float GetSequencePosition() const { return TimeCursorPosition + StartTime; }

protected:

	//~ IMovieScenePlayer interface
	virtual FMovieSceneRootEvaluationTemplateInstance& GetEvaluationTemplate() override { return RootTemplateInstance; }
	virtual EMovieScenePlayerStatus::Type GetPlaybackStatus() const override;
	virtual FMovieSceneSpawnRegister& GetSpawnRegister() override;

	virtual void SetPlaybackStatus(EMovieScenePlayerStatus::Type InPlaybackStatus) override {}
	virtual void SetViewportSettings(const TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) override {}
	virtual void GetViewportSettings(TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) const override {}
	virtual void UpdateCameraCut(UObject* CameraObject, UObject* UnlockIfCameraObject, bool bJumpCut) override {}
	virtual void ResolveBoundObjects(const FGuid& InBindingId, FMovieSceneSequenceID SequenceID, UMovieSceneSequence& Sequence, UObject* ResolutionContext, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const override;
	virtual const IMovieSceneBindingOverridesInterface* GetBindingOverrides() const override { return PlaybackSettings.BindingOverrides ? &*PlaybackSettings.BindingOverrides : nullptr; }

	//~ UObject interface
	virtual void BeginDestroy() override;

protected:

	virtual bool CanPlay() const { return true; }
	virtual void OnStartedPlaying() {}
	virtual void OnLooped() {}
	virtual void OnPaused() {}
	virtual void OnStopped() {}
	
private:

	/** Apply any latent actions which may have accumulated while the sequence was being evaluated */
	void ApplyLatentActions();

protected:

	/** Movie player status. */
	UPROPERTY()
	TEnumAsByte<EMovieScenePlayerStatus::Type> Status;

	/** Whether we're currently playing in reverse. */
	UPROPERTY()
	uint32 bReversePlayback : 1;

	/** True where we're waiting for the first update of the sequence after calling StartPlayingNextTick. */
	UPROPERTY()
	uint32 bPendingFirstUpdate : 1;

	/** Set to true while evaluating to prevent reentrancy */
	bool bIsEvaluating : 1;

	/** The sequence to play back */
	UPROPERTY(transient)
	UMovieSceneSequence* Sequence;

	/** The current time cursor position within the sequence (in seconds) */
	UPROPERTY()
	float TimeCursorPosition;

	/** Time time at which to start playing the sequence (defaults to the lower bound of the sequence's play range) */
	UPROPERTY()
	float StartTime;

	/** Time time at which to end playing the sequence (defaults to the upper bound of the sequence's play range) */
	UPROPERTY()
	float EndTime;

	/** The number of times we have looped in the current playback */
	UPROPERTY(transient)
	int32 CurrentNumLoops;

	enum class ELatentAction
	{
		Stop, Pause
	};

	/** Set of latent actions that are to be performed when the sequence has finished evaluating this frame */
	TArray<ELatentAction> LatentActions;

	/** Specific playback settings for the animation. */
	UPROPERTY()
	FMovieSceneSequencePlaybackSettings PlaybackSettings;

	/** The root template instance we're evaluating */
	FMovieSceneRootEvaluationTemplateInstance RootTemplateInstance;

	/** Play position helper */
	FMovieScenePlaybackPosition PlayPosition;

	TSharedPtr<FMovieSceneSpawnRegister> SpawnRegister;

private:

	/** The event that will be broadcast every time the sequence is updated */
	mutable FOnMovieSceneSequencePlayerUpdated OnMovieSceneSequencePlayerUpdate;

	/** The maximum tick rate prior to playing (used for overriding delta time during playback). */
	TOptional<double> OldMaxTickRate;
};
