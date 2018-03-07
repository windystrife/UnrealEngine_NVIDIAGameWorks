// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimationAsset.h"
#include "AlphaBlend.h"
#include "Animation/AnimStateMachineTypes.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimInstance.h"
#include "AnimNode_StateMachine.generated.h"

class IAnimClassInterface;
class UBlendProfile;
struct FAnimNode_AssetPlayerBase;
struct FAnimNode_StateMachine;
struct FAnimNode_TransitionPoseEvaluator;

// Information about an active transition on the transition stack
USTRUCT()
struct FAnimationActiveTransitionEntry
{
	GENERATED_USTRUCT_BODY()

	// Elapsed time for this transition
	float ElapsedTime;

	// The transition alpha between next and previous states
	float Alpha;

	// Duration of this cross-fade (may be shorter than the nominal duration specified by the state machine if the target state had non-zero weight at the start)
	float CrossfadeDuration;

	// Type of blend to use
	EAlphaBlendOption BlendOption;

	// Is this transition active?
	bool bActive;

	// Cached Pose for this transition
	TArray<FTransform> InputPose;

	// Graph to run that determines the final pose for this transition
	FPoseLink CustomTransitionGraph;

	// To and from state ids
	int32 NextState;

	int32 PreviousState;

	// Notifies are copied from the reference transition info
	int32 StartNotify;

	int32 EndNotify;

	int32 InterruptNotify;
	
	TEnumAsByte<ETransitionLogicType::Type> LogicType;

	TArray<FAnimNode_TransitionPoseEvaluator*> PoseEvaluators;

	// Blend data used for per-bone animation evaluation
	TArray<FBlendSampleData> StateBlendData;

	TArray<int32, TInlineAllocator<3>> SourceTransitionIndices;

	// Blend profile to use for this transition. Specifying this will make the transition evaluate per-bone
	UPROPERTY()
	UBlendProfile* BlendProfile;

public:
	FAnimationActiveTransitionEntry();
	FAnimationActiveTransitionEntry(int32 NextStateID, float ExistingWeightOfNextState, FAnimationActiveTransitionEntry* ExistingTransitionForNextState, int32 PreviousStateID, const FAnimationTransitionBetweenStates& ReferenceTransitionInfo);

	void InitializeCustomGraphLinks(const FAnimationUpdateContext& Context, const FBakedStateExitTransition& TransitionRule);

	void Update(const FAnimationUpdateContext& Context, int32 CurrentStateIndex, bool &OutFinished);
	
	void UpdateCustomTransitionGraph(const FAnimationUpdateContext& Context, FAnimNode_StateMachine& StateMachine, int32 ActiveTransitionIndex);
	void EvaluateCustomTransitionGraph(FPoseContext& Output, FAnimNode_StateMachine& StateMachine, bool IntermediatePoseIsValid, int32 ActiveTransitionIndex);

	bool Serialize(FArchive& Ar);

protected:
	float CalculateInverseAlpha(EAlphaBlendOption BlendMode, float InFraction) const;
	float CalculateAlpha(float InFraction) const;

	// Blend object to handle alpha interpolation
	FAlphaBlend Blend;
};

USTRUCT()
struct FAnimationPotentialTransition
{
	GENERATED_USTRUCT_BODY()

	int32 TargetState;

	const FBakedStateExitTransition* TransitionRule;

	TArray<int32, TInlineAllocator<3>> SourceTransitionIndices;

public:
	FAnimationPotentialTransition();
	bool IsValid() const;
	void Clear();
};

//@TODO: ANIM: Need to implement WithSerializer and Identical for FAnimationActiveTransitionEntry?

// State machine node
USTRUCT()
struct ENGINE_API FAnimNode_StateMachine : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

public:
	// Index into the BakedStateMachines array in the owning UAnimBlueprintGeneratedClass
	UPROPERTY()
	int32 StateMachineIndexInClass;

	// The maximum number of transitions that can be taken by this machine 'simultaneously' in a single frame
	UPROPERTY(EditAnywhere, Category=Settings)
	int32 MaxTransitionsPerFrame;

	// Skip transition from entry state on first update?
	// default is true, we throw away transition data on first update
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bSkipFirstUpdateTransition;

	// Reinitialize the state machine if we have become relevant to the graph
	// after not being ticked on the previous frame(s)
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bReinitializeOnBecomingRelevant;

public:

	int32 GetCurrentState() const
	{
		return CurrentState;
	}

	float GetCurrentStateElapsedTime() const
	{
		return ElapsedTime;
	}

	FName GetCurrentStateName() const;

	bool IsTransitionActive(int32 TransIndex) const;

protected:
	// The state machine description this is an instance of
	const FBakedAnimationStateMachine* PRIVATE_MachineDescription;

	// The current state within the state machine
	UPROPERTY()
	int32 CurrentState;

	// Elapsed time since entering the current state
	UPROPERTY()
	float ElapsedTime;

	// Current Transition Index being evaluated
	int32 EvaluatingTransitionIndex;

	// The set of active transitions, if there are any
	TArray<FAnimationActiveTransitionEntry> ActiveTransitionArray;

	// The set of states in this state machine
	TArray<FPoseLink> StatePoseLinks;
	
	// Used during transitions to make sure we don't double tick a state if it appears multiple times
	TArray<int32> StatesUpdated;

	// Delegates that native code can hook into to handle state entry
	TArray<FOnGraphStateChanged> OnGraphStatesEntered;

	// Delegates that native code can hook into to handle state exits
	TArray<FOnGraphStateChanged> OnGraphStatesExited;

private:
	// true if it is the first update.
	bool bFirstUpdate;

	TArray<FPoseContext*> StateCachedPoses;

	FGraphTraversalCounter UpdateCounter;

	TArray<FGraphTraversalCounter> StateCacheBoneCounters;

public:
	FAnimNode_StateMachine()
		: MaxTransitionsPerFrame(3)
		, bSkipFirstUpdateTransition(true)
		, bReinitializeOnBecomingRelevant(true)
		, PRIVATE_MachineDescription(NULL)
		, CurrentState(INDEX_NONE)
		, bFirstUpdate(true)
	{
	}

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

	void ConditionallyCacheBonesForState(int32 StateIndex, FAnimationBaseContext Context);

	// Returns the blend weight of the specified state, as calculated by the last call to Update()
	float GetStateWeight(int32 StateIndex) const;

	const FBakedAnimationState& GetStateInfo(int32 StateIndex) const;
	const FAnimationTransitionBetweenStates& GetTransitionInfo(int32 TransIndex) const;
	
	bool IsValidTransitionIndex(int32 TransitionIndex) const;

	/** Cache the internal machine description */
	void CacheMachineDescription(IAnimClassInterface* AnimBlueprintClass);

protected:
	// Tries to get the instance information for the state machine
	const FBakedAnimationStateMachine* GetMachineDescription() const;

	void SetState(const FAnimationBaseContext& Context, int32 NewStateIndex);
	void SetStateInternal(int32 NewStateIndex);

	const FBakedAnimationState& GetStateInfo() const;
	const int32 GetStateIndex(const FBakedAnimationState& StateInfo) const;
	
	// finds the highest priority valid transition, information pass via the OutPotentialTransition variable.
	// OutVisitedStateIndices will let you know what states were checked, but is also used to make sure we don't get stuck in an infinite loop or recheck states
	bool FindValidTransition(const FAnimationUpdateContext& Context, 
							const FBakedAnimationState& StateInfo,
							/*OUT*/ FAnimationPotentialTransition& OutPotentialTransition,
							/*OUT*/ TArray<int32, TInlineAllocator<4>>& OutVisitedStateIndices);

	// Helper function that will update the states associated with a transition
	void UpdateTransitionStates(const FAnimationUpdateContext& Context, FAnimationActiveTransitionEntry& Transition);

	// helper function to test if a state is a conduit
	bool IsAConduitState(int32 StateIndex) const;

	// helper functions for calling update and evaluate on state nodes
	void UpdateState(int32 StateIndex, const FAnimationUpdateContext& Context);
	const FPoseContext& EvaluateState(int32 StateIndex, const FPoseContext& Context);

	// transition type evaluation functions
	void EvaluateTransitionStandardBlend(FPoseContext& Output, FAnimationActiveTransitionEntry& Transition, bool bIntermediatePoseIsValid);
	void EvaluateTransitionStandardBlendInternal(FPoseContext& Output, FAnimationActiveTransitionEntry& Transition, const FPoseContext& PreviousStateResult, const FPoseContext& NextStateResult);
	void EvaluateTransitionCustomBlend(FPoseContext& Output, FAnimationActiveTransitionEntry& Transition, bool bIntermediatePoseIsValid);

	FAnimNode_AssetPlayerBase* GetRelevantAssetPlayerFromState(const FAnimationUpdateContext& Context, const FBakedAnimationState& StateInfo);

public:
	friend struct FAnimInstanceProxy;
};
