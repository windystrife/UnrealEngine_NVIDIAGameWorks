// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSequenceInstance.h"

DECLARE_CYCLE_STAT(TEXT("Entire Evaluation Cost"), MovieSceneEval_EntireEvaluationCost, STATGROUP_MovieSceneEval);
DECLARE_CYCLE_STAT(TEXT("Gather Entries For Frame"), MovieSceneEval_GatherEntries, STATGROUP_MovieSceneEval);
DECLARE_CYCLE_STAT(TEXT("Call Setup() and TearDown()"), MovieSceneEval_CallSetupTearDown, STATGROUP_MovieSceneEval);
DECLARE_CYCLE_STAT(TEXT("Evaluate Group"), MovieSceneEval_EvaluateGroup, STATGROUP_MovieSceneEval);

/** Scoped helper class that facilitates the delayed restoration of preanimated state for specific evaluation keys */
struct FDelayedPreAnimatedStateRestore
{
	FDelayedPreAnimatedStateRestore(IMovieScenePlayer& InPlayer)
		: Player(InPlayer)
	{}

	~FDelayedPreAnimatedStateRestore()
	{
		RestoreNow();
	}

	void Add(FMovieSceneEvaluationKey Key)
	{
		KeysToRestore.Add(Key);
	}

	void RestoreNow()
	{
		for (FMovieSceneEvaluationKey Key : KeysToRestore)
		{
			Player.PreAnimatedState.RestorePreAnimatedState(Player, Key);
		}
		KeysToRestore.Reset();
	}

private:
	/** The movie scene player to restore with */
	IMovieScenePlayer& Player;
	/** The array of keys to restore */
	TArray<FMovieSceneEvaluationKey> KeysToRestore;
};

FMovieSceneEvaluationTemplateInstance::FMovieSceneEvaluationTemplateInstance(UMovieSceneSequence& InSequence, const FMovieSceneEvaluationTemplate& InTemplate)
	: Sequence(&InSequence)
	, Template(&InTemplate)
	, PreRollRange(TRange<float>::Empty())
	, PostRollRange(TRange<float>::Empty())
	, HierarchicalBias(0)
{
	if (Template->bHasLegacyTrackInstances)
	{
		// Initialize the legacy sequence instance
		LegacySequenceInstance = MakeShareable(new FMovieSceneSequenceInstance(InSequence, MovieSceneSequenceID::Root));
	}
}

FMovieSceneEvaluationTemplateInstance::FMovieSceneEvaluationTemplateInstance(const FMovieSceneSubSequenceData& InSubData, const FMovieSceneEvaluationTemplate& InTemplate, FMovieSceneSequenceIDRef InSequenceID)
	: Sequence(InSubData.Sequence)
	, RootToSequenceTransform(InSubData.RootToSequenceTransform)
	, Template(&InTemplate)
	, PreRollRange(InSubData.PreRollRange)
	, PostRollRange(InSubData.PostRollRange)
	, HierarchicalBias(InSubData.HierarchicalBias)
{
	if (Template->bHasLegacyTrackInstances)
	{
		// Initialize the legacy sequence instance
		LegacySequenceInstance = MakeShareable(new FMovieSceneSequenceInstance(*InSubData.Sequence, InSequenceID));
	}
}

FMovieSceneRootEvaluationTemplateInstance::FMovieSceneRootEvaluationTemplateInstance()
	: RootSequence(nullptr)
	, TemplateStore(MakeShareable(new FMovieSceneSequenceTemplateStore))
	, bIsDirty(false)
{

}

FMovieSceneRootEvaluationTemplateInstance::~FMovieSceneRootEvaluationTemplateInstance()
{
	Reset();
}

void FMovieSceneRootEvaluationTemplateInstance::Reset()
{
	if (TemplateStore->AreTemplatesVolatile())
	{
		if (UMovieSceneSequence* Sequence = RootInstance.Sequence.Get())
		{
			Sequence->OnSignatureChanged().RemoveAll(this);
		}

		for (auto& Pair : SubInstances)
		{
			if (UMovieSceneSequence* Sequence = Pair.Value.Sequence.Get())
			{
				Sequence->OnSignatureChanged().RemoveAll(this);
			}
		}
	}

	SubInstances.Reset();
}

void FMovieSceneRootEvaluationTemplateInstance::Initialize(UMovieSceneSequence& InRootSequence, IMovieScenePlayer& Player, TSharedRef<FMovieSceneSequenceTemplateStore> InTemplateStore)
{
	if (RootSequence != &InRootSequence)
	{
		Finish(Player);
	}

	// Ensure we reset everything before we overwrite the template store (which potentially owns templates we've previously referenced)
	Reset();

	TemplateStore = MoveTemp(InTemplateStore);

	Initialize(InRootSequence, Player);
}

void FMovieSceneRootEvaluationTemplateInstance::Initialize(UMovieSceneSequence& InRootSequence, IMovieScenePlayer& Player)
{
	Reset();

	if (RootSequence.Get() != &InRootSequence)
	{
		// Always ensure that there is no persistent data when initializing a new sequence
		// to ensure we don't collide with the previous sequence's entity keys
		Player.State.PersistentEntityData.Reset();
		Player.State.PersistentSharedData.Reset();

		LastFrameMetaData.Reset();
		ThisFrameMetaData.Reset();
		ExecutionTokens = FMovieSceneExecutionTokens();
	}

	const bool bAddEvents = TemplateStore->AreTemplatesVolatile();
	
	RootSequence = &InRootSequence;

	const FMovieSceneEvaluationTemplate& RootTemplate = TemplateStore->GetCompiledTemplate(InRootSequence);
	
	Player.State.AssignSequence(MovieSceneSequenceID::Root, InRootSequence, Player);
	RootInstance = FMovieSceneEvaluationTemplateInstance(InRootSequence, RootTemplate);

	if (bAddEvents)
	{
		InRootSequence.OnSignatureChanged().AddRaw(this, &FMovieSceneRootEvaluationTemplateInstance::OnSequenceChanged);
	}

	for (auto& Pair : RootTemplate.Hierarchy.AllSubSequenceData())
	{
		const FMovieSceneSubSequenceData& Data = Pair.Value;
		FMovieSceneSequenceID SequenceID = Pair.Key;

		if (!Data.Sequence)
		{
			continue;
		}

		Player.State.AssignSequence(SequenceID, *Data.Sequence, Player);

		const FMovieSceneEvaluationTemplate& ChildTemplate = TemplateStore->GetCompiledTemplate(*Data.Sequence, FObjectKey(Data.SequenceKeyObject));
		FMovieSceneEvaluationTemplateInstance& Instance = SubInstances.Add(SequenceID, FMovieSceneEvaluationTemplateInstance(Data, ChildTemplate, SequenceID));

		if (bAddEvents)
		{
			Data.Sequence->OnSignatureChanged().AddRaw(this, &FMovieSceneRootEvaluationTemplateInstance::OnSequenceChanged);
		}
	}

	bIsDirty = false;

	OnUpdatedEvent.Broadcast();
}

void FMovieSceneRootEvaluationTemplateInstance::Finish(IMovieScenePlayer& Player)
{
	Swap(ThisFrameMetaData, LastFrameMetaData);
	ThisFrameMetaData.Reset();

	CallSetupTearDown(Player);
}

void FMovieSceneRootEvaluationTemplateInstance::Evaluate(FMovieSceneContext Context, IMovieScenePlayer& Player, FMovieSceneSequenceID OverrideRootID)
{
	if (bIsDirty && RootSequence.IsValid())
	{
		Initialize(*RootSequence.Get(), Player);
	}

	SCOPE_CYCLE_COUNTER(MovieSceneEval_EntireEvaluationCost);

	Swap(ThisFrameMetaData, LastFrameMetaData);
	ThisFrameMetaData.Reset();

	const FMovieSceneEvaluationTemplateInstance* Instance = GetInstance(OverrideRootID);

	ensureMsgf(Instance, TEXT("Could not find instance for supplied sequence ID."));

	const int32 FieldIndex = (Instance && Instance->Template) ? Instance->Template->EvaluationField.GetSegmentFromTime(Context.GetTime() * Instance->RootToSequenceTransform) : INDEX_NONE;
	if (FieldIndex == INDEX_NONE)
	{
		CallSetupTearDown(Player);
		return;
	}

	// Construct a path that allows us to remap sequence IDs from the local (OverrideRootID) template, to the master template
	ReverseOverrideRootPath.Reset();
	{
		FMovieSceneSequenceID CurrentSequenceID = OverrideRootID;
		const FMovieSceneSequenceHierarchy& Hierarchy = GetHierarchy();
		while (CurrentSequenceID != MovieSceneSequenceID::Root)
		{
			const FMovieSceneSequenceHierarchyNode* CurrentNode = Hierarchy.FindNode(CurrentSequenceID);
			const FMovieSceneSubSequenceData* SubData = Hierarchy.FindSubData(CurrentSequenceID);
			if (!ensureAlwaysMsgf(CurrentNode && SubData, TEXT("Malformed sequence hierarchy")))
			{
				return;
			}

			ReverseOverrideRootPath.Add(SubData->DeterministicSequenceID);
			CurrentSequenceID = CurrentNode->ParentID;
		}
	}

	const FMovieSceneEvaluationGroup& Group = Instance->Template->EvaluationField.Groups[FieldIndex];

	// Take a copy of this frame's meta-data
	ThisFrameMetaData = Instance->Template->EvaluationField.MetaData[FieldIndex];
	if (OverrideRootID != MovieSceneSequenceID::Root)
	{
		ThisFrameMetaData.RemapSequenceIDsForRoot(OverrideRootID);
	}

	// Cause stale tracks to not restore until after evaluation. This fixes issues when tracks that are set to 'Restore State' are regenerated, causing the state to be restored then re-animated by the new track
	FDelayedPreAnimatedStateRestore DelayedRestore(Player);

	// Run the post root evaluate steps which invoke tear downs for anything no longer evaluated.
	// Do this now to ensure they don't undo any of the current frame's execution tokens 
	CallSetupTearDown(Player, &DelayedRestore);

	// Ensure any null objects are not cached
	Player.State.InvalidateExpiredObjects();

	// Accumulate execution tokens into this structure
	EvaluateGroup(Group, Context, Player);

	// Process execution tokens
	ExecutionTokens.Apply(Context, Player);
}

void FMovieSceneRootEvaluationTemplateInstance::EvaluateGroup(const FMovieSceneEvaluationGroup& Group, const FMovieSceneContext& RootContext, IMovieScenePlayer& Player)
{
	MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_EvaluateGroup);

	FPersistentEvaluationData PersistentDataProxy(Player.State.PersistentEntityData, Player.State.PersistentSharedData);
	
	FMovieSceneEvaluationOperand Operand;

	FMovieSceneContext Context = RootContext;
	FMovieSceneContext SubContext = Context;

	for (const FMovieSceneEvaluationGroupLUTIndex& Index : Group.LUTIndices)
	{
		int32 TrackIndex = Index.LUTOffset;
		
		// Initialize anything that wants to be initialized first
		for ( ; TrackIndex < Index.LUTOffset + Index.NumInitPtrs; ++TrackIndex)
		{
			FMovieSceneEvaluationFieldSegmentPtr SegmentPtr = Group.SegmentPtrLUT[TrackIndex];

			// Ensure we're able to find the sequence instance in our root if we've overridden
			SegmentPtr.SequenceID = GetSequenceIdForRoot(SegmentPtr.SequenceID);

			const FMovieSceneEvaluationTemplateInstance& Instance = GetInstanceChecked(SegmentPtr.SequenceID);
			const FMovieSceneEvaluationTrack* Track = Instance.Template->FindTrack(SegmentPtr.TrackIdentifier);

			if (Track)
			{
				Operand.ObjectBindingID = Track->GetObjectBindingID();
				Operand.SequenceID = SegmentPtr.SequenceID;
				
				FMovieSceneEvaluationKey TrackKey(SegmentPtr.SequenceID, SegmentPtr.TrackIdentifier);

				PersistentDataProxy.SetTrackKey(TrackKey);
				Player.PreAnimatedState.SetCaptureEntity(TrackKey, EMovieSceneCompletionMode::KeepState);

				SubContext = Context;
				if (SegmentPtr.SequenceID != MovieSceneSequenceID::Root)
				{
					SubContext = Context.Transform(Instance.RootToSequenceTransform);

					// Hittest against the sequence's pre and postroll ranges
					SubContext.ReportOuterSectionRanges(Instance.PreRollRange, Instance.PostRollRange);
					SubContext.SetHierarchicalBias(Instance.HierarchicalBias);
				}

				Track->Initialize(SegmentPtr.SegmentIndex, Operand, SubContext, PersistentDataProxy, Player);
			}
		}

		// Then evaluate

		// *Threading candidate*
		// @todo: if we want to make this threaded, we need to:
		//  - Make the execution tokens threadsafe, and sortable (one container per thread + append?)
		//  - Do the above in a lockless manner
		for (; TrackIndex < Index.LUTOffset + Index.NumInitPtrs + Index.NumEvalPtrs; ++TrackIndex)
		{
			FMovieSceneEvaluationFieldSegmentPtr SegmentPtr = Group.SegmentPtrLUT[TrackIndex];
			
			// Ensure we're able to find the sequence instance in our root if we've overridden
			SegmentPtr.SequenceID = GetSequenceIdForRoot(SegmentPtr.SequenceID);

			const FMovieSceneEvaluationTemplateInstance& Instance = GetInstanceChecked(SegmentPtr.SequenceID);
			const FMovieSceneEvaluationTrack* Track = Instance.Template->FindTrack(SegmentPtr.TrackIdentifier);

			if (Track)
			{
				Operand.ObjectBindingID = Track->GetObjectBindingID();
				Operand.SequenceID = SegmentPtr.SequenceID;

				FMovieSceneEvaluationKey TrackKey(SegmentPtr.SequenceID, SegmentPtr.TrackIdentifier);

				PersistentDataProxy.SetTrackKey(TrackKey);

				ExecutionTokens.SetOperand(Operand);
				ExecutionTokens.SetCurrentScope(FMovieSceneEvaluationScope(TrackKey, EMovieSceneCompletionMode::KeepState));

				SubContext = Context;
				if (SegmentPtr.SequenceID != MovieSceneSequenceID::Root)
				{
					SubContext = Context.Transform(Instance.RootToSequenceTransform);

					// Hittest against the sequence's pre and postroll ranges
					SubContext.ReportOuterSectionRanges(Instance.PreRollRange, Instance.PostRollRange);
					SubContext.SetHierarchicalBias(Instance.HierarchicalBias);
				}

				Track->Evaluate(
					SegmentPtr.SegmentIndex,
					Operand,
					SubContext,
					PersistentDataProxy,
					ExecutionTokens);
			}
		}

		ExecutionTokens.Apply(Context, Player);
	}
}

void FMovieSceneRootEvaluationTemplateInstance::CallSetupTearDown(IMovieScenePlayer& Player, FDelayedPreAnimatedStateRestore* DelayedRestore)
{
	MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_CallSetupTearDown);

	FPersistentEvaluationData PersistentDataProxy(Player.State.PersistentEntityData, Player.State.PersistentSharedData);

	TArray<FMovieSceneOrderedEvaluationKey> ExpiredEntities;
	TArray<FMovieSceneOrderedEvaluationKey> NewEntities;
	ThisFrameMetaData.DiffEntities(LastFrameMetaData, &NewEntities, &ExpiredEntities);
	
	for (const FMovieSceneOrderedEvaluationKey& OrderedKey : ExpiredEntities)
	{
		const FMovieSceneEvaluationKey Key = OrderedKey.Key;

		const FMovieSceneEvaluationTemplateInstance* Instance = FindInstance(Key.SequenceID);
		if (Instance)
		{
			const FMovieSceneEvaluationTrack* Track = Instance->Template->FindTrack(Key.TrackIdentifier);
			const bool bStaleTrack = Instance->Template->IsTrackStale(Key.TrackIdentifier);

			// Track data key may be required by both tracks and sections
			PersistentDataProxy.SetTrackKey(Key.AsTrack());

			if (Key.SectionIdentifier == uint32(-1))
			{
				if (Track)
				{
					Track->OnEndEvaluation(PersistentDataProxy, Player);
				}
				
				PersistentDataProxy.ResetTrackData();
			}
			else
			{
				PersistentDataProxy.SetSectionKey(Key);
				if (Track)
				{
					Track->GetChildTemplate(Key.SectionIdentifier).OnEndEvaluation(PersistentDataProxy, Player);
				}

				PersistentDataProxy.ResetSectionData();
			}

			if (bStaleTrack && DelayedRestore)
			{
				DelayedRestore->Add(Key);
			}
			else
			{
				Player.PreAnimatedState.RestorePreAnimatedState(Player, Key);
			}
		}
	}

	for (const FMovieSceneOrderedEvaluationKey& OrderedKey : NewEntities)
	{
		const FMovieSceneEvaluationKey Key = OrderedKey.Key;

		const FMovieSceneEvaluationTemplateInstance& Instance = GetInstanceChecked(Key.SequenceID);

		if (const FMovieSceneEvaluationTrack* Track = Instance.Template->FindTrack(Key.TrackIdentifier))
		{
			PersistentDataProxy.SetTrackKey(Key.AsTrack());

			if (Key.SectionIdentifier == uint32(-1))
			{
				Track->OnBeginEvaluation(PersistentDataProxy, Player);
			}
			else
			{
				PersistentDataProxy.SetSectionKey(Key);
				Track->GetChildTemplate(Key.SectionIdentifier).OnBeginEvaluation(PersistentDataProxy, Player);
			}
		}
	}

	// Tear down spawned objects
	FMovieSceneSpawnRegister& Register = Player.GetSpawnRegister();

	TArray<FMovieSceneSequenceID> ExpiredSequenceIDs;
	ThisFrameMetaData.DiffSequences(LastFrameMetaData, nullptr, &ExpiredSequenceIDs);
	for (FMovieSceneSequenceID ExpiredID : ExpiredSequenceIDs)
	{
		Register.OnSequenceExpired(ExpiredID, Player);
	}
}

void FMovieSceneRootEvaluationTemplateInstance::CopyActuators(FMovieSceneBlendingAccumulator& Accumulator) const
{
	Accumulator.Actuators = ExecutionTokens.GetBlendingAccumulator().Actuators;
}

void FMovieSceneRootEvaluationTemplateInstance::OnSequenceChanged()
{
	bIsDirty = true;
}
