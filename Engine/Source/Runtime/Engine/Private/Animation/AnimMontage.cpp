// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimMontage.cpp: Montage classes that contains slots
=============================================================================*/ 

#include "Animation/AnimMontage.h"
#include "UObject/LinkerLoad.h"
#include "Package.h"
#include "Animation/AssetMappingTable.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include "AnimationUtils.h"
#include "AnimationRuntime.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY(LogAnimMontage);

// Pre-built FNames so we don't take the hit of constructing FNames at spawn time
namespace MontageFNames
{
	static FName TimeStretchCurveName(TEXT("MontageTimeStretchCurve"));
}

///////////////////////////////////////////////////////////////////////////
//
UAnimMontage::UAnimMontage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BlendIn.SetBlendTime(0.25f);
	BlendOut.SetBlendTime(0.25f);
	BlendOutTriggerTime = -1.f;
	SyncSlotIndex = 0;

	BlendInTime_DEPRECATED = -1.f;
	BlendOutTime_DEPRECATED = -1.f;

	AddSlot(FAnimSlotGroup::DefaultSlotName);

	TimeStretchCurveName = MontageFNames::TimeStretchCurveName;
}

FSlotAnimationTrack& UAnimMontage::AddSlot(FName SlotName)
{
	int32 NewSlot = SlotAnimTracks.AddDefaulted(1);
	SlotAnimTracks[NewSlot].SlotName = SlotName;
	return SlotAnimTracks[NewSlot];
}

bool UAnimMontage::IsValidSlot(FName InSlotName) const
{
	for (int32 I=0; I<SlotAnimTracks.Num(); ++I)
	{
		if ( SlotAnimTracks[I].SlotName == InSlotName )
		{
			// if data is there, return true. Otherwise, it doesn't matter
			return ( SlotAnimTracks[I].AnimTrack.AnimSegments.Num() >  0 );
		}
	}

	return false;
}

const FAnimTrack* UAnimMontage::GetAnimationData(FName InSlotName) const
{
	for (int32 I=0; I<SlotAnimTracks.Num(); ++I)
	{
		if ( SlotAnimTracks[I].SlotName == InSlotName )
		{
			// if data is there, return true. Otherwise, it doesn't matter
			return &( SlotAnimTracks[I].AnimTrack );
		}
	}

	return NULL;
}

bool UAnimMontage::IsWithinPos(int32 FirstIndex, int32 SecondIndex, float CurrentTime) const
{
	float StartTime;
	float EndTime;
	if ( CompositeSections.IsValidIndex(FirstIndex) )
	{
		StartTime = CompositeSections[FirstIndex].GetTime();
	}
	else // if first index isn't valid, set to be 0.f, so it starts from reset
	{
		StartTime = 0.f;
	}

	if ( CompositeSections.IsValidIndex(SecondIndex) )
	{
		EndTime = CompositeSections[SecondIndex].GetTime();
	}
	else // if end index isn't valid, set to be BIG_NUMBER
	{
		// @todo anim, I don't know if using SequenceLength is better or BIG_NUMBER
		// I don't think that'd matter. 
		EndTime = SequenceLength;
	}

	// since we do range of [StartTime, EndTime) (excluding EndTime) 
	// there is blindspot of when CurrentTime becomes >= SequenceLength
	// include that frame if CurrentTime gets there. 
	// Otherwise, we continue to use [StartTime, EndTime)
	if (CurrentTime >= SequenceLength)
	{
		return (StartTime <= CurrentTime && EndTime >= CurrentTime);
	}

	return (StartTime <= CurrentTime && EndTime > CurrentTime);
}

float UAnimMontage::CalculatePos(FCompositeSection &Section, float PosWithinCompositeSection) const
{
	float Offset = Section.GetTime();
	Offset += PosWithinCompositeSection;
	// @todo anim
	return Offset;
}

int32 UAnimMontage::GetSectionIndexFromPosition(float Position) const
{
	for (int32 I=0; I<CompositeSections.Num(); ++I)
	{
		// if within
		if( IsWithinPos(I, I+1, Position) )
		{
			return I;
		}
	}

	return INDEX_NONE;
}

int32 UAnimMontage::GetAnimCompositeSectionIndexFromPos(float CurrentTime, float& PosWithinCompositeSection) const
{
	PosWithinCompositeSection = 0.f;

	for (int32 I=0; I<CompositeSections.Num(); ++I)
	{
		// if within
		if (IsWithinPos(I, I+1, CurrentTime))
		{
			PosWithinCompositeSection = CurrentTime - CompositeSections[I].GetTime();
			return I;
		}
	}

	return INDEX_NONE;
}

float UAnimMontage::GetSectionTimeLeftFromPos(float Position)
{
	const int32 SectionID = GetSectionIndexFromPosition(Position);
	if( SectionID != INDEX_NONE )
	{
		if( IsValidSectionIndex(SectionID+1) )
		{
			return (GetAnimCompositeSection(SectionID+1).GetTime() - Position);
		}
		else
		{
			return (SequenceLength - Position);
		}
	}

	return -1.f;
}

const FCompositeSection& UAnimMontage::GetAnimCompositeSection(int32 SectionIndex) const
{
	check ( CompositeSections.IsValidIndex(SectionIndex) );
	return CompositeSections[SectionIndex];
}

FCompositeSection& UAnimMontage::GetAnimCompositeSection(int32 SectionIndex)
{
	check ( CompositeSections.IsValidIndex(SectionIndex) );
	return CompositeSections[SectionIndex];
}

int32 UAnimMontage::GetSectionIndex(FName InSectionName) const
{
	// I can have operator== to check SectionName, but then I have to construct
	// empty FCompositeSection all the time whenever I search :(
	for (int32 I=0; I<CompositeSections.Num(); ++I)
	{
		if ( CompositeSections[I].SectionName == InSectionName ) 
		{
			return I;
		}
	}

	return INDEX_NONE;
}

FName UAnimMontage::GetSectionName(int32 SectionIndex) const
{
	if ( CompositeSections.IsValidIndex(SectionIndex) )
	{
		return CompositeSections[SectionIndex].SectionName;
	}

	return NAME_None;
}

bool UAnimMontage::IsValidSectionName(FName InSectionName) const
{
	return GetSectionIndex(InSectionName) != INDEX_NONE;
}

bool UAnimMontage::IsValidSectionIndex(int32 SectionIndex) const
{
	return (CompositeSections.IsValidIndex(SectionIndex));
}

void UAnimMontage::GetSectionStartAndEndTime(int32 SectionIndex, float& OutStartTime, float& OutEndTime) const
{
	OutStartTime = 0.f;
	OutEndTime = SequenceLength;
	if ( IsValidSectionIndex(SectionIndex) )
	{
		OutStartTime = GetAnimCompositeSection(SectionIndex).GetTime();		
	}

	if ( IsValidSectionIndex(SectionIndex + 1))
	{
		OutEndTime = GetAnimCompositeSection(SectionIndex + 1).GetTime();		
	}
}

float UAnimMontage::GetSectionLength(int32 SectionIndex) const
{
	float StartTime = 0.f;
	float EndTime = SequenceLength;
	if ( IsValidSectionIndex(SectionIndex) )
	{
		StartTime = GetAnimCompositeSection(SectionIndex).GetTime();		
	}

	if ( IsValidSectionIndex(SectionIndex + 1))
	{
		EndTime = GetAnimCompositeSection(SectionIndex + 1).GetTime();		
	}

	return EndTime - StartTime;
}

#if WITH_EDITOR
int32 UAnimMontage::AddAnimCompositeSection(FName InSectionName, float StartTime)
{
	FCompositeSection NewSection;

	// make sure same name doesn't exists
	if ( InSectionName != NAME_None )
	{
		NewSection.SectionName = InSectionName;
	}
	else
	{
		// just give default name
		NewSection.SectionName = FName(*FString::Printf(TEXT("Section%d"), CompositeSections.Num()+1));
	}

	// we already have that name
	if ( GetSectionIndex(InSectionName)!=INDEX_NONE )
	{
		UE_LOG(LogAnimMontage, Warning, TEXT("AnimCompositeSection : %s(%s) already exists. Choose different name."), 
			*NewSection.SectionName.ToString(), *InSectionName.ToString());
		return INDEX_NONE;
	}

	NewSection.LinkMontage(this, StartTime);

	// we'd like to sort them in the order of time
	int32 NewSectionIndex = CompositeSections.Add(NewSection);

	// when first added, just make sure to link previous one to add me as next if previous one doesn't have any link
	// it's confusing first time when you add this data
	int32 PrevSectionIndex = NewSectionIndex-1;
	if ( CompositeSections.IsValidIndex(PrevSectionIndex) )
	{
		if (CompositeSections[PrevSectionIndex].NextSectionName == NAME_None)
		{
			CompositeSections[PrevSectionIndex].NextSectionName = InSectionName;
		}
	}

	return NewSectionIndex;
}

bool UAnimMontage::DeleteAnimCompositeSection(int32 SectionIndex)
{
	if ( CompositeSections.IsValidIndex(SectionIndex) )
	{
		CompositeSections.RemoveAt(SectionIndex);
		return true;
	}

	return false;
}
void UAnimMontage::SortAnimCompositeSectionByPos()
{
	// sort them in the order of time
	struct FCompareFCompositeSection
	{
		FORCEINLINE bool operator()( const FCompositeSection &A, const FCompositeSection &B ) const
		{
			return A.GetTime() < B.GetTime();
		}
	};
	CompositeSections.Sort( FCompareFCompositeSection() );
}

void UAnimMontage::RegisterOnMontageChanged(const FOnMontageChanged& Delegate)
{
	OnMontageChanged.Add(Delegate);
}
void UAnimMontage::UnregisterOnMontageChanged(void* Unregister)
{
	OnMontageChanged.RemoveAll(Unregister);
}
#endif	//WITH_EDITOR

void UAnimMontage::PreSave(const class ITargetPlatform* TargetPlatform)
{
	BakeTimeStretchCurve();

	Super::PreSave(TargetPlatform);
}

void UAnimMontage::PostLoad()
{
	Super::PostLoad();

	// copy deprecated variable to new one, temporary code to keep data copied. Am deleting it right after this
	for ( auto SlotIter = SlotAnimTracks.CreateIterator() ; SlotIter ; ++SlotIter)
	{
		FAnimTrack & Track = (*SlotIter).AnimTrack;
		Track.ValidateSegmentTimes();

		const float CurrentCalculatedLength = CalculateSequenceLength();

		if(CurrentCalculatedLength != SequenceLength)
		{
			UE_LOG(LogAnimMontage, Display, TEXT("UAnimMontage::PostLoad: The actual sequence length for %s does not match the length stored in the asset, please resave the asset."), *GetFullName());
			SequenceLength = CurrentCalculatedLength;
		}
	}

	for(auto& Composite : CompositeSections)
	{
		if(Composite.StartTime_DEPRECATED != 0.0f)
		{
			Composite.Clear();
			Composite.LinkMontage(this, Composite.StartTime_DEPRECATED);
		}
		else
		{
			Composite.RefreshSegmentOnLoad();
			Composite.LinkMontage(this, Composite.GetTime());
		}
	}

	bool bRootMotionEnabled = bEnableRootMotionTranslation || bEnableRootMotionRotation;

	if (bRootMotionEnabled)
	{
		for (FSlotAnimationTrack& Slot : SlotAnimTracks)
		{
			for (FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
			{
				if (Segment.AnimReference)
				{
					Segment.AnimReference->EnableRootMotionSettingFromMontage(true, RootMotionRootLock);
				}
			}
		}
	}
	// find preview base pose if it can
#if WITH_EDITORONLY_DATA
	if ( IsValidAdditive() && PreviewBasePose == NULL )
	{
		for (int32 I=0; I<SlotAnimTracks.Num(); ++I)
		{
			if ( SlotAnimTracks[I].AnimTrack.AnimSegments.Num() > 0 )
			{
				UAnimSequenceBase* SequenceBase = SlotAnimTracks[I].AnimTrack.AnimSegments[0].AnimReference;
				UAnimSequence* BaseAdditivePose = (SequenceBase) ? SequenceBase->GetAdditiveBasePose() : nullptr;
				if (BaseAdditivePose)
				{
					PreviewBasePose = BaseAdditivePose;
					MarkPackageDirty();
					break;
				}
			}
		}
	}

	// verify if skeleton matches, otherwise clear it, this can happen if anim sequence has been modified when this hasn't been loaded. 
	{
		USkeleton* MySkeleton = GetSkeleton();
		for (int32 I=0; I<SlotAnimTracks.Num(); ++I)
		{
			if ( SlotAnimTracks[I].AnimTrack.AnimSegments.Num() > 0 )
			{
				UAnimSequenceBase* SequenceBase = SlotAnimTracks[I].AnimTrack.AnimSegments[0].AnimReference;
				if (SequenceBase && SequenceBase->GetSkeleton() != MySkeleton )
				{
					SlotAnimTracks[I].AnimTrack.AnimSegments[0].AnimReference = nullptr;
					MarkPackageDirty();
					break;
				}
			}
		}
	}
#endif // WITH_EDITORONLY_DATA

	// Register Slots w/ Skeleton
	{
		USkeleton* MySkeleton = GetSkeleton();
		if (MySkeleton)
		{
			for (int32 SlotIndex = 0; SlotIndex < SlotAnimTracks.Num(); SlotIndex++)
			{
				FName SlotName = SlotAnimTracks[SlotIndex].SlotName;
				MySkeleton->RegisterSlotNode(SlotName);
			}
		}
	}

	for(FAnimNotifyEvent& Notify : Notifies)
	{
		if(Notify.DisplayTime_DEPRECATED != 0.0f)
		{
			Notify.Clear();
			Notify.LinkMontage(this, Notify.DisplayTime_DEPRECATED);
		}
		else
		{
			Notify.LinkMontage(this, Notify.GetTime());
		}

		if(Notify.Duration != 0.0f)
		{
			Notify.EndLink.LinkMontage(this, Notify.GetTime() + Notify.Duration);
		}
	}

	// Convert BranchingPoints to AnimNotifies.
	if (GetLinker() && (GetLinker()->UE4Ver() < VER_UE4_MONTAGE_BRANCHING_POINT_REMOVAL) )
	{
		ConvertBranchingPointsToAnimNotifies();
	}

	// fix up blending time deprecated variable
	if (BlendInTime_DEPRECATED != -1.f)
	{
		BlendIn.SetBlendTime(BlendInTime_DEPRECATED);
		BlendInTime_DEPRECATED = -1.f;
	}

	if(BlendOutTime_DEPRECATED != -1.f)
	{
		BlendOut.SetBlendTime(BlendOutTime_DEPRECATED);
		BlendOutTime_DEPRECATED = -1.f;
	}

	// collect markers if it's valid
	CollectMarkers();
}

void UAnimMontage::ConvertBranchingPointsToAnimNotifies()
{
	if (BranchingPoints_DEPRECATED.Num() > 0)
	{
		// Handle deprecated DisplayTime first
		for (auto& BranchingPoint : BranchingPoints_DEPRECATED)
		{
			if (BranchingPoint.DisplayTime_DEPRECATED != 0.0f)
			{
				BranchingPoint.Clear();
				BranchingPoint.LinkMontage(this, BranchingPoint.DisplayTime_DEPRECATED);
			}
			else
			{
				BranchingPoint.LinkMontage(this, BranchingPoint.GetTime());
			}
		}

		// Then convert to AnimNotifies
		USkeleton * MySkeleton = GetSkeleton();

#if WITH_EDITORONLY_DATA
		// Add a new AnimNotifyTrack, and place all branching points in there.
		int32 TrackIndex = AnimNotifyTracks.Num();

		FAnimNotifyTrack NewItem;
		NewItem.TrackName = *FString::FromInt(TrackIndex + 1);
		NewItem.TrackColor = FLinearColor::White;
		AnimNotifyTracks.Add(NewItem);
#endif

		for (auto BranchingPoint : BranchingPoints_DEPRECATED)
		{
			int32 NewNotifyIndex = Notifies.Add(FAnimNotifyEvent());
			FAnimNotifyEvent& NewEvent = Notifies[NewNotifyIndex];
			NewEvent.NotifyName = BranchingPoint.EventName;

			float TriggerTime = BranchingPoint.GetTriggerTime();
			NewEvent.LinkMontage(this, TriggerTime);
#if WITH_EDITOR
			NewEvent.TriggerTimeOffset = GetTriggerTimeOffsetForType(CalculateOffsetForNotify(TriggerTime));
#endif
#if WITH_EDITORONLY_DATA
			NewEvent.TrackIndex = TrackIndex;
#endif
			NewEvent.Notify = NULL;
			NewEvent.NotifyStateClass = NULL;
			NewEvent.bConvertedFromBranchingPoint = true;
			NewEvent.MontageTickType = EMontageNotifyTickType::BranchingPoint;

#if WITH_EDITORONLY_DATA
			// Add as a custom AnimNotify event to Skeleton.
			if (MySkeleton)
			{
				MySkeleton->AnimationNotifies.AddUnique(NewEvent.NotifyName);
			}
#endif
		}

		BranchingPoints_DEPRECATED.Empty();
		RefreshBranchingPointMarkers();
	}
}

void UAnimMontage::RefreshBranchingPointMarkers()
{
	BranchingPointMarkers.Empty();
	BranchingPointStateNotifyIndices.Empty();

	// Verify that we have no overlapping trigger times, this is not supported, and markers would not be triggered then.
	TMap<float, FAnimNotifyEvent*> TriggerTimes;

	int32 NumNotifies = Notifies.Num();
	for (int32 NotifyIndex = 0; NotifyIndex < NumNotifies; NotifyIndex++)
	{
		FAnimNotifyEvent& NotifyEvent = Notifies[NotifyIndex];

		if (NotifyEvent.IsBranchingPoint())
		{
			AddBranchingPointMarker(FBranchingPointMarker(NotifyIndex, NotifyEvent.GetTriggerTime(), EAnimNotifyEventType::Begin), TriggerTimes);

			if (NotifyEvent.NotifyStateClass)
			{
				// Track end point of AnimNotifyStates.
				AddBranchingPointMarker(FBranchingPointMarker(NotifyIndex, NotifyEvent.GetEndTriggerTime(), EAnimNotifyEventType::End), TriggerTimes);

				// Also track AnimNotifyStates separately, so we can tick them between their Begin and End points.
				BranchingPointStateNotifyIndices.Add(NotifyIndex);
			}
		}
	}
	
	if (BranchingPointMarkers.Num() > 0)
	{
		// Sort markers
		struct FCompareNotifyTickMarkersTime
		{
			FORCEINLINE bool operator()(const FBranchingPointMarker &A, const FBranchingPointMarker &B) const
			{
				return A.TriggerTime < B.TriggerTime;
			}
		};

		BranchingPointMarkers.Sort(FCompareNotifyTickMarkersTime());
	}
}

void UAnimMontage::RefreshCacheData()
{
	Super::RefreshCacheData();

	// This gets called whenever notifies are modified in the editor, so refresh our branch list
	RefreshBranchingPointMarkers();
#if WITH_EDITOR
	PropagateChanges();
#endif // WITH_EDITOR
}

void UAnimMontage::AddBranchingPointMarker(FBranchingPointMarker TickMarker, TMap<float, FAnimNotifyEvent*>& TriggerTimes)
{
	// Add Marker
	BranchingPointMarkers.Add(TickMarker);

	// Check that there is no overlapping marker, as we don't support this.
	// This would mean one of them is not getting triggered!
	FAnimNotifyEvent** FoundNotifyEventPtr = TriggerTimes.Find(TickMarker.TriggerTime);
	if (FoundNotifyEventPtr)
	{
		UE_ASSET_LOG(LogAnimMontage, Warning, this, TEXT("Branching Point '%s' overlaps with '%s' at time: %f. One of them will not get triggered!"),
			*Notifies[TickMarker.NotifyIndex].NotifyName.ToString(), *(*FoundNotifyEventPtr)->NotifyName.ToString(), TickMarker.TriggerTime);
	}
	else
	{
		TriggerTimes.Add(TickMarker.TriggerTime, &Notifies[TickMarker.NotifyIndex]);
	}
}

const FBranchingPointMarker* UAnimMontage::FindFirstBranchingPointMarker(float StartTrackPos, float EndTrackPos) const
{
	if (BranchingPointMarkers.Num() > 0)
	{
		const bool bSearchBackwards = (EndTrackPos < StartTrackPos);
		if (!bSearchBackwards)
		{
			for (int32 Index = 0; Index < BranchingPointMarkers.Num(); Index++)
			{
				const FBranchingPointMarker& Marker = BranchingPointMarkers[Index];
				if (Marker.TriggerTime <= StartTrackPos)
				{
					continue;
				}
				if (Marker.TriggerTime > EndTrackPos)
				{
					break;
				}
				return &Marker;
			}
		}
		else
		{
			for (int32 Index = BranchingPointMarkers.Num() - 1; Index >= 0; Index--)
			{
				const FBranchingPointMarker& Marker = BranchingPointMarkers[Index];
				if (Marker.TriggerTime >= StartTrackPos)
				{
					continue;
				}
				if (Marker.TriggerTime < EndTrackPos)
				{
					break;
				}
				return &Marker;
			}
		}
	}
	return nullptr;
}

void UAnimMontage::FilterOutNotifyBranchingPoints(TArray<const FAnimNotifyEvent*>& InAnimNotifies)
{
	for (int32 Index = InAnimNotifies.Num()-1; Index >= 0; Index--)
	{
		if (InAnimNotifies[Index]->IsBranchingPoint())
		{
			InAnimNotifies.RemoveAt(Index, 1);
		}
	}
}

#if WITH_EDITOR
void UAnimMontage::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	// It is unclear if CollectMarkers should be here or in RefreshCacheData
	if (SyncGroup != NAME_None)
	{
		CollectMarkers();
	}

	PropagateChanges();
}

void UAnimMontage::PropagateChanges()
{
	// @note propagate to children
	// this isn't that slow yet, but if this gets slow, we'll have to do guid method
	if (ChildrenAssets.Num() > 0)
	{
		for (UAnimationAsset* Child : ChildrenAssets)
		{
			if (Child)
			{
				Child->UpdateParentAsset();
			}
		}
	}
}
#endif // WITH_EDITOR

bool UAnimMontage::IsValidAdditive() const
{
	// if first one is additive, this is additive
	if ( SlotAnimTracks.Num() > 0 )
	{
		for (int32 I=0; I<SlotAnimTracks.Num(); ++I)
		{
			if (!SlotAnimTracks[I].AnimTrack.IsAdditive())
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

bool UAnimMontage::IsValidAdditiveSlot(const FName& SlotNodeName) const
{
	// if first one is additive, this is additive
	if ( SlotAnimTracks.Num() > 0 )
	{
		for (int32 I=0; I<SlotAnimTracks.Num(); ++I)
		{
			if (SlotAnimTracks[I].SlotName == SlotNodeName)
			{
				return SlotAnimTracks[I].AnimTrack.IsAdditive();
			}
		}
	}

	return false;
}

EAnimEventTriggerOffsets::Type UAnimMontage::CalculateOffsetFromSections(float Time) const
{
	for(auto Iter = CompositeSections.CreateConstIterator(); Iter; ++Iter)
	{
		float SectionTime = Iter->GetTime();
		if(FMath::IsNearlyEqual(SectionTime,Time))
		{
			return EAnimEventTriggerOffsets::OffsetBefore;
		}
	}
	return EAnimEventTriggerOffsets::NoOffset;
}

#if WITH_EDITOR
EAnimEventTriggerOffsets::Type UAnimMontage::CalculateOffsetForNotify(float NotifyDisplayTime) const
{
	EAnimEventTriggerOffsets::Type Offset = Super::CalculateOffsetForNotify(NotifyDisplayTime);
	if(Offset == EAnimEventTriggerOffsets::NoOffset)
	{
		Offset = CalculateOffsetFromSections(NotifyDisplayTime);
	}
	return Offset;
}
#endif

bool UAnimMontage::HasRootMotion() const
{
	for (const FSlotAnimationTrack& Track : SlotAnimTracks)
	{
		if (Track.AnimTrack.HasRootMotion())
		{
			return true;
		}
	}
	return false;
}

/** Extract RootMotion Transform from a contiguous Track position range.
 * *CONTIGUOUS* means that if playing forward StartTractPosition < EndTrackPosition.
 * No wrapping over if looping. No jumping across different sections.
 * So the AnimMontage has to break the update into contiguous pieces to handle those cases.
 *
 * This does handle Montage playing backwards (StartTrackPosition > EndTrackPosition).
 *
 * It will break down the range into steps if needed to handle looping animations, or different animations.
 * These steps will be processed sequentially, and output the RootMotion transform in component space.
 */
FTransform UAnimMontage::ExtractRootMotionFromTrackRange(float StartTrackPosition, float EndTrackPosition) const
{
	FRootMotionMovementParams RootMotion;

	// For now assume Root Motion only comes from first track.
	if( SlotAnimTracks.Num() > 0 )
	{
		const FAnimTrack& SlotAnimTrack = SlotAnimTracks[0].AnimTrack;

		// Get RootMotion pieces from this track.
		// We can deal with looping animations, or multiple animations. So we break those up into sequential operations.
		// (Animation, StartFrame, EndFrame) so we can then extract root motion sequentially.
		ExtractRootMotionFromTrack(SlotAnimTrack, StartTrackPosition, EndTrackPosition, RootMotion);

	}

	UE_LOG(LogRootMotion, Log,  TEXT("\tUAnimMontage::ExtractRootMotionForTrackRange RootMotionTransform: Translation: %s, Rotation: %s")
		, *RootMotion.GetRootMotionTransform().GetTranslation().ToCompactString()
		, *RootMotion.GetRootMotionTransform().GetRotation().Rotator().ToCompactString()
		);

	return RootMotion.GetRootMotionTransform();
}

/** Get Montage's Group Name */
FName UAnimMontage::GetGroupName() const
{
	USkeleton* MySkeleton = GetSkeleton();
	if (MySkeleton && (SlotAnimTracks.Num() > 0))
	{
		return MySkeleton->GetSlotGroupName(SlotAnimTracks[0].SlotName);
	}

	return FAnimSlotGroup::DefaultGroupName;
}

bool UAnimMontage::HasValidSlotSetup() const
{
	// We only need to worry about this if we have multiple tracks.
	// Montages with a single track will always have a valid slot setup.
	int32 NumAnimTracks = SlotAnimTracks.Num();
	if (NumAnimTracks > 1)
	{
		USkeleton* MySkeleton = GetSkeleton();
		if (MySkeleton)
		{
			FName MontageGroupName = GetGroupName();
			TArray<FName> UniqueSlotNameList;
			UniqueSlotNameList.Add(SlotAnimTracks[0].SlotName);

			for (int32 TrackIndex = 1; TrackIndex < NumAnimTracks; TrackIndex++)
			{
				// Verify that slot names are unique.
				FName CurrentSlotName = SlotAnimTracks[TrackIndex].SlotName;
				bool bSlotNameAlreadyInUse = UniqueSlotNameList.Contains(CurrentSlotName);
				if (!bSlotNameAlreadyInUse)
				{
					UniqueSlotNameList.Add(CurrentSlotName);
				}
				else
				{
					UE_LOG(LogAnimMontage, Warning, TEXT("Montage '%s' not properly setup. Slot named '%s' is already used in this Montage. All slots must be unique"),
						*GetFullName(), *CurrentSlotName.ToString());
					return false;
				}

				// Verify that all slots belong to the same group.
				FName CurrentSlotGroupName = MySkeleton->GetSlotGroupName(CurrentSlotName);
				bool bDifferentGroupName = (CurrentSlotGroupName != MontageGroupName);
				if (bDifferentGroupName)
				{
					UE_LOG(LogAnimMontage, Warning, TEXT("Montage '%s' not properly setup. Slot's group '%s' is different than the Montage's group '%s'. All slots must belong to the same group."),
						*GetFullName(), *CurrentSlotGroupName.ToString(), *MontageGroupName.ToString());
					return false;
				}
			}
		}
	}

	return true;
}

float UAnimMontage::CalculateSequenceLength()
{
	float CalculatedSequenceLength = 0.f;
	for (auto Iter = SlotAnimTracks.CreateIterator(); Iter; ++Iter)
	{
		FSlotAnimationTrack& SlotAnimTrack = (*Iter);
		if (SlotAnimTrack.AnimTrack.AnimSegments.Num() > 0)
		{
			CalculatedSequenceLength = FMath::Max(CalculatedSequenceLength, SlotAnimTrack.AnimTrack.GetLength());
		}
	}
	return CalculatedSequenceLength;
}

const TArray<class UAnimMetaData*> UAnimMontage::GetSectionMetaData(FName SectionName, bool bIncludeSequence/*=true*/, FName SlotName /*= NAME_None*/)
{
	TArray<class UAnimMetaData*> MetadataList;
	bool bShouldIIncludeSequence = bIncludeSequence;

	for (int32 SectionIndex = 0; SectionIndex < CompositeSections.Num(); ++SectionIndex)
	{
		const auto& CurSection = CompositeSections[SectionIndex];
		if (SectionName == NAME_None || CurSection.SectionName == SectionName)
		{
			// add to the list
			MetadataList.Append(CurSection.GetMetaData());

			if (bShouldIIncludeSequence)
			{
				if (SectionName == NAME_None)
				{
					for (auto& SlotIter : SlotAnimTracks)
					{
						if (SlotName == NAME_None || SlotIter.SlotName == SlotName)
						{
							// now add the animations within this section
							for (auto& SegmentIter : SlotIter.AnimTrack.AnimSegments)
							{
								if (SegmentIter.AnimReference)
								{
									// only add unique here
									TArray<UAnimMetaData*> RefMetadata = SegmentIter.AnimReference->GetMetaData();

									for (auto& RefData : RefMetadata)
									{
										MetadataList.AddUnique(RefData);
									}
								}
							}
						}
					}

					// if section name == None, we only grab slots once
					// otherwise, it will grab multiple times
					bShouldIIncludeSequence = false;
				}
				else
				{
					float SectionStartTime = 0.f, SectionEndTime = 0.f;
					GetSectionStartAndEndTime(SectionIndex, SectionStartTime, SectionEndTime);
					for (auto& SlotIter : SlotAnimTracks)
					{
						if (SlotName == NAME_None || SlotIter.SlotName == SlotName)
						{
							// now add the animations within this section
							for (auto& SegmentIter : SlotIter.AnimTrack.AnimSegments)
							{
								if (SegmentIter.IsIncluded(SectionStartTime, SectionEndTime))
								{
									if (SegmentIter.AnimReference)
									{
										// only add unique here
										TArray<UAnimMetaData*> RefMetadata = SegmentIter.AnimReference->GetMetaData();

										for (auto& RefData : RefMetadata)
										{
											MetadataList.AddUnique(RefData);
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	return MetadataList;
}

#if WITH_EDITOR
bool UAnimMontage::GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& AnimationAssets, bool bRecursive /*= true*/)
{
	Super::GetAllAnimationSequencesReferred(AnimationAssets, bRecursive);

	for (auto Iter = SlotAnimTracks.CreateConstIterator(); Iter; ++Iter)
	{
		const FSlotAnimationTrack& Track = (*Iter);
		Track.AnimTrack.GetAllAnimationSequencesReferred(AnimationAssets, bRecursive);
	}

	if (PreviewBasePose)
	{
		PreviewBasePose->HandleAnimReferenceCollection(AnimationAssets, bRecursive);
	}

	return (AnimationAssets.Num() > 0);
}

void UAnimMontage::ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& ReplacementMap)
{
	Super::ReplaceReferredAnimations(ReplacementMap);

	for (auto Iter = SlotAnimTracks.CreateIterator(); Iter; ++Iter)
	{
		FSlotAnimationTrack& Track = (*Iter);
		Track.AnimTrack.ReplaceReferredAnimations(ReplacementMap);
	}

	if (PreviewBasePose)
	{
		UAnimSequence* const* ReplacementAsset = (UAnimSequence*const*)ReplacementMap.Find(PreviewBasePose);
		if (ReplacementAsset)
		{
			PreviewBasePose = *ReplacementAsset;
			PreviewBasePose->ReplaceReferredAnimations(ReplacementMap);
		}
	}
}

void UAnimMontage::UpdateLinkableElements()
{
	// Update all linkable elements
	for (FCompositeSection& Section : CompositeSections)
	{
		Section.Update();
	}

	for (FAnimNotifyEvent& Notify : Notifies)
	{
		Notify.Update();
		Notify.RefreshTriggerOffset(CalculateOffsetForNotify(Notify.GetTime()));

		Notify.EndLink.Update();
		Notify.RefreshEndTriggerOffset(CalculateOffsetForNotify(Notify.EndLink.GetTime()));
	}
}

void UAnimMontage::UpdateLinkableElements(int32 SlotIdx, int32 SegmentIdx)
{
	FAnimSegment* UpdatedSegment = &SlotAnimTracks[SlotIdx].AnimTrack.AnimSegments[SegmentIdx];

	for (FCompositeSection& Section : CompositeSections)
	{
		if (Section.GetSlotIndex() == SlotIdx && Section.GetSegmentIndex() == SegmentIdx)
		{
			// Update the link
			Section.Update();
		}
	}

	for (FAnimNotifyEvent& Notify : Notifies)
	{
		if (Notify.GetSlotIndex() == SlotIdx && Notify.GetSegmentIndex() == SegmentIdx)
		{
			Notify.Update();
			Notify.RefreshTriggerOffset(CalculateOffsetForNotify(Notify.GetTime()));
		}

		if (Notify.EndLink.GetSlotIndex() == SlotIdx && Notify.EndLink.GetSegmentIndex() == SegmentIdx)
		{
			Notify.EndLink.Update();
			Notify.RefreshEndTriggerOffset(CalculateOffsetForNotify(Notify.EndLink.GetTime()));
		}
	}
}

void UAnimMontage::RefreshParentAssetData()
{
	Super::RefreshParentAssetData();

	UAnimMontage* ParentMontage = CastChecked<UAnimMontage>(ParentAsset);

	BlendIn = ParentMontage->BlendIn;
	BlendOut = ParentMontage->BlendOut;
	BlendOutTriggerTime = ParentMontage->BlendOutTriggerTime;
	SyncGroup = ParentMontage->SyncGroup;
	SyncSlotIndex = ParentMontage->SyncSlotIndex;

	MarkerData = ParentMontage->MarkerData;
	CompositeSections = ParentMontage->CompositeSections;
	SlotAnimTracks = ParentMontage->SlotAnimTracks;

	PreviewBasePose = ParentMontage->PreviewBasePose;
	BranchingPointMarkers = ParentMontage->BranchingPointMarkers;
	BranchingPointStateNotifyIndices = ParentMontage->BranchingPointStateNotifyIndices;

	for (int32 SlotIdx = 0; SlotIdx < SlotAnimTracks.Num(); ++SlotIdx)
	{
		FSlotAnimationTrack& SlotTrack = SlotAnimTracks[SlotIdx];
		
		for (int32 SegmentIdx = 0; SegmentIdx < SlotTrack.AnimTrack.AnimSegments.Num(); ++SegmentIdx)
		{
			FAnimSegment& Segment = SlotTrack.AnimTrack.AnimSegments[SegmentIdx];
			FAnimSegment& ParentSegment = ParentMontage->SlotAnimTracks[SlotIdx].AnimTrack.AnimSegments[SegmentIdx];
			UAnimSequenceBase* SourceReference = Segment.AnimReference;
			UAnimSequenceBase* TargetReference = Cast<UAnimSequenceBase>(AssetMappingTable->GetMappedAsset(SourceReference));
			Segment.AnimReference = TargetReference;

			float LengthChange = FMath::IsNearlyZero(SourceReference->SequenceLength) ? 0.f : TargetReference->SequenceLength / SourceReference->SequenceLength;
			float RateChange = FMath::IsNearlyZero(SourceReference->RateScale) ? 0.f : FMath::Abs(TargetReference->RateScale / SourceReference->RateScale);
			float TotalRateChange = FMath::IsNearlyZero(RateChange)? 0.f : (LengthChange / RateChange);
			Segment.AnimPlayRate *= TotalRateChange;
			Segment.AnimStartTime *= LengthChange;
			Segment.AnimEndTime *= LengthChange;
		}
	}

	// this delegate causes it to reconstrucand this code can be called by UI. 
	// that is dangerous as it can cause the UI to reconstruct in the middle of it. 
	// until this, the multi window won't work well. 
	//OnMontageChanged.Broadcast();
}

#endif

FString MakePositionMessage(const FMarkerSyncAnimPosition& Position)
{
	return FString::Printf(TEXT("Names(PrevName: %s | NextName: %s) PosBetweenMarkers: %.2f"), *Position.PreviousMarkerName.ToString(), *Position.NextMarkerName.ToString(), Position.PositionBetweenMarkers);
}

void UAnimMontage::TickAssetPlayer(FAnimTickRecord& Instance, struct FAnimNotifyQueue& NotifyQueue, FAnimAssetTickContext& Context) const
{
	bool bRecordNeedsResetting = true;

	// nothing has to happen here
	// we just have to make sure we set Context data correct
	//if (ensure (Context.IsLeader()))
	if ((Context.IsLeader()))
	{
		const float CurrentTime = Instance.Montage.CurrentPosition;
		const float PreviousTime = Instance.Montage.PreviousPosition;
		const float MoveDelta = Instance.Montage.MoveDelta;

		Context.SetLeaderDelta(MoveDelta);
		Context.SetPreviousAnimationPositionRatio(PreviousTime / SequenceLength);

		if (MoveDelta != 0.f)
		{
			if (Instance.bCanUseMarkerSync && Instance.MarkerTickRecord && Context.CanUseMarkerPosition())
			{
				FMarkerTickRecord* MarkerTickRecord = Instance.MarkerTickRecord;
				FMarkerTickContext& MarkerTickContext = Context.MarkerTickContext;

				if (MarkerTickRecord->IsValid())
				{
					MarkerTickContext.SetMarkerSyncStartPosition(GetMarkerSyncPositionfromMarkerIndicies(MarkerTickRecord->PreviousMarker.MarkerIndex, MarkerTickRecord->NextMarker.MarkerIndex, PreviousTime));

				}
				else
				{
					// only thing is that passed markers won't work in this frame. To do that, I have to figure out how it jumped from where to where, 
					FMarkerPair PreviousMarker;
					FMarkerPair NextMarker;
					GetMarkerIndicesForTime(PreviousTime, false, MarkerTickContext.GetValidMarkerNames(), PreviousMarker, NextMarker);
					MarkerTickContext.SetMarkerSyncStartPosition(GetMarkerSyncPositionfromMarkerIndicies(PreviousMarker.MarkerIndex, NextMarker.MarkerIndex, PreviousTime));
				}

				// @todo this won't work well once we start jumping
				// only thing is that passed markers won't work in this frame. To do that, I have to figure out how it jumped from where to where, 
				GetMarkerIndicesForTime(CurrentTime, false, MarkerTickContext.GetValidMarkerNames(), MarkerTickRecord->PreviousMarker, MarkerTickRecord->NextMarker);
				bRecordNeedsResetting = false; // we have updated it now, no need to reset
				MarkerTickContext.SetMarkerSyncEndPosition(GetMarkerSyncPositionfromMarkerIndicies(MarkerTickRecord->PreviousMarker.MarkerIndex, MarkerTickRecord->NextMarker.MarkerIndex, CurrentTime));

				MarkerTickContext.MarkersPassedThisTick = *Instance.Montage.MarkersPassedThisTick;

#if DO_CHECK
				if(MarkerTickContext.MarkersPassedThisTick.Num() == 0)
				{
					const FMarkerSyncAnimPosition& StartPosition = MarkerTickContext.GetMarkerSyncStartPosition();
					const FMarkerSyncAnimPosition& EndPosition = MarkerTickContext.GetMarkerSyncEndPosition();
					checkf(StartPosition.NextMarkerName == EndPosition.NextMarkerName, TEXT("StartPosition %s\nEndPosition %s\nPrevTime to CurrentTimeAsset: %.3f - %.3f Delta: %.3f\nAsset = %s"), *MakePositionMessage(StartPosition), *MakePositionMessage(EndPosition), PreviousTime, CurrentTime, MoveDelta, *Instance.SourceAsset->GetFullName());
					checkf(StartPosition.PreviousMarkerName == EndPosition.PreviousMarkerName, TEXT("StartPosition %s\nEndPosition %s\nPrevTime - CurrentTimeAsset: %.3f - %.3f Delta: %.3f\nAsset = %s"), *MakePositionMessage(StartPosition), *MakePositionMessage(EndPosition), PreviousTime, CurrentTime, MoveDelta, *Instance.SourceAsset->GetFullName());
				}
#endif

				UE_LOG(LogAnimMarkerSync, Log, TEXT("Montage Leading SyncGroup: %s(%s) Start [%s], End [%s]"),
					*GetNameSafe(this), *SyncGroup.ToString(), *MarkerTickContext.GetMarkerSyncStartPosition().ToString(), *MarkerTickContext.GetMarkerSyncEndPosition().ToString());
			}
		}

		Context.SetAnimationPositionRatio(CurrentTime / SequenceLength);
	}

	if (bRecordNeedsResetting && Instance.MarkerTickRecord)
	{
		Instance.MarkerTickRecord->Reset();
	}
}

void UAnimMontage::CollectMarkers()
{
	MarkerData.AuthoredSyncMarkers.Reset();

	// we want to make sure anim reference actually contains markers
	if (SyncGroup != NAME_None && SlotAnimTracks.IsValidIndex(SyncSlotIndex))
	{
		const FAnimTrack& AnimTrack = SlotAnimTracks[SyncSlotIndex].AnimTrack;
		for (const auto& Seg : AnimTrack.AnimSegments)
		{
			const UAnimSequence* Sequence = Cast<UAnimSequence>(Seg.AnimReference);
			if (Sequence && Sequence->AuthoredSyncMarkers.Num() > 0)
			{
				// @todo this won't work well if you have starttime < end time and it does have negative playrate
				for (const auto& Marker : Sequence->AuthoredSyncMarkers)
				{
					if (Marker.Time >= Seg.AnimStartTime && Marker.Time <= Seg.AnimEndTime)
					{
						const float TotalSegmentLength = (Seg.AnimEndTime - Seg.AnimStartTime)*Seg.AnimPlayRate;
						// i don't think we can do negative in this case
						ensure(TotalSegmentLength >= 0.f);

						// now add to the list
						for (int32 LoopCount = 0; LoopCount < Seg.LoopingCount; ++LoopCount)
						{
							FAnimSyncMarker NewMarker;

							NewMarker.Time = Seg.StartPos + (Marker.Time - Seg.AnimStartTime)*Seg.AnimPlayRate + TotalSegmentLength*LoopCount;
							NewMarker.MarkerName = Marker.MarkerName;
							MarkerData.AuthoredSyncMarkers.Add(NewMarker);
						}
					}
				}
			}
		}

		MarkerData.CollectUniqueNames();
	}
}

void UAnimMontage::GetMarkerIndicesForTime(float CurrentTime, bool bLooping, const TArray<FName>& ValidMarkerNames, FMarkerPair& OutPrevMarker, FMarkerPair& OutNextMarker) const
{
	MarkerData.GetMarkerIndicesForTime(CurrentTime, bLooping, ValidMarkerNames, OutPrevMarker, OutNextMarker, SequenceLength);
}

FMarkerSyncAnimPosition UAnimMontage::GetMarkerSyncPositionfromMarkerIndicies(int32 PrevMarker, int32 NextMarker, float CurrentTime) const
{
	return MarkerData.GetMarkerSyncPositionfromMarkerIndicies(PrevMarker, NextMarker, CurrentTime, SequenceLength);
}

void UAnimMontage::InvalidateRecursiveAsset()
{
	for (FSlotAnimationTrack& SlotTrack : SlotAnimTracks)
	{
		SlotTrack.AnimTrack.InvalidateRecursiveAsset(this);
	}
}

bool UAnimMontage::ContainRecursive(TArray<UAnimCompositeBase*>& CurrentAccumulatedList) 
{
	// am I included already?
	if (CurrentAccumulatedList.Contains(this))
	{
		return true;
	}

	// otherwise, add myself to it
	CurrentAccumulatedList.Add(this);

	for (FSlotAnimationTrack& SlotTrack : SlotAnimTracks)
	{
		// otherwise send to animation track
		if (SlotTrack.AnimTrack.ContainRecursive(CurrentAccumulatedList))
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// MontageInstance
/////////////////////////////////////////////////////////////////////////////////////////////

FAnimMontageInstance::FAnimMontageInstance()
	: Montage(NULL)
	, bPlaying(false)
	, DefaultBlendTimeMultiplier(1.0f)
	, bDidUseMarkerSyncThisTick(false)
	, AnimInstance(NULL)
	, InstanceID(INDEX_NONE)
	, Position(0.f)
	, PlayRate(1.f)
	, bInterrupted(false)
	, PreviousWeight(0.f)
	, NotifyWeight(0.f)
	, DeltaMoved(0.f)
	, PreviousPosition(0.f)
	, SyncGroupIndex(INDEX_NONE)
	, DisableRootMotionCount(0)
	, MontageSyncLeader(NULL)
	, MontageSyncUpdateFrameCounter(INDEX_NONE)
{
}

FAnimMontageInstance::FAnimMontageInstance(UAnimInstance * InAnimInstance)
	: Montage(NULL)
	, bPlaying(false)
	, DefaultBlendTimeMultiplier(1.0f)
	, bDidUseMarkerSyncThisTick(false)
	, AnimInstance(InAnimInstance)
	, InstanceID(INDEX_NONE)
	, Position(0.f)
	, PlayRate(1.f)
	, bInterrupted(false)
	, PreviousWeight(0.f)
	, NotifyWeight(0.f)
	, DeltaMoved(0.f)
	, PreviousPosition(0.f)
	, SyncGroupIndex(INDEX_NONE)
	, DisableRootMotionCount(0)
	, MontageSyncLeader(NULL)
	, MontageSyncUpdateFrameCounter(INDEX_NONE)
{
}

void FAnimMontageInstance::Play(float InPlayRate)
{
	bPlaying = true;
	PlayRate = InPlayRate;

	// if this doesn't exist, nothing works
	check(Montage);
	
	// set blend option
	float CurrentWeight = Blend.GetBlendedValue();
	InitializeBlend(Montage->BlendIn);	
	Blend.SetBlendTime(Montage->BlendIn.GetBlendTime() * DefaultBlendTimeMultiplier);
	Blend.SetValueRange(CurrentWeight, 1.f);
}

void FAnimMontageInstance::InitializeBlend(const FAlphaBlend& InAlphaBlend)
{
	Blend.SetBlendOption(InAlphaBlend.GetBlendOption());
	Blend.SetCustomCurve(InAlphaBlend.GetCustomCurve());
	Blend.SetBlendTime(InAlphaBlend.GetBlendTime());
}

void FAnimMontageInstance::Stop(const FAlphaBlend& InBlendOut, bool bInterrupt)
{
	if (Montage)
	{
		UE_LOG(LogAnimMontage, Verbose, TEXT("Montage.Stop Before: AnimMontage: %s,  (DesiredWeight:%0.2f, Weight:%0.2f)"),
			*Montage->GetName(), GetDesiredWeight(), GetWeight());
	}

	// overwrite bInterrupted if it hasn't already interrupted
	// once interrupted, you don't go back to non-interrupted
	if (!bInterrupted && bInterrupt)
	{
		bInterrupted = bInterrupt;
	}

	// if it hasn't stopped, stop now
	if (IsStopped() == false)
	{
		// do not use default Montage->BlendOut 
		// depending on situation, the BlendOut time can change 
		InitializeBlend(InBlendOut);
		Blend.SetDesiredValue(0.f);

		if(Montage)
		{
			if (UAnimInstance* Inst = AnimInstance.Get())
			{
				// Let AnimInstance know we are being stopped.
				Inst->OnMontageInstanceStopped(*this);
				Inst->QueueMontageBlendingOutEvent(FQueuedMontageBlendingOutEvent(Montage, bInterrupted, OnMontageBlendingOutStarted));
			}
		}
	}
	else
	{
		// it is already stopped, but new montage blendtime is shorter than what 
		// I'm blending out, that means this needs to readjust blendtime
		// that way we don't accumulate old longer blendtime for newer montage to play
		if (InBlendOut.GetBlendTime() < Blend.GetBlendTime())
		{
			// I don't know if also using inBlendOut is better than
			// currently set up blend option, but it might be worse to switch between 
			// blending out, but it is possible options in the future
			Blend.SetBlendTime(InBlendOut.GetBlendTime());
			// have to call this again to restart blending with new blend time
			// we don't change blend options
			Blend.SetDesiredValue(0.f);
		}
	}

	// if blending time < 0.f
	// set the playing to be false
	// @todo is this better to be IsComplete? 
	// or maybe we need this for if somebody sets blend time to be 0.f
	if (Blend.GetBlendTime() <= 0.0f)
	{
		bPlaying = false;
	}

	if (Montage != nullptr)
	{
		UE_LOG(LogAnimMontage, Verbose, TEXT("Montage.Stop After: AnimMontage: %s,  (DesiredWeight:%0.2f, Weight:%0.2f)"),
			*Montage->GetName(), GetDesiredWeight(), GetWeight());
	}
}

void FAnimMontageInstance::Pause()
{
	bPlaying = false;
}

void FAnimMontageInstance::Initialize(class UAnimMontage * InMontage)
{
	// Generate unique ID for this instance
	static int32 IncrementInstanceID = 0;
	InstanceID = IncrementInstanceID++;

	if (InMontage)
	{
		Montage = InMontage;
		SetPosition(0.f);
		// initialize Blend
		Blend.SetValueRange(0.f, 1.0f);
		RefreshNextPrevSections();

		if (AnimInstance.IsValid() && Montage->CanUseMarkerSync())
		{
			SyncGroupIndex = AnimInstance.Get()->GetSyncGroupIndexFromName(Montage->SyncGroup);
		}

		MontageSubStepper.Initialize(*this);
	}
}

void FAnimMontageInstance::RefreshNextPrevSections()
{
	// initialize next section
	if ( Montage->CompositeSections.Num() > 0 )
	{
		NextSections.Empty(Montage->CompositeSections.Num());
		NextSections.AddUninitialized(Montage->CompositeSections.Num());
		PrevSections.Empty(Montage->CompositeSections.Num());
		PrevSections.AddUninitialized(Montage->CompositeSections.Num());

		for (int32 I=0; I<Montage->CompositeSections.Num(); ++I)
		{
			PrevSections[I] = INDEX_NONE;
		}

		for (int32 I=0; I<Montage->CompositeSections.Num(); ++I)
		{
			FCompositeSection & Section = Montage->CompositeSections[I];
			int32 NextSectionIdx = Montage->GetSectionIndex(Section.NextSectionName);
			NextSections[I] = NextSectionIdx;
			if (NextSections.IsValidIndex(NextSectionIdx))
			{
				PrevSections[NextSectionIdx] = I;
			}
		}
	}
}

void FAnimMontageInstance::AddReferencedObjects( FReferenceCollector& Collector )
{
	if (Montage)
	{
		Collector.AddReferencedObject(Montage);
	}
}

void FAnimMontageInstance::Terminate()
{
	if (Montage == NULL)
	{
		return;
	}

	UAnimMontage* OldMontage = Montage;
	
	UAnimInstance* Inst = AnimInstance.Get();
	if (Inst)
	{
		// End all active State BranchingPoints
		for (int32 Index = ActiveStateBranchingPoints.Num() - 1; Index >= 0; Index--)
		{
			FAnimNotifyEvent& NotifyEvent = ActiveStateBranchingPoints[Index];
			FBranchingPointNotifyPayload BranchingPointNotifyPayload(AnimInstance->GetSkelMeshComponent(), Montage, &NotifyEvent, InstanceID);
			NotifyEvent.NotifyStateClass->BranchingPointNotifyEnd(BranchingPointNotifyPayload);
		}
		ActiveStateBranchingPoints.Empty();

		// terminating, trigger end
		Inst->QueueMontageEndedEvent(FQueuedMontageEndedEvent(OldMontage, bInterrupted, OnMontageEnded));

		// Clear references to this MontageInstance. Needs to happen before Montage is cleared to nullptr, as TMaps can use that as a key.
		Inst->ClearMontageInstanceReferences(*this);
	}

	// clear Blend curve
	Blend.SetCustomCurve(NULL);
	Blend.SetBlendOption(EAlphaBlendOption::Linear);

	Montage = nullptr;

	UE_LOG(LogAnimMontage, Verbose, TEXT("Terminating: AnimMontage: %s"), *GetNameSafe(OldMontage));
}

bool FAnimMontageInstance::JumpToSectionName(FName const & SectionName, bool bEndOfSection)
{
	const int32 SectionID = Montage->GetSectionIndex(SectionName);

	if (Montage->IsValidSectionIndex(SectionID))
	{
		FCompositeSection & CurSection = Montage->GetAnimCompositeSection(SectionID);
		const float NewPosition = Montage->CalculatePos(CurSection, bEndOfSection ? Montage->GetSectionLength(SectionID) - KINDA_SMALL_NUMBER : 0.0f);
		SetPosition(NewPosition);
		OnMontagePositionChanged(SectionName);
		return true;
	}

	UE_LOG(LogAnimMontage, Warning, TEXT("JumpToSectionName %s bEndOfSection: %d failed for Montage %s"),
		*SectionName.ToString(), bEndOfSection, *GetNameSafe(Montage));
	return false;
}

bool FAnimMontageInstance::SetNextSectionName(FName const & SectionName, FName const & NewNextSectionName)
{
	int32 const SectionID = Montage->GetSectionIndex(SectionName);
	int32 const NewNextSectionID = Montage->GetSectionIndex(NewNextSectionName);

	return SetNextSectionID(SectionID, NewNextSectionID);
}

bool FAnimMontageInstance::SetNextSectionID(int32 const & SectionID, int32 const & NewNextSectionID)
{
	bool const bHasValidNextSection = NextSections.IsValidIndex(SectionID);

	// disconnect prev section
	if (bHasValidNextSection && (NextSections[SectionID] != INDEX_NONE) && PrevSections.IsValidIndex(NextSections[SectionID]))
	{
		PrevSections[NextSections[SectionID]] = INDEX_NONE;
	}

	// update in-reverse next section
	if (PrevSections.IsValidIndex(NewNextSectionID))
	{
		PrevSections[NewNextSectionID] = SectionID;
	}

	// update next section for the SectionID
	// NextSection can be invalid
	if (bHasValidNextSection)
	{
		NextSections[SectionID] = NewNextSectionID;
		OnMontagePositionChanged(GetSectionNameFromID(NewNextSectionID));
		return true;
	}

	UE_LOG(LogAnimMontage, Warning, TEXT("SetNextSectionName %s to %s failed for Montage %s"),
		*GetSectionNameFromID(SectionID).ToString(), *GetSectionNameFromID(NewNextSectionID).ToString(), *GetNameSafe(Montage));

	return false;
}

void FAnimMontageInstance::OnMontagePositionChanged(FName const & ToSectionName) 
{
	if (bPlaying && IsStopped())
	{
		UE_LOG(LogAnimMontage, Warning, TEXT("Changing section on Montage (%s) to '%s' during blend out. This can cause incorrect visuals!"),
			*GetNameSafe(Montage), *ToSectionName.ToString());

		Play(PlayRate);
	}
}

FName FAnimMontageInstance::GetCurrentSection() const
{
	if ( Montage )
	{
		float CurrentPosition;
		const int32 CurrentSectionIndex = Montage->GetAnimCompositeSectionIndexFromPos(Position, CurrentPosition);
		if ( Montage->IsValidSectionIndex(CurrentSectionIndex) )
		{
			FCompositeSection& CurrentSection = Montage->GetAnimCompositeSection(CurrentSectionIndex);
			return CurrentSection.SectionName;
		}
	}

	return NAME_None;
}

FName FAnimMontageInstance::GetNextSection() const
{
	if (Montage)
	{
		float CurrentPosition;
		const int32 CurrentSectionIndex = Montage->GetAnimCompositeSectionIndexFromPos(Position, CurrentPosition);
		if (Montage->IsValidSectionIndex(CurrentSectionIndex))
		{
			FCompositeSection& CurrentSection = Montage->GetAnimCompositeSection(CurrentSectionIndex);
			return CurrentSection.NextSectionName;
		}
	}

	return NAME_None;
}

int32 FAnimMontageInstance::GetNextSectionID(int32 const & CurrentSectionID) const
{
	return NextSections.IsValidIndex(CurrentSectionID) ? NextSections[CurrentSectionID] : INDEX_NONE;
}

FName FAnimMontageInstance::GetSectionNameFromID(int32 const & SectionID) const
{
	if (Montage && Montage->IsValidSectionIndex(SectionID))
	{
		FCompositeSection const & CurrentSection = Montage->GetAnimCompositeSection(SectionID);
		return CurrentSection.SectionName;
	}

	return NAME_None;
}

void FAnimMontageInstance::MontageSync_Follow(struct FAnimMontageInstance* NewLeaderMontageInstance)
{
	// Stop following previous leader if any.
	MontageSync_StopFollowing();

	// Follow new leader
	// Note: we don't really care about detecting loops there, there's no real harm in doing so.
	if (NewLeaderMontageInstance && (NewLeaderMontageInstance != this))
	{
		NewLeaderMontageInstance->MontageSyncFollowers.AddUnique(this);
		MontageSyncLeader = NewLeaderMontageInstance;
	}
}

void FAnimMontageInstance::MontageSync_StopLeading()
{
	for (auto MontageSyncFollower : MontageSyncFollowers)
	{
		if (MontageSyncFollower)
		{
			ensure(MontageSyncFollower->MontageSyncLeader == this);
			MontageSyncFollower->MontageSyncLeader = NULL;
		}
	}
	MontageSyncFollowers.Empty();
}

void FAnimMontageInstance::MontageSync_StopFollowing()
{
	if (MontageSyncLeader)
	{
		MontageSyncLeader->MontageSyncFollowers.RemoveSingleSwap(this);
		MontageSyncLeader = NULL;
	}
}

uint32 FAnimMontageInstance::MontageSync_GetFrameCounter() const
{
	return (GFrameCounter % MAX_uint32);
}

bool FAnimMontageInstance::MontageSync_HasBeenUpdatedThisFrame() const
{
	return (MontageSyncUpdateFrameCounter == MontageSync_GetFrameCounter());
}

void FAnimMontageInstance::MontageSync_PreUpdate()
{
	// If we are being synchronized to a leader
	// And our leader HASN'T been updated yet, then we need to synchronize ourselves now.
	// We're basically synchronizing to last frame's values.
	// If we want to avoid that frame of lag, a tick prerequisite should be put between the follower and the leader.
	if (MontageSyncLeader && !MontageSyncLeader->MontageSync_HasBeenUpdatedThisFrame())
	{
		MontageSync_PerformSyncToLeader();
	}
}

void FAnimMontageInstance::MontageSync_PostUpdate()
{
	// Tag ourselves as updated this frame.
	MontageSyncUpdateFrameCounter = MontageSync_GetFrameCounter();

	// If we are being synchronized to a leader
	// And our leader HAS already been updated, then we can synchronize ourselves now.
	// To make sure we are in sync before rendering.
	if (MontageSyncLeader && MontageSyncLeader->MontageSync_HasBeenUpdatedThisFrame())
	{
		MontageSync_PerformSyncToLeader();
	}
}

void FAnimMontageInstance::MontageSync_PerformSyncToLeader()
{
	if (MontageSyncLeader)
	{
		// Sync follower position only if significant error.
		// We don't want continually 'teleport' it, which could have side-effects and skip AnimNotifies.
		const float LeaderPosition = MontageSyncLeader->GetPosition();
		const float FollowerPosition = GetPosition();
		if (FMath::Abs(FollowerPosition - LeaderPosition) > KINDA_SMALL_NUMBER)
		{
			SetPosition(LeaderPosition);
		}

		SetPlayRate(MontageSyncLeader->GetPlayRate());

		// If source and target share same section names, keep them in sync as well. So we properly handle jumps and loops.
		const FName LeaderCurrentSectionName = MontageSyncLeader->GetCurrentSection();
		if ((LeaderCurrentSectionName != NAME_None) && (GetCurrentSection() == LeaderCurrentSectionName))
		{
			const FName LeaderNextSectionName = MontageSyncLeader->GetNextSection();
			SetNextSectionName(LeaderCurrentSectionName, LeaderNextSectionName);
		}
	}
}

void FAnimMontageInstance::UpdateWeight(float DeltaTime)
{
	if ( IsValid() )
	{
		PreviousWeight = Blend.GetBlendedValue();

		// update weight
		Blend.Update(DeltaTime);

		// Notify weight is max of previous and current as notify could have come
		// from any point between now and last tick
		NotifyWeight = FMath::Max(PreviousWeight, Blend.GetBlendedValue());

		UE_LOG(LogAnimMontage, Verbose, TEXT("UpdateWeight: AnimMontage: %s,  (DesiredWeight:%0.2f, Weight:%0.2f, PreviousWeight: %0.2f)"),
			*Montage->GetName(), GetDesiredWeight(), GetWeight(), PreviousWeight);
		UE_LOG(LogAnimMontage, Verbose, TEXT("Blending Info: BlendOption : %d, AlphaLerp : %0.2f, BlendTime: %0.2f"),
			(int32)Blend.GetBlendOption(), Blend.GetAlpha(), Blend.GetBlendTime());
	}
}

bool FAnimMontageInstance::SimulateAdvance(float DeltaTime, float& InOutPosition, FRootMotionMovementParams & OutRootMotionParams) const
{
	if (!IsValid())
	{
		return false;
	}

	const bool bExtractRootMotion = Montage->HasRootMotion() && !IsRootMotionDisabled();

	FMontageSubStepper SimulateMontageSubStepper;
	SimulateMontageSubStepper.Initialize(*this);
	SimulateMontageSubStepper.AddEvaluationTime(DeltaTime);
	while (SimulateMontageSubStepper.HasTimeRemaining())
	{
		const float PreviousSubStepPosition = InOutPosition;
		EMontageSubStepResult SubStepResult = SimulateMontageSubStepper.Advance(InOutPosition, nullptr);

		if (SubStepResult != EMontageSubStepResult::Moved)
		{
			// stop and leave this loop
			break;
		}

		// Extract Root Motion for this time slice, and accumulate it.
		if (bExtractRootMotion)
		{
			OutRootMotionParams.Accumulate(Montage->ExtractRootMotionFromTrackRange(PreviousSubStepPosition, InOutPosition));
		}

		// if we reached end of section, and we were not processing a branching point, and no events has messed with out current position..
		// .. Move to next section.
		// (this also handles looping, the same as jumping to a different section).
		if (SimulateMontageSubStepper.HasReachedEndOfSection())
		{
			const int32 CurrentSectionIndex = SimulateMontageSubStepper.GetCurrentSectionIndex();
			const bool bPlayingForward = SimulateMontageSubStepper.GetbPlayingForward();

			// Get recent NextSectionIndex in case it's been changed by previous events.
			const int32 RecentNextSectionIndex = bPlayingForward ? NextSections[CurrentSectionIndex] : PrevSections[CurrentSectionIndex];
			if (RecentNextSectionIndex != INDEX_NONE)
			{
				float LatestNextSectionStartTime;
				float LatestNextSectionEndTime;
				Montage->GetSectionStartAndEndTime(RecentNextSectionIndex, LatestNextSectionStartTime, LatestNextSectionEndTime);

				// Jump to next section's appropriate starting point (start or end).
				InOutPosition = bPlayingForward ? LatestNextSectionStartTime : (LatestNextSectionEndTime - KINDA_SMALL_NUMBER); // remain within section
			}
			else
			{
				// Reached end of last section. Exit.
				break;
			}
		}
	}

	return true;
}

void FMontageSubStepper::Initialize(const struct FAnimMontageInstance& InAnimInstance)
{
	MontageInstance = &InAnimInstance;
	Montage = MontageInstance->Montage;
}

EMontageSubStepResult FMontageSubStepper::Advance(float& InOut_P_Original, const FBranchingPointMarker** OutBranchingPointMarkerPtr)
{
	DeltaMove = 0.f;

	if (MontageInstance == nullptr || (Montage == nullptr))
	{
		return EMontageSubStepResult::InvalidMontage;
	}

	bReachedEndOfSection = false;

	// Update Current Section info in case it's needed by the montage's update loop.
	// We need to do this even if we're not going to move this frame.
	// We could have been moved externally via a SetPosition() call.
	float PositionInSection;
	CurrentSectionIndex = Montage->GetAnimCompositeSectionIndexFromPos(InOut_P_Original, PositionInSection);
	if (!Montage->IsValidSectionIndex(CurrentSectionIndex))
	{
		return EMontageSubStepResult::InvalidSection;
	}

	const FCompositeSection& CurrentSection = Montage->GetAnimCompositeSection(CurrentSectionIndex);
	CurrentSectionStartTime = CurrentSection.GetTime();

	// Find end of current section. We only update one section at a time.
	CurrentSectionLength = Montage->GetSectionLength(CurrentSectionIndex);

	if (!MontageInstance->bPlaying || FMath::IsNearlyZero(TimeRemaining))
	{
		return EMontageSubStepResult::NotMoved;
	}

	// If we're forcing next position, this is our DeltaMove.
	// We don't use play rate and delta time to move.
	if (MontageInstance->ForcedNextPosition.IsSet())
	{
		const float NewPosition = MontageInstance->ForcedNextPosition.GetValue();
		DeltaMove = NewPosition - InOut_P_Original;
		PlayRate = DeltaMove / TimeRemaining;
		bPlayingForward = (DeltaMove >= 0.f);
		TimeStretchMarkerIndex = INDEX_NONE;
	}
	else 
	{
		PlayRate = MontageInstance->PlayRate * Montage->RateScale;

		if (FMath::IsNearlyZero(PlayRate))
		{
			return EMontageSubStepResult::NotMoved;
		}

		// See if we can attempt to use a TimeStretchCurve.
		const bool bAttemptTimeStretchCurve = Montage->TimeStretchCurve.IsValid() && !FMath::IsNearlyEqual(PlayRate, 1.f);
		if (bAttemptTimeStretchCurve)
		{
			// First we need to see if we have valid cached data and if it is up to date.
			ConditionallyUpdateTimeStretchCurveCachedData();
		}

		// If we're not using a TimeStretchCurve, play rate is constant.
		if (!bAttemptTimeStretchCurve || !bHasValidTimeStretchCurveData)
		{
			bPlayingForward = (PlayRate > 0.f);
			DeltaMove = TimeRemaining * PlayRate;
			TimeStretchMarkerIndex = INDEX_NONE;
		}
		else
		{
			// We're using a TimeStretchCurve.

			// Find P_Target for current InOut_P_Original.
			// Not that something external could have modified the montage's position.
			// So we need to refresh our P_Target.
			float P_Target = FindMontagePosition_Target(InOut_P_Original);

			// With P_Target, we're in 'play back time' space. 
			// So we can add our delta time there directly.
			P_Target += bPlayingForward ? TimeRemaining : -TimeRemaining;
			// Make sure we don't exceed our boundaries.
			P_Target = TimeStretchCurveInstance.Clamp_P_Target(P_Target);

			// Now we can map this back into 'original' space and find which frame of animation we should play.
			const float NewP_Original = FindMontagePosition_Original(P_Target);

			// And from there, derive our DeltaMove and actual PlayRate for this substep.
			DeltaMove = NewP_Original - InOut_P_Original;
			PlayRate = DeltaMove / TimeRemaining;
		}
	}

	// Now look for a branching point. If we have one, stop there first to handle it.
	// We need to stop at branching points, because they can trigger events that can cause side effects
	// (jumping to a new position, changing sections, changing play rate, etc).
	if (OutBranchingPointMarkerPtr)
	{
		*OutBranchingPointMarkerPtr = Montage->FindFirstBranchingPointMarker(InOut_P_Original, InOut_P_Original + DeltaMove);
		if (*OutBranchingPointMarkerPtr)
		{
			// If we have a branching point, adjust DeltaMove so we stop there.
			DeltaMove = (*OutBranchingPointMarkerPtr)->TriggerTime - InOut_P_Original;
		}
	}

	// Finally clamp DeltaMove by section markers.
	{
		// Clamp DeltaMove based on move allowed within current section
		// We stop at each section marker to evaluate whether we should jump to another section marker or not.
		// Test is inclusive, so we know if we've reached marker or not.
		if (bPlayingForward)
		{
			const float MaxSectionMove = CurrentSectionLength - PositionInSection;
			if (DeltaMove >= MaxSectionMove)
			{
				DeltaMove = MaxSectionMove;
				bReachedEndOfSection = true;
			}
		}
		else
		{
			const float MinSectionMove = /* 0.f */ - PositionInSection;
			if (DeltaMove <= MinSectionMove)
			{
				DeltaMove = MinSectionMove;
				bReachedEndOfSection = true;
			}
		}
	}

	// DeltaMove is now final, see if it has any effect on our position.
	if (FMath::Abs(DeltaMove) > 0.f)
	{
		// Note that we don't worry about looping and wrapping around here.
		// We step per section to simplify code to extract notifies/root motion/etc.
		InOut_P_Original += DeltaMove;

		// Decrease RemainingTime with actual time elapsed 
		// So we can take more substeps as needed.
		const float TimeStep = DeltaMove / PlayRate;
		ensure(TimeStep >= 0.f);
		TimeRemaining = FMath::Max(TimeRemaining - TimeStep, 0.f);

		return EMontageSubStepResult::Moved;
	}
	else
	{
		return EMontageSubStepResult::NotMoved;
	}
}

void FMontageSubStepper::ConditionallyUpdateTimeStretchCurveCachedData()
{
	// CombinedPlayRate defines our overall desired play back time, aka T_Target.
	// When using a TimeStretchCurve, this also defines S and U.
	// Only update these if CombinedPlayRate has changed.
	const float CombinedPlayRate = MontageInstance->PlayRate * Montage->RateScale;
	if (CombinedPlayRate == Cached_CombinedPlayRate)
	{
		return;
	}
	Cached_CombinedPlayRate = CombinedPlayRate;
	
	// We'll set this to true at the end, if we succeed with valid data.
	bHasValidTimeStretchCurveData = false;

	// We should not be using this code path with a 0 play rate
	// or a 1 play rate. we can use traditional cheaper update without curve.
	ensure(!FMath::IsNearlyZero(CombinedPlayRate));
	ensure(!FMath::IsNearlyEqual(CombinedPlayRate, 1.f));

	bPlayingForward = (CombinedPlayRate > 0.f);
	TimeStretchCurveInstance.InitializeFromPlayRate(CombinedPlayRate, Montage->TimeStretchCurve);

	/*
		Section Segment Positions in Target space will have to be re-cached, as needed.
		This is to determine 'remaining time until end' to trigger blend outs.
		But most montages don't use sections.
		So this is optional and done on demand.
	*/
	{
		const int32 NumSections = Montage->CompositeSections.Num();
		SectionStartPositions_Target.Reset(NumSections);
		SectionStartPositions_Target.Init(-1.f, NumSections);
		SectionEndPositions_Target.Reset(NumSections);
		SectionEndPositions_Target.Init(-1.f, NumSections);
	}

	bHasValidTimeStretchCurveData = TimeStretchCurveInstance.HasValidData();
}

float FMontageSubStepper::FindMontagePosition_Target(float In_P_Original)
{
	check(bHasValidTimeStretchCurveData);

	// See if our cached version is not up to date.
	// Then we need to update it.
	if (In_P_Original != Cached_P_Original)
	{
		// Update cached value.
		Cached_P_Original = In_P_Original;

		// Update TimeStretchMarkerIndex if needed.
		// This would happen if we jumped position due to sections or external input.
		TimeStretchCurveInstance.UpdateMarkerIndexForPosition(TimeStretchMarkerIndex, Cached_P_Original, TimeStretchCurveInstance.GetMarkers_Original());

		// With an accurate TimeStretchMarkerIndex, we can map P_Original to P_Target
		Cached_P_Target = TimeStretchCurveInstance.Convert_P_Original_To_Target(TimeStretchMarkerIndex, Cached_P_Original);
	}

	return Cached_P_Target;
}

float FMontageSubStepper::FindMontagePosition_Original(float In_P_Target)
{
	check(bHasValidTimeStretchCurveData);

	// See if our cached version is not up to date.
	// Then we need to update it.
	if (In_P_Target != Cached_P_Target)
	{
		// Update cached value.
		Cached_P_Target = In_P_Target;

		// Update TimeStretchMarkerIndex if needed.
		// This would happen if we jumped position due to sections or external input.
		TimeStretchCurveInstance.UpdateMarkerIndexForPosition(TimeStretchMarkerIndex, Cached_P_Target, TimeStretchCurveInstance.GetMarkers_Target());

		// With an accurate TimeStretchMarkerIndex, we can map P_Original to P_Target
		Cached_P_Original = TimeStretchCurveInstance.Convert_P_Target_To_Original(TimeStretchMarkerIndex, Cached_P_Target);
	}

	return Cached_P_Original;
}

float FMontageSubStepper::GetCurrSectionStartPosition_Target() const
{
	check(bHasValidTimeStretchCurveData);

	const float CachedSectionStartPosition_Target = SectionStartPositions_Target[CurrentSectionIndex];
	if (CachedSectionStartPosition_Target >= 0.f)
	{
		return CachedSectionStartPosition_Target;
	}

	const int32 SectionStartMarkerIndex = TimeStretchCurveInstance.BinarySearchMarkerIndex(CurrentSectionStartTime, TimeStretchCurveInstance.GetMarkers_Original());
	const float SectionStart_Target = TimeStretchCurveInstance.Convert_P_Original_To_Target(SectionStartMarkerIndex, CurrentSectionStartTime);

	SectionStartPositions_Target[CurrentSectionIndex] = SectionStart_Target;

	return SectionStart_Target;
}

float FMontageSubStepper::GetCurrSectionEndPosition_Target() const
{
	check(bHasValidTimeStretchCurveData);

	const float CachedSectionEndPosition_Target = SectionEndPositions_Target[CurrentSectionIndex];
	if (CachedSectionEndPosition_Target >= 0.f)
	{
		return CachedSectionEndPosition_Target;
	}

	const float SectionEnd_Original = CurrentSectionStartTime + CurrentSectionLength;
	const int32 SectionEndMarkerIndex = TimeStretchCurveInstance.BinarySearchMarkerIndex(SectionEnd_Original, TimeStretchCurveInstance.GetMarkers_Original());
	const float SectionEnd_Target = TimeStretchCurveInstance.Convert_P_Original_To_Target(SectionEndMarkerIndex, SectionEnd_Original);

	SectionEndPositions_Target[CurrentSectionIndex] = SectionEnd_Target;

	return SectionEnd_Target;
}

float FMontageSubStepper::GetRemainingPlayTimeToSectionEnd(const float In_P_Original)
{
	// If our current play rate is zero, we can't predict our remaining play time.
	if (FMath::IsNearlyZero(PlayRate))
	{
		return BIG_NUMBER;
	}

	// Find position in montage where current section ends.
	const float CurrSectionEnd_Original = bPlayingForward
		? (CurrentSectionStartTime + CurrentSectionLength)
		: CurrentSectionStartTime;

	// If we have no TimeStretchCurve, it's pretty straight forward.
	// Assume constant play rate.
	if (TimeStretchMarkerIndex == INDEX_NONE)
	{
		const float DeltaPositionToEnd = CurrSectionEnd_Original - In_P_Original;
		const float RemainingPlayTime = FMath::Abs(DeltaPositionToEnd / PlayRate);
		return RemainingPlayTime;
	}

	// We're using a TimeStretchCurve.
	check(bHasValidTimeStretchCurveData);

	// Find our position in 'target' space. This is in play back time.
	const float P_Target = FindMontagePosition_Target(In_P_Original);
	if (bPlayingForward)
	{
		// Find CurrSectionEnd_Target.
		if (FMath::IsNearlyEqual(CurrSectionEnd_Original, TimeStretchCurveInstance.Get_T_Original()))
		{
			const float RemainingPlayTime = (TimeStretchCurveInstance.Get_T_Target() - P_Target);
			return RemainingPlayTime;
		}
		else
		{
			const float CurrSectionEnd_Target = GetCurrSectionEndPosition_Target();
			const float RemainingPlayTime = (CurrSectionEnd_Target - P_Target);
			return RemainingPlayTime;
		}
	}
	// Playing Backwards
	else
	{
		// Find CurrSectionEnd_Target.
		if (FMath::IsNearlyEqual(CurrSectionEnd_Original, 0.f))
		{
			const float RemainingPlayTime = P_Target;
			return RemainingPlayTime;
		}
		else
		{
			const float CurrSectionStart_Target = GetCurrSectionStartPosition_Target();
			const float RemainingPlayTime = (P_Target - CurrSectionStart_Target);
			return RemainingPlayTime;
		}
	}
}

#if WITH_EDITOR
void FAnimMontageInstance::EditorOnly_PreAdvance()
{
	// this is necessary and it is not easy to do outside of here
	// since undo also can change composite sections
	if ((Montage->CompositeSections.Num() != NextSections.Num()) || (Montage->CompositeSections.Num() != PrevSections.Num()))
	{
		RefreshNextPrevSections();
	}

	// Auto refresh this in editor to catch changes being made to AnimNotifies.
	// RefreshCacheData should handle this but I'm not 100% sure it will cover all existing cases
	Montage->RefreshBranchingPointMarkers();

	// Bake TimeStretchCurve in editor to catch any edits made to source curve.
	Montage->BakeTimeStretchCurve();
	// Clear cached data, so it can be recached from updated time stretch curve.
	MontageSubStepper.ClearCachedData();
}
#endif

void FAnimMontageInstance::Advance(float DeltaTime, struct FRootMotionMovementParams* OutRootMotionParams, bool bBlendRootMotion)
{
	SCOPE_CYCLE_COUNTER(STAT_AnimMontageInstance_Advance);
	FScopeCycleCounterUObject MontageScope(Montage);

	if (IsValid())
	{
		// with custom curves, we can't just filter by weight
		// also if you have custom curve with longer 0, you'll likely to pause montage during that blending time
		// I think that is a bug. It still should move, the weight might come back later. 
		if (bPlaying)
		{
			const bool bExtractRootMotion = (OutRootMotionParams != nullptr) && Montage->HasRootMotion();
			
			DeltaMoved = 0.f;
			PreviousPosition = Position;

			bDidUseMarkerSyncThisTick = CanUseMarkerSync();
			if (bDidUseMarkerSyncThisTick)
			{
				MarkersPassedThisTick.Reset();
			}
			
			/** 
				Limit number of iterations for performance.
				This can get out of control if PlayRate is set really high, or there is a hitch, and Montage is looping for example.
			*/
			const int32 MaxIterations = 10;
			int32 NumIterations = 0;

			/** 
				If we're hitting our max number of iterations for whatever reason,
				make sure we're not accumulating too much time, and go out of range.
			*/
			if (MontageSubStepper.GetRemainingTime() < 10.f)
			{
				MontageSubStepper.AddEvaluationTime(DeltaTime);
			}

			while (bPlaying && MontageSubStepper.HasTimeRemaining() && (++NumIterations < MaxIterations))
			{
				SCOPE_CYCLE_COUNTER(STAT_AnimMontageInstance_Advance_Iteration);

				const float PreviousSubStepPosition = Position;
				const FBranchingPointMarker* BranchingPointMarker = nullptr;
				EMontageSubStepResult SubStepResult = MontageSubStepper.Advance(Position, &BranchingPointMarker);

				if (SubStepResult == EMontageSubStepResult::InvalidSection
					|| SubStepResult == EMontageSubStepResult::InvalidMontage)
				{
					// stop and leave this loop
					Stop(FAlphaBlend(Montage->BlendOut, Montage->BlendOut.GetBlendTime() * DefaultBlendTimeMultiplier), false);
					break;
				}

				const float SubStepDeltaMove = MontageSubStepper.GetDeltaMove();
				DeltaMoved += SubStepDeltaMove;
				const bool bPlayingForward = MontageSubStepper.GetbPlayingForward();

				// If current section is last one, check to trigger a blend out and if it hasn't stopped yet, see if we should stop
				// We check this even if we haven't moved, in case our position was different from last frame.
				// (Code triggered a position jump).
				if (!IsStopped())
				{
					const int32 CurrentSectionIndex = MontageSubStepper.GetCurrentSectionIndex();
					check(NextSections.IsValidIndex(CurrentSectionIndex));
					const int32 NextSectionIndex = bPlayingForward ? NextSections[CurrentSectionIndex] : PrevSections[CurrentSectionIndex];
					if (NextSectionIndex == INDEX_NONE)
					{
						const float PlayTimeToEnd = MontageSubStepper.GetRemainingPlayTimeToSectionEnd(Position);

						const bool bCustomBlendOutTriggerTime = (Montage->BlendOutTriggerTime >= 0);
						const float DefaultBlendOutTime = Montage->BlendOut.GetBlendTime() * DefaultBlendTimeMultiplier;
						const float BlendOutTriggerTime = bCustomBlendOutTriggerTime ? Montage->BlendOutTriggerTime : DefaultBlendOutTime;

						// ... trigger blend out if within blend out time window.
						if (PlayTimeToEnd <= FMath::Max<float>(BlendOutTriggerTime, KINDA_SMALL_NUMBER))
						{
							const float BlendOutTime = bCustomBlendOutTriggerTime ? DefaultBlendOutTime : PlayTimeToEnd;
							Stop(FAlphaBlend(Montage->BlendOut, BlendOutTime), false);
						}
					}
				}

				const bool bHaveMoved = (SubStepResult == EMontageSubStepResult::Moved);
				if (bHaveMoved)
				{
					if (bDidUseMarkerSyncThisTick)
					{
						Montage->MarkerData.CollectMarkersInRange(PreviousSubStepPosition, Position, MarkersPassedThisTick, SubStepDeltaMove);
					}

					// Extract Root Motion for this time slice, and accumulate it.
					// IsRootMotionDisabled() can be changed by AnimNotifyState BranchingPoints while advancing, so it needs to be checked here.
					if (bExtractRootMotion && AnimInstance.IsValid() && !IsRootMotionDisabled())
					{
						const FTransform RootMotion = Montage->ExtractRootMotionFromTrackRange(PreviousSubStepPosition, Position);
						if (bBlendRootMotion)
						{
							// Defer blending in our root motion until after we get our slot weight updated
							const float Weight = Blend.GetBlendedValue();
							AnimInstance.Get()->QueueRootMotionBlend(RootMotion, Montage->SlotAnimTracks[0].SlotName, Weight);
						}
						else
						{
							OutRootMotionParams->Accumulate(RootMotion);
						}

						UE_LOG(LogRootMotion, Log, TEXT("\tFAnimMontageInstance::Advance ExtractedRootMotion: %s, AccumulatedRootMotion: %s, bBlendRootMotion: %d")
							, *RootMotion.GetTranslation().ToCompactString()
							, *OutRootMotionParams->GetRootMotionTransform().GetTranslation().ToCompactString()
							, bBlendRootMotion
							);
					}

					// Delegate has to be called last in this loop
					// so that if this changes position, the new position will be applied in the next loop
					// first need to have event handler to handle it
					// Save off position before triggering events, in case they cause a jump to another position
					const float PositionBeforeFiringEvents = Position;

					// Save position before firing events.
					if (!bInterrupted)
					{
						HandleEvents(PreviousSubStepPosition, Position, BranchingPointMarker);
					}

					// if we reached end of section, and we were not processing a branching point, and no events has messed with out current position..
					// .. Move to next section.
					// (this also handles looping, the same as jumping to a different section).
					if (MontageSubStepper.HasReachedEndOfSection() && !BranchingPointMarker && (PositionBeforeFiringEvents == Position))
					{
						// Get recent NextSectionIndex in case it's been changed by previous events.
						const int32 CurrentSectionIndex = MontageSubStepper.GetCurrentSectionIndex();
						const int32 RecentNextSectionIndex = bPlayingForward ? NextSections[CurrentSectionIndex] : PrevSections[CurrentSectionIndex];
						if (RecentNextSectionIndex != INDEX_NONE)
						{
							float LatestNextSectionStartTime, LatestNextSectionEndTime;
							Montage->GetSectionStartAndEndTime(RecentNextSectionIndex, LatestNextSectionStartTime, LatestNextSectionEndTime);

							// Jump to next section's appropriate starting point (start or end).
							const float EndOffset = KINDA_SMALL_NUMBER / 2.f; //KINDA_SMALL_NUMBER/2 because we use KINDA_SMALL_NUMBER to offset notifies for triggering and SMALL_NUMBER is too small
							Position = bPlayingForward ? LatestNextSectionStartTime : (LatestNextSectionEndTime - EndOffset);
						}
						else
						{
							// If there is no next section and we've reached the end of this one, exit
							break;
						}
					}
				}

				if (SubStepResult == EMontageSubStepResult::NotMoved)
				{
					// If it hasn't moved, there is nothing much to do but weight update
					break;
				}
			}
		
			// if we had a ForcedNextPosition set, reset it.
			ForcedNextPosition.Reset();
		}
	}

	// If this Montage has no weight, it should be terminated.
	if (IsStopped() && (Blend.IsComplete()))
	{
		// nothing else to do
		Terminate();
		return;
	}

	if (!bInterrupted)
	{
		SCOPE_CYCLE_COUNTER(STAT_AnimMontageInstance_TickBranchPoints);

		// Tick all active state branching points
		for (int32 Index = 0; Index < ActiveStateBranchingPoints.Num(); Index++)
		{
			FAnimNotifyEvent& NotifyEvent = ActiveStateBranchingPoints[Index];
			FBranchingPointNotifyPayload BranchingPointNotifyPayload(AnimInstance->GetSkelMeshComponent(), Montage, &NotifyEvent, InstanceID);
			NotifyEvent.NotifyStateClass->BranchingPointNotifyTick(BranchingPointNotifyPayload, DeltaTime);
		}
	}
}

void FAnimMontageInstance::HandleEvents(float PreviousTrackPos, float CurrentTrackPos, const FBranchingPointMarker* BranchingPointMarker)
{
	// Skip notifies and branching points if montage has been interrupted.
	if (bInterrupted)
	{
		return;
	}

	// now get active Notifies based on how it advanced
	if (AnimInstance.IsValid())
	{
		TArray<const FAnimNotifyEvent*> Notifies;
		TMap<FName, TArray<const FAnimNotifyEvent*>> NotifyMap;

		// We already break up AnimMontage update to handle looping, so we guarantee that PreviousPos and CurrentPos are contiguous.
		Montage->GetAnimNotifiesFromDeltaPositions(PreviousTrackPos, CurrentTrackPos, Notifies);

		// For Montage only, remove notifies marked as 'branching points'. They are not queued and are handled separately.
		Montage->FilterOutNotifyBranchingPoints(Notifies);

		// now trigger notifies for all animations within montage
		// we'll do this for all slots for now
		for (auto SlotTrack = Montage->SlotAnimTracks.CreateIterator(); SlotTrack; ++SlotTrack)
		{
			TArray<const FAnimNotifyEvent*>& SlotTrackNotifies = NotifyMap.FindOrAdd(SlotTrack->SlotName);
			SlotTrack->AnimTrack.GetAnimNotifiesFromTrackPositions(PreviousTrackPos, CurrentTrackPos, SlotTrackNotifies);
		}

		// Queue all these notifies.
		AnimInstance->NotifyQueue.AddAnimNotifies(Notifies, NotifyWeight);
		AnimInstance->NotifyQueue.AddAnimNotifies(NotifyMap, NotifyWeight);
	}

	// Update active state branching points, before we handle the immediate tick marker.
	// In case our position jumped on the timeline, we need to begin/end state branching points accordingly.
	UpdateActiveStateBranchingPoints(CurrentTrackPos);

	// Trigger ImmediateTickMarker event if we have one
	if (BranchingPointMarker)
	{
		BranchingPointEventHandler(BranchingPointMarker);
	}
}

void FAnimMontageInstance::UpdateActiveStateBranchingPoints(float CurrentTrackPosition)
{
	int32 NumStateBranchingPoints = Montage->BranchingPointStateNotifyIndices.Num();
	if (NumStateBranchingPoints > 0)
	{
		// End no longer active events first. We want this to happen before we trigger NotifyBegin on newly active events.
		for (int32 Index = ActiveStateBranchingPoints.Num() - 1; Index >= 0; Index--)
		{
			FAnimNotifyEvent& NotifyEvent = ActiveStateBranchingPoints[Index];

			const float NotifyStartTime = NotifyEvent.GetTriggerTime();
			const float NotifyEndTime = NotifyEvent.GetEndTriggerTime();
			bool bNotifyIsActive = (CurrentTrackPosition > NotifyStartTime) && (CurrentTrackPosition <= NotifyEndTime);

			if (!bNotifyIsActive)
			{
				FBranchingPointNotifyPayload BranchingPointNotifyPayload(AnimInstance->GetSkelMeshComponent(), Montage, &NotifyEvent, InstanceID);
				NotifyEvent.NotifyStateClass->BranchingPointNotifyEnd(BranchingPointNotifyPayload);
				ActiveStateBranchingPoints.RemoveAt(Index, 1);
			}
		}

		// Then, begin newly active notifies
		for (int32 Index = 0; Index < NumStateBranchingPoints; Index++)
		{
			const int32 NotifyIndex = Montage->BranchingPointStateNotifyIndices[Index];
			FAnimNotifyEvent& NotifyEvent = Montage->Notifies[NotifyIndex];

			const float NotifyStartTime = NotifyEvent.GetTriggerTime();
			const float NotifyEndTime = NotifyEvent.GetEndTriggerTime();

			bool bNotifyIsActive = (CurrentTrackPosition > NotifyStartTime) && (CurrentTrackPosition <= NotifyEndTime);
			if (bNotifyIsActive && !ActiveStateBranchingPoints.Contains(NotifyEvent))
			{
				FBranchingPointNotifyPayload BranchingPointNotifyPayload(AnimInstance->GetSkelMeshComponent(), Montage, &NotifyEvent, InstanceID);
				NotifyEvent.NotifyStateClass->BranchingPointNotifyBegin(BranchingPointNotifyPayload);
				ActiveStateBranchingPoints.Add(NotifyEvent);
			}
		}
	}
}

void FAnimMontageInstance::BranchingPointEventHandler(const FBranchingPointMarker* BranchingPointMarker)
{
	if (AnimInstance.IsValid() && Montage && BranchingPointMarker)
	{
		FAnimNotifyEvent* NotifyEvent = (BranchingPointMarker->NotifyIndex < Montage->Notifies.Num()) ? &Montage->Notifies[BranchingPointMarker->NotifyIndex] : NULL;
		if (NotifyEvent)
		{
			// Handle backwards compatibility with older BranchingPoints.
			if (NotifyEvent->bConvertedFromBranchingPoint && (NotifyEvent->NotifyName != NAME_None))
			{
				FString FuncName = FString::Printf(TEXT("MontageBranchingPoint_%s"), *NotifyEvent->NotifyName.ToString());
				FName FuncFName = FName(*FuncName);

				UFunction* Function = AnimInstance.Get()->FindFunction(FuncFName);
				if (Function)
				{
					AnimInstance.Get()->ProcessEvent(Function, NULL);
				}
				// In case older BranchingPoint has been re-implemented as a new Custom Notify, this is if BranchingPoint function hasn't been found.
				else
				{
					AnimInstance.Get()->TriggerSingleAnimNotify(NotifyEvent);
				}
			}
			else if (NotifyEvent->NotifyStateClass != NULL)
			{
				if (BranchingPointMarker->NotifyEventType == EAnimNotifyEventType::Begin)
				{
					FBranchingPointNotifyPayload BranchingPointNotifyPayload(AnimInstance->GetSkelMeshComponent(), Montage, NotifyEvent, InstanceID);
					NotifyEvent->NotifyStateClass->BranchingPointNotifyBegin(BranchingPointNotifyPayload);
					ActiveStateBranchingPoints.Add(*NotifyEvent);
				}
				else
				{
					FBranchingPointNotifyPayload BranchingPointNotifyPayload(AnimInstance->GetSkelMeshComponent(), Montage, NotifyEvent, InstanceID);
					NotifyEvent->NotifyStateClass->BranchingPointNotifyEnd(BranchingPointNotifyPayload);
					ActiveStateBranchingPoints.RemoveSingleSwap(*NotifyEvent);
				}
			}
			// Non state notify with a native notify class
			else if	(NotifyEvent->Notify != nullptr)
			{
				// Implemented notify: just call Notify. UAnimNotify will forward this to the event which will do the work.
				FBranchingPointNotifyPayload BranchingPointNotifyPayload(AnimInstance->GetSkelMeshComponent(), Montage, NotifyEvent, InstanceID);
				NotifyEvent->Notify->BranchingPointNotify(BranchingPointNotifyPayload);
			}
			// Try to match a notify function by name.
			else
			{
				AnimInstance.Get()->TriggerSingleAnimNotify(NotifyEvent);
			}
		}
	}
}

UAnimMontage* FAnimMontageInstance::InitializeMatineeControl(FName SlotName, USkeletalMeshComponent* SkeletalMeshComponent, UAnimSequenceBase* InAnimSequence, bool bLooping)
{
	UAnimMontage* MontageToPlay = Cast<UAnimMontage>(InAnimSequence);

	if (UAnimSingleNodeInstance* SingleNodeInst = SkeletalMeshComponent->GetSingleNodeInstance())
	{
		// Single node anim instance
		if (SingleNodeInst->GetCurrentAsset() != InAnimSequence)
		{
			SingleNodeInst->SetAnimationAsset(InAnimSequence, bLooping);
			SingleNodeInst->SetPosition(0.0f);
		}

		if (SingleNodeInst->IsLooping() != bLooping)
		{
			SingleNodeInst->SetLooping(bLooping);
		}

		return MontageToPlay;
	}
	else if (UAnimInstance* AnimInst = SkeletalMeshComponent->GetAnimInstance())
	{
		UAnimMontage* PreviousMontage = nullptr;

		if (MontageToPlay)
		{
			if (!AnimInst->Montage_IsPlaying(MontageToPlay))
			{
				// Will reuse an existing montage instance for this montage, if one already exists
				AnimInst->Montage_Play(MontageToPlay, 0.f);
			}

			return MontageToPlay;
		}

		// We need to attempt find an existing slot animation
		for (FAnimMontageInstance* MontageInstance : AnimInst->MontageInstances)
		{
			if (!MontageInstance || !MontageInstance->IsActive())
			{
				continue;
			}

			// Try and find an anim track for the desired slot
			UAnimMontage* ThisMontage = MontageInstance->Montage;
			const FAnimTrack* AnimTrack = ( ThisMontage && ThisMontage->GetOuter() == GetTransientPackage() ) ? ThisMontage->GetAnimationData(SlotName) : nullptr;
			if (!AnimTrack)
			{
				continue;
			}

			// Try and find our asset on this track
			if (AnimTrack->AnimSegments.Num() == 1 && AnimTrack->AnimSegments[0].AnimReference == InAnimSequence)
			{
				// We've already found an active animation in this slot that's playing our animation. Use that.
				return ThisMontage;
			}

			// Something animating this slot that's not us - set weight to be 0 on this slot
			MontageInstance->Blend.SetDesiredValue(0.f);
			MontageInstance->Blend.SetAlpha(1.f);
		}

		return AnimInst->PlaySlotAnimationAsDynamicMontage(InAnimSequence, SlotName, 0.0f, 0.0f, 0.f, 1);
	}

	return nullptr;
}

UAnimMontage* FAnimMontageInstance::SetMatineeAnimPositionInner(FName SlotName, USkeletalMeshComponent* SkeletalMeshComponent, UAnimSequenceBase* InAnimSequence, float InPosition, bool bLooping)
{
	UAnimMontage* PlayingMontage = InitializeMatineeControl(SlotName, SkeletalMeshComponent, InAnimSequence, bLooping);
	UAnimInstance* AnimInst = SkeletalMeshComponent->GetAnimInstance();
	if (UAnimSingleNodeInstance* SingleNodeInst = SkeletalMeshComponent->GetSingleNodeInstance())
	{
		if (SingleNodeInst->GetCurrentTime() != InPosition)
		{
			SingleNodeInst->SetPosition(InPosition);
		}
	}
	else if (PlayingMontage && AnimInst)
	{
		FAnimMontageInstance* AnimMontageInst = AnimInst->GetActiveInstanceForMontage(PlayingMontage);
		if (!AnimMontageInst)
		{
			UE_LOG(LogSkeletalMesh, Warning, TEXT("Unable to set animation position for montage on slot name: %s"), *SlotName.ToString());
			return nullptr;
		}

		// ensure full weighting to this instance
		AnimMontageInst->Blend.SetDesiredValue(1.f);
		AnimMontageInst->Blend.SetAlpha(1.f);

		AnimMontageInst->SetNextPositionWithEvents(InPosition);
	}
	else
	{
		UE_LOG(LogSkeletalMesh, Warning, TEXT("Invalid animation configuration when attempting to set animation possition with : %s"), *InAnimSequence->GetName());
	}

	return PlayingMontage;
}

UAnimMontage* FAnimMontageInstance::PreviewMatineeSetAnimPositionInner(FName SlotName, USkeletalMeshComponent* SkeletalMeshComponent, UAnimSequenceBase* InAnimSequence, float InPosition, bool bLooping, bool bFireNotifies, float DeltaTime)
{
	// Codepath for updating an animation when the skeletal mesh component is not going to be ticked (ie in editor)
	UAnimMontage* PlayingMontage = InitializeMatineeControl(SlotName, SkeletalMeshComponent, InAnimSequence, bLooping);

	UAnimInstance* AnimInst = SkeletalMeshComponent->GetAnimInstance();

	FAnimMontageInstance* MontageInstanceToUpdate = AnimInst && PlayingMontage ? AnimInst->GetActiveInstanceForMontage(PlayingMontage) : nullptr;
	float PreviousPosition = InPosition;

	if (UAnimSingleNodeInstance* SingleNodeInst = SkeletalMeshComponent->GetSingleNodeInstance())
	{
		PreviousPosition = SingleNodeInst->GetCurrentTime();

		// If we're playing a montage, we fire notifies explicitly below (rather than allowing the single node instance to do it)
		const bool bFireNotifiesHere = bFireNotifies && PlayingMontage == nullptr;

		if (DeltaTime == 0.f)
		{
			const float PreviousTime = InPosition;
			SingleNodeInst->SetPositionWithPreviousTime(InPosition, PreviousTime, bFireNotifiesHere);
		}
		else
		{
			SingleNodeInst->SetPosition(InPosition, bFireNotifiesHere);
		}
	}
	else if (MontageInstanceToUpdate)
	{
		// ensure full weighting to this instance
		MontageInstanceToUpdate->Blend.SetDesiredValue(1.f);
		MontageInstanceToUpdate->Blend.SetAlpha(1.f);

		PreviousPosition = AnimInst->Montage_GetPosition(PlayingMontage);
		AnimInst->Montage_SetPosition(PlayingMontage, InPosition);
	}
	else
	{
		UE_LOG(LogSkeletalMesh, Warning, TEXT("Invalid animation configuration when attempting to set animation possition with : %s"), *InAnimSequence->GetName());
	}

	// Now force the animation system to update, if we have a montage instance
	if (MontageInstanceToUpdate)
	{
		AnimInst->UpdateAnimation(DeltaTime, false);

		// since we don't advance montage in the tick, we manually have to handle notifies
		MontageInstanceToUpdate->HandleEvents(PreviousPosition, InPosition, NULL);

		if (!bFireNotifies)
		{
			AnimInst->NotifyQueue.Reset(SkeletalMeshComponent);
		}

		// Allow the proxy to update (this also filters unfiltered notifies)
		if (AnimInst->NeedsUpdate())
		{
			AnimInst->ParallelUpdateAnimation();
		}

		// Explicitly call post update (also triggers notifies)
		AnimInst->PostUpdateAnimation();
	}

	// Update space bases so new animation position has an effect.
	SkeletalMeshComponent->RefreshBoneTransforms();
	SkeletalMeshComponent->RefreshSlaveComponents();
	SkeletalMeshComponent->UpdateComponentToWorld();
	SkeletalMeshComponent->FinalizeBoneTransform();
	SkeletalMeshComponent->MarkRenderTransformDirty();
	SkeletalMeshComponent->MarkRenderDynamicDataDirty();

	return PlayingMontage;
}

bool FAnimMontageInstance::CanUseMarkerSync() const
{
	// for now we only allow non-full weight and when blending out
	return SyncGroupIndex != INDEX_NONE && IsStopped() && Blend.IsComplete() == false;
}

void UAnimMontage::BakeTimeStretchCurve()
{
	TimeStretchCurve.Reset();

	// See if Montage is hosting a curve named 'TimeStretchCurveName'
	FFloatCurve* TimeStretchFloatCurve = nullptr;
	if (const USkeleton* MySkeleton = GetSkeleton())
	{
		if (const FSmartNameMapping* CurveNameMapping = MySkeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName))
		{
			if (const USkeleton::AnimCurveUID CurveUID = CurveNameMapping->FindUID(TimeStretchCurveName))
			{
				TimeStretchFloatCurve = (FFloatCurve*)(GetCurveData().GetCurveData(CurveUID));
			}
		}
	}

	if (TimeStretchFloatCurve == nullptr)
	{
		return;
	}
	
	TimeStretchCurve.BakeFromFloatCurve(*TimeStretchFloatCurve, SequenceLength);
}
