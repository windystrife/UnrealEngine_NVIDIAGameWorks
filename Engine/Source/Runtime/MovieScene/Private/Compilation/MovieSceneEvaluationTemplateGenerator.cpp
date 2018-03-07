// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Compilation/MovieSceneEvaluationTemplateGenerator.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "Algo/Sort.h"
#include "UObject/ObjectKey.h"
#include "IMovieSceneModule.h"

namespace 
{
	void AddPtrsToGroup(FMovieSceneEvaluationGroup& Group, TArray<FMovieSceneEvaluationFieldSegmentPtr>& InitPtrs, TArray<FMovieSceneEvaluationFieldSegmentPtr>& EvalPtrs)
	{
		if (!InitPtrs.Num() && !EvalPtrs.Num())
		{
			return;
		}

		FMovieSceneEvaluationGroupLUTIndex Index;

		Index.LUTOffset = Group.SegmentPtrLUT.Num();
		Index.NumInitPtrs = InitPtrs.Num();
		Index.NumEvalPtrs = EvalPtrs.Num();

		Group.LUTIndices.Add(Index);
		Group.SegmentPtrLUT.Append(InitPtrs);
		Group.SegmentPtrLUT.Append(EvalPtrs);

		InitPtrs.Reset();
		EvalPtrs.Reset();
	}
}

FMovieSceneEvaluationTemplateGenerator::FMovieSceneEvaluationTemplateGenerator(UMovieSceneSequence& InSequence, FMovieSceneEvaluationTemplate& OutTemplate, FMovieSceneSequenceTemplateStore& InStore)
	: SourceSequence(InSequence)
	, Template(OutTemplate)
	, TransientArgs(*this, InStore)
{
}

void FMovieSceneEvaluationTemplateGenerator::AddLegacyTrack(FMovieSceneEvaluationTrack&& InTrackTemplate, const UMovieSceneTrack& SourceTrack)
{
	AddOwnedTrack(MoveTemp(InTrackTemplate), SourceTrack);
	Template.bHasLegacyTrackInstances = true;
}

void FMovieSceneEvaluationTemplateGenerator::AddOwnedTrack(FMovieSceneEvaluationTrack&& InTrackTemplate, const UMovieSceneTrack& SourceTrack)
{
	// Add the track to the template
	Template.AddTrack(SourceTrack.GetSignature(), MoveTemp(InTrackTemplate));
	CompiledSignatures.Add(SourceTrack.GetSignature());
}

void FMovieSceneEvaluationTemplateGenerator::AddSharedTrack(FMovieSceneEvaluationTrack&& InTrackTemplate, FMovieSceneSharedDataId SharedId, const UMovieSceneTrack& SourceTrack)
{
	FSharedPersistentDataKey Key(SharedId, FMovieSceneEvaluationOperand(MovieSceneSequenceID::Root, InTrackTemplate.GetObjectBindingID()));

	if (AddedSharedTracks.Contains(Key))
	{
		return;
	}

	AddedSharedTracks.Add(Key);
	AddOwnedTrack(MoveTemp(InTrackTemplate), SourceTrack);
}

void FMovieSceneEvaluationTemplateGenerator::AddExternalSegments(TRange<float> RootRange, TArrayView<const FMovieSceneEvaluationFieldSegmentPtr> SegmentPtrs, ESectionEvaluationFlags Flags)
{
	if (RootRange.IsEmpty())
	{
		return;
	}

	SegmentData.Reserve(SegmentData.Num() + SegmentPtrs.Num());

	for (const FMovieSceneEvaluationFieldSegmentPtr& SegmentPtr : SegmentPtrs)
	{
		// Add one segment to the external segment map per flag configuration
		FExternalSegment NewSegment{ -1, Flags };
		for (auto It = ExternalSegmentLookup.CreateConstKeyIterator(SegmentPtr); It; ++It)
		{
			if (It.Value().Flags == Flags)
			{
				NewSegment = It.Value();
				break;
			}
		}

		if (NewSegment.Index == -1)
		{
			NewSegment.Index = TrackLUT.Num();
			ExternalSegmentLookup.Add(SegmentPtr, NewSegment);
			TrackLUT.Add(SegmentPtr);
		}

		// Flags are irrelevant for the final segment compilation
		SegmentData.Add(FMovieSceneSectionData(RootRange, FSectionEvaluationData(NewSegment.Index)));
	}
}

FMovieSceneSequenceTransform FMovieSceneEvaluationTemplateGenerator::GetSequenceTransform(FMovieSceneSequenceIDRef InSequenceID) const
{
	if (InSequenceID == MovieSceneSequenceID::Root)
	{
		return FMovieSceneSequenceTransform();
	}

	const FMovieSceneSubSequenceData* Data = Template.Hierarchy.FindSubData(InSequenceID);
	return ensure(Data) ? Data->RootToSequenceTransform : FMovieSceneSequenceTransform();
}

void FMovieSceneEvaluationTemplateGenerator::AddSubSequence(FMovieSceneSubSequenceData SequenceData, FMovieSceneSequenceIDRef ParentID, FMovieSceneSequenceID SequenceID)
{
	FMovieSceneSequenceHierarchyNode* ParentNode = Template.Hierarchy.FindNode(ParentID);
	checkf(ParentNode, TEXT("Cannot generate a sequence ID for a ParentID that doesn't yet exist"));

	check(SequenceID.IsValid());
	
#if WITH_EDITORONLY_DATA
	if (const FMovieSceneSubSequenceData* ParentSubSequenceData = Template.Hierarchy.FindSubData(ParentID))
	{
		// Clamp this sequence's valid play range by its parent's valid play range
		TRange<float> ParentPlayRangeChildSpace = ParentSubSequenceData->ValidPlayRange * (SequenceData.RootToSequenceTransform * ParentSubSequenceData->RootToSequenceTransform.Inverse());
		SequenceData.ValidPlayRange = TRange<float>::Intersection(ParentPlayRangeChildSpace, SequenceData.ValidPlayRange);
	}
#endif

	// Ensure we have a unique ID. This should never happen in reality.
	while(!ensureMsgf(!Template.Hierarchy.FindNode(SequenceID), TEXT("CRC collision on deterministic hashes. Manually hashing a random new one.")))
	{
		SequenceID = SequenceID.AccumulateParentID(SequenceID);
	}

	Template.Hierarchy.Add(SequenceData, SequenceID, ParentID);
}


void FMovieSceneEvaluationTemplateGenerator::Generate(FMovieSceneTrackCompilationParams InParams)
{
	Template.Hierarchy = FMovieSceneSequenceHierarchy();

	// Generate templates for each track in the movie scene
	UMovieScene* MovieScene = SourceSequence.GetMovieScene();

	TransientArgs.Params = InParams;

	if (UMovieSceneTrack* Track = MovieScene->GetCameraCutTrack())
	{
		ProcessTrack(*Track);
	}

	for (UMovieSceneTrack* Track : MovieScene->GetMasterTracks())
	{
		ProcessTrack(*Track);
	}

	for (const FMovieSceneBinding& ObjectBinding : MovieScene->GetBindings())
	{
#if WITH_EDITOR
		// Skip object bindings that are optimized out
		auto ShouldRemoveObject = [](const UMovieSceneTrack* Track){ return EnumHasAnyFlags(Track->GetCookOptimizationFlags(), ECookOptimizationFlags::RemoveObject); };
		if (ObjectBinding.GetTracks().ContainsByPredicate(ShouldRemoveObject))
		{
			continue;
		}
#endif

		for (UMovieSceneTrack* Track : ObjectBinding.GetTracks())
		{
			ProcessTrack(*Track, ObjectBinding.GetObjectGuid());
		}
	}

	// Remove references to old tracks
	RemoveOldTrackReferences();

	// Add all the tracks in *this* sequence (these exist after any sub section ptrs, not that its important for this algorithm)
	for (auto& Pair : Template.GetTracks())
	{
		TArrayView<const FMovieSceneSegment> Segments = Pair.Value.GetSegments();

		// Add the segment range data to the master collection for overall compilation
		for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
		{
			SegmentData.Add(FMovieSceneSectionData(Segments[SegmentIndex].Range, FSectionEvaluationData(TrackLUT.Num())));
			TrackLUT.Add(FMovieSceneEvaluationFieldSegmentPtr(MovieSceneSequenceID::Root, Pair.Key, SegmentIndex));
		}
	}

	TMap<FMovieSceneSequenceID, FMovieSceneEvaluationTemplate*> SequenceIdToTemplate;
	for (auto& Pair : Template.Hierarchy.AllSubSequenceData())
	{
		if (UMovieSceneSequence* Sequence = Pair.Value.Sequence)
		{
			SequenceIdToTemplate.Add(Pair.Key, &TransientArgs.SubSequenceStore.GetCompiledTemplate(*Sequence, FObjectKey(Pair.Value.SequenceKeyObject)));
		}
	}

	// Compile the new evaluation field
	TArray<FMovieSceneSegment> NewSegments = FMovieSceneSegmentCompiler().Compile(SegmentData);
	UpdateEvaluationField(NewSegments, TrackLUT, SequenceIdToTemplate);
}

void FMovieSceneEvaluationTemplateGenerator::ProcessTrack(const UMovieSceneTrack& InTrack, const FGuid& ObjectId)
{
#if WITH_EDITOR
	// Skip tracks that are optimized out
	if (EnumHasAnyFlags(InTrack.GetCookOptimizationFlags(), ECookOptimizationFlags::RemoveTrack | ECookOptimizationFlags::RemoveObject))
	{
		return;
	}
#endif

	FGuid Signature = InTrack.GetSignature();

	// See if this track signature already exists in the ledger, if it does, we don't need to regenerate it
	if (Template.FindTracks(Signature).Num())
	{
		CompiledSignatures.Add(Signature);
		return;
	}

	TransientArgs.ObjectBindingId = ObjectId;

	// Potentially expensive generation is required
	InTrack.GenerateTemplate(TransientArgs);
}

void FMovieSceneEvaluationTemplateGenerator::RemoveOldTrackReferences()
{
	TArray<FGuid> SignaturesToRemove;

	// Go through the template ledger, and remove anything that is no longer referenced
	for (auto& Pair : Template.GetLedger().TrackSignatureToTrackIdentifier)
	{
		if (!CompiledSignatures.Contains(Pair.Key))
		{
			SignaturesToRemove.Add(Pair.Key);
		}
	}

	// Remove the signatures, updating entries in the evaluation field as we go
	for (const FGuid& Signature : SignaturesToRemove)
	{
		Template.RemoveTrack(Signature);
	}
}

void FMovieSceneEvaluationTemplateGenerator::UpdateEvaluationField(const TArray<FMovieSceneSegment>& Segments, const TArray<FMovieSceneEvaluationFieldSegmentPtr>& Ptrs, const TMap<FMovieSceneSequenceID, FMovieSceneEvaluationTemplate*>& Templates)
{
	IMovieSceneModule& MovieSceneModule = IMovieSceneModule::Get();

	FMovieSceneEvaluationField& Field = Template.EvaluationField;

	Field = FMovieSceneEvaluationField();

	TArray<FMovieSceneEvaluationFieldSegmentPtr> AllTracksInSegment;

	for (int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		const FMovieSceneSegment& Segment = Segments[Index];
		if (Segment.Impls.Num() == 0)
		{
			continue;
		}

		Field.Ranges.Add(Segment.Range);

		AllTracksInSegment.Reset();
		for (const FSectionEvaluationData& LUTData : Segment.Impls)
		{
			AllTracksInSegment.Add(Ptrs[LUTData.ImplIndex]);
		}

		// Sort the track ptrs, and define flush ranges
		AllTracksInSegment.Sort(
			[&](const FMovieSceneEvaluationFieldSegmentPtr& A, const FMovieSceneEvaluationFieldSegmentPtr& B){
				return SortPredicate(A, B, Templates, MovieSceneModule);
			}
		);

		FMovieSceneEvaluationGroup& Group = Field.Groups[Field.Groups.Emplace()];

		TArray<FMovieSceneEvaluationFieldSegmentPtr> EvalPtrs;
		TArray<FMovieSceneEvaluationFieldSegmentPtr> InitPtrs;

		// Now iterate the tracks and insert indices for initialization and evaluation
		FName CurrentEvaluationGroup, LastEvaluationGroup;
		TOptional<uint32> LastPriority;

		for (const FMovieSceneEvaluationFieldSegmentPtr& Ptr : AllTracksInSegment)
		{
			const FMovieSceneEvaluationTrack* Track = LookupTrack(Ptr, Templates);
			if (!ensure(Track))
			{
				continue;
			}

			// If we're now in a different flush group, add the ptrs
			CurrentEvaluationGroup = Track->GetEvaluationGroup();
			if (CurrentEvaluationGroup != LastEvaluationGroup)
			{
				AddPtrsToGroup(Group, InitPtrs, EvalPtrs);
			}
			LastEvaluationGroup = Track->GetEvaluationGroup();

			const bool bRequiresInitialization = Track->GetSegment(Ptr.SegmentIndex).Impls.ContainsByPredicate(
				[Track](FSectionEvaluationData EvalData)
				{
					return Track->GetChildTemplate(EvalData.ImplIndex).RequiresInitialization();
				}
			);

			if (bRequiresInitialization)
			{
				InitPtrs.Add(Ptr);
			}

			EvalPtrs.Add(Ptr);
		}

		AddPtrsToGroup(Group, InitPtrs, EvalPtrs);

		// Copmpute meta data for this segment
		FMovieSceneEvaluationMetaData& MetaData = Field.MetaData[Field.MetaData.Emplace()];
		InitializeMetaData(MetaData, Group, Templates);
	}
}

void FMovieSceneEvaluationTemplateGenerator::InitializeMetaData(FMovieSceneEvaluationMetaData& MetaData, FMovieSceneEvaluationGroup& Group, const TMap<FMovieSceneSequenceID, FMovieSceneEvaluationTemplate*>& Templates)
{
	MetaData.Reset();

	TSet<FMovieSceneSequenceID> ActiveSequenceIDs;
	TSet<FMovieSceneEvaluationKey> ActiveEntitySet;
	for (const FMovieSceneEvaluationFieldSegmentPtr& SegmentPtr : Group.SegmentPtrLUT)
	{
		const FMovieSceneEvaluationTrack* Track = LookupTrack(SegmentPtr, Templates);
		if (!ensure(Track))
		{
			continue;
		}

		// Add the active sequence to the meta data
		MetaData.ActiveSequences.AddUnique(SegmentPtr.SequenceID);

		// Add the track key
		FMovieSceneEvaluationKey TrackKey(SegmentPtr.SequenceID, SegmentPtr.TrackIdentifier);
		if (!ActiveEntitySet.Contains(TrackKey))
		{
			ActiveEntitySet.Add(TrackKey);
			MetaData.ActiveEntities.Add(FMovieSceneOrderedEvaluationKey{ TrackKey, uint32(MetaData.ActiveEntities.Num()) });
		}

		for (FSectionEvaluationData EvalData : Track->GetSegment(SegmentPtr.SegmentIndex).Impls)
		{
			FMovieSceneEvaluationKey SectionKey = TrackKey.AsSection(EvalData.ImplIndex);
			if (!ActiveEntitySet.Contains(SectionKey))
			{
				MetaData.ActiveEntities.Add(FMovieSceneOrderedEvaluationKey{ SectionKey, uint32(MetaData.ActiveEntities.Num()) });
			}
		}
	}

	Algo::SortBy(MetaData.ActiveEntities, &FMovieSceneOrderedEvaluationKey::Key);
	MetaData.ActiveSequences.Sort();
}

const FMovieSceneEvaluationTrack* FMovieSceneEvaluationTemplateGenerator::LookupTrack(const FMovieSceneEvaluationFieldTrackPtr& InPtr, const TMap<FMovieSceneSequenceID, FMovieSceneEvaluationTemplate*>& Templates)
{
	if (InPtr.SequenceID == MovieSceneSequenceID::Root)
	{
		return Template.FindTrack(InPtr.TrackIdentifier);
	}
	else if (const FMovieSceneEvaluationTemplate* SubTemplate = Templates.FindRef(InPtr.SequenceID))
	{
		return SubTemplate->FindTrack(InPtr.TrackIdentifier);
	}
	ensure(false);
	return nullptr;
}

bool FMovieSceneEvaluationTemplateGenerator::SortPredicate(const FMovieSceneEvaluationFieldTrackPtr& InPtrA, const FMovieSceneEvaluationFieldTrackPtr& InPtrB, const TMap<FMovieSceneSequenceID, FMovieSceneEvaluationTemplate*>& Templates, IMovieSceneModule& MovieSceneModule)
{
	const FMovieSceneEvaluationTrack* A = LookupTrack(InPtrA, Templates);
	const FMovieSceneEvaluationTrack* B = LookupTrack(InPtrB, Templates);
	if (!ensure(A && B))
	{
		return false;
	}

	FMovieSceneEvaluationGroupParameters GroupA = MovieSceneModule.GetEvaluationGroupParameters(A->GetEvaluationGroup());
	FMovieSceneEvaluationGroupParameters GroupB = MovieSceneModule.GetEvaluationGroupParameters(B->GetEvaluationGroup());

	if (GroupA.EvaluationPriority != GroupB.EvaluationPriority)
	{
		return GroupA.EvaluationPriority > GroupB.EvaluationPriority;
	}

	const FMovieSceneSubSequenceData* DataA = Template.Hierarchy.FindSubData(InPtrA.SequenceID);
	const FMovieSceneSubSequenceData* DataB = Template.Hierarchy.FindSubData(InPtrB.SequenceID);

	if (DataA || DataB)
	{
		int32 HierarchicalBiasA = DataA ? DataA->HierarchicalBias : 0;
		int32 HierarchicalBiasB = DataB ? DataB->HierarchicalBias : 0;

		if (HierarchicalBiasA != HierarchicalBiasB)
		{
			return HierarchicalBiasA < HierarchicalBiasB;
		}
	}

	return A->GetEvaluationPriority() > B->GetEvaluationPriority();
}
