// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Animation/AnimTypes.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimMontage.h"
#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimNotifyQueue.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimInstance.generated.h"

// Post Compile Validation requires WITH_EDITOR
#define ANIMINST_PostCompileValidation WITH_EDITOR

class FDebugDisplayInfo;
class IAnimClassInterface;
class UAnimInstance;
class UCanvas;
struct FAnimInstanceProxy;
struct FAnimNode_AssetPlayerBase;
struct FAnimNode_StateMachine;
struct FAnimNode_SubInput;
struct FBakedAnimationStateMachine;

UENUM()
enum class EAnimCurveType : uint8 
{
	AttributeCurve,
	MaterialCurve, 
	MorphTargetCurve, 
	// make sure to update MaxCurve 
	MaxAnimCurveType
};

UENUM()
enum class EMontagePlayReturnType : uint8
{
	//Return value is the length of the montage (in seconds)
	MontageLength,
	//Return value is the play duration of the montage (length / play rate, in seconds)
	Duration,
};

DECLARE_DELEGATE_OneParam(FOnMontageStarted, UAnimMontage*)
DECLARE_DELEGATE_TwoParams(FOnMontageEnded, UAnimMontage*, bool /*bInterrupted*/)
DECLARE_DELEGATE_TwoParams(FOnMontageBlendingOutStarted, UAnimMontage*, bool /*bInterrupted*/)
/**
* Delegate for when Montage is started
*/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMontageStartedMCDelegate, UAnimMontage*, Montage);

/**
* Delegate for when Montage is completed, whether interrupted or finished
* Weight of this montage is 0.f, so it stops contributing to output pose
*
* bInterrupted = true if it was not property finished
*/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMontageEndedMCDelegate, UAnimMontage*, Montage, bool, bInterrupted);

/** Delegate for when all montage instances have ended. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllMontageInstancesEndedMCDelegate);

/**
* Delegate for when Montage started to blend out, whether interrupted or finished
* DesiredWeight of this montage becomes 0.f, but this still contributes to the output pose
*
* bInterrupted = true if it was not property finished
*/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMontageBlendingOutStartedMCDelegate, UAnimMontage*, Montage, bool, bInterrupted);

/** Delegate that native code can hook to to provide additional transition logic */
DECLARE_DELEGATE_RetVal(bool, FCanTakeTransition);

/** Delegate that native code can hook into to handle state entry/exit */
DECLARE_DELEGATE_ThreeParams(FOnGraphStateChanged, const struct FAnimNode_StateMachine& /*Machine*/, int32 /*PrevStateIndex*/, int32 /*NextStateIndex*/);

/** Delegate that allows users to insert custom animation curve values - for now, it's only single, not sure how to make this to multi delegate and retrieve value sequentially, so */
DECLARE_DELEGATE_OneParam(FOnAddCustomAnimationCurves, UAnimInstance*)

/** Delegate called by 'PlayMontageNotify' and 'PlayMontageNotifyWindow' **/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPlayMontageAnimNotifyDelegate, FName, NotifyName, const FBranchingPointNotifyPayload&, BranchingPointPayload);


USTRUCT()
struct FA2Pose
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FTransform> Bones;

	FA2Pose() {}
};

/** Component space poses. */
USTRUCT()
struct ENGINE_API FA2CSPose : public FA2Pose
{
	GENERATED_USTRUCT_BODY()

private:
	/** Pointer to current BoneContainer. */
	const struct FBoneContainer* BoneContainer;

	/** Once evaluated to be mesh space, this flag will be set. */
	UPROPERTY()
	TArray<uint8> ComponentSpaceFlags;

public:
	FA2CSPose()
		: BoneContainer(NULL)
	{
	}

	/** Constructor - needs LocalPoses. */
	void AllocateLocalPoses(const FBoneContainer& InBoneContainer, const FA2Pose & LocalPose);

	/** Constructor - needs LocalPoses. */
	void AllocateLocalPoses(const FBoneContainer& InBoneContainer, const FTransformArrayA2 & LocalBones);

	/** Returns if this struct is valid. */
	bool IsValid() const;

	/** Get parent bone index for given bone index. */
	int32 GetParentBoneIndex(const int32 BoneIndex) const;

	/** Returns local transform for the bone index. **/
	FTransform GetLocalSpaceTransform(int32 BoneIndex);

	/** Do not access Bones array directly; use this instead. This will fill up gradually mesh space bases. */
	FTransform GetComponentSpaceTransform(int32 BoneIndex);

	/** convert to local poses **/
	void ConvertToLocalPoses(FA2Pose & LocalPoses) const;

private:
	/** Calculate all transform till parent **/
	void CalculateComponentSpaceTransform(int32 Index);
	void SetComponentSpaceTransform(int32 Index, const FTransform& NewTransform);

	/**
	 * Convert Bone to Local Space.
	 */
	void ConvertBoneToLocalSpace(int32 BoneIndex);


	void SetLocalSpaceTransform(int32 Index, const FTransform& NewTransform);

	// This is not really best way to protect SetComponentSpaceTransform, but we'd like to make sure that isn't called by anywhere else.
	friend class FAnimationRuntime;
};



/** Helper struct for Slot node pose evaluation. */
USTRUCT()
struct FSlotEvaluationPose
{
	GENERATED_USTRUCT_BODY()

	/** Type of additive for pose */
	UPROPERTY()
	TEnumAsByte<EAdditiveAnimationType> AdditiveType;

	/** Weight of pose */
	UPROPERTY()
	float Weight;

	/*** ATTENTION *****/
	/* These Pose/Curve is stack allocator. You should not use it outside of stack. */
	FCompactPose Pose;
	FBlendedCurve Curve;

	FSlotEvaluationPose()
	{
	}

	FSlotEvaluationPose(float InWeight, EAdditiveAnimationType InAdditiveType)
		: AdditiveType(InAdditiveType)
		, Weight(InWeight)
	{
	}
};

/** Helper struct to store a Queued Montage BlendingOut event. */
struct FQueuedMontageBlendingOutEvent
{
	class UAnimMontage* Montage;
	bool bInterrupted;
	FOnMontageBlendingOutStarted Delegate;

	FQueuedMontageBlendingOutEvent()
		: Montage(NULL)
		, bInterrupted(false)
	{}

	FQueuedMontageBlendingOutEvent(class UAnimMontage* InMontage, bool InbInterrupted, FOnMontageBlendingOutStarted InDelegate)
		: Montage(InMontage)
		, bInterrupted(InbInterrupted)
		, Delegate(InDelegate)
	{}
};

/** Helper struct to store a Queued Montage Ended event. */
struct FQueuedMontageEndedEvent
{
	class UAnimMontage* Montage;
	bool bInterrupted;
	FOnMontageEnded Delegate;

	FQueuedMontageEndedEvent()
		: Montage(NULL)
		, bInterrupted(false)
	{}

	FQueuedMontageEndedEvent(class UAnimMontage* InMontage, bool InbInterrupted, FOnMontageEnded InDelegate)
		: Montage(InMontage)
		, bInterrupted(InbInterrupted)
		, Delegate(InDelegate)
	{}
};

/** Binding allowing native transition rule evaluation */
struct FNativeTransitionBinding
{
	/** State machine to bind to */
	FName MachineName;

	/** Previous state the transition comes from */
	FName PreviousStateName;

	/** Next state the transition goes to */
	FName NextStateName;

	/** Delegate to use when checking transition */
	FCanTakeTransition NativeTransitionDelegate;

#if WITH_EDITORONLY_DATA
	/** Name of this transition rule */
	FName TransitionName;
#endif

	FNativeTransitionBinding(const FName& InMachineName, const FName& InPreviousStateName, const FName& InNextStateName, const FCanTakeTransition& InNativeTransitionDelegate, const FName& InTransitionName = NAME_None)
		: MachineName(InMachineName)
		, PreviousStateName(InPreviousStateName)
		, NextStateName(InNextStateName)
		, NativeTransitionDelegate(InNativeTransitionDelegate)
#if WITH_EDITORONLY_DATA
		, TransitionName(InTransitionName)
#endif
	{
	}
};

/** Binding allowing native notification of state changes */
struct FNativeStateBinding
{
	/** State machine to bind to */
	FName MachineName;

	/** State to bind to */
	FName StateName;

	/** Delegate to use when checking transition */
	FOnGraphStateChanged NativeStateDelegate;

#if WITH_EDITORONLY_DATA
	/** Name of this binding */
	FName BindingName;
#endif

	FNativeStateBinding(const FName& InMachineName, const FName& InStateName, const FOnGraphStateChanged& InNativeStateDelegate, const FName& InBindingName = NAME_None)
		: MachineName(InMachineName)
		, StateName(InStateName)
		, NativeStateDelegate(InNativeStateDelegate)
#if WITH_EDITORONLY_DATA
		, BindingName(InBindingName)
#endif
	{
	}
};

/** Tracks state of active slot nodes in the graph */
struct FMontageActiveSlotTracker
{
	/** Local weight of Montages being played (local to the slot node) */
	float MontageLocalWeight;

	/** Global weight of this slot node */
	float NodeGlobalWeight;

	//Is the montage slot part of the active graph this tick
	bool  bIsRelevantThisTick;

	//Was the montage slot part of the active graph last tick
	bool  bWasRelevantOnPreviousTick;

	FMontageActiveSlotTracker()
		: MontageLocalWeight(0.f)
		, NodeGlobalWeight(0.f)
		, bIsRelevantThisTick(false)
		, bWasRelevantOnPreviousTick(false) 
	{}
};

struct FMontageEvaluationState
{
	FMontageEvaluationState(UAnimMontage* InMontage, float InWeight, float InDesiredWeight, float InPosition, bool bInIsPlaying, bool bInIsActive) 
		: Montage(InMontage)
		, MontageWeight(InWeight)
		, DesiredWeight(InDesiredWeight)
		, MontagePosition(InPosition)
		, bIsPlaying(bInIsPlaying)
		, bIsActive(bInIsActive)
	{}

	// The montage to evaluate
	TWeakObjectPtr<UAnimMontage> Montage;

	// The weight to use for this montage
	float MontageWeight;

	// The desired weight of this montage
	float DesiredWeight;

	// The position to evaluate this montage at
	float MontagePosition;

	// Whether this montage is playing
	bool bIsPlaying;

	// Whether this montage is valid and not stopped
	bool bIsActive;
};

UCLASS(transient, Blueprintable, hideCategories=AnimInstance, BlueprintType, meta=(BlueprintThreadSafe), Within=SkeletalMeshComponent)
class ENGINE_API UAnimInstance : public UObject
{
	GENERATED_UCLASS_BODY()

	typedef FAnimInstanceProxy ProxyType;

	// Disable compiler-generated deprecation warnings by implementing our own destructor
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	~UAnimInstance() {}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** DeltaTime **/
	UPROPERTY()
	float DeltaTime_DEPRECATED;

	/** This is used to extract animation. If Mesh exists, this will be overwritten by Mesh->Skeleton */
	UPROPERTY(transient)
	USkeleton* CurrentSkeleton;

	// Sets where this blueprint pulls Root Motion from
	UPROPERTY(Category = RootMotion, EditDefaultsOnly)
	TEnumAsByte<ERootMotionMode::Type> RootMotionMode;

	/** 
	 * DEPRECATED: No longer used.
	 * Allows this anim instance to update its native update, blend tree, montages and asset players on
	 * a worker thread. this requires certain conditions to be met:
	 * - All access of variables in the blend tree should be a direct access of a member variable
	 * - No BlueprintUpdateAnimation event should be used (i.e. the event graph should be empty). Only native update is permitted.
	 */
	DEPRECATED(4.15, "This variable is no longer used. Use bUseMultiThreadedAnimationUpdate on the UAnimBlueprint to control this.")
	UPROPERTY()
	bool bRunUpdatesInWorkerThreads_DEPRECATED;

	/** 
	 * DEPRECATED: No longer used.
	 * Whether we can use parallel updates for our animations.
	 * Conditions affecting this include:
	 * - Use of BlueprintUpdateAnimation
	 * - Use of non 'fast-path' EvaluateGraphExposedInputs in the node graph
	 */
	DEPRECATED(4.15, "This variable is no longer used. Use bUseMultiThreadedAnimationUpdate on the UAnimBlueprint to control this.")
	UPROPERTY()
	bool bCanUseParallelUpdateAnimation_DEPRECATED;

	/**
	 * Allows this anim instance to update its native update, blend tree, montages and asset players on
	 * a worker thread. This flag is propagated from the UAnimBlueprint to this instance by the compiler.
	 * The compiler will attempt to pick up any issues that may occur with threaded update.
	 * For updates to run in multiple threads both this flag and the project setting "Allow Multi Threaded 
	 * Animation Update" should be set.
	 */
	UPROPERTY(meta=(BlueprintCompilerGeneratedDefaults))
	bool bUseMultiThreadedAnimationUpdate;

	/**
	 * Selecting this option will cause the compiler to emit warnings whenever a call into Blueprint
	 * is made from the animation graph. This can help track down optimizations that need to be made.
	 */
	DEPRECATED(4.15, "This variable is no longer used. Use bWarnAboutBlueprintUsage on the UAnimBlueprint to control this.")
	UPROPERTY()
	bool bWarnAboutBlueprintUsage_DEPRECATED;

	/** Flag to check back on the game thread that indicates we need to run PostUpdateAnimation() in the post-eval call */
	bool bNeedsUpdate;

public:

	// @todo document
	void MakeMontageTickRecord(FAnimTickRecord& TickRecord, class UAnimMontage* Montage, float CurrentPosition, float PreviousPosition, float MoveDelta, float Weight, TArray<FPassedMarker>& MarkersPassedThisTick, FMarkerTickRecord& MarkerTickRecord);

	bool IsSlotNodeRelevantForNotifies(FName SlotNodeName) const;

	/** Get global weight in AnimGraph for this slot node.
	* Note: this is the weight of the node, not the weight of any potential montage it is playing. */
	float GetSlotNodeGlobalWeight(const FName& SlotNodeName) const;

	// Should Extract Root Motion or not. Return true if we do. 
	bool ShouldExtractRootMotion() const { return RootMotionMode == ERootMotionMode::RootMotionFromEverything || RootMotionMode == ERootMotionMode::IgnoreRootMotion; }

	/** Get Global weight of any montages this slot node is playing.
	* If this slot is not currently playing a montage, it will return 0. */
	float GetSlotMontageGlobalWeight(const FName& SlotNodeName) const;

	/** Get local weight of any montages this slot node is playing.
	* If this slot is not currently playing a montage, it will return 0.
	* This is double buffered, will return last frame data if called from Update or Evaluate. */
	float GetSlotMontageLocalWeight(const FName& SlotNodeName) const;

	/** Get local weight of any montages this slot is playing.
	* If this slot is not current playing a montage, it will return 0.
	* This will return up to date data if called during Update or Evaluate. */
	float CalcSlotMontageLocalWeight(const FName& SlotNodeName) const;

	// kismet event functions

	UFUNCTION(BlueprintCallable, Category = "Animation", meta=(NotBlueprintThreadSafe))
	virtual APawn* TryGetPawnOwner() const;

	/** 
	 * Takes a snapshot of the current skeletal mesh component pose & saves it internally.
	 * This snapshot can then be retrieved by name in the animation blueprint for blending. 
	 * The snapshot is taken at the current LOD, so if for example you took the snapshot at LOD1 and then used it at LOD0 any bones not in LOD1 will use the reference pose 
	 */
	UFUNCTION(BlueprintCallable, Category = "Pose")
	virtual void SavePoseSnapshot(FName SnapshotName);

	/**
	 * Takes a snapshot of the current skeletal mesh component pose and saves it to the specified snapshot.
	 * The snapshot is taken at the current LOD, so if for example you took the snapshot at LOD1 
	 * and then used it at LOD0 any bones not in LOD1 will use the reference pose 
	 */
	UFUNCTION(BlueprintCallable, Category = "Pose")
	virtual void SnapshotPose(UPARAM(ref) FPoseSnapshot& Snapshot);

	// Are we being evaluated on a worker thread
	bool IsRunningParallelEvaluation() const;

	// Can does this anim instance need an update (parallel or not)?
	bool NeedsUpdate() const;

private:
	// Does this anim instance need immediate update (rather than parallel)?
	bool NeedsImmediateUpdate(float DeltaSeconds) const;

public:
	/** Returns the owning actor of this AnimInstance */
	UFUNCTION(BlueprintCallable, Category = "Animation", meta=(NotBlueprintThreadSafe))
	AActor* GetOwningActor() const;
	
	// Returns the skeletal mesh component that has created this AnimInstance
	UFUNCTION(BlueprintCallable, Category = "Animation", meta=(NotBlueprintThreadSafe))
	USkeletalMeshComponent* GetOwningComponent() const;

public:

	/** Executed when the Animation is initialized */
	UFUNCTION(BlueprintImplementableEvent)
	void BlueprintInitializeAnimation();

	/** Executed when the Animation is updated */
	UFUNCTION(BlueprintImplementableEvent)
	void BlueprintUpdateAnimation(float DeltaTimeX);

	/** Executed after the Animation is evaluated */
	UFUNCTION(BlueprintImplementableEvent)
	void BlueprintPostEvaluateAnimation();

	/** Executed when begin play is called on the owning component */
	UFUNCTION(BlueprintImplementableEvent)
	void BlueprintBeginPlay();

	bool CanTransitionSignature() const;
	
	/*********************************************************************************************
	* SlotAnimation
	********************************************************************************************* */
public:

	/** DEPRECATED. Use PlaySlotAnimationAsDynamicMontage instead, it returns the UAnimMontage created instead of time, allowing more control */
	/** Play normal animation asset on the slot node. You can only play one asset (whether montage or animsequence) at a time. */
	DEPRECATED(4.9, "This function is deprecated, please use PlaySlotAnimationAsDynamicMontage instead.")
	UFUNCTION(BlueprintCallable, Category="Animation")
	float PlaySlotAnimation(UAnimSequenceBase* Asset, FName SlotNodeName, float BlendInTime = 0.25f, float BlendOutTime = 0.25f, float InPlayRate = 1.f, int32 LoopCount = 1);

	/** Play normal animation asset on the slot node by creating a dynamic UAnimMontage. You can only play one asset (whether montage or animsequence) at a time per SlotGroup. */
	UFUNCTION(BlueprintCallable, Category="Animation")
	UAnimMontage* PlaySlotAnimationAsDynamicMontage(UAnimSequenceBase* Asset, FName SlotNodeName, float BlendInTime = 0.25f, float BlendOutTime = 0.25f, float InPlayRate = 1.f, int32 LoopCount = 1, float BlendOutTriggerTime = -1.f, float InTimeToStartMontageAt = 0.f);

	/** Stops currently playing slot animation slot or all*/
	UFUNCTION(BlueprintCallable, Category="Animation")
	void StopSlotAnimation(float InBlendOutTime = 0.25f, FName SlotNodeName = NAME_None);

	/** Return true if it's playing the slot animation */
	UFUNCTION(BlueprintPure, Category="Animation")
	bool IsPlayingSlotAnimation(const UAnimSequenceBase* Asset, FName SlotNodeName) const;

	/** Return true if this instance playing the slot animation, also returning the montage it is playing on */
	bool IsPlayingSlotAnimation(const UAnimSequenceBase* Asset, FName SlotNodeName, UAnimMontage*& OutMontage) const;

	/*********************************************************************************************
	 * AnimMontage
	 ********************************************************************************************* */
public:
	/** Plays an animation montage. Returns the length of the animation montage in seconds. Returns 0.f if failed to play. */
	UFUNCTION(BlueprintCallable, Category = "Montage")
	float Montage_Play(UAnimMontage* MontageToPlay, float InPlayRate = 1.f, EMontagePlayReturnType ReturnValueType = EMontagePlayReturnType::MontageLength, float InTimeToStartMontageAt=0.f);

	/** Stops the animation montage. If reference is NULL, it will stop ALL active montages. */
	UFUNCTION(BlueprintCallable, Category = "Montage")
	void Montage_Stop(float InBlendOutTime, const UAnimMontage* Montage = NULL);

	/** Pauses the animation montage. If reference is NULL, it will pause ALL active montages. */
	UFUNCTION(BlueprintCallable, Category = "Montage")
	void Montage_Pause(const UAnimMontage* Montage = NULL);

	/** Resumes a paused animation montage. If reference is NULL, it will resume ALL active montages. */
	UFUNCTION(BlueprintCallable, Category = "Montage")
	void Montage_Resume(const UAnimMontage* Montage);

	/** Makes a montage jump to a named section. If Montage reference is NULL, it will do that to all active montages. */
	UFUNCTION(BlueprintCallable, Category="Montage")
	void Montage_JumpToSection(FName SectionName, const UAnimMontage* Montage = NULL);

	/** Makes a montage jump to the end of a named section. If Montage reference is NULL, it will do that to all active montages. */
	UFUNCTION(BlueprintCallable, Category="Montage")
	void Montage_JumpToSectionsEnd(FName SectionName, const UAnimMontage* Montage = NULL);

	/** Relink new next section AFTER SectionNameToChange in run-time
	 *	You can link section order the way you like in editor, but in run-time if you'd like to change it dynamically, 
	 *	use this function to relink the next section
	 *	For example, you can have Start->Loop->Loop->Loop.... but when you want it to end, you can relink
	 *	next section of Loop to be End to finish the montage, in which case, it stops looping by Loop->End. 
	 
	 * @param SectionNameToChange : This should be the name of the Montage Section after which you want to insert a new next section
	 * @param NextSection	: new next section 
	 */
	UFUNCTION(BlueprintCallable, Category="Montage")
	void Montage_SetNextSection(FName SectionNameToChange, FName NextSection, const UAnimMontage* Montage = NULL);

	/** Change AnimMontage play rate. NewPlayRate = 1.0 is the default playback rate. */
	UFUNCTION(BlueprintCallable, Category="Montage")
	void Montage_SetPlayRate(const UAnimMontage* Montage, float NewPlayRate = 1.f);

	/** Returns true if the animation montage is active. If the Montage reference is NULL, it will return true if any Montage is active. */
	UFUNCTION(BlueprintPure, Category="Montage", meta=(NotBlueprintThreadSafe))
	bool Montage_IsActive(const UAnimMontage* Montage) const;

	/** Returns true if the animation montage is currently active and playing. 
	If reference is NULL, it will return true is ANY montage is currently active and playing. */
	UFUNCTION(BlueprintPure, Category="Montage", meta = (NotBlueprintThreadSafe))
	bool Montage_IsPlaying(const UAnimMontage* Montage) const;

	/** Returns the name of the current animation montage section. */
	UFUNCTION(BlueprintPure, Category="Montage", meta = (NotBlueprintThreadSafe))
	FName Montage_GetCurrentSection(const UAnimMontage* Montage = NULL) const;

	/** Get Current Montage Position */
	UFUNCTION(BlueprintPure, Category = "Montage", meta = (NotBlueprintThreadSafe))
	float Montage_GetPosition(const UAnimMontage* Montage) const;
	
	/** Set position. */
	UFUNCTION(BlueprintCallable, Category = "Montage")
	void Montage_SetPosition(const UAnimMontage* Montage, float NewPosition);
	
	/** return true if Montage is not currently active. (not valid or blending out) */
	UFUNCTION(BlueprintPure, Category = "Montage", meta = (NotBlueprintThreadSafe))
	bool Montage_GetIsStopped(const UAnimMontage* Montage) const;

	/** Get the current blend time of the Montage.
	If Montage reference is NULL, it will return the current blend time on the first active Montage found. */
	UFUNCTION(BlueprintPure, Category = "Montage", meta = (NotBlueprintThreadSafe))
	float Montage_GetBlendTime(const UAnimMontage* Montage) const;

	/** Get PlayRate for Montage.
	If Montage reference is NULL, PlayRate for any Active Montage will be returned.
	If Montage is not playing, 0 is returned. */
	UFUNCTION(BlueprintPure, Category = "Montage", meta = (NotBlueprintThreadSafe))
	float Montage_GetPlayRate(const UAnimMontage* Montage) const;

	/** Returns true if any montage is playing currently. Doesn't mean it's active though, it could be blending out. */
	UFUNCTION(BlueprintPure, Category = "Montage", meta = (NotBlueprintThreadSafe))
	bool IsAnyMontagePlaying() const;

	/** Get a current Active Montage in this AnimInstance. 
		Note that there might be multiple Active at the same time. This will only return the first active one it finds. **/
	UFUNCTION(BlueprintPure, Category = "Montage", meta = (NotBlueprintThreadSafe))
	UAnimMontage* GetCurrentActiveMontage() const;

	/** Called when a montage starts blending out, whether interrupted or finished */
	UPROPERTY(BlueprintAssignable)
	FOnMontageBlendingOutStartedMCDelegate OnMontageBlendingOut;
	
	/** Called when a montage has started */
	UPROPERTY(BlueprintAssignable)
	FOnMontageStartedMCDelegate OnMontageStarted;

	/** Called when a montage has ended, whether interrupted or finished*/
	UPROPERTY(BlueprintAssignable)
	FOnMontageEndedMCDelegate OnMontageEnded;

	/** Called when all Montage instances have ended. */
	UPROPERTY(BlueprintAssignable)
	FOnAllMontageInstancesEndedMCDelegate OnAllMontageInstancesEnded;

	/*********************************************************************************************
	* AnimMontage native C++ interface
	********************************************************************************************* */
public:	
	void Montage_SetEndDelegate(FOnMontageEnded & InOnMontageEnded, UAnimMontage* Montage = NULL);
	
	void Montage_SetBlendingOutDelegate(FOnMontageBlendingOutStarted & InOnMontageBlendingOut, UAnimMontage* Montage = NULL);
	
	/** Get pointer to BlendingOutStarted delegate for Montage.
	If Montage reference is NULL, it will pick the first active montage found. */
	FOnMontageBlendingOutStarted* Montage_GetBlendingOutDelegate(UAnimMontage* Montage = NULL);

	/** Get next sectionID for given section ID */
	int32 Montage_GetNextSectionID(const UAnimMontage* Montage, int32 const & CurrentSectionID) const;

	/** Get Currently active montage instance.
		Note that there might be multiple Active at the same time. This will only return the first active one it finds. **/
	FAnimMontageInstance* GetActiveMontageInstance() const;

	/** Get Active FAnimMontageInstance for given Montage asset. Will return NULL if Montage is not currently Active. */
	DEPRECATED(4.13, "Please use GetActiveInstanceForMontage(const UAnimMontage* Montage)")
	FAnimMontageInstance* GetActiveInstanceForMontage(UAnimMontage const& Montage) const;

	/** Get Active FAnimMontageInstance for given Montage asset. Will return NULL if Montage is not currently Active. */
	FAnimMontageInstance* GetActiveInstanceForMontage(const UAnimMontage* Montage) const;

	/** Get the FAnimMontageInstance currently running that matches this ID.  Will return NULL if no instance is found. */
	FAnimMontageInstance* GetMontageInstanceForID(int32 MontageInstanceID);

	/** AnimMontage instances that are running currently
	* - only one is primarily active per group, and the other ones are blending out
	*/
	TArray<struct FAnimMontageInstance*> MontageInstances;

	virtual void OnMontageInstanceStopped(FAnimMontageInstance & StoppedMontageInstance);
	void ClearMontageInstanceReferences(FAnimMontageInstance& InMontageInstance);

	FAnimNode_SubInput* GetSubInputNode() const;

protected:
	/** Map between Active Montages and their FAnimMontageInstance */
	TMap<class UAnimMontage*, struct FAnimMontageInstance*> ActiveMontagesMap;

	/** Stop all montages that are active **/
	void StopAllMontages(float BlendOut);

	/** Stop all active montages belonging to 'InGroupName' */
	void StopAllMontagesByGroupName(FName InGroupName, const FAlphaBlend& BlendOut);

	/** Update weight of montages  **/
	virtual void Montage_UpdateWeight(float DeltaSeconds);
	/** Advance montages **/
	virtual void Montage_Advance(float DeltaSeconds);

public:
	/** Queue a Montage BlendingOut Event to be triggered. */
	void QueueMontageBlendingOutEvent(const FQueuedMontageBlendingOutEvent& MontageBlendingOutEvent);

	/** Queue a Montage Ended Event to be triggered. */
	void QueueMontageEndedEvent(const FQueuedMontageEndedEvent& MontageEndedEvent);

private:
	/** True when Montages are being ticked, and Montage Events should be queued. 
	 * When Montage are being ticked, we queue AnimNotifies and Events. We trigger notifies first, then Montage events. */
	UPROPERTY(Transient)
	bool bQueueMontageEvents;

	/** Trigger queued Montage events. */
	void TriggerQueuedMontageEvents();

	/** Queued Montage BlendingOut events. */
	TArray<FQueuedMontageBlendingOutEvent> QueuedMontageBlendingOutEvents;

	/** Queued Montage Ended Events */
	TArray<FQueuedMontageEndedEvent> QueuedMontageEndedEvents;

	/** Trigger a Montage BlendingOut event */
	void TriggerMontageBlendingOutEvent(const FQueuedMontageBlendingOutEvent& MontageBlendingOutEvent);

	/** Trigger a Montage Ended event */
	void TriggerMontageEndedEvent(const FQueuedMontageEndedEvent& MontageEndedEvent);

	/** Used to guard against recursive calls to UpdateAnimation */
	bool bUpdatingAnimation;

	/** Used to guard against recursive calls to UpdateAnimation */
	bool bPostUpdatingAnimation;

public:

	/** Is this animation currently running post update */
	bool IsPostUpdatingAnimation() const { return bPostUpdatingAnimation; }

	/** Set RootMotionMode */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void SetRootMotionMode(TEnumAsByte<ERootMotionMode::Type> Value);

	/** 
	 * NOTE: Derived anim getters
	 *
	 * Anim getter functions can be defined for any instance deriving UAnimInstance.
	 * To do this the function must be marked BlueprintPure, and have the AnimGetter metadata entry set to
	 * "true". Following the instructions below, getters should appear correctly in the blueprint node context
	 * menu for the derived classes
	 *
	 * A context string can be provided in the GetterContext metadata and can contain any (or none) of the
	 * following entries separated by a pipe (|)
	 * Transition  - Only available in a transition rule
	 * AnimGraph   - Only available in an animgraph (also covers state anim graphs)
	 * CustomBlend - Only available in a custom blend graph
	 *
	 * Anim getters support a number of automatic parameters that will be baked at compile time to be passed
	 * to the functions. They will not appear as pins on the graph node. They are as follows:
	 * AssetPlayerIndex - Index of an asset player node to operate on, one getter will be added to the blueprint action list per asset node available
	 * MachineIndex     - Index of a state machine in the animation blueprint, one getter will be added to the blueprint action list per state machine
	 * StateIndex       - Index of a state inside a state machine, also requires MachineIndex. One getter will be added to the blueprint action list per state
	 * TransitionIndex  - Index of a transition inside a state machine, also requires MachineIndex. One getter will be added to the blueprint action list per transition
	 */

	/** Gets the length in seconds of the asset referenced in an asset player node */
	UFUNCTION(BlueprintPure, Category="Asset Player", meta=(DisplayName="Length", BlueprintInternalUseOnly="true", AnimGetter="true"))
	float GetInstanceAssetPlayerLength(int32 AssetPlayerIndex);

	/** Get the current accumulated time in seconds for an asset player node */
	UFUNCTION(BlueprintPure, Category="Asset Player", meta = (DisplayName = "Current Time", BlueprintInternalUseOnly = "true", AnimGetter = "true"))
	float GetInstanceAssetPlayerTime(int32 AssetPlayerIndex);

	/** Get the current accumulated time as a fraction for an asset player node */
	UFUNCTION(BlueprintPure, Category="Asset Player", meta=(DisplayName="Current Time (ratio)", BlueprintInternalUseOnly="true", AnimGetter="true"))
	float GetInstanceAssetPlayerTimeFraction(int32 AssetPlayerIndex);

	/** Get the time in seconds from the end of an animation in an asset player node */
	UFUNCTION(BlueprintPure, Category="Asset Player", meta=(DisplayName="Time Remaining", BlueprintInternalUseOnly="true", AnimGetter="true"))
	float GetInstanceAssetPlayerTimeFromEnd(int32 AssetPlayerIndex);

	/** Get the time as a fraction of the asset length of an animation in an asset player node */
	UFUNCTION(BlueprintPure, Category="Asset Player", meta=(DisplayName="Time Remaining (ratio)", BlueprintInternalUseOnly="true", AnimGetter="true"))
	float GetInstanceAssetPlayerTimeFromEndFraction(int32 AssetPlayerIndex);

	/** Get the blend weight of a specified state machine */
	UFUNCTION(BlueprintPure, Category = "States", meta = (DisplayName = "Machine Weight", BlueprintInternalUseOnly = "true", AnimGetter = "true"))
	float GetInstanceMachineWeight(int32 MachineIndex);

	/** Get the blend weight of a specified state */
	UFUNCTION(BlueprintPure, Category="States", meta = (DisplayName="State Weight", BlueprintInternalUseOnly = "true", AnimGetter="true"))
	float GetInstanceStateWeight(int32 MachineIndex, int32 StateIndex);

	/** Get the current elapsed time of a state within the specified state machine */
	UFUNCTION(BlueprintPure, Category="States", meta = (DisplayName="Current State Time", BlueprintInternalUseOnly = "true", AnimGetter="true", GetterContext="Transition"))
	float GetInstanceCurrentStateElapsedTime(int32 MachineIndex);

	/** Get the crossfade duration of a specified transition */
	UFUNCTION(BlueprintPure, Category="Transitions", meta = (DisplayName="Get Transition Crossfade Duration", BlueprintInternalUseOnly = "true", AnimGetter="true"))
	float GetInstanceTransitionCrossfadeDuration(int32 MachineIndex, int32 TransitionIndex);

	/** Get the elapsed time in seconds of a specified transition */
	UFUNCTION(BlueprintPure, Category="Transitions", meta = (DisplayName="Get Transition Time Elapsed", BlueprintInternalUseOnly = "true", AnimGetter="true", GetterContext="CustomBlend"))
	float GetInstanceTransitionTimeElapsed(int32 MachineIndex, int32 TransitionIndex);

	/** Get the elapsed time as a fraction of the crossfade duration of a specified transition */
	UFUNCTION(BlueprintPure, Category="Transitions", meta = (DisplayName="Get Transition Time Elapsed (ratio)", BlueprintInternalUseOnly = "true", AnimGetter="true", GetterContext="CustomBlend"))
	float GetInstanceTransitionTimeElapsedFraction(int32 MachineIndex, int32 TransitionIndex);

	/** Get the time remaining in seconds for the most relevant animation in the source state */
	UFUNCTION(BlueprintPure, Category="Asset Player", meta = (BlueprintInternalUseOnly = "true", AnimGetter="true", GetterContext="Transition"))
	float GetRelevantAnimTimeRemaining(int32 MachineIndex, int32 StateIndex);

	/** Get the time remaining as a fraction of the duration for the most relevant animation in the source state */
	UFUNCTION(BlueprintPure, Category = "Asset Player", meta = (BlueprintInternalUseOnly = "true", AnimGetter = "true", GetterContext = "Transition"))
	float GetRelevantAnimTimeRemainingFraction(int32 MachineIndex, int32 StateIndex);

	/** Get the length in seconds of the most relevant animation in the source state */
	UFUNCTION(BlueprintPure, Category = "Asset Player", meta = (BlueprintInternalUseOnly = "true", AnimGetter = "true", GetterContext = "Transition"))
	float GetRelevantAnimLength(int32 MachineIndex, int32 StateIndex);

	/** Get the current accumulated time in seconds for the most relevant animation in the source state */
	UFUNCTION(BlueprintPure, Category = "Asset Player", meta = (BlueprintInternalUseOnly = "true", AnimGetter = "true", GetterContext = "Transition"))
	float GetRelevantAnimTime(int32 MachineIndex, int32 StateIndex);

	/** Get the current accumulated time as a fraction of the length of the most relevant animation in the source state */
	UFUNCTION(BlueprintPure, Category = "Asset Player", meta = (BlueprintInternalUseOnly = "true", AnimGetter = "true", GetterContext = "Transition"))
	float GetRelevantAnimTimeFraction(int32 MachineIndex, int32 StateIndex);

	/** Gets the runtime instance of the specified state machine by Name */
	FAnimNode_StateMachine* GetStateMachineInstanceFromName(FName MachineName);

	/** Get the machine description for the specified instance. Does not rely on PRIVATE_MachineDescription being initialized */
	const FBakedAnimationStateMachine* GetMachineDescription(IAnimClassInterface* AnimBlueprintClass, FAnimNode_StateMachine* MachineInstance);

	void GetStateMachineIndexAndDescription(FName InMachineName, int32& OutMachineIndex, const FBakedAnimationStateMachine** OutMachineDescription);

	/** Returns the baked sync group index from the compile step */
	int32 GetSyncGroupIndexFromName(FName SyncGroupName) const;

	/** Gets the index of the state machine matching MachineName */
	int32 GetStateMachineIndex(FName MachineName);

	/** Gets the runtime instance of the specified state machine */
	FAnimNode_StateMachine* GetStateMachineInstance(int32 MachineIndex);

	/** 
	 * Get the index of the specified instance asset player. Useful to pass to GetInstanceAssetPlayerLength (etc.).
	 * Passing NAME_None to InstanceName will return the first (assumed only) player instance index found.
	 */
	int32 GetInstanceAssetPlayerIndex(FName MachineName, FName StateName, FName InstanceName = NAME_None);

	/** Gets the runtime instance desc of the state machine specified by name */
	const FBakedAnimationStateMachine* GetStateMachineInstanceDesc(FName MachineName);

	/** Gets the most relevant asset player in a specified state */
	FAnimNode_AssetPlayerBase* GetRelevantAssetPlayerFromState(int32 MachineIndex, int32 StateIndex);

	//////////////////////////////////////////////////////////////////////////

public:
	/** Returns the value of a named curve. */
	UFUNCTION(BlueprintPure, Category="Animation")
	float GetCurveValue(FName CurveName);

	/** Returns value of named curved in OutValue, returns whether the curve was actually found or not. */
	bool GetCurveValue(FName CurveName, float& OutValue);

	/** Returns the name of a currently active state in a state machine. */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(BlueprintInternalUseOnly = "true", AnimGetter = "true"))
	FName GetCurrentStateName(int32 MachineIndex);

	/** Sets a morph target to a certain weight. */
	UFUNCTION(BlueprintCallable, Category="Animation")
	void SetMorphTarget(FName MorphTargetName, float Value);

	/** Clears the current morph targets. */
	UFUNCTION(BlueprintCallable, Category="Animation")
	void ClearMorphTargets();

	/** 
	 * Returns degree of the angle betwee velocity and Rotation forward vector
	 * The range of return will be from [-180, 180], and this can be used to feed blendspace directional value
	 */
	UFUNCTION(BlueprintCallable, Category="Animation")
	float CalculateDirection(const FVector& Velocity, const FRotator& BaseRotation);

	//--- AI communication start ---//
	/** locks indicated AI resources of animated pawn
	 *	DEPRECATED. Use LockAIResourcesWithAnimation instead */
	UFUNCTION(BlueprintCallable, Category = "Animation", BlueprintAuthorityOnly, Meta=(DeprecatedFunction, DeprecationMessage="Use LockAIResourcesWithAnimation instead"))
	void LockAIResources(bool bLockMovement, bool LockAILogic);

	/** unlocks indicated AI resources of animated pawn. Will unlock only animation-locked resources.
	 *	DEPRECATED. Use UnlockAIResourcesWithAnimation instead */
	UFUNCTION(BlueprintCallable, Category = "Animation", BlueprintAuthorityOnly, Meta=(DeprecatedFunction, DeprecationMessage="Use UnlockAIResourcesWithAnimation instead"))
	void UnlockAIResources(bool bUnlockMovement, bool UnlockAILogic);
	//--- AI communication end ---//

	UFUNCTION(BlueprintCallable, Category = "SyncGroup")
	bool GetTimeToClosestMarker(FName SyncGroup, FName MarkerName, float& OutMarkerTime) const;

	UFUNCTION(BlueprintCallable, Category = "SyncGroup")
	bool HasMarkerBeenHitThisFrame(FName SyncGroup, FName MarkerName) const;

	UFUNCTION(BlueprintCallable, Category = "SyncGroup")
	bool IsSyncGroupBetweenMarkers(FName InSyncGroupName, FName PreviousMarker, FName NextMarker, bool bRespectMarkerOrder = true) const;

	UFUNCTION(BlueprintCallable, Category = "SyncGroup")
	FMarkerSyncAnimPosition GetSyncGroupPosition(FName InSyncGroupName) const;

public:
	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void BeginDestroy() override;
	virtual void PostInitProperties() override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	//~ End UObject Interface

#if WITH_EDITORONLY_DATA // ANIMINST_PostCompileValidation
	/** Name of Class to do Post Compile Validation.
	* See Class UAnimBlueprintPostCompileValidation. */
	UPROPERTY()
	FSoftClassPath PostCompileValidationClassName;

	/** Warn if AnimNodes are not using fast path during AnimBP compilation. */
	virtual bool PCV_ShouldWarnAboutNodesNotUsingFastPath() const { return false; }
#endif // WITH_EDITORONLY_DATA

	virtual void OnUROSkipTickAnimation() {}
	virtual void OnUROPreInterpolation() {}

	// Animation phase trigger
	// start with initialize
	// update happens in every tick. Can happen in parallel with others if conditions are right.
	// evaluate happens when condition is met - i.e. depending on your skeletalmeshcomponent update flag
	// post eval happens after evaluation is done
	// uninitialize happens when owner is unregistered
	void InitializeAnimation();
	void UpdateAnimation(float DeltaSeconds, bool bNeedsValidRootMotion);

	/** Run update animation work on a worker thread */
	void ParallelUpdateAnimation();

	/** Called after updates are completed, dispatches notifies etc. */
	void PostUpdateAnimation();

	/** Called on the game thread pre-evaluation. */
	void PreEvaluateAnimation();

	/** Check whether evaluation can be performed on the supplied skeletal mesh. Can be called from worker threads. */
	bool ParallelCanEvaluate(const USkeletalMesh* InSkeletalMesh) const;

	/** Perform evaluation. Can be called from worker threads. */
	void ParallelEvaluateAnimation(bool bForceRefPose, const USkeletalMesh* InSkeletalMesh, TArray<FTransform>& OutBoneSpaceTransforms, FBlendedHeapCurve& OutCurve, FCompactPose& OutPose);

	void PostEvaluateAnimation();
	void UninitializeAnimation();

	// the below functions are the native overrides for each phase
	// Native initialization override point
	virtual void NativeInitializeAnimation();
	// Native update override point. It is usually a good idea to simply gather data in this step and 
	// for the bulk of the work to be done in NativeUpdateAnimation.
	virtual void NativeUpdateAnimation(float DeltaSeconds);
	// Native update override point. Can be called from a worker thread. This is a good place to do any
	// heavy lifting (as opposed to NativeUpdateAnimation_GameThread()).
	// This function should not be used. Worker thread updates should be performed in the FAnimInstanceProxy attached to this instance.
	DEPRECATED(4.15, "This function is only called for backwards-compatibility. It is no longer called on a worker thread.")
	virtual void NativeUpdateAnimation_WorkerThread(float DeltaSeconds);
	// Native Post Evaluate override point
	virtual void NativePostEvaluateAnimation();
	// Native Uninitialize override point
	virtual void NativeUninitializeAnimation();

	// Sets up a native transition delegate between states with PrevStateName and NextStateName, in the state machine with name MachineName.
	// Note that a transition already has to exist for this to succeed
	void AddNativeTransitionBinding(const FName& MachineName, const FName& PrevStateName, const FName& NextStateName, const FCanTakeTransition& NativeTransitionDelegate, const FName& TransitionName = NAME_None);

	// Check for whether a native rule is bound to the specified transition
	bool HasNativeTransitionBinding(const FName& MachineName, const FName& PrevStateName, const FName& NextStateName, FName& OutBindingName);

	// Sets up a native state entry delegate from state with StateName, in the state machine with name MachineName.
	void AddNativeStateEntryBinding(const FName& MachineName, const FName& StateName, const FOnGraphStateChanged& NativeEnteredDelegate);
	
	// Check for whether a native entry delegate is bound to the specified state
	bool HasNativeStateEntryBinding(const FName& MachineName, const FName& StateName, FName& OutBindingName);

	// Sets up a native state exit delegate from state with StateName, in the state machine with name MachineName.
	void AddNativeStateExitBinding(const FName& MachineName, const FName& StateName, const FOnGraphStateChanged& NativeExitedDelegate);

	// Check for whether a native exit delegate is bound to the specified state
	bool HasNativeStateExitBinding(const FName& MachineName, const FName& StateName, FName& OutBindingName);

	// Debug output for this anim instance 
	void DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos);

	/** Reset any dynamics running simulation-style updates (e.g. on teleport, time skip etc.) */
	void ResetDynamics();

public:
	/** Access a read only version of the Updater Counter from the AnimInstanceProxy on the GameThread. */
	const FGraphTraversalCounter& GetUpdateCounter() const; 

	/** Access the required bones array */
	FBoneContainer& GetRequiredBones();
	const FBoneContainer& GetRequiredBones() const;

	/** Animation Notifies that has been triggered in the latest tick **/
	FAnimNotifyQueue NotifyQueue;

	/** Currently Active AnimNotifyState, stored as a copy of the event as we need to
		call NotifyEnd on the event after a deletion in the editor. After this the event
		is removed correctly. */
	UPROPERTY(transient)
	TArray<FAnimNotifyEvent> ActiveAnimNotifyState;

private:
	/** This is if you want to separate to another array**/
	TMap<FName, float>	AnimationCurves[(uint8)EAnimCurveType::MaxAnimCurveType];

	/** Reset Animation Curves */
	void ResetAnimationCurves();

	/** Material parameters that we had been changing and now need to clear */
	TArray<FName> MaterialParamatersToClear;

	//This frames marker sync data
	FMarkerTickContext MarkerTickContext;

public: 
	/** Update all internal curves from Blended Curve */
	void UpdateCurves(const FBlendedHeapCurve& InCurves);

	/** Refresh currently existing curves */
	void RefreshCurves(USkeletalMeshComponent* Component);

	/** Check whether we have active morph target curves */
	bool HasMorphTargetCurves() const;

	/** 
	 * Retrieve animation curve list by Curve Flags, it will return list of {UID, value} 
	 * It will clear the OutCurveList before adding
	 */
	void GetAnimationCurveList(EAnimCurveType Type, TMap<FName, float>& OutCurveList) const;

#if WITH_EDITORONLY_DATA
	// Maximum playback position ever reached (only used when debugging in Persona)
	double LifeTimer;

	// Current scrubbing playback position (only used when debugging in Persona)
	double CurrentLifeTimerScrubPosition;
#endif

public:
	FGraphTraversalCounter DebugDataCounter;

private:
	TMap<FName, FMontageActiveSlotTracker> SlotWeightTracker;

public:
	/** 
	 * Recalculate Required Bones [RequiredBones]
	 * Is called when bRequiredBonesUpToDate = false
	 */
	void RecalcRequiredBones();

	/**
	* Recalculate Required Curves based on Required Bones [RequiredBones]
	*/
	void RecalcRequiredCurves(const FCurveEvaluationOption& CurveEvalOption);

	// @todo document
	inline USkeletalMeshComponent* GetSkelMeshComponent() const { return CastChecked<USkeletalMeshComponent>(GetOuter()); }

	virtual UWorld* GetWorld() const override;

	/** Trigger AnimNotifies **/
	void TriggerAnimNotifies(float DeltaSeconds);
	void TriggerSingleAnimNotify(const FAnimNotifyEvent* AnimNotifyEvent);

	/** Triggers end on active notify states and clears the array */
	void EndNotifyStates();

	/** Add curve float data using a curve Uid, the name of the curve will be resolved from the skeleton **/
	void AddCurveValue(const USkeleton::AnimCurveUID Uid, float Value);

	/** Given a machine index, record a state machine weight for this frame */
	void RecordMachineWeight(const int32 InMachineClassIndex, const float InMachineWeight);
	/** 
	 * Add curve float data, using a curve name. External values should all be added using
	 * The curve UID to the public version of this method
	 */
	void AddCurveValue(const FName& CurveName, float Value);

	/** Given a machine and state index, record a state weight for this frame */
	void RecordStateWeight(const int32 InMachineClassIndex, const int32 InStateIndex, const float InStateWeight);

protected:
#if WITH_EDITORONLY_DATA
	// Returns true if a snapshot is being played back and the remainder of Update should be skipped.
	bool UpdateSnapshotAndSkipRemainingUpdate();
#endif

	// Root Motion
public:
	/** Get current RootMotion FAnimMontageInstance if any. NULL otherwise. */
	FAnimMontageInstance * GetRootMotionMontageInstance() const;

	/** Get current accumulated root motion, removing it from the AnimInstance in the process */
	FRootMotionMovementParams ConsumeExtractedRootMotion(float Alpha);

	/**  
	 * Queue blended root motion. This is used to blend in root motion transforms according to 
	 * the correctly-updated slot weight (after the animation graph has been updated).
	 */
	void QueueRootMotionBlend(const FTransform& RootTransform, const FName& SlotName, float Weight);

private:
	/** Active Root Motion Montage Instance, if any. */
	struct FAnimMontageInstance* RootMotionMontageInstance;

	/** Temporarily queued root motion blend */
	struct FQueuedRootMotionBlend
	{
		FQueuedRootMotionBlend(const FTransform& InTransform, const FName& InSlotName, float InWeight)
			: Transform(InTransform)
			, SlotName(InSlotName)
			, Weight(InWeight)
		{}

		FTransform Transform;
		FName SlotName;
		float Weight;
	};

	/** 
	 * Blend queue for blended root motion. This is used to blend in root motion transforms according to 
	 * the correctly-updated slot weight (after the animation graph has been updated).
	 */
	TArray<FQueuedRootMotionBlend> RootMotionBlendQueue;

	// Root motion read from proxy (where it is calculated) and stored here to avoid potential stalls by calling GetProxyOnGameThread
	FRootMotionMovementParams ExtractedRootMotion;

private:
	// update montage
	void UpdateMontage(float DeltaSeconds);
	void UpdateMontageSyncGroup();

protected:
	// Updates the montage data used for evaluation based on the current playing montages
	void UpdateMontageEvaluationData();

	/** Called to setup for updates */
	virtual void PreUpdateAnimation(float DeltaSeconds);

	/** update animation curves to component */
	void UpdateCurvesToComponents(USkeletalMeshComponent* Component);

	/** Override point for derived classes to create their own proxy objects (allows custom allocation) */
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy();

	/** Override point for derived classes to destroy their own proxy objects (allows custom allocation) */
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy);

	/** Access the proxy but block if a task is currently in progress as it wouldn't be safe to access it */
	template <typename T /*= FAnimInstanceProxy*/>	// @TODO: Cant default parameters to this function on Xbox One until we move off the VS2012 compiler
	FORCEINLINE T& GetProxyOnGameThread()
	{
		check(IsInGameThread());
		if(GetOuter() && GetOuter()->IsA<USkeletalMeshComponent>())
		{
			bool bBlockOnTask = true;
			bool bPerformPostAnimEvaluation = true;
			GetSkelMeshComponent()->HandleExistingParallelEvaluationTask(bBlockOnTask, bPerformPostAnimEvaluation);
		}
		if(AnimInstanceProxy == nullptr)
		{
			AnimInstanceProxy = CreateAnimInstanceProxy();
		}
		return *static_cast<T*>(AnimInstanceProxy);
	}

	/** Access the proxy but block if a task is currently in progress as it wouldn't be safe to access it */
	template <typename T/* = FAnimInstanceProxy*/>	// @TODO: Cant default parameters to this function on Xbox One until we move off the VS2012 compiler
	FORCEINLINE const T& GetProxyOnGameThread() const
	{
		check(IsInGameThread());
		if(GetOuter() && GetOuter()->IsA<USkeletalMeshComponent>())
		{
			bool bBlockOnTask = true;
			bool bPerformPostAnimEvaluation = true;
			GetSkelMeshComponent()->HandleExistingParallelEvaluationTask(bBlockOnTask, bPerformPostAnimEvaluation);
		}
		if(AnimInstanceProxy == nullptr)
		{
			AnimInstanceProxy = const_cast<UAnimInstance*>(this)->CreateAnimInstanceProxy();
		}
		return *static_cast<const T*>(AnimInstanceProxy);
	}

	/** Access the proxy but block if a task is currently in progress (and we are on the game thread) as it wouldn't be safe to access it */
	template <typename T/* = FAnimInstanceProxy*/>	// @TODO: Cant default parameters to this function on Xbox One until we move off the VS2012 compiler
	FORCEINLINE T& GetProxyOnAnyThread()
	{
		if(GetOuter() && GetOuter()->IsA<USkeletalMeshComponent>())
		{
			if(IsInGameThread())
			{
				bool bBlockOnTask = true;
				bool bPerformPostAnimEvaluation = true;
				GetSkelMeshComponent()->HandleExistingParallelEvaluationTask(bBlockOnTask, bPerformPostAnimEvaluation);
			}
		}
		if(AnimInstanceProxy == nullptr)
		{
			AnimInstanceProxy = CreateAnimInstanceProxy();
		}
		return *static_cast<T*>(AnimInstanceProxy);
	}

	/** Access the proxy but block if a task is currently in progress (and we are on the game thread) as it wouldn't be safe to access it */
	template <typename T/* = FAnimInstanceProxy*/>	// @TODO: Cant default parameters to this function on Xbox One until we move off the VS2012 compiler
	FORCEINLINE const T& GetProxyOnAnyThread() const
	{
		if(GetOuter() && GetOuter()->IsA<USkeletalMeshComponent>())
		{
			if(IsInGameThread())
			{
				bool bBlockOnTask = true;
				bool bPerformPostAnimEvaluation = true;
				GetSkelMeshComponent()->HandleExistingParallelEvaluationTask(bBlockOnTask, bPerformPostAnimEvaluation);
			}
		}
		if(AnimInstanceProxy == nullptr)
		{
			AnimInstanceProxy = const_cast<UAnimInstance*>(this)->CreateAnimInstanceProxy();
		}
		return *static_cast<const T*>(AnimInstanceProxy);
	}

	friend struct FAnimNode_SubInstance;

protected:
	/** Proxy object, nothing should access this from an externally-callable API as it is used as a scratch area on worker threads */
	mutable FAnimInstanceProxy* AnimInstanceProxy;

public:
	/** Called when a montage hits a 'PlayMontageNotify' or 'PlayMontageNotifyWindow' begin */
	FPlayMontageAnimNotifyDelegate OnPlayMontageNotifyBegin;

	/** Called when a montage hits a 'PlayMontageNotify' or 'PlayMontageNotifyWindow' end */
	FPlayMontageAnimNotifyDelegate OnPlayMontageNotifyEnd;

public:
	/** Dispatch AnimEvents (AnimNotifies, Montage Events) queued during UpdateAnimation() */
	void DispatchQueuedAnimEvents();
};
