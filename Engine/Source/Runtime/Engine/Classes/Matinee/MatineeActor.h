// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//  AActor  used to controll matinee's and to replicate activation, playback, and other relevant flags to net clients

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "MatineeActor.generated.h"

class APlayerController;
class UActorChannel;
class UInterpGroupInst;

/** Signature of function to handle a matinee event track key */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMatineeEvent);


/** Helper struct for storing the camera world-position for each camera cut in the cinematic. */
USTRUCT()
struct FCameraCutInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FVector Location;

	UPROPERTY()
	float TimeStamp;


	FCameraCutInfo()
		: Location(ForceInit)
		, TimeStamp(0)
	{
	}

};

/**
 * A group and all the actors controlled by the group
 */
USTRUCT()
struct FInterpGroupActorInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=InterpGroupActorInfo)
	FName ObjectName;

	UPROPERTY(EditAnywhere, Category=InterpGroupActorInfo)
	TArray<class AActor*> Actors;

};

UCLASS(NotBlueprintable, hidecategories=(Collision, Game, Input), showcategories=("Input|MouseInput", "Input|TouchInput", "Game|Damage"))
class ENGINE_API AMatineeActor : public AActor
{
	GENERATED_UCLASS_BODY()

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;

	/** The matinee data used by this actor*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MatineeActor, replicated, meta=(ForceRebuildProperty = "GroupActorInfos"))
	class UInterpData* MatineeData;

	/** Name of controller node in level script, used to know what function to try and find for events */
	UPROPERTY()
	FName MatineeControllerName;

	/** Time multiplier for playback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, replicated, Category=Play)
	float PlayRate;

	/** If true, the matinee will play when the level is loaded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Play)
	uint32 bPlayOnLevelLoad:1;

	/** Lets you force the sequence to always start at ForceStartPosition */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category=Play)
	uint32 bForceStartPos:1;

	/** Time position to always start at if bForceStartPos is set to true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category=Play)
	float ForceStartPosition;

	/**
	 *	If sequence should pop back to beginning when finished.
	 *	Note, if true, will never get Completed/Reversed events - sequence must be explicitly Stopped.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, replicated, Category=Play)
	uint32 bLooping:1;

	/** If true, sequence will rewind itself back to the start each time the Play input is activated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Rewind)
	uint32 bRewindOnPlay:1;

	/**
	 *	If true, when rewinding this interpolation, reset the 'initial positions' of any RelateToInitial movements to the current location.
	 *	This allows the next loop of movement to proceed from the current locations.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Rewind)
	uint32 bNoResetOnRewind:1;

	/**
	 *	Only used if bRewindOnPlay if true. Defines what should happen if the Play input is activated while currently playing.
	 *	If true, hitting Play while currently playing will pop the position back to the start and begin playback over again.
	 *	If false, hitting Play while currently playing will do nothing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Rewind)
	uint32 bRewindIfAlreadyPlaying:1;

	/** If true, disables the realtime radio effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category=MatineeActor)
	uint32 bDisableRadioFilter:1;

	/** Indicates that this interpolation does not affect gameplay. This means that:
	 * -it is not replicated via MatineeActor
	 * -it is not ticked if no affected Actors are visible
	 * -on dedicated servers, it is completely ignored
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category=MatineeActor)
	uint32 bClientSideOnly:1;

	/** if bClientSideOnly is true, whether this matinee should be completely skipped if none of the affected Actors are visible */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Play)
	uint32 bSkipUpdateIfNotVisible:1;

	/** Lets you skip the matinee with the CANCELMATINEE exec command. Triggers all events to the end along the way. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Play)
	uint32 bIsSkippable:1;

	/** Preferred local viewport number (when split screen is active) the director track should associate with, or zero for 'all'. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category=MatineeActor)
	int32 PreferredSplitScreenNum;

	/** Disable Input from player during play */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Cinematic)
	uint32 bDisableMovementInput:1;

	/** Disable LookAt Input from player during play */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Cinematic)
	uint32 bDisableLookAtInput:1;

	/** Hide Player Pawn during play */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Cinematic)
	uint32 bHidePlayer:1;

	/** Hide HUD during play */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Cinematic)
	uint32 bHideHud:1;

	/** @todo UE4 matinee - shouldnt be directly editable.  Needs a nice interface in matinee */
	UPROPERTY(replicated)
	TArray<struct FInterpGroupActorInfo> GroupActorInfos;

	/** Cached value that indicates whether or not gore was enabled when the sequence was started */
	UPROPERTY(transient)
	uint32 bShouldShowGore:1;

	/** Instance data for interp groups. One for each variable/group combination. */
	UPROPERTY(transient)
	TArray<class UInterpGroupInst*> GroupInst;

	/** Contains the camera world-position for each camera cut in the cinematic. */
	UPROPERTY(transient)
	TArray<struct FCameraCutInfo> CameraCuts;

#if WITH_EDITORONLY_DATA

	// Reference to the actor sprite
private:
	UPROPERTY()
	class UBillboardComponent* SpriteComponent;
public:

	UPROPERTY(transient)
	uint32 bIsBeingEdited:1;

	/** Set by the editor when scrubbing data */
	UPROPERTY(transient)
	uint32 bIsScrubbing : 1;

#endif // WITH_EDITORONLY_DATA

	/** properties that may change on InterpAction that we need to notify clients about, since the object's properties will not be replicated */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, replicated, transient, Category = Play)
	uint32 bIsPlaying:1;

	UPROPERTY(replicated)
	uint32 bReversePlayback:1;

	UPROPERTY(replicated,transient)
	uint32 bPaused:1;

	// The below property is deprecated and will be removed in 4.9.
	UPROPERTY(replicated,transient)
	uint32 bPendingStop:1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, replicated, Category = Play)
	float InterpPosition;

	/** This is just optimization flag to skip checking it again. If all is initialized, it will set this to be true **/

	/** How much error is tolerated in the client-side position before the position that the server replicated is applied */
	float ClientSidePositionErrorTolerance;

private:

	/** Counter to indicate that play count has changed. Used to work around single frames that go from play-stop-play where bIsPlaying won't get replicated. */
	UPROPERTY(replicated)
	uint8 ReplicationForceIsPlaying;

	/** Special flag to ignore internal matinee actor selection*/
	static uint8 IgnoreActorSelectionCount;

public:

	/** Increment the count to ignore internal matinee actor selection */
	static void PushIgnoreActorSelection();

	/** Decrement the count to ignore internal matinee actor selection */
	static void PopIgnoreActorSelection();

	/** @return Should we ignore internal matinee actor selection? */
	static bool IgnoreActorSelection();

	/**
	 * Check if we should perform a network positional update of this matinee
	 * to make sure it's in sync even if it hasn't had significant changes
	 * because it's really important (e.g. a player is standing on it or being controlled by it)
	 */
	virtual void CheckPriorityRefresh();

	/**
	 * Begin playback of the matinee. Only called in game.
	 * Will then advance Position by (PlayRate * Deltatime) each time the matinee is ticked.
	 */
	UFUNCTION(BlueprintCallable, Category="Cinematic")
	virtual void Play();

	/** Stops playback at the current position */
	UFUNCTION(BlueprintCallable, Category="Cinematic")
	virtual void Stop();

	/** Similar to play, but the playback will go backwards until the beginning of the sequence is reached. */
	UFUNCTION(BlueprintCallable, Category="Cinematic")
	virtual void Reverse();

	/** Hold playback at its current position. Calling Pause again will continue playback in its current direction. */
	UFUNCTION(BlueprintCallable, Category="Cinematic")
	virtual void Pause();

	/** 
	 * Set the position of the interpolation.
	 * @note if the interpolation is not currently active, this function doesn't send any Kismet events
	 * @param NewPosition the new position to set the interpolation to
	 * @param bJump if true, teleport to the new position (don't trigger any events between the old and new positions, etc)
	 */
	UFUNCTION(BlueprintCallable, Category="Cinematic")
	void SetPosition(float NewPosition, bool bJump = false);

	/** Changes the direction of playback (go in reverse if it was going forward, or vice versa) */
	UFUNCTION(BlueprintCallable, Category="Cinematic")
	virtual void ChangePlaybackDirection();

	/** Change the looping behaviour of this matinee */
	UFUNCTION(BlueprintCallable, Category="Cinematic")
	virtual void SetLoopingState(bool bNewLooping);

#if WITH_EDITOR
	/** Fix up our references to any objects that have been replaced (e.g. through blueprint compiling) */
	void OnObjectsReplaced(const TMap<UObject*,UObject*>& ReplacementMap);
#endif //WITH_EDITOR

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UFUNCTION(BlueprintCallable, Category = "Cinematic")
	void EnableGroupByName(FString GroupName, bool bEnable);

	/** Event triggered when the matinee is played for whatever reason */
	UPROPERTY(BlueprintAssignable, Category = "Cinematic")
	FOnMatineeEvent OnPlay;

	/** Event triggered when the matinee is stopped for whatever reason */
	UPROPERTY(BlueprintAssignable, Category = "Cinematic")
	FOnMatineeEvent OnStop;

	/** Event triggered when the matinee is paused for whatever reason */
	UPROPERTY(BlueprintAssignable, Category = "Cinematic")
	FOnMatineeEvent OnPause;
protected:

	/** Handle for efficient management of CheckPriorityRefresh timer */
	FTimerHandle TimerHandle_CheckPriorityRefresh;

private:

#if WITH_EDITORONLY_DATA
	/** Helper type for storing actors' World-space locations/rotations. */
	struct FSavedTransform
	{
		FVector		Translation;
		FRotator	Rotation;

		FSavedTransform()
		: Translation(ForceInit)
		, Rotation(ForceInit)
		{}

		friend FArchive& operator<<(FArchive& Ar,FSavedTransform& MySavedTransform)
		{
			return Ar << MySavedTransform.Translation << MySavedTransform.Rotation;
		}
	};

	/** A map from actors to their pre-Matinee world-space positions/orientations.  Includes actors attached to Matinee-affected actors. */
	TMap<TWeakObjectPtr<AActor>,FSavedTransform> SavedActorTransforms;

	/** A map from actors to their pre-Matinee visibility state */
	TMap<TWeakObjectPtr<AActor>,uint8> SavedActorVisibilities;
#endif // WITH_EDITORONLY_DATA

public:

#if WITH_EDITOR
	enum EActorAddWarningType
	{
		ActorAddOk,					// The actor is valid to add
		ActorAddWarningSameLevel,	// The actor should be the same level as the matinee actor
		ActorAddWarningStatic,		// The actor is static
		ActorAddWarningGroup		// The actor is can't be added to the group
	};
#endif

	//~ Begin AActor Interface
	virtual float GetNetPriority(const FVector& ViewPos, const FVector& ViewDir, AActor* Viewer, AActor* ViewTarget, UActorChannel* InChannel, float Time, bool bLowBandwidth) override;
	virtual void Tick(float DeltaTime) override;
	virtual void PreNetReceive() override;
	virtual void PostNetReceive() override;
	virtual void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift) override;
	virtual void PostLoad() override;

protected:
	virtual void BeginPlay() override;

public:

#if WITH_EDITOR
	virtual bool GetReferencedContentObjects( TArray<UObject*>& Objects ) const override;
#endif
	//~ Begin AActor Interface

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent ) override; 
	virtual bool CanEditChange( const UProperty* Property ) const override;
#endif // WITH_EDITOR
	//~ End UObject Interface

	/** Increment track forwards by given timestep and iterate over each track updating any properties. */
	void StepInterp(float DeltaTime, bool bPreview=false);

	/** Move interpolation to new position and iterate over each track updating any properties. */
	void UpdateInterp(float NewPosition, bool bPreview=false, bool bJump=false);

	/** For each InterGroup/Actor combination, create a UInterpGroupInst, assign  AActor  and initialise each track. */
	void InitInterp();

	/** Destroy all UInterpGroupInst. */
	void TermInterp();

	/** Scan the matinee data for camera cuts and sets up the CameraCut array. */
	void SetupCameraCuts();

	/** Used when setting up the camera cuts to make sure the parent is updated. */
	void UpdateInterpForParentMovementTracks( float Time, UInterpGroupInst* ViewGroupInst );

	/** Disable the radio filter effect if "Disable Radio Filter" is checked. */
	void DisableRadioFilterIfNeeded();

	/** Enable the radio filter */
	void EnableRadioFilter();

	/** Enable Cinematic Mode **/
	void EnableCinematicMode(bool bEnable);

	/** 
	 *	Get all Actors currently being used on by this Matinee actor. Only returns valid array while matinee is active.
	 *	If bMovementTrackOnly is set, Actors must have a Movement track in their group to be included in the results.
	 */
	void GetAffectedActors( TArray<AActor*>& OutActors, bool bMovementTrackOnly );

	/** Get all actors controlled by this matinee. Iterates over GroupActorInfos and finds all referenced actors, regardless of matinee state. */
	void GetControlledActors(TArray<AActor*>& OutActors);

	/**
	 *	Update the streaming system with the camera locations for the upcoming camera cuts, so
	 *	that it can start streaming in textures for those locations now.
	 *
	 *	@param	CurrentTime		Current time within the matinee, in seconds
	 *	@param	bPreview		If we are previewing sequence (ie. viewing in editor without gameplay running)
	 */
	void UpdateStreamingForCameraCuts(float CurrentTime, bool bPreview=false);

	/** Called when the level that contains this sequence object is being removed/unloaded */
	void CleanUp();

	/**
	 * Check to see if this Matinee should be associated with the specified player.  This is a relatively
	 * quick test to perform.
	 *
	 * @param InPC The player controller to check
	 *
	 * @return true if this Matinee sequence is compatible with the specified player
	 */
	bool IsMatineeCompatibleWithPlayer( APlayerController* InPC ) const;

	/** @return a group instance referring to the supplied Actor. Returns NULL if no group can be found. */
	UInterpGroupInst* FindGroupInst(const AActor* InActor) const;



#if WITH_EDITOR

	/** Ensure that the group to actor mapping is consistent with the interp data groups */
	void EnsureActorGroupConsistency();

	/** Validate All Actors in the group **/
	void ValidateActorGroups();

	/** Replace Group Actors */
	void ReplaceActorGroupInfo(class UInterpGroup * Group, AActor* OldActor, AActor* NewActor);

	/** Delete Group Actors */
	void DeleteActorGroupInfo(class UInterpGroup * Group, AActor* ActorToDelete);

	/** Rename groupinfo **/
	void DeleteGroupinfo(class UInterpGroup * GroupToDelete);

	/** Save whether or not the passed in actor is hidden so we can restore it later */
	void SaveActorVisibility( AActor*  AActor  );

	/** return if this actor is valid to add **/
	EActorAddWarningType IsValidActorToAdd(const AActor* NewActor) const;

	/**
	 * Conditionally save state for the specified actor and its children
	 */
	void ConditionallySaveActorState( UInterpGroupInst* GroupInst, AActor*  AActor  );

	/**
	 * Add the specified actor and any actors attached to it to the list
	 * of saved actor transforms.  Does nothing if an actor has already
	 * been saved.
	 */
	void SaveActorTransforms( AActor*  AActor  );

	/**
	 * Apply the saved locations and rotations to all saved actors.
	 */
	void RestoreActorTransforms();
	
	/** Apply the saved visibility state for all saved actors */
	void RestoreActorVisibilities();

	/**
	 * Store the current scrub position, restores all saved actor transforms,
	 * then saves off the transforms for actors referenced (directly or indirectly)
	 * by group instances, and finally restores the scrub position.
	 */
	void RecaptureActorState();

	/** Set up the group actor for the specified UInterpGroup. */
	void InitGroupActorForGroup(class UInterpGroup* InGroup, class AActor* GroupActor);
#endif

	/** Find the first group instance based on the given UInterpGroup. */
	UInterpGroupInst* FindFirstGroupInst(class UInterpGroup* InGroup);

	/** Find the first group instance based on the UInterpGroup with the given name. */
	UInterpGroupInst* FindFirstGroupInstByName(const FString& InGroupName);

	/** 
	 * If there is DirectorGroup, use it to find the viewed group at the current position, 
	 * then the first instance of that group, and the  AActor  it is bound to. 
	 */
	AActor* FindViewedActor();

	
	void AddPlayerToDirectorTracks( class APlayerController* PC );

	/** called by InterpAction when significant changes occur. Updates replicated data. */
	virtual void UpdateReplicatedData( bool bIsBeginningPlay );
	

	/** Try to invoke the event with the given name in the level script */
	virtual void NotifyEventTriggered(FName EventName, float EventTime, bool bUseCustomEventName=false);

	/** Util to get the name of the function to find for the given event name */
	FName GetFunctionNameForEvent(FName EventName,bool bUseCustomEventName=false);

public:
#if WITH_EDITORONLY_DATA
	/** Returns SpriteComponent subobject **/
	class UBillboardComponent* GetSpriteComponent() const;
#endif
};



