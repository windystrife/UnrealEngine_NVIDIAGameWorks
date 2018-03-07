// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_RandomPlayer.h"
#include "AnimationRuntime.h"
#include "Animation/AnimInstanceProxy.h"

FAnimNode_RandomPlayer::FAnimNode_RandomPlayer()
: CurrentEntry(INDEX_NONE)
, NextEntry(INDEX_NONE)
, CurrentDataIndex(0)
{

}

void FAnimNode_RandomPlayer::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	EvaluateGraphExposedInputs.Execute(Context);

	const int32 NumEntries = Entries.Num();

	if(NumEntries == 0)
	{
		// early out here, no need to do anything at all if we're not playing anything
		return;
	}

	NormalizedPlayChances.Empty(NormalizedPlayChances.Num());
	NormalizedPlayChances.AddUninitialized(NumEntries);

	// Initialize normalized play chance for each entry and validate entry data
	float SumChances = 0.0f;
	for(FRandomPlayerSequenceEntry& Entry : Entries)
	{
		SumChances += Entry.ChanceToPlay;

		if(Entry.MaxLoopCount < Entry.MinLoopCount)
		{
			Swap(Entry.MaxLoopCount, Entry.MinLoopCount);
		}

		if(Entry.MaxPlayRate < Entry.MinPlayRate)
		{
			Swap(Entry.MaxLoopCount, Entry.MinLoopCount);
		}
	}

	for(int32 Idx = 0 ; Idx < NumEntries ; ++Idx)
	{
		NormalizedPlayChances[Idx] = Entries[Idx].ChanceToPlay / SumChances;
	}

	// Initialize random stream and pick first entry
	RandomStream.Initialize(FPlatformTime::Cycles());
	
	CurrentEntry = GetNextEntryIndex();
	NextEntry = GetNextEntryIndex();

	PlayData.Empty(2);
	PlayData.AddDefaulted(2);

	FRandomAnimPlayData& CurrentData = PlayData[GetDataIndex(ERandomDataIndexType::Current)];
	FRandomAnimPlayData& NextData = PlayData[GetDataIndex(ERandomDataIndexType::Next)];

	// Init play data
	CurrentData.BlendWeight = 1.0f;
	CurrentData.PlayRate = RandomStream.FRandRange(Entries[CurrentEntry].MinPlayRate, Entries[CurrentEntry].MaxPlayRate);
	CurrentData.RemainingLoops = FMath::Clamp(RandomStream.RandRange(Entries[CurrentEntry].MinLoopCount, Entries[CurrentEntry].MaxLoopCount), 0, MAX_int32);

	NextData.BlendWeight = 0.0f;
	NextData.PlayRate = RandomStream.FRandRange(Entries[NextEntry].MinPlayRate, Entries[NextEntry].MaxPlayRate);
	NextData.RemainingLoops = FMath::Clamp(RandomStream.RandRange(Entries[NextEntry].MinLoopCount, Entries[NextEntry].MaxLoopCount), 0, MAX_int32);
}

void FAnimNode_RandomPlayer::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	EvaluateGraphExposedInputs.Execute(Context);

	if(Entries.Num() == 0)
	{
		// We don't have any entries, play data will be invalid - early out
		return;
	}

	FRandomAnimPlayData* CurrentData = &PlayData[GetDataIndex(ERandomDataIndexType::Current)];
	FRandomAnimPlayData* NextData = &PlayData[GetDataIndex(ERandomDataIndexType::Next)];

	if(UAnimSequence* CurrentSequence = Entries[CurrentEntry].Sequence)
	{
		float TimeRemaining = CurrentSequence->SequenceLength - CurrentData->InternalTimeAccumulator;

		if(CurrentData->InternalTimeAccumulator < CurrentData->PreviousTimeAccumulator)
		{
			// We've looped, update remaining
			--CurrentData->RemainingLoops;

			if(CurrentData->RemainingLoops < 0)
			{
				// If we're switching to the same anim
				if(CurrentEntry == NextEntry)
				{
					// Need to switch to the next anim, but first put our accumulator in the next data we're about to switch
					// to so we don't see a pop
					NextData->InternalTimeAccumulator = CurrentData->InternalTimeAccumulator;
				}

				SwitchNextToCurrent();

				// Re-get data as we've switched over
				CurrentData = &PlayData[GetDataIndex(ERandomDataIndexType::Current)];
				NextData = &PlayData[GetDataIndex(ERandomDataIndexType::Next)];
			}
		}

		// Cache time to detect loops
		CurrentData->PreviousTimeAccumulator = CurrentData->InternalTimeAccumulator;
		NextData->PreviousTimeAccumulator = NextData->InternalTimeAccumulator;

		// If we're in the blend window start blending, but only if we're moving to a new animation,
		// otherwise just keep looping.
		const bool bInCrossfadeTime = TimeRemaining <= Entries[NextEntry].BlendIn.GetBlendTime();
		const bool bNextAnimIsDifferent = NextEntry != CurrentEntry;
		const bool bNeedMoreLoops = CurrentData->RemainingLoops > 0;

		if(bInCrossfadeTime && !bNeedMoreLoops)
		{
			if(bNextAnimIsDifferent)
			{
				// Blending to next
				Entries[NextEntry].BlendIn.Update(Context.GetDeltaTime());

				float BlendedAlpha = Entries[NextEntry].BlendIn.GetBlendedValue();

				if(BlendedAlpha < 1.0f)
				{
					NextData->BlendWeight = BlendedAlpha;
					CurrentData->BlendWeight = 1.0f - BlendedAlpha;
				}
			}
		}

		// If we were blending but now we're done, switch play data
		if(Entries[NextEntry].BlendIn.IsComplete())
		{
			SwitchNextToCurrent();

			// Re-get data as we've switched over
			CurrentData = &PlayData[GetDataIndex(ERandomDataIndexType::Current)];
			NextData = &PlayData[GetDataIndex(ERandomDataIndexType::Next)];
		}

		if(FAnimInstanceProxy* AnimProxy = Context.AnimInstanceProxy)
		{
			FAnimGroupInstance* SyncGroup;
			FAnimTickRecord& TickRecord = AnimProxy->CreateUninitializedTickRecord(INDEX_NONE, SyncGroup);
			AnimProxy->MakeSequenceTickRecord(TickRecord, Entries[CurrentEntry].Sequence, true, CurrentData->PlayRate, CurrentData->BlendWeight, CurrentData->InternalTimeAccumulator, CurrentData->MarkerTickRecord);

			if(NextData->BlendWeight > 0.0f)
			{
				FAnimTickRecord& NextTickRecord = AnimProxy->CreateUninitializedTickRecord(INDEX_NONE, SyncGroup);
				AnimProxy->MakeSequenceTickRecord(NextTickRecord, Entries[NextEntry].Sequence, true, NextData->PlayRate, NextData->BlendWeight, NextData->InternalTimeAccumulator, NextData->MarkerTickRecord);
			}
		}
	}
}

void FAnimNode_RandomPlayer::Evaluate_AnyThread(FPoseContext& Output)
{
	if(Entries.Num() > 0)
	{
		UAnimSequence* CurrentSequence = Entries[CurrentEntry].Sequence;

		if(CurrentSequence)
		{
			FRandomAnimPlayData& CurrentData = PlayData[GetDataIndex(ERandomDataIndexType::Current)];
			FRandomAnimPlayData& NextData = PlayData[GetDataIndex(ERandomDataIndexType::Next)];

			if(CurrentData.BlendWeight != 1.0f)
			{
				if(FAnimInstanceProxy* AnimProxy = Output.AnimInstanceProxy)
				{
					// Start Blending
					FCompactPose Poses[2];
					FBlendedCurve Curves[2];
					float Weights[2];

					const FBoneContainer& RequiredBone = AnimProxy->GetRequiredBones();
					Poses[0].SetBoneContainer(&RequiredBone);
					Poses[1].SetBoneContainer(&RequiredBone);

					Curves[0].InitFrom(RequiredBone);
					Curves[1].InitFrom(RequiredBone);

					Weights[0] = CurrentData.BlendWeight;
					Weights[1] = NextData.BlendWeight;

					UAnimSequence* NextSequence = Entries[NextEntry].Sequence;

					CurrentSequence->GetAnimationPose(Poses[0], Curves[0], FAnimExtractContext(CurrentData.InternalTimeAccumulator, AnimProxy->ShouldExtractRootMotion()));
					NextSequence->GetAnimationPose(Poses[1], Curves[1], FAnimExtractContext(NextData.InternalTimeAccumulator, AnimProxy->ShouldExtractRootMotion()));

					FAnimationRuntime::BlendPosesTogether(Poses, Curves, Weights, Output.Pose, Output.Curve);
				}
				else
				{
					Output.ResetToRefPose();
				}
			}
			else
			{
				// Single anim
				CurrentSequence->GetAnimationPose(Output.Pose, Output.Curve, FAnimExtractContext(CurrentData.InternalTimeAccumulator, Output.AnimInstanceProxy->ShouldExtractRootMotion()));
			}
		}
		else
		{
			Output.ResetToRefPose();
		}
	}
	else
	{
		Output.ResetToRefPose();
	}
}

void FAnimNode_RandomPlayer::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);

	DebugData.AddDebugItem(DebugLine, true);
}

int32 FAnimNode_RandomPlayer::GetNextEntryIndex()
{
	if(Entries.Num() > 0)
	{
		if(bShuffleMode)
		{
			if(ShuffleList.Num() == 0)
			{
				// Need a new list
				BuildShuffleList();
			}

			// Get the top value, don't allow realloc
			return ShuffleList.Pop(false);
		}
		else
		{
			float RandomVal = RandomStream.GetFraction();
			const int32 NumEntries = Entries.Num();

			// Grab the entry index corresponding to the value
			for(int32 Idx = 0 ; Idx < NumEntries ; ++Idx)
			{
				RandomVal -= NormalizedPlayChances[Idx];
				if(RandomVal <= 0.0f)
				{
					return Idx;
				}
			}
		}
	}

	return INDEX_NONE;
}

int32 FAnimNode_RandomPlayer::GetDataIndex(const ERandomDataIndexType& Type)
{
	if(Type == ERandomDataIndexType::Current)
	{
		return CurrentDataIndex;
	}
	else
	{
		// Next Accumulator
		return (CurrentDataIndex + 1) % 2;
	}
}

void FAnimNode_RandomPlayer::SwitchNextToCurrent()
{
	// reset alpha blend we've possibly just taken
	Entries[NextEntry].BlendIn.Reset();

	// Switch which entry to get sequences and parameters from, and pre-generate the next entry index
	CurrentEntry = NextEntry;
	NextEntry = GetNextEntryIndex();

	// Switch play data
	CurrentDataIndex = (CurrentDataIndex + 1) %2;

	// Get our play data
	FRandomAnimPlayData& CurrentData = PlayData[GetDataIndex(ERandomDataIndexType::Current)];
	FRandomAnimPlayData& NextData = PlayData[GetDataIndex(ERandomDataIndexType::Next)];

	// Reset blendweights
	CurrentData.BlendWeight = 1.0f;
	NextData.BlendWeight = 0.0f;

	// Set up data for next switch
	NextData.InternalTimeAccumulator = 0.0f;
	NextData.PreviousTimeAccumulator = 0.0f;
	NextData.PlayRate = RandomStream.FRandRange(Entries[NextEntry].MinPlayRate, Entries[NextEntry].MaxPlayRate);
	NextData.RemainingLoops = FMath::Clamp(RandomStream.RandRange(Entries[NextEntry].MinLoopCount, Entries[NextEntry].MaxLoopCount), 0, MAX_int32);
	NextData.MarkerTickRecord.Reset();
}

void FAnimNode_RandomPlayer::BuildShuffleList()
{
	ShuffleList.Reset(Entries.Num());

	// Build entry index list
	const int32 NumEntries = Entries.Num();
	for(int32 i = 0 ; i < NumEntries ; ++i)
	{
		ShuffleList.Add(i);
	}

	// Shuffle the list
	const int32 NumShuffles = ShuffleList.Num() - 1;
	for(int32 i = 0 ; i < NumShuffles ; ++i)
	{
		int32 SwapIdx = RandomStream.RandRange(i, NumShuffles);
		ShuffleList.Swap(i, SwapIdx);
	}

	if(ShuffleList.Num() > 1)
	{
		// Make sure we don't play the same thing twice, one at the end and one at the beginning of
		// the list
		if(ShuffleList.Last() == CurrentEntry)
		{
			// Swap first and last
			ShuffleList.Swap(0, ShuffleList.Num() - 1);
		}
	}
}
