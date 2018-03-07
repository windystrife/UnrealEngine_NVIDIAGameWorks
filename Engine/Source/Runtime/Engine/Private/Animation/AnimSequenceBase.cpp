// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimSequenceBase.h"
#include "AnimationUtils.h"
#include "AnimationRuntime.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimInstance.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "UObject/FrameworkObjectVersion.h"

DEFINE_LOG_CATEGORY(LogAnimMarkerSync);

#define LOCTEXT_NAMESPACE "AnimSequenceBase"
/////////////////////////////////////////////////////

UAnimSequenceBase::UAnimSequenceBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, RateScale(1.0f)
{
}

void UAnimSequenceBase::PostLoad()
{
	Super::PostLoad();

	// Convert Notifies to new data
	if( GIsEditor && Notifies.Num() > 0 )
	{
		if(GetLinkerUE4Version() < VER_UE4_CLEAR_NOTIFY_TRIGGERS)
		{
			for(FAnimNotifyEvent Notify : Notifies)
			{
				if(Notify.Notify)
				{
					// Clear end triggers for notifies that are not notify states
					Notify.EndTriggerTimeOffset = 0.0f;
				}
			}
		}
	}

#if WITH_EDITOR
	InitializeNotifyTrack();
#endif
	RefreshCacheData();

	if(USkeleton* MySkeleton = GetSkeleton())
	{
		VerifyCurveNames<FFloatCurve>(*MySkeleton, USkeleton::AnimCurveMappingName, RawCurveData.FloatCurves);

#if WITH_EDITOR
		VerifyCurveNames<FTransformCurve>(*MySkeleton, USkeleton::AnimTrackCurveMappingName, RawCurveData.TransformCurves);
#endif

		// this should continue to add if skeleton hasn't been saved either 
		// we don't wipe out data, so make sure you add back in if required
		if (GetLinkerCustomVersion(FFrameworkObjectVersion::GUID) < FFrameworkObjectVersion::MoveCurveTypesToSkeleton
			|| MySkeleton->GetLinkerCustomVersion(FFrameworkObjectVersion::GUID) < FFrameworkObjectVersion::MoveCurveTypesToSkeleton)
		{
			// fix up curve flags to skeleton
			for (const FFloatCurve& Curve : RawCurveData.FloatCurves)
			{
				bool bMorphtargetSet = Curve.GetCurveTypeFlag(AACF_DriveMorphTarget_DEPRECATED);
				bool bMaterialSet = Curve.GetCurveTypeFlag(AACF_DriveMaterial_DEPRECATED);

				// only add this if that has to 
				if (bMorphtargetSet || bMaterialSet)
				{
					MySkeleton->AccumulateCurveMetaData(Curve.Name.DisplayName, bMaterialSet, bMorphtargetSet);
				}
			}
		}
	}
}

float UAnimSequenceBase::GetPlayLength()
{
	return SequenceLength;
}

void UAnimSequenceBase::SortNotifies()
{
	// Sorts using FAnimNotifyEvent::operator<()
	Notifies.Sort();
}

bool UAnimSequenceBase::RemoveNotifies(const TArray<FName>& NotifiesToRemove)
{
	bool bSequenceModified = false;
	for (int32 NotifyIndex = Notifies.Num() - 1; NotifyIndex >= 0; --NotifyIndex)
	{
		FAnimNotifyEvent& AnimNotify = Notifies[NotifyIndex];
		if (NotifiesToRemove.Contains(AnimNotify.NotifyName))
		{
			if (!bSequenceModified)
			{
				Modify();
				bSequenceModified = true;
			}
			Notifies.RemoveAtSwap(NotifyIndex);
		}
	}

	if (bSequenceModified)
	{
		MarkPackageDirty();
		RefreshCacheData();
	}
	return bSequenceModified;
}

bool UAnimSequenceBase::IsNotifyAvailable() const
{
	return (Notifies.Num() != 0) && (SequenceLength > 0.f);
}

#if WITH_EDITOR
void UAnimSequenceBase::RegisterOnAnimCurvesChanged(const FOnAnimCurvesChanged& Delegate)
{
	OnAnimCurvesChanged.Add(Delegate);	
}

void UAnimSequenceBase::UnregisterOnAnimCurvesChanged(void* Unregister)
{
	OnAnimCurvesChanged.RemoveAll(Unregister);
}

void UAnimSequenceBase::RegisterOnAnimTrackCurvesChanged(const FOnAnimTrackCurvesChanged& Delegate)
{
	OnAnimTrackCurvesChanged.Add(Delegate);
}

void UAnimSequenceBase::UnregisterOnAnimTrackCurvesChanged(void* Unregister)
{
	OnAnimTrackCurvesChanged.RemoveAll(Unregister);
}
#endif 

/** 
 * Retrieves AnimNotifies given a StartTime and a DeltaTime.
 * Time will be advanced and support looping if bAllowLooping is true.
 * Supports playing backwards (DeltaTime<0).
 * Returns notifies between StartTime (exclusive) and StartTime+DeltaTime (inclusive)
 */
void UAnimSequenceBase::GetAnimNotifies(const float& StartTime, const float& DeltaTime, const bool bAllowLooping, TArray<const FAnimNotifyEvent *> & OutActiveNotifies) const
{
	if(DeltaTime == 0.f)
	{
		return;
	}

	// Early out if we have no notifies
	if (!IsNotifyAvailable())
	{
		return;
	}
	
	bool const bPlayingBackwards = (DeltaTime < 0.f);
	float PreviousPosition = StartTime;
	float CurrentPosition = StartTime;
	float DesiredDeltaMove = DeltaTime;

	do 
	{
		// Disable looping here. Advance to desired position, or beginning / end of animation 
		const ETypeAdvanceAnim AdvanceType = FAnimationRuntime::AdvanceTime(false, DesiredDeltaMove, CurrentPosition, SequenceLength);

		// Verify position assumptions
		ensureMsgf(bPlayingBackwards ? (CurrentPosition <= PreviousPosition) : (CurrentPosition >= PreviousPosition), TEXT("in Animation %s(Skeleton %s) : bPlayingBackwards(%d), PreviousPosition(%0.2f), Current Position(%0.2f)"), 
			*GetName(), *GetNameSafe(GetSkeleton()), bPlayingBackwards, PreviousPosition, CurrentPosition);
		
		GetAnimNotifiesFromDeltaPositions(PreviousPosition, CurrentPosition, OutActiveNotifies);
	
		// If we've hit the end of the animation, and we're allowed to loop, keep going.
		if( (AdvanceType == ETAA_Finished) &&  bAllowLooping )
		{
			const float ActualDeltaMove = (CurrentPosition - PreviousPosition);
			DesiredDeltaMove -= ActualDeltaMove; 

			PreviousPosition = bPlayingBackwards ? SequenceLength : 0.f;
			CurrentPosition = PreviousPosition;
		}
		else
		{
			break;
		}
	} 
	while( true );
}

/** 
 * Retrieves AnimNotifies between two time positions. ]PreviousPosition, CurrentPosition]
 * Between PreviousPosition (exclusive) and CurrentPosition (inclusive).
 * Supports playing backwards (CurrentPosition<PreviousPosition).
 * Only supports contiguous range, does NOT support looping and wrapping over.
 */
void UAnimSequenceBase::GetAnimNotifiesFromDeltaPositions(const float& PreviousPosition, const float& CurrentPosition, TArray<const FAnimNotifyEvent *> & OutActiveNotifies) const
{
	// Early out if we have no notifies
	if( (Notifies.Num() == 0) || (PreviousPosition == CurrentPosition) )
	{
		return;
	}

	bool const bPlayingBackwards = (CurrentPosition < PreviousPosition);

	// If playing backwards, flip Min and Max.
	if( bPlayingBackwards )
	{
		for (int32 NotifyIndex=0; NotifyIndex<Notifies.Num(); NotifyIndex++)
		{
			const FAnimNotifyEvent& AnimNotifyEvent = Notifies[NotifyIndex];
			const float NotifyStartTime = AnimNotifyEvent.GetTriggerTime();
			const float NotifyEndTime = AnimNotifyEvent.GetEndTriggerTime();

			if( (NotifyStartTime < PreviousPosition) && (NotifyEndTime >= CurrentPosition) )
			{
				OutActiveNotifies.Add(&AnimNotifyEvent);
			}
		}
	}
	else
	{
		for (int32 NotifyIndex=0; NotifyIndex<Notifies.Num(); NotifyIndex++)
		{
			const FAnimNotifyEvent& AnimNotifyEvent = Notifies[NotifyIndex];
			const float NotifyStartTime = AnimNotifyEvent.GetTriggerTime();
			const float NotifyEndTime = AnimNotifyEvent.GetEndTriggerTime();

			if( (NotifyStartTime <= CurrentPosition) && (NotifyEndTime > PreviousPosition) )
			{
				OutActiveNotifies.Add(&AnimNotifyEvent);
			}
		}
	}
}

void UAnimSequenceBase::TickAssetPlayer(FAnimTickRecord& Instance, struct FAnimNotifyQueue& NotifyQueue, FAnimAssetTickContext& Context) const
{
	float& CurrentTime = *(Instance.TimeAccumulator);
	float PreviousTime = CurrentTime;
	const float PlayRate = Instance.PlayRateMultiplier * this->RateScale;

	float MoveDelta = 0.f;

	if( Context.IsLeader() )
	{
		const float DeltaTime = Context.GetDeltaTime();
		MoveDelta = PlayRate * DeltaTime;

		Context.SetLeaderDelta(MoveDelta);
		Context.SetPreviousAnimationPositionRatio(PreviousTime / SequenceLength);

		if (MoveDelta != 0.f)
		{
			if (Instance.bCanUseMarkerSync && Context.CanUseMarkerPosition())
			{
				TickByMarkerAsLeader(*Instance.MarkerTickRecord, Context.MarkerTickContext, CurrentTime, PreviousTime, MoveDelta, Instance.bLooping);
			}
			else
			{
				// Advance time
				FAnimationRuntime::AdvanceTime(Instance.bLooping, MoveDelta, CurrentTime, SequenceLength);
				UE_LOG(LogAnimMarkerSync, Log, TEXT("Leader (%s) (normal advance)  - PreviousTime (%0.2f), CurrentTime (%0.2f), MoveDelta (%0.2f), Looping (%d) "), *GetName(), PreviousTime, CurrentTime, MoveDelta, Instance.bLooping ? 1 : 0);
			}
		}

		Context.SetAnimationPositionRatio(CurrentTime / SequenceLength);
	}
	else
	{
		// Follow the leader
		if (Instance.bCanUseMarkerSync)
		{
			if (Context.CanUseMarkerPosition() && Context.MarkerTickContext.IsMarkerSyncStartValid())
			{
				TickByMarkerAsFollower(*Instance.MarkerTickRecord, Context.MarkerTickContext, CurrentTime, PreviousTime, Context.GetLeaderDelta(), Instance.bLooping);
			}
			else
			{
				// If leader is not valid, advance time as normal, do not jump position and pop.
				FAnimationRuntime::AdvanceTime(Instance.bLooping, MoveDelta, CurrentTime, SequenceLength);
				UE_LOG(LogAnimMarkerSync, Log, TEXT("Follower (%s) (normal advance)  - PreviousTime (%0.2f), CurrentTime (%0.2f), MoveDelta (%0.2f), Looping (%d) "), *GetName(), PreviousTime, CurrentTime, MoveDelta, Instance.bLooping ? 1 : 0);
			}
		}
		else
		{
			PreviousTime = Context.GetPreviousAnimationPositionRatio() * SequenceLength;
			CurrentTime = Context.GetAnimationPositionRatio() * SequenceLength;
			UE_LOG(LogAnimMarkerSync, Log, TEXT("Follower (%s) (normalized position advance) - PreviousTime (%0.2f), CurrentTime (%0.2f), MoveDelta (%0.2f), Looping (%d) "), *GetName(), PreviousTime, CurrentTime, MoveDelta, Instance.bLooping ? 1 : 0);
		}

		//@TODO: NOTIFIES: Calculate AdvanceType based on what the new delta time is

		if( CurrentTime != PreviousTime )
		{
			// Figure out delta time 
			MoveDelta = CurrentTime - PreviousTime;
			// if we went against play rate, then loop around.
			if( (MoveDelta * PlayRate) < 0.f )
			{
				MoveDelta += FMath::Sign<float>(PlayRate) * SequenceLength;
			}
		}
	}

	HandleAssetPlayerTickedInternal(Context, PreviousTime, MoveDelta, Instance, NotifyQueue);
}

void UAnimSequenceBase::TickByMarkerAsFollower(FMarkerTickRecord &Instance, FMarkerTickContext &MarkerContext, float& CurrentTime, float& OutPreviousTime, const float MoveDelta, const bool bLooping) const
{
	if (!Instance.IsValid())
	{
		GetMarkerIndicesForPosition(MarkerContext.GetMarkerSyncStartPosition(), bLooping, Instance.PreviousMarker, Instance.NextMarker, CurrentTime);
	}

	OutPreviousTime = CurrentTime;

	AdvanceMarkerPhaseAsFollower(MarkerContext, MoveDelta, bLooping, CurrentTime, Instance.PreviousMarker, Instance.NextMarker);
	UE_LOG(LogAnimMarkerSync, Log, TEXT("Follower (%s) - PreviousTime (%0.2f), CurrentTime (%0.2f), MoveDelta (%0.2f), Looping(%d) "), *GetName(), OutPreviousTime, CurrentTime, MoveDelta, bLooping ? 1 : 0);
}

void UAnimSequenceBase::TickByMarkerAsLeader(FMarkerTickRecord& Instance, FMarkerTickContext& MarkerContext, float& CurrentTime, float& OutPreviousTime, const float MoveDelta, const bool bLooping) const
{
	if (!Instance.IsValid())
	{
		GetMarkerIndicesForTime(CurrentTime, bLooping, MarkerContext.GetValidMarkerNames(), Instance.PreviousMarker, Instance.NextMarker);
	}

	MarkerContext.SetMarkerSyncStartPosition(GetMarkerSyncPositionfromMarkerIndicies(Instance.PreviousMarker.MarkerIndex, Instance.NextMarker.MarkerIndex, CurrentTime));

	OutPreviousTime = CurrentTime;

	AdvanceMarkerPhaseAsLeader(bLooping, MoveDelta, MarkerContext.GetValidMarkerNames(), CurrentTime, Instance.PreviousMarker, Instance.NextMarker, MarkerContext.MarkersPassedThisTick);

	MarkerContext.SetMarkerSyncEndPosition(GetMarkerSyncPositionfromMarkerIndicies(Instance.PreviousMarker.MarkerIndex, Instance.NextMarker.MarkerIndex, CurrentTime));

	UE_LOG(LogAnimMarkerSync, Log, TEXT("Leader (%s) - PreviousTime (%0.2f), CurrentTime (%0.2f), MoveDelta (%0.2f), Looping(%d) "), *GetName(), OutPreviousTime, CurrentTime, MoveDelta, bLooping ? 1 : 0);
}

bool CanNotifyUseTrack(const FAnimNotifyTrack& Track, const FAnimNotifyEvent& Notify)
{
	for (const FAnimNotifyEvent* Event : Track.Notifies)
	{
		if (FMath::IsNearlyEqual(Event->GetTime(), Notify.GetTime()))
		{
			return false;
		}
	}
	return true;
}

FAnimNotifyTrack& AddNewTrack(TArray<FAnimNotifyTrack>& Tracks)
{
	const int32 Index = Tracks.Add(FAnimNotifyTrack(*FString::FromInt(Tracks.Num() + 1), FLinearColor::White));
	return Tracks[Index];
}

void UAnimSequenceBase::RefreshCacheData()
{
	SortNotifies();

#if WITH_EDITOR
	for (int32 TrackIndex = 0; TrackIndex < AnimNotifyTracks.Num(); ++TrackIndex)
	{
		AnimNotifyTracks[TrackIndex].Notifies.Empty();
	}

	for (FAnimNotifyEvent& Notify : Notifies)
	{
		// Handle busted track indices
		if (!AnimNotifyTracks.IsValidIndex(Notify.TrackIndex))
		{
			// This really shouldn't happen, but try to handle it
			ensureMsgf(0, TEXT("AnimNotifyTrack: Anim (%s) has notify (%s) with track index (%i) that does not exist"), *GetFullName(), *Notify.NotifyName.ToString(), Notify.TrackIndex);

			// Don't create lots of extra tracks if we are way off supporting this track
			if (Notify.TrackIndex < 0 || Notify.TrackIndex > 20)
			{
				Notify.TrackIndex = 0;
			}
			else
			{
				while (!AnimNotifyTracks.IsValidIndex(Notify.TrackIndex))
				{
					AddNewTrack(AnimNotifyTracks);
				}
			}
		}

		// Handle overlapping notifies
		FAnimNotifyTrack* TrackToUse = nullptr;
		for (int32 TrackOffset = 0; TrackOffset < AnimNotifyTracks.Num(); ++TrackOffset)
		{
			const int32 TrackIndex = (Notify.TrackIndex + TrackOffset) % AnimNotifyTracks.Num();
			if (CanNotifyUseTrack(AnimNotifyTracks[TrackIndex], Notify))
			{
				TrackToUse = &AnimNotifyTracks[TrackIndex];
				break;
			}
		}

		if (TrackToUse == nullptr)
		{
			TrackToUse = &AddNewTrack(AnimNotifyTracks);
		}

		check(TrackToUse);

		TrackToUse->Notifies.Add(&Notify);
	}

	// this is a separate loop of checkin if they contains valid notifies
	for (int32 NotifyIndex = 0; NotifyIndex < Notifies.Num(); ++NotifyIndex)
	{
		const FAnimNotifyEvent& Notify = Notifies[NotifyIndex];
		// make sure if they can be placed in editor
		if (Notify.Notify)
		{
			if (Notify.Notify->CanBePlaced(this) == false)
			{
				static FName NAME_LoadErrors("LoadErrors");
				FMessageLog LoadErrors(NAME_LoadErrors);

				TSharedRef<FTokenizedMessage> Message = LoadErrors.Error();
				Message->AddToken(FTextToken::Create(LOCTEXT("InvalidAnimNotify1", "The Animation ")));
				Message->AddToken(FAssetNameToken::Create(GetPathName(), FText::FromString(GetNameSafe(this))));
				Message->AddToken(FTextToken::Create(LOCTEXT("InvalidAnimNotify2", " contains invalid notify ")));
				Message->AddToken(FAssetNameToken::Create(Notify.Notify->GetPathName(), FText::FromString(GetNameSafe(Notify.Notify))));
				LoadErrors.Open();
			}
		}

		if (Notify.NotifyStateClass)
		{
			if (Notify.NotifyStateClass->CanBePlaced(this) == false)
			{
				static FName NAME_LoadErrors("LoadErrors");
				FMessageLog LoadErrors(NAME_LoadErrors);

				TSharedRef<FTokenizedMessage> Message = LoadErrors.Error();
				Message->AddToken(FTextToken::Create(LOCTEXT("InvalidAnimNotify1", "The Animation ")));
				Message->AddToken(FAssetNameToken::Create(GetPathName(), FText::FromString(GetNameSafe(this))));
				Message->AddToken(FTextToken::Create(LOCTEXT("InvalidAnimNotify2", " contains invalid notify ")));
				Message->AddToken(FAssetNameToken::Create(Notify.NotifyStateClass->GetPathName(), FText::FromString(GetNameSafe(Notify.NotifyStateClass))));
				LoadErrors.Open();
			}
		}
	}
	// notification broadcast
	OnNotifyChanged.Broadcast();
#endif //WITH_EDITOR
}

#if WITH_EDITOR
void UAnimSequenceBase::RefreshCurveData()
{
	OnAnimCurvesChanged.Broadcast();
	OnAnimTrackCurvesChanged.Broadcast();
}

void UAnimSequenceBase::InitializeNotifyTrack()
{
	if ( AnimNotifyTracks.Num() == 0 ) 
	{
		AnimNotifyTracks.Add(FAnimNotifyTrack(TEXT("1"), FLinearColor::White ));
	}
}

int32 UAnimSequenceBase::GetNumberOfFrames() const
{
	static float DefaultSampleRateInterval = 1.f/DEFAULT_SAMPLERATE;
	// because of float error, add small margin at the end, so it can clamp correctly
	return (SequenceLength/DefaultSampleRateInterval + KINDA_SMALL_NUMBER);
}

int32 UAnimSequenceBase::GetFrameAtTime(const float Time) const
{
	const float Frac = Time / SequenceLength;
	return FMath::FloorToInt(Frac * GetNumberOfFrames());
}

float UAnimSequenceBase::GetTimeAtFrame(const int32 Frame) const
{
	const float FrameTime = SequenceLength / GetNumberOfFrames();
	return FrameTime * Frame;
}

void UAnimSequenceBase::RegisterOnNotifyChanged(const FOnNotifyChanged& Delegate)
{
	OnNotifyChanged.Add(Delegate);
}
void UAnimSequenceBase::UnregisterOnNotifyChanged(void* Unregister)
{
	OnNotifyChanged.RemoveAll(Unregister);
}

void UAnimSequenceBase::ClampNotifiesAtEndOfSequence()
{
	const float NotifyClampTime = SequenceLength - 0.01f; //Slight offset so that notify is still draggable
	for(int i = 0; i < Notifies.Num(); ++ i)
	{
		if(Notifies[i].GetTime() >= SequenceLength)
		{
			Notifies[i].SetTime(NotifyClampTime);
			Notifies[i].TriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::OffsetBefore);
		}
	}
}

EAnimEventTriggerOffsets::Type UAnimSequenceBase::CalculateOffsetForNotify(float NotifyDisplayTime) const
{
	if(NotifyDisplayTime == 0.f)
	{
		return EAnimEventTriggerOffsets::OffsetAfter;
	}
	else if(NotifyDisplayTime == SequenceLength)
	{
		return EAnimEventTriggerOffsets::OffsetBefore;
	}
	return EAnimEventTriggerOffsets::NoOffset;
}

void UAnimSequenceBase::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);
	if(Notifies.Num() > 0)
	{
		FString NotifyList;

		// add notifies to 
		for(auto Iter=Notifies.CreateConstIterator(); Iter; ++Iter)
		{
			// only add if not BP anim notify since they're handled separate
			if(Iter->IsBlueprintNotify() == false)
			{
				NotifyList += FString::Printf(TEXT("%s%s"), *Iter->NotifyName.ToString(), *USkeleton::AnimNotifyTagDelimiter);
			}
		}
		
		if(NotifyList.Len() > 0)
		{
			OutTags.Add(FAssetRegistryTag(USkeleton::AnimNotifyTag, NotifyList, FAssetRegistryTag::TT_Hidden));
		}
	}

	// Add curve IDs to a tag list, or a blank tag if we have no curves.
	// The blank list is necessary when we attempt to delete a curve so
	// an old asset can be detected from its asset data so we load as few
	// as possible.
	FString CurveNameList;

	for(const FFloatCurve& Curve : RawCurveData.FloatCurves)
	{
		CurveNameList += FString::Printf(TEXT("%s%s"), *Curve.Name.DisplayName.ToString(), *USkeleton::CurveTagDelimiter);
	}
	OutTags.Add(FAssetRegistryTag(USkeleton::CurveNameTag, CurveNameList, FAssetRegistryTag::TT_Hidden));
}

uint8* UAnimSequenceBase::FindNotifyPropertyData(int32 NotifyIndex, UArrayProperty*& ArrayProperty)
{
	// initialize to NULL
	ArrayProperty = NULL;

	if(Notifies.IsValidIndex(NotifyIndex))
	{
		return FindArrayProperty(TEXT("Notifies"), ArrayProperty, NotifyIndex);
	}
	return NULL;
}

uint8* UAnimSequenceBase::FindArrayProperty(const TCHAR* PropName, UArrayProperty*& ArrayProperty, int32 ArrayIndex)
{
	// find Notifies property start point
	UProperty* Property = FindField<UProperty>(GetClass(), PropName);

	// found it and if it is array
	if (Property && Property->IsA(UArrayProperty::StaticClass()))
	{
		// find Property Value from UObject we got
		uint8* PropertyValue = Property->ContainerPtrToValuePtr<uint8>(this);

		// it is array, so now get ArrayHelper and find the raw ptr of the data
		ArrayProperty = CastChecked<UArrayProperty>(Property);
		FScriptArrayHelper ArrayHelper(ArrayProperty, PropertyValue);

		if (ArrayProperty->Inner && ArrayIndex < ArrayHelper.Num())
		{
			//Get property data based on selected index
			return ArrayHelper.GetRawPtr(ArrayIndex);
		}
	}
	return NULL;
}

void UAnimSequenceBase::RefreshParentAssetData()
{
	Super::RefreshParentAssetData();

	check(ParentAsset);

	UAnimSequenceBase* ParentSeqBase = CastChecked<UAnimSequenceBase>(ParentAsset);

	// should do deep copy because notify contains outer
	Notifies = ParentSeqBase->Notifies;

	// update notify
	for (int32 NotifyIdx = 0; NotifyIdx < Notifies.Num(); ++NotifyIdx)
	{
		FAnimNotifyEvent& NotifyEvent = Notifies[NotifyIdx];
		if (NotifyEvent.Notify)
		{
			class UAnimNotify* NewNotifyClass = DuplicateObject(NotifyEvent.Notify, this);
			NotifyEvent.Notify = NewNotifyClass;
		}
		if (NotifyEvent.NotifyStateClass)
		{
			class UAnimNotifyState* NewNotifyStateClass = DuplicateObject(NotifyEvent.NotifyStateClass, this);
			NotifyEvent.NotifyStateClass = (NewNotifyStateClass);
		}

		NotifyEvent.Link(this, NotifyEvent.GetTime(), NotifyEvent.GetSlotIndex());
		NotifyEvent.EndLink.Link(this, NotifyEvent.GetTime() + NotifyEvent.Duration, NotifyEvent.GetSlotIndex());
	}

	SequenceLength = ParentSeqBase->SequenceLength;
	RateScale = ParentSeqBase->RateScale;
	RawCurveData = ParentSeqBase->RawCurveData;

#if WITH_EDITORONLY_DATA
	// if you change Notifies array, this will need to be rebuilt
	AnimNotifyTracks = ParentSeqBase->AnimNotifyTracks;

	// fix up notify links, brute force and ugly code
	for (FAnimNotifyTrack& Track : AnimNotifyTracks)
	{
		for (FAnimNotifyEvent*& Notify : Track.Notifies)
		{
			for (int32 ParentNotifyIdx = 0; ParentNotifyIdx < ParentSeqBase->Notifies.Num(); ++ParentNotifyIdx)
			{
				if (Notify == &ParentSeqBase->Notifies[ParentNotifyIdx])
				{
					Notify = &Notifies[ParentNotifyIdx];
					break;
				}
			}
		}
	}
#endif // WITH_EDITORONLY_DATA

}
#endif	//WITH_EDITOR


/** Add curve data to Instance at the time of CurrentTime **/
void UAnimSequenceBase::EvaluateCurveData(FBlendedCurve& OutCurve, float CurrentTime, bool bForceUseRawData) const
{
	RawCurveData.EvaluateCurveData(OutCurve, CurrentTime);
}

void UAnimSequenceBase::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);

	Super::Serialize(Ar);

	// fix up version issue and so on
	RawCurveData.PostSerialize(Ar);
}

void UAnimSequenceBase::HandleAssetPlayerTickedInternal(FAnimAssetTickContext &Context, const float PreviousTime, const float MoveDelta, const FAnimTickRecord &Instance, struct FAnimNotifyQueue& NotifyQueue) const
{
	if (Context.ShouldGenerateNotifies())
	{
		// Harvest and record notifies
		TArray<const FAnimNotifyEvent*> AnimNotifies;
		GetAnimNotifies(PreviousTime, MoveDelta, Instance.bLooping, AnimNotifies);
		NotifyQueue.AddAnimNotifies(AnimNotifies, Instance.EffectiveBlendWeight);
	}
}
#undef LOCTEXT_NAMESPACE 
