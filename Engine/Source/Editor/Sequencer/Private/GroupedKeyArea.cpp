// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GroupedKeyArea.h"
#include "Widgets/SNullWidget.h"
#include "EditorStyleSet.h"
#include "DisplayNodes/SequencerSectionKeyAreaNode.h"
#include "DisplayNodes/SequencerTrackNode.h"

FSequencerKeyCollectionSignature FSequencerKeyCollectionSignature::FromNodes(const TArray<FSequencerDisplayNode*>& InNodes, float InDuplicateThresholdTime)
{
	FSequencerKeyCollectionSignature Result;
	Result.DuplicateThresholdTime = InDuplicateThresholdTime;

	for (const FSequencerDisplayNode* Node : InNodes)
	{
		const FSequencerSectionKeyAreaNode* KeyAreaNode = nullptr;

		check(Node);
		if (Node->GetType() == ESequencerNode::KeyArea)
		{
			KeyAreaNode = static_cast<const FSequencerSectionKeyAreaNode*>(Node);
		}
		else if (Node->GetType() == ESequencerNode::Track)
		{
			KeyAreaNode = static_cast<const FSequencerTrackNode*>(Node)->GetTopLevelKeyNode().Get();
		}

		if (KeyAreaNode)
		{
			for (const TSharedRef<IKeyArea>& KeyArea : KeyAreaNode->GetAllKeyAreas())
			{
				const UMovieSceneSection* Section = KeyArea->GetOwningSection();
				Result.KeyAreaToSignature.Add(KeyArea, Section ? Section->GetSignature() : FGuid());
			}
		}
	}

	return Result;
}

FSequencerKeyCollectionSignature FSequencerKeyCollectionSignature::FromNodesRecursive(const TArray<FSequencerDisplayNode*>& InNodes, float InDuplicateThresholdTime)
{
	FSequencerKeyCollectionSignature Result;
	Result.DuplicateThresholdTime = InDuplicateThresholdTime;

	TArray<TSharedRef<FSequencerSectionKeyAreaNode>> AllKeyAreaNodes;
	AllKeyAreaNodes.Reserve(36);
	for (FSequencerDisplayNode* Node : InNodes)
	{
		if (Node->GetType() == ESequencerNode::KeyArea)
		{
			AllKeyAreaNodes.Add(StaticCastSharedRef<FSequencerSectionKeyAreaNode>(Node->AsShared()));
		}

		Node->GetChildKeyAreaNodesRecursively(AllKeyAreaNodes);
	}

	for (const TSharedRef<FSequencerSectionKeyAreaNode>& Node : AllKeyAreaNodes)
	{
		for (const TSharedRef<IKeyArea>& KeyArea : Node->GetAllKeyAreas())
		{
			const UMovieSceneSection* Section = KeyArea->GetOwningSection();
			Result.KeyAreaToSignature.Add(KeyArea, Section ? Section->GetSignature() : FGuid());
		}
	}

	return Result;
}

FSequencerKeyCollectionSignature FSequencerKeyCollectionSignature::FromNodeRecursive(FSequencerDisplayNode& InNode, UMovieSceneSection* InSection, float InDuplicateThresholdTime)
{
	FSequencerKeyCollectionSignature Result;
	Result.DuplicateThresholdTime = InDuplicateThresholdTime;

	TArray<TSharedRef<FSequencerSectionKeyAreaNode>> AllKeyAreaNodes;
	AllKeyAreaNodes.Reserve(36);
	InNode.GetChildKeyAreaNodesRecursively(AllKeyAreaNodes);

	for (const auto& Node : AllKeyAreaNodes)
	{
		TSharedPtr<IKeyArea> KeyArea = Node->GetKeyArea(InSection);
		if (KeyArea.IsValid())
		{
			Result.KeyAreaToSignature.Add(KeyArea.ToSharedRef(), InSection ? InSection->GetSignature() : FGuid());
		}
	}

	return Result;
}

bool FSequencerKeyCollectionSignature::HasUncachableContent() const
{
	for (auto& Pair : KeyAreaToSignature)
	{
		if (!Pair.Value.IsValid())
		{
			return true;
		}
	}
	return false;
}

bool operator!=(const FSequencerKeyCollectionSignature& A, const FSequencerKeyCollectionSignature& B)
{
	if (A.HasUncachableContent() || B.HasUncachableContent())
	{
		return true;
	}

	if (A.KeyAreaToSignature.Num() != B.KeyAreaToSignature.Num() || A.DuplicateThresholdTime != B.DuplicateThresholdTime)
	{
		return true;
	}

	for (auto& Pair : A.KeyAreaToSignature)
	{
		const FGuid* BSig = B.KeyAreaToSignature.Find(Pair.Key);
		if (!BSig || *BSig != Pair.Value)
		{
			return true;
		}
	}

	return false;
}

bool operator==(const FSequencerKeyCollectionSignature& A, const FSequencerKeyCollectionSignature& B)
{
	if (A.HasUncachableContent() || B.HasUncachableContent())
	{
		return false;
	}

	if (A.KeyAreaToSignature.Num() != B.KeyAreaToSignature.Num() || A.DuplicateThresholdTime != B.DuplicateThresholdTime)
	{
		return false;
	}

	for (auto& Pair : A.KeyAreaToSignature)
	{
		const FGuid* BSig = B.KeyAreaToSignature.Find(Pair.Key);
		if (!BSig || *BSig != Pair.Value)
		{
			return false;
		}
	}

	return true;
}

FIndexKey::FIndexKey(FName InNodePath, UMovieSceneSection* InSection)
	: NodePath(MoveTemp(InNodePath))
	, Section(InSection)
{}

namespace 
{
	/** Structure that defines the index for a particular section */
	struct FIndexEntry
	{
		TMap<FKeyHandle, int32> HandleToGroup;
		TArray<FKeyHandle> GroupHandles;
		TArray<float> RepresentativeTimes;
	};

	/** A persistent index is required to ensure that generated key handles are maintained for the lifetime of specific display nodes */
	TMap<FIndexKey, FIndexEntry> GlobalIndex;
}

FGroupedKeyCollection::FGroupedKeyCollection()
	: IndexKey(FName(), nullptr)
{
	GroupingThreshold = SMALL_NUMBER;
}

bool FGroupedKeyCollection::InitializeExplicit(const TArray<FSequencerDisplayNode*>& InNodes, float DuplicateThreshold)
{
	FSequencerKeyCollectionSignature UpToDateSignature = FSequencerKeyCollectionSignature::FromNodes(InNodes, DuplicateThreshold);
	if (CacheSignature == UpToDateSignature)
	{
		return false;
	}

	Swap(UpToDateSignature, CacheSignature);

	KeyAreas.Reset();
	Groups.Reset();

	for (auto& Pair : CacheSignature.GetKeyAreas())
	{
		AddKeyArea(Pair.Key);
	}

	RemoveDuplicateKeys(DuplicateThreshold);
	return true;
}

bool FGroupedKeyCollection::InitializeRecursive(const TArray<FSequencerDisplayNode*>& InNodes, float DuplicateThreshold)
{
	FSequencerKeyCollectionSignature UpToDateSignature = FSequencerKeyCollectionSignature::FromNodesRecursive(InNodes, DuplicateThreshold);
	if (CacheSignature == UpToDateSignature)
	{
		return false;
	}

	Swap(UpToDateSignature, CacheSignature);

	KeyAreas.Reset();
	Groups.Reset();

	for (auto& Pair : CacheSignature.GetKeyAreas())
	{
		AddKeyArea(Pair.Key);
	}

	RemoveDuplicateKeys(DuplicateThreshold);
	return true;
}

bool FGroupedKeyCollection::InitializeRecursive(FSequencerDisplayNode& InNode, UMovieSceneSection* InSection, float DuplicateThreshold)
{
	FSequencerKeyCollectionSignature UpToDateSignature = FSequencerKeyCollectionSignature::FromNodeRecursive(InNode, InSection, DuplicateThreshold);
	if (CacheSignature == UpToDateSignature)
	{
		return false;
	}

	Swap(UpToDateSignature, CacheSignature);

	KeyAreas.Reset();
	Groups.Reset();

	for (auto& Pair : CacheSignature.GetKeyAreas())
	{
		AddKeyArea(Pair.Key);
	}

	RemoveDuplicateKeys(DuplicateThreshold);
	return true;
}

void FGroupedKeyCollection::AddKeyArea(const TSharedRef<IKeyArea>& InKeyArea)
{
	const int32 KeyAreaIndex = KeyAreas.Num();
	KeyAreas.Add(InKeyArea);

	TArray<FKeyHandle> AllKeyHandles = InKeyArea->GetUnsortedKeyHandles();
	Groups.Reserve(Groups.Num() + AllKeyHandles.Num());

	for (const FKeyHandle& KeyHandle : AllKeyHandles)
	{
		Groups.Emplace(InKeyArea->GetKeyTime(KeyHandle), KeyAreaIndex, KeyHandle);
	}
}

void FGroupedKeyCollection::RemoveDuplicateKeys(float DuplicateThreshold)
{
	GroupingThreshold = DuplicateThreshold;

	TArray<FKeyGrouping> OldGroups;
	Swap(OldGroups, Groups);

	Groups.Reserve(OldGroups.Num());

	OldGroups.Sort([](const FKeyGrouping& A, const FKeyGrouping& B){
		return A.RepresentativeTime < B.RepresentativeTime;
	});

	// Remove duplicates
	for (int32 ReadIndex = 0; ReadIndex < OldGroups.Num(); ++ReadIndex)
	{
		const float CurrentTime = OldGroups[ReadIndex].RepresentativeTime;

		int32 NumToMerge = 0;
		for (int32 DuplIndex = ReadIndex + 1; DuplIndex < OldGroups.Num(); ++DuplIndex)
		{
			if (!FMath::IsNearlyEqual(CurrentTime, OldGroups[DuplIndex].RepresentativeTime, DuplicateThreshold))
			{
				break;
			}
			++NumToMerge;
		}

		Groups.Add(MoveTemp(OldGroups[ReadIndex]));

		if (NumToMerge)
		{
			const int32 Start = ReadIndex + 1, End = Start + NumToMerge;
			for (int32 MergeIndex = Start; MergeIndex < End; ++MergeIndex)
			{
				Groups.Last().Keys.Append(MoveTemp(OldGroups[MergeIndex].Keys));
			}

			ReadIndex += NumToMerge;
		}
	}
}

void FGroupedKeyCollection::UpdateIndex() const
{
	auto& IndexEntry = GlobalIndex.FindOrAdd(IndexKey);

	TArray<FKeyHandle> NewKeyHandles;
	TArray<float> NewRepresentativeTimes;

	IndexEntry.HandleToGroup.Reset();

	for (int32 GroupIndex = 0; GroupIndex < Groups.Num(); ++GroupIndex)
	{
		float RepresentativeTime = Groups[GroupIndex].RepresentativeTime;

		// Find a key handle we can recycle
		int32 RecycledIndex = IndexEntry.RepresentativeTimes.IndexOfByPredicate([&](float Time){
			// Must be an *exact* match to recycle
			return Time == RepresentativeTime;
		});
		
		if (RecycledIndex != INDEX_NONE)
		{
			NewKeyHandles.Add(IndexEntry.GroupHandles[RecycledIndex]);
			NewRepresentativeTimes.Add(IndexEntry.RepresentativeTimes[RecycledIndex]);

			IndexEntry.GroupHandles.RemoveAt(RecycledIndex, 1, false);
			IndexEntry.RepresentativeTimes.RemoveAt(RecycledIndex, 1, false);
		}
		else
		{
			NewKeyHandles.Add(FKeyHandle());
			NewRepresentativeTimes.Add(RepresentativeTime);
		}

		IndexEntry.HandleToGroup.Add(NewKeyHandles.Last(), GroupIndex);
	}

	IndexEntry.GroupHandles = MoveTemp(NewKeyHandles);
	IndexEntry.RepresentativeTimes = MoveTemp(NewRepresentativeTimes);
}

void FGroupedKeyCollection::IterateKeys(const TFunctionRef<bool(float)>& Iter)
{
	for (const FKeyGrouping& Grouping : Groups)
	{
		if (!Iter(Grouping.RepresentativeTime))
		{
			return;
		}
	}
}

float FGroupedKeyCollection::GetKeyGroupingThreshold() const
{
	return GroupingThreshold;
}

TOptional<float> FGroupedKeyCollection::FindFirstKeyInRange(const TRange<float>& InRange, EFindKeyDirection Direction) const
{
	// @todo: linear search may be slow where there are lots of keys

	bool bWithinRange = false;
	if (Direction == EFindKeyDirection::Backwards)
	{
		for (int32 Index = Groups.Num() - 1; Index >= 0; --Index)
		{
			if (Groups[Index].RepresentativeTime < InRange.GetUpperBoundValue())
			{
				// Just entered the range
				return Groups[Index].RepresentativeTime;
			}
			else if (InRange.HasLowerBound() && Groups[Index].RepresentativeTime < InRange.GetLowerBoundValue())
			{
				// No longer inside the range
				return TOptional<float>();
			}
		}
	}
	else
	{
		for (int32 Index = 0; Index < Groups.Num(); ++Index)
		{
			if (Groups[Index].RepresentativeTime > InRange.GetLowerBoundValue())
			{
				// Just entered the range
				return Groups[Index].RepresentativeTime;
			}
			else if (InRange.HasUpperBound() && Groups[Index].RepresentativeTime > InRange.GetUpperBoundValue())
			{
				// No longer inside the range
				return TOptional<float>();
			}
		}
	}

	return TOptional<float>();
}

const FKeyGrouping* FGroupedKeyCollection::FindGroup(FKeyHandle InHandle) const
{
	if (auto* IndexEntry = GlobalIndex.Find(IndexKey))
	{
		if (int32* GroupIndex = IndexEntry->HandleToGroup.Find(InHandle))
		{
			return Groups.IsValidIndex(*GroupIndex) ? &Groups[*GroupIndex] : nullptr;
		}
	}
	return nullptr;
}

FKeyGrouping* FGroupedKeyCollection::FindGroup(FKeyHandle InHandle)
{
	if (auto* IndexEntry = GlobalIndex.Find(IndexKey))
	{
		if (int32* GroupIndex = IndexEntry->HandleToGroup.Find(InHandle))
		{
			return Groups.IsValidIndex(*GroupIndex) ? &Groups[*GroupIndex] : nullptr;
		}
	}
	return nullptr;
}

FLinearColor FGroupedKeyCollection::GetKeyTint(FKeyHandle InHandle) const
{
	// Everything is untinted for now
	return FLinearColor::White;
}

const FSlateBrush* FGroupedKeyCollection::GetBrush(FKeyHandle InHandle) const
{
	static const FSlateBrush* PartialKeyBrush = FEditorStyle::GetBrush("Sequencer.PartialKey");
	const auto* Group = FindGroup(InHandle);

	// Ensure that each key area is represented at least once for it to be considered a 'complete key'
	for (int32 AreaIndex = 0; Group && AreaIndex < KeyAreas.Num(); ++AreaIndex)
	{
		if (!Group->Keys.ContainsByPredicate([&](const FKeyGrouping::FKeyIndex& Key){ return Key.AreaIndex == AreaIndex; }))
		{
			return PartialKeyBrush;
		}
	}

	return nullptr;
}

FGroupedKeyArea::FGroupedKeyArea(FSequencerDisplayNode& InNode, UMovieSceneSection* InSection)
	: DisplayNode(InNode.AsShared())
	, Section(InSection)
{
	check(Section);
	IndexKey = FIndexKey(*InNode.GetPathName(), Section);

	Update();
}

void FGroupedKeyArea::Update()
{
	TSharedPtr<FSequencerDisplayNode> PinnedNode = DisplayNode.Pin();
	if (PinnedNode.IsValid() && InitializeRecursive(*PinnedNode, Section))
	{
		UpdateIndex();
	}
}

TArray<FKeyHandle> FGroupedKeyArea::GetUnsortedKeyHandles() const
{
	TArray<FKeyHandle> Array;
	if (auto* IndexEntry = GlobalIndex.Find(IndexKey))
	{
		IndexEntry->HandleToGroup.GenerateKeyArray(Array);
	}
	return Array;
}

void FGroupedKeyArea::SetKeyTime(FKeyHandle KeyHandle, float NewKeyTime)
{
	auto* Group = FindGroup(KeyHandle);

	for (auto& Key : Group->Keys)
	{
		KeyAreas[Key.AreaIndex]->SetKeyTime(Key.KeyHandle, NewKeyTime);
	}
}

float FGroupedKeyArea::GetKeyTime(FKeyHandle KeyHandle) const
{
	auto* Group = FindGroup(KeyHandle);
	return Group ? Group->RepresentativeTime : 0.f;
}

FKeyHandle FGroupedKeyArea::DilateKey(FKeyHandle KeyHandle, float Scale, float Origin)
{
	int32* GroupIndex = nullptr;

	auto* IndexEntry = GlobalIndex.Find(IndexKey);
	if (IndexEntry)
	{
		GroupIndex = IndexEntry->HandleToGroup.Find(KeyHandle);
	}

	if (!GroupIndex || !Groups.IsValidIndex(*GroupIndex))
	{
		return KeyHandle;
	}

	bool bIsTimeUpdated = false;

	FKeyGrouping& Group = Groups[*GroupIndex];

	// Move all the keys
	for (auto& Key : Group.Keys)
	{
		Key.KeyHandle = KeyAreas[Key.AreaIndex]->DilateKey(Key.KeyHandle, Scale, Origin);

		const float KeyTime = KeyAreas[Key.AreaIndex]->GetKeyTime(Key.KeyHandle);
		Group.RepresentativeTime = bIsTimeUpdated ? FMath::Min(Group.RepresentativeTime, KeyTime) : KeyTime;
		bIsTimeUpdated = true;
	}

	// Update the representative time to the smallest of all the keys (so it will deterministically get the same key handle on regeneration)
	IndexEntry->RepresentativeTimes[*GroupIndex] = Group.RepresentativeTime;

	return KeyHandle;
}

FKeyHandle FGroupedKeyArea::MoveKey(FKeyHandle KeyHandle, float DeltaPosition)
{
	int32* GroupIndex = nullptr;

	auto* IndexEntry = GlobalIndex.Find(IndexKey);
	if (IndexEntry)
	{
		GroupIndex = IndexEntry->HandleToGroup.Find(KeyHandle);
	}

	if (!GroupIndex || !Groups.IsValidIndex(*GroupIndex))
	{
		return KeyHandle;
	}

	bool bIsTimeUpdated = false;

	FKeyGrouping& Group = Groups[*GroupIndex];

	// Move all the keys
	for (auto& Key : Group.Keys)
	{
		Key.KeyHandle = KeyAreas[Key.AreaIndex]->MoveKey(Key.KeyHandle, DeltaPosition);

		const float KeyTime = KeyAreas[Key.AreaIndex]->GetKeyTime(Key.KeyHandle);
		Group.RepresentativeTime = bIsTimeUpdated ? FMath::Min(Group.RepresentativeTime, KeyTime) : KeyTime;
		bIsTimeUpdated = true;
	}

	// Update the representative time to the smallest of all the keys (so it will deterministically get the same key handle on regeneration)
	IndexEntry->RepresentativeTimes[*GroupIndex] = Group.RepresentativeTime;

	return KeyHandle;
}

void FGroupedKeyArea::DeleteKey(FKeyHandle KeyHandle)
{
	if (auto* Group = FindGroup(KeyHandle))
	{
		for (auto& Key : Group->Keys)
		{
			KeyAreas[Key.AreaIndex]->DeleteKey(Key.KeyHandle);
		}
	}
}

void                    
FGroupedKeyArea::SetKeyInterpMode(FKeyHandle KeyHandle, ERichCurveInterpMode InterpMode)
{
	if (auto* Group = FindGroup(KeyHandle))
	{
		for (auto& Key : Group->Keys)
		{
			KeyAreas[Key.AreaIndex]->SetKeyInterpMode(Key.KeyHandle, InterpMode);
		}
	}
}

ERichCurveInterpMode
FGroupedKeyArea::GetKeyInterpMode(FKeyHandle KeyHandle) const
{
	// Return RCIM_None if the keys don't all have the same interp mode
	ERichCurveInterpMode InterpMode = RCIM_None;
	if (auto* Group = FindGroup(KeyHandle))
	{
		for (auto& Key : Group->Keys)
		{
			if (InterpMode == RCIM_None)
			{
				InterpMode = KeyAreas[Key.AreaIndex]->GetKeyInterpMode(Key.KeyHandle);
			}
			else if (InterpMode != KeyAreas[Key.AreaIndex]->GetKeyInterpMode(Key.KeyHandle))
			{
				return RCIM_None;
			}
		}
	}
	return InterpMode;
}

void                    
FGroupedKeyArea::SetKeyTangentMode(FKeyHandle KeyHandle, ERichCurveTangentMode TangentMode)
{
	if (auto* Group = FindGroup(KeyHandle))
	{
		for (auto& Key : Group->Keys)
		{
			KeyAreas[Key.AreaIndex]->SetKeyTangentMode(Key.KeyHandle, TangentMode);
		}
	}
}

ERichCurveTangentMode
FGroupedKeyArea::GetKeyTangentMode(FKeyHandle KeyHandle) const
{
	// Return RCTM_None if the keys don't all have the same tangent mode
	ERichCurveTangentMode TangentMode = RCTM_None;
	if (auto* Group = FindGroup(KeyHandle))
	{
		for (auto& Key : Group->Keys)
		{
			if (TangentMode == RCTM_None)
			{
				TangentMode = KeyAreas[Key.AreaIndex]->GetKeyTangentMode(Key.KeyHandle);
			}
			else if (TangentMode != KeyAreas[Key.AreaIndex]->GetKeyTangentMode(Key.KeyHandle))
			{
				return RCTM_None;
			}
		}
	}
	return TangentMode;
}

void FGroupedKeyArea::SetExtrapolationMode(ERichCurveExtrapolation ExtrapMode, bool bPreInfinity)
{
	for (auto& Area : KeyAreas)
	{
		Area->SetExtrapolationMode(ExtrapMode, bPreInfinity);
	}
}

ERichCurveExtrapolation FGroupedKeyArea::GetExtrapolationMode(bool bPreInfinity) const
{
	ERichCurveExtrapolation ExtrapMode = RCCE_None;
	for (auto& Area : KeyAreas)
	{
		if (ExtrapMode == RCCE_None)
		{
			ExtrapMode = Area->GetExtrapolationMode(bPreInfinity);
		}
		else if (Area->GetExtrapolationMode(bPreInfinity) != ExtrapMode)
		{
			return RCCE_None;
		}
	}
	return ExtrapMode;
}

bool FGroupedKeyArea::CanSetExtrapolationMode() const
{
	for (auto& Area : KeyAreas)
	{
		if (Area->CanSetExtrapolationMode())
		{
			return true;
		}
	}
	return false;
}

TArray<FKeyHandle> FGroupedKeyArea::AddKeyUnique(float Time, EMovieSceneKeyInterpolation InKeyInterpolation, float TimeToCopyFrom)
{
	TArray<FKeyHandle> AddedKeyHandles;

	for (auto& Area : KeyAreas)
	{
		// If TimeToCopyFrom is valid, add a key only if there is a key to copy from
		if (TimeToCopyFrom != FLT_MAX)
		{
			if (FRichCurve* Curve = Area->GetRichCurve())
			{
				if (!Curve->IsKeyHandleValid(Curve->FindKey(TimeToCopyFrom)))
				{
					continue;
				}
			}
		}

		TArray<FKeyHandle> AddedGroupKeyHandles = Area->AddKeyUnique(Time, InKeyInterpolation, TimeToCopyFrom);
		AddedKeyHandles.Append(AddedGroupKeyHandles);
	}

	return AddedKeyHandles;
}

TOptional<FKeyHandle> FGroupedKeyArea::DuplicateKey(FKeyHandle KeyToDuplicate)
{
	FKeyGrouping* Group = FindGroup(KeyToDuplicate);
	if (!Group)
	{
		return TOptional<FKeyHandle>();
	}

	const float Time = Group->RepresentativeTime;

	const int32 NewGroupIndex = Groups.Num();
	Groups.Emplace(Time);
	
	for (const FKeyGrouping::FKeyIndex& Key : Group->Keys)
	{
		TOptional<FKeyHandle> NewKeyHandle = KeyAreas[Key.AreaIndex]->DuplicateKey(Key.KeyHandle);
		if (NewKeyHandle.IsSet())
		{
			Groups[NewGroupIndex].Keys.Emplace(Key.AreaIndex, NewKeyHandle.GetValue());
		}
	}

	// Update the global index with our new key
	FIndexEntry* IndexEntry = GlobalIndex.Find(IndexKey);
	if (IndexEntry)
	{
		FKeyHandle ThisGroupKeyHandle;

		IndexEntry->GroupHandles.Add(ThisGroupKeyHandle);
		IndexEntry->HandleToGroup.Add(ThisGroupKeyHandle, NewGroupIndex);
		IndexEntry->RepresentativeTimes.Add(Time);

		return ThisGroupKeyHandle;
	}

	return TOptional<FKeyHandle>();
}

FRichCurve* FGroupedKeyArea::GetRichCurve()
{
	return nullptr;
}

UMovieSceneSection* FGroupedKeyArea::GetOwningSection()
{
	return Section;
}

bool FGroupedKeyArea::CanCreateKeyEditor()
{
	return false;
}


void FGroupedKeyArea::CopyKeys(FMovieSceneClipboardBuilder& ClipboardBuilder, const TFunctionRef<bool(FKeyHandle, const IKeyArea&)>& KeyMask) const
{
	const FIndexEntry* IndexEntry = GlobalIndex.Find(IndexKey);
	if (!IndexEntry)
	{
		return;
	}

	// Since we are a group of nested key areas, we test the key mask for our key handles, and forward on the results to each key area

	// Using ptr as map key is fine here as we know they will not change
	TMap<const IKeyArea*, TSet<FKeyHandle>> AllValidHandles;

	for (auto& Pair : IndexEntry->HandleToGroup)
	{
		if (!KeyMask(Pair.Key, *this) || !Groups.IsValidIndex(Pair.Value))
		{
			continue;
		}

		const FKeyGrouping& Group = Groups[Pair.Value];
		for (const FKeyGrouping::FKeyIndex& KeyIndex : Group.Keys)
		{
			const IKeyArea& KeyArea = KeyAreas[KeyIndex.AreaIndex].Get();
			AllValidHandles.FindOrAdd(&KeyArea).Add(KeyIndex.KeyHandle);
		}
	}

	for (auto& Pair : AllValidHandles)
	{
		Pair.Key->CopyKeys(ClipboardBuilder, [&](FKeyHandle Handle, const IKeyArea&){
			return Pair.Value.Contains(Handle);
		});
	}
}


TSharedRef<SWidget> FGroupedKeyArea::CreateKeyEditor(ISequencer* Sequencer)
{
	return SNullWidget::NullWidget;
}


TOptional<FLinearColor> FGroupedKeyArea::GetColor()
{
	return TOptional<FLinearColor>();
}


TSharedPtr<FStructOnScope> FGroupedKeyArea::GetKeyStruct(FKeyHandle KeyHandle)
{
	FKeyGrouping* Group = FindGroup(KeyHandle);
	
	if (Group == nullptr)
	{
		return nullptr;
	}

	TArray<FKeyHandle> KeyHandles;

	for (auto& Key : Group->Keys)
	{
		KeyHandles.Add(Key.KeyHandle);
	}

	return Section->GetKeyStruct(KeyHandles);
}


void FGroupedKeyArea::PasteKeys(const FMovieSceneClipboardKeyTrack& KeyTrack, const FMovieSceneClipboardEnvironment& SrcEnvironment, const FSequencerPasteEnvironment& DstEnvironment)
{
	checkf(false, TEXT("Pasting into grouped key areas is not supported, and should not be used. Iterate child tracks instead."));
}
