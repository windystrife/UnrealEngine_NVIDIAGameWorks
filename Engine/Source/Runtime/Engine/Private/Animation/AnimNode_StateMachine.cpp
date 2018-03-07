// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNode_StateMachine.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_TransitionResult.h"
#include "Animation/AnimNode_TransitionPoseEvaluator.h"
#include "Animation/AnimNode_AssetPlayerBase.h"
#include "Animation/BlendProfile.h"

DECLARE_CYCLE_STAT(TEXT("StateMachine SetState"), Stat_StateMachineSetState, STATGROUP_Anim);

//////////////////////////////////////////////////////////////////////////
// FAnimationActiveTransitionEntry

FAnimationActiveTransitionEntry::FAnimationActiveTransitionEntry()
	: ElapsedTime(0.0f)
	, Alpha(0.0f)
	, CrossfadeDuration(0.0f)
	, BlendOption(EAlphaBlendOption::HermiteCubic)
	, bActive(false)
	, NextState(INDEX_NONE)
	, PreviousState(INDEX_NONE)
	, StartNotify(INDEX_NONE)
	, EndNotify(INDEX_NONE)
	, InterruptNotify(INDEX_NONE)
	, LogicType(ETransitionLogicType::TLT_StandardBlend)
	, BlendProfile(nullptr)
{
}

FAnimationActiveTransitionEntry::FAnimationActiveTransitionEntry(int32 NextStateID, float ExistingWeightOfNextState, FAnimationActiveTransitionEntry* ExistingTransitionForNextState, int32 PreviousStateID, const FAnimationTransitionBetweenStates& ReferenceTransitionInfo)
	: ElapsedTime(0.0f)
	, Alpha(0.0f)
	, BlendOption(ReferenceTransitionInfo.BlendMode)
	, bActive(true)
	, NextState(NextStateID)
	, PreviousState(PreviousStateID)
	, StartNotify(ReferenceTransitionInfo.StartNotify)
	, EndNotify(ReferenceTransitionInfo.EndNotify)
	, InterruptNotify(ReferenceTransitionInfo.InterruptNotify)
	, LogicType(ReferenceTransitionInfo.LogicType)
	, BlendProfile(ReferenceTransitionInfo.BlendProfile)
{
	const float Scaler = 1.0f - ExistingWeightOfNextState;
	CrossfadeDuration = ReferenceTransitionInfo.CrossfadeDuration * CalculateInverseAlpha(BlendOption, Scaler);

	Blend.SetBlendTime(CrossfadeDuration);
	Blend.SetBlendOption(BlendOption);
	Blend.SetCustomCurve(ReferenceTransitionInfo.CustomCurve);
	Blend.SetValueRange(0.0f, 1.0f);
}

float FAnimationActiveTransitionEntry::CalculateInverseAlpha(EAlphaBlendOption BlendMode, float InFraction) const
{
	if(BlendMode == EAlphaBlendOption::HermiteCubic)
	{
		const float A = 4.0f / 3.0f;
		const float B = -2.0f;
		const float C = 5.0f / 3.0f;

		const float T = InFraction;
		const float TT = InFraction*InFraction;
		const float TTT = InFraction*InFraction*InFraction;

		return TTT*A + TT*B + T*C;
	}
	else
	{
		return FMath::Clamp<float>(InFraction, 0.0f, 1.0f);
	}
}

void FAnimationActiveTransitionEntry::InitializeCustomGraphLinks(const FAnimationUpdateContext& Context, const FBakedStateExitTransition& TransitionRule)
{
	if (TransitionRule.CustomResultNodeIndex != INDEX_NONE)
	{
		if (const IAnimClassInterface* AnimBlueprintClass = Context.GetAnimClass())
		{
			CustomTransitionGraph.LinkID = AnimBlueprintClass->GetAnimNodeProperties().Num() - 1 - TransitionRule.CustomResultNodeIndex; //@TODO: Crazysauce
			FAnimationInitializeContext InitContext(Context.AnimInstanceProxy);
			CustomTransitionGraph.Initialize(InitContext);

			if (Context.AnimInstanceProxy)
			{
				for (int32 Index = 0; Index < TransitionRule.PoseEvaluatorLinks.Num(); ++Index)
				{
					FAnimNode_TransitionPoseEvaluator* PoseEvaluator = GetNodeFromPropertyIndex<FAnimNode_TransitionPoseEvaluator>(Context.AnimInstanceProxy->GetAnimInstanceObject(), AnimBlueprintClass, TransitionRule.PoseEvaluatorLinks[Index]);
					PoseEvaluators.Add(PoseEvaluator);
				}
			}
		}
	}

	// Initialize blend data if necessary
	if(BlendProfile)
	{
		StateBlendData.AddZeroed(2);
		StateBlendData[0].PerBoneBlendData.AddZeroed(BlendProfile->GetNumBlendEntries());
		StateBlendData[1].PerBoneBlendData.AddZeroed(BlendProfile->GetNumBlendEntries());
	}
}

void FAnimationActiveTransitionEntry::Update(const FAnimationUpdateContext& Context, int32 CurrentStateIndex, bool& bOutFinished)
{
	bOutFinished = false;

	// Advance time
	if (bActive)
	{
		ElapsedTime += Context.GetDeltaTime();
		Blend.Update(Context.GetDeltaTime());

		float QueryAlpha = 1.0f;

		// If non-zero, calculate the query alpha
		if (CrossfadeDuration > 0.0f)
		{
			QueryAlpha = ElapsedTime / CrossfadeDuration;
		}

		Alpha = FAlphaBlend::AlphaToBlendOption(QueryAlpha, Blend.GetBlendOption(), Blend.GetCustomCurve());

		if(Blend.IsComplete())
		{
			bActive = false;
			bOutFinished = true;
		}

		// Update state blend data (only when we're using per-bone)
		if(BlendProfile)
		{
			for(int32 Idx = 0 ; Idx < 2 ; ++Idx)
			{
				bool bForwards = Idx == 0;
				FBlendSampleData& CurrentData = StateBlendData[Idx];

				CurrentData.TotalWeight = (bForwards) ? Alpha : 1.0f - Alpha;

				for(int32 PerBoneIndex = 0 ; PerBoneIndex < CurrentData.PerBoneBlendData.Num() ; ++PerBoneIndex)
				{
					float& BoneBlend = CurrentData.PerBoneBlendData[PerBoneIndex];
					float WeightScale = BlendProfile->GetEntryBlendScale(PerBoneIndex);

					if(!bForwards)
					{
						WeightScale = 1.0f / WeightScale;
					}

					BoneBlend = CurrentData.TotalWeight * WeightScale;
				}
			}

			FBlendSampleData::NormalizeDataWeight(StateBlendData);
		}
	}
}

bool FAnimationActiveTransitionEntry::Serialize(FArchive& Ar)
{
	Ar << ElapsedTime;
	Ar << Alpha;
	Ar << CrossfadeDuration;
	Ar << bActive;
	Ar << NextState;
	Ar << PreviousState;

	return true;
}

/////////////////////////////////////////////////////
// FAnimationPotentialTransition

FAnimationPotentialTransition::FAnimationPotentialTransition()
: 	TargetState(INDEX_NONE)
,	TransitionRule(NULL)
{
}

bool FAnimationPotentialTransition::IsValid() const
{
	return (TargetState != INDEX_NONE) && (TransitionRule != NULL) && (TransitionRule->TransitionIndex != INDEX_NONE);
}

void FAnimationPotentialTransition::Clear()
{
	TargetState = INDEX_NONE;
	TransitionRule = NULL;
	SourceTransitionIndices.Reset();
}


/////////////////////////////////////////////////////
// FAnimNode_StateMachine

// Tries to get the instance information for the state machine
const FBakedAnimationStateMachine* FAnimNode_StateMachine::GetMachineDescription() const
{
	if (PRIVATE_MachineDescription != nullptr)
	{
		return PRIVATE_MachineDescription;
	}
	else
	{
		UE_LOG(LogAnimation, Warning, TEXT("FAnimNode_StateMachine: Bad machine ptr"));
		return nullptr;
	}
}

void FAnimNode_StateMachine::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);

	IAnimClassInterface* AnimBlueprintClass = Context.GetAnimClass();

	if (const FBakedAnimationStateMachine* Machine = GetMachineDescription())
	{
		ElapsedTime = 0.0f;

		CurrentState = INDEX_NONE;

		if (Machine->States.Num() > 0)
		{
			// Create a pose link for each state we can reach
			StatePoseLinks.Reset();
			StatePoseLinks.Reserve(Machine->States.Num());
			for (int32 StateIndex = 0; StateIndex < Machine->States.Num(); ++StateIndex)
			{
				const FBakedAnimationState& State = Machine->States[StateIndex];
				FPoseLink* StatePoseLink = new (StatePoseLinks) FPoseLink();

				// because conduits don't contain bound graphs, this link is no longer guaranteed to be valid
				if (State.StateRootNodeIndex != INDEX_NONE)
				{
					StatePoseLink->LinkID = AnimBlueprintClass->GetAnimNodeProperties().Num() - 1 - State.StateRootNodeIndex; //@TODO: Crazysauce
				}

				// also initialize transitions
				if(State.EntryRuleNodeIndex != INDEX_NONE)
				{
					if (FAnimNode_TransitionResult* TransitionNode = GetNodeFromPropertyIndex<FAnimNode_TransitionResult>(Context.AnimInstanceProxy->GetAnimInstanceObject(), AnimBlueprintClass, State.EntryRuleNodeIndex))
					{
						TransitionNode->Initialize_AnyThread(Context);
					}
				}

				for(int32 TransitionIndex = 0; TransitionIndex < State.Transitions.Num(); ++TransitionIndex)
				{
					const FBakedStateExitTransition& TransitionRule = State.Transitions[TransitionIndex];
					if (TransitionRule.CanTakeDelegateIndex != INDEX_NONE)
					{
						if (FAnimNode_TransitionResult* TransitionNode = GetNodeFromPropertyIndex<FAnimNode_TransitionResult>(Context.AnimInstanceProxy->GetAnimInstanceObject(), AnimBlueprintClass, TransitionRule.CanTakeDelegateIndex))
						{
							TransitionNode->Initialize_AnyThread(Context);
						}
					}
				}
			}

			// Reset transition related variables
			StatesUpdated.Reset();
			ActiveTransitionArray.Reset();

			StateCacheBoneCounters.Reset(Machine->States.Num());
			StateCacheBoneCounters.AddDefaulted(Machine->States.Num());
		
			// Move to the default state
			SetState(Context, Machine->InitialState);

			// initialize first update
			bFirstUpdate = true;
		}
	}
}

void FAnimNode_StateMachine::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) 
{
	if (const FBakedAnimationStateMachine* Machine = GetMachineDescription())
	{
		for (int32 StateIndex = 0; StateIndex < Machine->States.Num(); ++StateIndex)
		{
			if (GetStateWeight(StateIndex) > 0.f)
			{
				ConditionallyCacheBonesForState(StateIndex, Context);
			}
		}
	}

	// @TODO GetStateWeight is O(N) transitions.
}

void FAnimNode_StateMachine::ConditionallyCacheBonesForState(int32 StateIndex, FAnimationBaseContext Context)
{
	// Only call CacheBones when needed.
	check(StateCacheBoneCounters.IsValidIndex(StateIndex));
	if (!StateCacheBoneCounters[StateIndex].IsSynchronizedWith(Context.AnimInstanceProxy->GetCachedBonesCounter()))
	{
		// keep track of states that have had CacheBones called on.
		StateCacheBoneCounters[StateIndex].SynchronizeWith(Context.AnimInstanceProxy->GetCachedBonesCounter());

		FAnimationCacheBonesContext CacheBoneContext(Context.AnimInstanceProxy);
		StatePoseLinks[StateIndex].CacheBones(CacheBoneContext);
	}
}

const FBakedAnimationState& FAnimNode_StateMachine::GetStateInfo() const
{
	return PRIVATE_MachineDescription->States[CurrentState];
}

const FBakedAnimationState& FAnimNode_StateMachine::GetStateInfo(int32 StateIndex) const
{
	return PRIVATE_MachineDescription->States[StateIndex];
}

const int32 FAnimNode_StateMachine::GetStateIndex( const FBakedAnimationState& StateInfo ) const
{
	for (int32 Index = 0; Index < PRIVATE_MachineDescription->States.Num(); ++Index)
	{
		if( &PRIVATE_MachineDescription->States[Index] == &StateInfo )
		{
			return Index;
		}
	}

	return INDEX_NONE;
}


const FAnimationTransitionBetweenStates& FAnimNode_StateMachine::GetTransitionInfo(int32 TransIndex) const
{
	return PRIVATE_MachineDescription->Transitions[TransIndex];
}

// Temporarily turned off while we track down and fix https://jira.ol.epicgames.net/browse/OR-17066
TAutoConsoleVariable<int32> CVarAnimStateMachineRelevancyReset(TEXT("a.AnimNode.StateMachine.EnableRelevancyReset"), 1, TEXT("Reset State Machine when it becomes relevant"));

void FAnimNode_StateMachine::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	Context.AnimInstanceProxy->RecordMachineWeight(StateMachineIndexInClass, Context.GetFinalBlendWeight());

	// If we just became relevant and haven't been initialized yet, then reinitialize state machine.
	if (!bFirstUpdate && bReinitializeOnBecomingRelevant &&(UpdateCounter.Get() != INDEX_NONE) && !UpdateCounter.WasSynchronizedInTheLastFrame(Context.AnimInstanceProxy->GetUpdateCounter()) && (CVarAnimStateMachineRelevancyReset.GetValueOnAnyThread() == 1))
	{
		FAnimationInitializeContext InitializationContext(Context.AnimInstanceProxy);
		Initialize_AnyThread(InitializationContext);
	}
	UpdateCounter.SynchronizeWith(Context.AnimInstanceProxy->GetUpdateCounter());

	const FBakedAnimationStateMachine* Machine = GetMachineDescription();
	if (Machine != nullptr)
	{
		if (Machine->States.Num() == 0)
		{
			return;
		}
		else if(!Machine->States.IsValidIndex(CurrentState))
		{
			// Attempting to catch a crash where the state machine has been freed.
			// Reported as a symptom of a crash in UE-24732 for 4.10. This log message should not appear given changes to
			// re-instancing in 4.11 (see CL 2823202). If it does appear we need to spot integrate CL 2823202 (and supporting 
			// anim re-init changes, probably 2799786 & 2801372).
			UE_LOG(LogAnimation, Warning, TEXT("FAnimNode_StateMachine::Update - Invalid current state, please report. Attempting to use state %d of %d in state machine %d (ptr 0x%x)"), CurrentState, Machine->States.Num(), StateMachineIndexInClass, Machine);
			UE_LOG(LogAnimation, Warning, TEXT("\t\tWhen updating AnimInstance: %s"), *Context.AnimInstanceProxy->GetAnimInstanceObject()->GetName())

			return;
		}
	}
	else
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_AnimStateMachineUpdate);
#if	STATS
	// Record name of state machine we are updating
	FScopeCycleCounter MachineNameCycleCounter(Machine->GetStatID());
#endif // STATS

	bool bFoundValidTransition = false;
	int32 TransitionCountThisFrame = 0;
	int32 TransitionIndex = INDEX_NONE;

	// Look for legal transitions to take; can move across multiple states in one frame (up to MaxTransitionsPerFrame)
	do
	{
		bFoundValidTransition = false;
		FAnimationPotentialTransition PotentialTransition;
		
		{
			SCOPE_CYCLE_COUNTER(STAT_AnimStateMachineFindTransition);

			// Evaluate possible transitions out of this state
			//@TODO: Evaluate if a set is better than an array for the probably low N encountered here
			TArray<int32, TInlineAllocator<4>> VisitedStateIndices;
			FindValidTransition(Context, GetStateInfo(), /*Out*/ PotentialTransition, /*Out*/ VisitedStateIndices);
		}
				
		// If transition is valid and not waiting on other conditions
		if (PotentialTransition.IsValid())
		{
			bFoundValidTransition = true;

			// let the latest transition know it has been interrupted
			if ((ActiveTransitionArray.Num() > 0) && ActiveTransitionArray[ActiveTransitionArray.Num()-1].bActive)
			{
				Context.AnimInstanceProxy->AddAnimNotifyFromGeneratedClass(ActiveTransitionArray[ActiveTransitionArray.Num()-1].InterruptNotify);
			}

			const int32 PreviousState = CurrentState;
			const int32 NextState = PotentialTransition.TargetState;

			// Fire off Notifies for state transition
			if (!bFirstUpdate || !bSkipFirstUpdateTransition)
			{
				Context.AnimInstanceProxy->AddAnimNotifyFromGeneratedClass(GetStateInfo(PreviousState).EndNotify);
				Context.AnimInstanceProxy->AddAnimNotifyFromGeneratedClass(GetStateInfo(NextState).StartNotify);
			}
			
			// Get the current weight of the next state, which may be non-zero
			const float ExistingWeightOfNextState = GetStateWeight(NextState);

			FAnimationActiveTransitionEntry* PreviousTransitionForNextState = nullptr;
			for(int32 i = ActiveTransitionArray.Num() - 1 ;  i >= 0 ; --i)
			{
				FAnimationActiveTransitionEntry& TransitionEntry = ActiveTransitionArray[i];
				if(TransitionEntry.PreviousState == NextState)
				{
					PreviousTransitionForNextState = &TransitionEntry;
					break;
				}
			}

			// Push the transition onto the stack
			const FAnimationTransitionBetweenStates& ReferenceTransition = GetTransitionInfo(PotentialTransition.TransitionRule->TransitionIndex); //-V595
			FAnimationActiveTransitionEntry* NewTransition = new (ActiveTransitionArray) FAnimationActiveTransitionEntry(NextState, ExistingWeightOfNextState, PreviousTransitionForNextState, PreviousState, ReferenceTransition);
			if (NewTransition && PotentialTransition.TransitionRule)
			{
				NewTransition->InitializeCustomGraphLinks(Context, *(PotentialTransition.TransitionRule));

#if WITH_EDITORONLY_DATA
				NewTransition->SourceTransitionIndices = PotentialTransition.SourceTransitionIndices;
#endif

				if (!bFirstUpdate)
				{
					Context.AnimInstanceProxy->AddAnimNotifyFromGeneratedClass(NewTransition->StartNotify);
				}
			}

			SetState(Context, NextState);

			TransitionCountThisFrame++;
		}
	}
	while (bFoundValidTransition && (TransitionCountThisFrame < MaxTransitionsPerFrame));

	if (bFirstUpdate)
	{
		if (bSkipFirstUpdateTransition)
		{
			//Handle enter notify for "first" (after initial transitions) state
			Context.AnimInstanceProxy->AddAnimNotifyFromGeneratedClass(GetStateInfo().StartNotify);
			// in the first update, we don't like to transition from entry state
			// so we throw out any transition data at the first update
			ActiveTransitionArray.Reset();
		}
		bFirstUpdate = false;
	}

	StatesUpdated.Reset();

	// Tick the individual state/states that are active
	if (ActiveTransitionArray.Num() > 0)
	{
		for (int32 Index = 0; Index < ActiveTransitionArray.Num(); ++Index)
		{
			// The custom graph will tick the needed states
			bool bFinishedTrans = false;

			// The custom graph will tick the needed states
			ActiveTransitionArray[Index].Update(Context, CurrentState, /*out*/ bFinishedTrans);
			
			if (bFinishedTrans)
			{
				// only play these events if it is the last transition (most recent, going to current state)
				if (Index == (ActiveTransitionArray.Num() - 1))
				{
					Context.AnimInstanceProxy->AddAnimNotifyFromGeneratedClass(ActiveTransitionArray[Index].EndNotify);
					Context.AnimInstanceProxy->AddAnimNotifyFromGeneratedClass(GetStateInfo().FullyBlendedNotify);
				}
			}
			else
			{
				// transition is still active, so tick the required states
				UpdateTransitionStates(Context, ActiveTransitionArray[Index]);
			}
		}
		
		// remove finished transitions here, newer transitions ending means any older ones must complete as well
		for (int32 Index = (ActiveTransitionArray.Num()-1); Index >= 0; --Index)
		{
			// if we find an inactive one, remove all older transitions and break out
			if (!ActiveTransitionArray[Index].bActive)
			{
				ActiveTransitionArray.RemoveAt(0, Index+1);
				break;
			}
		}
	}

	//@TODO: StatesUpdated.Contains is a linear search
	// Update the only active state if there are no transitions still in flight
	if (ActiveTransitionArray.Num() == 0 && !IsAConduitState(CurrentState) && !StatesUpdated.Contains(CurrentState))
	{
		StatePoseLinks[CurrentState].Update(Context);
		Context.AnimInstanceProxy->RecordStateWeight(StateMachineIndexInClass, CurrentState, GetStateWeight(CurrentState));
	}

	ElapsedTime += Context.GetDeltaTime();
}

FAnimNode_AssetPlayerBase* FAnimNode_StateMachine::GetRelevantAssetPlayerFromState(const FAnimationUpdateContext& Context, const FBakedAnimationState& StateInfo)
{
	FAnimNode_AssetPlayerBase* ResultPlayer = nullptr;
	float MaxWeight = 0.0f;
	for (const int32& PlayerIdx : StateInfo.PlayerNodeIndices)
	{
		if (FAnimNode_AssetPlayerBase* Player = Context.AnimInstanceProxy->GetNodeFromIndex<FAnimNode_AssetPlayerBase>(PlayerIdx))
		{
			if (!Player->bIgnoreForRelevancyTest && (Player->GetCachedBlendWeight() > MaxWeight))
			{
				MaxWeight = Player->GetCachedBlendWeight();
				ResultPlayer = Player;
			}
		}
	}
	return ResultPlayer;
}

bool FAnimNode_StateMachine::FindValidTransition(const FAnimationUpdateContext& Context, const FBakedAnimationState& StateInfo, /*out*/ FAnimationPotentialTransition& OutPotentialTransition, /*out*/ TArray<int32, TInlineAllocator<4>>& OutVisitedStateIndices)
{
	// There is a possibility we'll revisit states connected through conduits,
	// so we can avoid doing unnecessary work (and infinite loops) by caching off states we have already checked
	const int32 CheckingStateIndex = GetStateIndex(StateInfo);
	if (OutVisitedStateIndices.Contains(CheckingStateIndex))
	{
		return false;
	}
	OutVisitedStateIndices.Add(CheckingStateIndex);

	const IAnimClassInterface* AnimBlueprintClass = Context.GetAnimClass();

	// Conduit 'states' have an additional entry rule which must be true to consider taking any transitions via the conduit
	//@TODO: It would add flexibility to be able to define this on normal state nodes as well, assuming the dual-graph editing is sorted out
	if (FAnimNode_TransitionResult* StateEntryRuleNode = GetNodeFromPropertyIndex<FAnimNode_TransitionResult>(Context.AnimInstanceProxy->GetAnimInstanceObject(), AnimBlueprintClass, StateInfo.EntryRuleNodeIndex))
	{
		if (StateEntryRuleNode->NativeTransitionDelegate.IsBound())
		{
			// attempt to evaluate native rule
			StateEntryRuleNode->bCanEnterTransition = StateEntryRuleNode->NativeTransitionDelegate.Execute();
		}
		else
		{
			// Execute it and see if we can take this rule
			StateEntryRuleNode->EvaluateGraphExposedInputs.Execute(Context);
		}

		// not ok, back out
		if (!StateEntryRuleNode->bCanEnterTransition)
		{
			return false;
		}
	}

	const int32 NumTransitions = StateInfo.Transitions.Num();
	for (int32 TransitionIndex = 0; TransitionIndex < NumTransitions; ++TransitionIndex)
	{
		const FBakedStateExitTransition& TransitionRule = StateInfo.Transitions[TransitionIndex];
		if (TransitionRule.CanTakeDelegateIndex == INDEX_NONE)
		{
			continue;
		}

		FAnimNode_TransitionResult* ResultNode = GetNodeFromPropertyIndex<FAnimNode_TransitionResult>(Context.AnimInstanceProxy->GetAnimInstanceObject(), AnimBlueprintClass, TransitionRule.CanTakeDelegateIndex);

		if (ResultNode->NativeTransitionDelegate.IsBound())
		{
			// attempt to evaluate native rule
			ResultNode->bCanEnterTransition = ResultNode->NativeTransitionDelegate.Execute();
		}
		else if (TransitionRule.bAutomaticRemainingTimeRule)
		{
			bool bCanEnterTransition = false;
			if (FAnimNode_AssetPlayerBase* RelevantPlayer = GetRelevantAssetPlayerFromState(Context, StateInfo))
			{
				if (UAnimationAsset* AnimAsset = RelevantPlayer->GetAnimAsset())
				{
					const float AnimTimeRemaining = AnimAsset->GetMaxCurrentTime() - RelevantPlayer->GetAccumulatedTime();
					const FAnimationTransitionBetweenStates& TransitionInfo = GetTransitionInfo(TransitionRule.TransitionIndex);
					bCanEnterTransition = (AnimTimeRemaining <= TransitionInfo.CrossfadeDuration);
				}
			}
			ResultNode->bCanEnterTransition = bCanEnterTransition;
		}			
		else 
		{
			// Execute it and see if we can take this rule
			ResultNode->EvaluateGraphExposedInputs.Execute(Context);
		}

		if (ResultNode->bCanEnterTransition == TransitionRule.bDesiredTransitionReturnValue)
		{
			int32 NextState = GetTransitionInfo(TransitionRule.TransitionIndex).NextState;
			const FBakedAnimationState& NextStateInfo = GetStateInfo(NextState);

			// if next state is a conduit we want to check for transitions using that state as the root
			if (NextStateInfo.bIsAConduit)
			{
				if (FindValidTransition(Context, NextStateInfo, /*out*/ OutPotentialTransition, /*out*/ OutVisitedStateIndices))
				{
					OutPotentialTransition.SourceTransitionIndices.Add(TransitionRule.TransitionIndex);

					return true;
				}					
			}
			// otherwise we have found a content state, so we can record our potential transition
			else
			{
				// clear out any potential transition we already have
				OutPotentialTransition.Clear();

				// fill out the potential transition information
				OutPotentialTransition.TransitionRule = &TransitionRule;
				OutPotentialTransition.TargetState = NextState;

				OutPotentialTransition.SourceTransitionIndices.Add(TransitionRule.TransitionIndex);

				return true;
			}
		}
	}

	return false;
}

void FAnimNode_StateMachine::UpdateTransitionStates(const FAnimationUpdateContext& Context, FAnimationActiveTransitionEntry& Transition)
{
	if (Transition.bActive)
	{
		switch (Transition.LogicType)
		{
		case ETransitionLogicType::TLT_StandardBlend:
			{
				// update both states
				UpdateState(Transition.PreviousState, Context.FractionalWeight(1.0f - Transition.Alpha));
				UpdateState(Transition.NextState, Context.FractionalWeight(Transition.Alpha));
			}
			break;

		case ETransitionLogicType::TLT_Custom:
			{
				if (Transition.CustomTransitionGraph.LinkID != INDEX_NONE)
				{
					Transition.CustomTransitionGraph.Update(Context);

					for (TArray<FAnimNode_TransitionPoseEvaluator*>::TIterator PoseEvaluatorListIt = Transition.PoseEvaluators.CreateIterator(); PoseEvaluatorListIt; ++PoseEvaluatorListIt)
					{
						FAnimNode_TransitionPoseEvaluator* Evaluator = *PoseEvaluatorListIt;
						if (Evaluator->InputNodeNeedsUpdate())
						{
							const bool bUsePreviousState = (Evaluator->DataSource == EEvaluatorDataSource::EDS_SourcePose);
							const int32 EffectiveStateIndex = bUsePreviousState ? Transition.PreviousState : Transition.NextState;
							FAnimationUpdateContext ContextToUse = Context.FractionalWeight(bUsePreviousState ? (1.0f - Transition.Alpha) : Transition.Alpha);
							UpdateState(EffectiveStateIndex, ContextToUse);
						}
					}
				}
			}
			break;

		default:
			break;
		}
	}
}

void FAnimNode_StateMachine::Evaluate_AnyThread(FPoseContext& Output)
{
	if (const FBakedAnimationStateMachine* Machine = GetMachineDescription())
	{
		if (Machine->States.Num() == 0 || !Machine->States.IsValidIndex(CurrentState))
		{
			Output.Pose.ResetToRefPose();
			return;
		}
	}
	else
	{
		Output.Pose.ResetToRefPose();
		return;
	}

	ANIM_MT_SCOPE_CYCLE_COUNTER(EvaluateAnimStateMachine, !IsInGameThread());
	
	if (ActiveTransitionArray.Num() > 0)
	{
		check(Output.AnimInstanceProxy->GetSkeleton());
		
		check(StateCachedPoses.Num() == 0);
		StateCachedPoses.AddZeroed(StatePoseLinks.Num());

		//each transition stomps over the last because they will already include the output from the transition before it
		for (int32 Index = 0; Index < ActiveTransitionArray.Num(); ++Index)
		{
			// if there is any source pose, blend it here
			FAnimationActiveTransitionEntry& ActiveTransition = ActiveTransitionArray[Index];
			
			// when evaluating multiple transitions we need to store the pose from previous results
			// so we can feed the next transitions
			const bool bIntermediatePoseIsValid = Index > 0;

			if (ActiveTransition.bActive)
			{
				switch (ActiveTransition.LogicType)
				{
				case ETransitionLogicType::TLT_StandardBlend:
					EvaluateTransitionStandardBlend(Output, ActiveTransition, bIntermediatePoseIsValid);
					break;
				case ETransitionLogicType::TLT_Custom:
					EvaluateTransitionCustomBlend(Output, ActiveTransition, bIntermediatePoseIsValid);
					break;
				default:
					break;
				}
			}
		}

		// Ensure that all of the resulting rotations are normalized
		Output.Pose.NormalizeRotations();

		// Clear our cache
		for (auto CachedPose : StateCachedPoses)
		{
			delete CachedPose;
		}
		StateCachedPoses.Empty();
	}
	else if (!IsAConduitState(CurrentState))
	{
		// Make sure CacheBones has been called before evaluating.
		ConditionallyCacheBonesForState(CurrentState, Output);

		// Evaluate the current state
		StatePoseLinks[CurrentState].Evaluate(Output);
	}
}


void FAnimNode_StateMachine::EvaluateTransitionStandardBlend(FPoseContext& Output, FAnimationActiveTransitionEntry& Transition, bool bIntermediatePoseIsValid)
{
	if (bIntermediatePoseIsValid)
	{
		FPoseContext PreviousStateResult(Output); 
		PreviousStateResult = Output;
		const FPoseContext& NextStateResult = EvaluateState(Transition.NextState, Output);
		EvaluateTransitionStandardBlendInternal(Output, Transition, PreviousStateResult, NextStateResult);
	}
	else
	{
		const FPoseContext& PreviousStateResult = EvaluateState(Transition.PreviousState, Output);
		const FPoseContext& NextStateResult = EvaluateState(Transition.NextState, Output);
		EvaluateTransitionStandardBlendInternal(Output, Transition, PreviousStateResult, NextStateResult);
	}
}

void FAnimNode_StateMachine::EvaluateTransitionStandardBlendInternal(FPoseContext& Output, FAnimationActiveTransitionEntry& Transition, const FPoseContext& PreviousStateResult, const FPoseContext& NextStateResult)
{	
	// Blend it in
	const ScalarRegister VPreviousWeight(1.0f - Transition.Alpha);
	const ScalarRegister VWeight(Transition.Alpha);

	// If we have a blend profile we need to blend per bone
	if(Transition.BlendProfile)
	{
		for(FCompactPoseBoneIndex BoneIndex : Output.Pose.ForEachBoneIndex())
		{
			const int32 PerBoneIndex = Transition.BlendProfile->GetPerBoneInterpolationIndex(BoneIndex.GetInt(), Output.AnimInstanceProxy->GetRequiredBones());

			// Use defined per-bone scale if the bone has a scale specified in the blend profile
			ScalarRegister FirstWeight = PerBoneIndex != INDEX_NONE ? ScalarRegister(Transition.StateBlendData[1].PerBoneBlendData[PerBoneIndex]) : ScalarRegister(VPreviousWeight);
			ScalarRegister SecondWeight = PerBoneIndex != INDEX_NONE ? ScalarRegister(Transition.StateBlendData[0].PerBoneBlendData[PerBoneIndex]) : ScalarRegister(VWeight);
			Output.Pose[BoneIndex] = PreviousStateResult.Pose[BoneIndex] * FirstWeight;
			Output.Pose[BoneIndex].AccumulateWithShortestRotation(NextStateResult.Pose[BoneIndex], SecondWeight);
		}
	}
	else
		for(FCompactPoseBoneIndex BoneIndex : Output.Pose.ForEachBoneIndex())
		{
			Output.Pose[BoneIndex] = PreviousStateResult.Pose[BoneIndex] * VPreviousWeight;
			Output.Pose[BoneIndex].AccumulateWithShortestRotation(NextStateResult.Pose[BoneIndex], VWeight);
		}
	
	// blend curve in
	Output.Curve.Override(PreviousStateResult.Curve, 1.0 - Transition.Alpha);
	Output.Curve.Accumulate(NextStateResult.Curve, Transition.Alpha);
}

void FAnimNode_StateMachine::EvaluateTransitionCustomBlend(FPoseContext& Output, FAnimationActiveTransitionEntry& Transition, bool bIntermediatePoseIsValid)
{
	if (Transition.CustomTransitionGraph.LinkID != INDEX_NONE)
	{
		for (TArray<FAnimNode_TransitionPoseEvaluator*>::TIterator PoseEvaluatorListIt(Transition.PoseEvaluators); PoseEvaluatorListIt; ++PoseEvaluatorListIt)
		{
			FAnimNode_TransitionPoseEvaluator* Evaluator = *PoseEvaluatorListIt;
			if (Evaluator->InputNodeNeedsEvaluate())
			{
				// All input evaluators that use the intermediate pose can grab it from the current output.
				const bool bUseIntermediatePose = bIntermediatePoseIsValid && (Evaluator->DataSource == EEvaluatorDataSource::EDS_SourcePose);

				// otherwise we need to evaluate the nodes they reference
				if (!bUseIntermediatePose)
				{
					const bool bUsePreviousState = (Evaluator->DataSource == EEvaluatorDataSource::EDS_SourcePose);
					const int32 EffectiveStateIndex = bUsePreviousState ? Transition.PreviousState : Transition.NextState;
					const FPoseContext& PoseEvalResult = EvaluateState(EffectiveStateIndex, Output);

					// push transform to node.
					Evaluator->CachePose(PoseEvalResult);
				}
				else
				{
					// push transform to node.
					Evaluator->CachePose(Output);
				}
			}
		}

		FPoseContext StatePoseResult(Output);
		Transition.CustomTransitionGraph.Evaluate(StatePoseResult);

		// First pose will just overwrite the destination
		for (const FCompactPoseBoneIndex BoneIndex : Output.Pose.ForEachBoneIndex())
		{
			Output.Pose[BoneIndex] = StatePoseResult.Pose[BoneIndex];
		}

		// Copy curve over also, replacing current.
		Output.Curve.CopyFrom(StatePoseResult.Curve);
	}
}

void FAnimNode_StateMachine::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(%s->%s)"), *GetMachineDescription()->MachineName.ToString(), *GetStateInfo().StateName.ToString());

	DebugData.AddDebugItem(DebugLine);
	for (int32 PoseIndex = 0; PoseIndex < StatePoseLinks.Num(); ++PoseIndex)
	{
		FString StateName = FString::Printf(TEXT("(State: %s)"), *GetStateInfo(PoseIndex).StateName.ToString());
		StatePoseLinks[PoseIndex].GatherDebugData(DebugData.BranchFlow(GetStateWeight(PoseIndex), StateName));
	}
}

void FAnimNode_StateMachine::SetStateInternal(int32 NewStateIndex)
{
	checkSlow(PRIVATE_MachineDescription);
	ensure(!IsAConduitState(NewStateIndex));
	CurrentState = FMath::Clamp<int32>(NewStateIndex, 0, PRIVATE_MachineDescription->States.Num() - 1);
	check(CurrentState == NewStateIndex);
	ElapsedTime = 0.0f;
}

void FAnimNode_StateMachine::SetState(const FAnimationBaseContext& Context, int32 NewStateIndex)
{
	SCOPE_CYCLE_COUNTER(Stat_StateMachineSetState);

	if (NewStateIndex != CurrentState)
	{
		const int32 PrevStateIndex = CurrentState;
		if(CurrentState != INDEX_NONE && CurrentState < OnGraphStatesExited.Num())
		{
			OnGraphStatesExited[CurrentState].ExecuteIfBound(*this, CurrentState, NewStateIndex);
		}

		bool bForceReset = false;

		if(PRIVATE_MachineDescription->States.IsValidIndex(NewStateIndex))
		{
			const FBakedAnimationState& BakedCurrentState = PRIVATE_MachineDescription->States[NewStateIndex];
			bForceReset = BakedCurrentState.bAlwaysResetOnEntry;
		}

		// Determine if the new state is active or not
		const bool bAlreadyActive = GetStateWeight(NewStateIndex) > 0.0f;

		SetStateInternal(NewStateIndex);

		// Clear any currently cached blend weights for asset player nodes.
		// This stops any zero length blends holding on to old weights
		for(const int32& PlayerIndex : GetStateInfo(CurrentState).PlayerNodeIndices)
		{
			if(FAnimNode_AssetPlayerBase* Player = Context.AnimInstanceProxy->GetNodeFromIndex<FAnimNode_AssetPlayerBase>(PlayerIndex))
			{
				Player->ClearCachedBlendWeight();
			}
		}

		if ((!bAlreadyActive || bForceReset) && !IsAConduitState(NewStateIndex))
		{
			// Initialize the new state since it's not part of an active transition (and thus not still initialized)
			FAnimationInitializeContext InitContext(Context.AnimInstanceProxy);
			StatePoseLinks[NewStateIndex].Initialize(InitContext);

			// Also call cache bones if needed
			ConditionallyCacheBonesForState(NewStateIndex, Context);
		}

		if(CurrentState != INDEX_NONE && CurrentState < OnGraphStatesEntered.Num())
		{
			OnGraphStatesEntered[CurrentState].ExecuteIfBound(*this, PrevStateIndex, CurrentState);
		}
	}
}

float FAnimNode_StateMachine::GetStateWeight(int32 StateIndex) const
{
	const int32 NumTransitions = ActiveTransitionArray.Num();
	if (NumTransitions > 0)
	{
		// Determine the overall weight of the state here.
		float TotalWeight = 0.0f;
		for (int32 Index = 0; Index < NumTransitions; ++Index)
		{
			const FAnimationActiveTransitionEntry& Transition = ActiveTransitionArray[Index];

			float SourceWeight = (1.0f - Transition.Alpha);

			// After the first transition, so source weight is the fraction of how much all previous transitions contribute to the final weight.
			// So if our second transition is 50% complete, and our target state was 80% of the first transition, then that number will be multiplied by this weight
			if (Index > 0)
			{
				TotalWeight *= SourceWeight;
			}
			//during the first transition the source weight represents the actual state weight
			else if (Transition.PreviousState == StateIndex)
			{
				TotalWeight += SourceWeight;
			}

			// The next state weight is the alpha of this transition. We always just add the value, it will be reduced down if there are any newer transitions
			if (Transition.NextState == StateIndex)
			{
				TotalWeight += Transition.Alpha;
			}

		}

		return FMath::Clamp<float>(TotalWeight, 0.0f, 1.0f);
	}
	else
	{
		return (StateIndex == CurrentState) ? 1.0f : 0.0f;
	}
}

bool FAnimNode_StateMachine::IsTransitionActive(int32 TransIndex) const
{
	for (int32 Index = 0; Index < ActiveTransitionArray.Num(); ++Index)
	{
		if (ActiveTransitionArray[Index].SourceTransitionIndices.Contains(TransIndex))
		{
			return true;
		}
	}

	return false;
}

void FAnimNode_StateMachine::UpdateState(int32 StateIndex, const FAnimationUpdateContext& Context)
{
	if ((StateIndex != INDEX_NONE) && !StatesUpdated.Contains(StateIndex) && !IsAConduitState(StateIndex))
	{
		StatesUpdated.Add(StateIndex);
		StatePoseLinks[StateIndex].Update(Context);

		Context.AnimInstanceProxy->RecordStateWeight(StateMachineIndexInClass, StateIndex, GetStateWeight(StateIndex));
	}
}


const FPoseContext& FAnimNode_StateMachine::EvaluateState(int32 StateIndex, const FPoseContext& Context)
{
	check(StateCachedPoses.Num() == StatePoseLinks.Num());

	FPoseContext* CachePosePtr = StateCachedPoses[StateIndex];
	if (CachePosePtr == nullptr)
	{
		CachePosePtr = new FPoseContext(Context.AnimInstanceProxy);
		check(CachePosePtr);
		StateCachedPoses[StateIndex] = CachePosePtr;

		if (!IsAConduitState(StateIndex))
		{
			// Make sure CacheBones has been called before evaluating.
			ConditionallyCacheBonesForState(StateIndex, Context);

			StatePoseLinks[StateIndex].Evaluate(*CachePosePtr);
		}
	}

	return *CachePosePtr;
}

bool FAnimNode_StateMachine::IsAConduitState(int32 StateIndex) const
{
	return ((PRIVATE_MachineDescription != NULL) && (StateIndex < PRIVATE_MachineDescription->States.Num())) ? GetStateInfo(StateIndex).bIsAConduit : false;
}

bool FAnimNode_StateMachine::IsValidTransitionIndex(int32 TransitionIndex) const
{
	return PRIVATE_MachineDescription->Transitions.IsValidIndex(TransitionIndex);
}

FName FAnimNode_StateMachine::GetCurrentStateName() const
{
	if(PRIVATE_MachineDescription->States.IsValidIndex(CurrentState))
	{
		return GetStateInfo().StateName;
	}
	return NAME_None;
}

void FAnimNode_StateMachine::CacheMachineDescription(IAnimClassInterface* AnimBlueprintClass)
{
	PRIVATE_MachineDescription = AnimBlueprintClass->GetBakedStateMachines().IsValidIndex(StateMachineIndexInClass) ? &(AnimBlueprintClass->GetBakedStateMachines()[StateMachineIndexInClass]) : nullptr;
}
