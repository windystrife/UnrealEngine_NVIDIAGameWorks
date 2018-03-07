// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimTypes.h"
#include "Animation/AnimationAsset.h"
#include "AnimationUtils.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"

#define NOTIFY_TRIGGER_OFFSET KINDA_SMALL_NUMBER;

float GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::Type OffsetType)
{
	switch (OffsetType)
	{
	case EAnimEventTriggerOffsets::OffsetBefore:
		{
			return -NOTIFY_TRIGGER_OFFSET;
			break;
		}
	case EAnimEventTriggerOffsets::OffsetAfter:
		{
			return NOTIFY_TRIGGER_OFFSET;
			break;
		}
	case EAnimEventTriggerOffsets::NoOffset:
		{
			return 0.f;
			break;
		}
	default:
		{
			check(false); // Unknown value supplied for OffsetType
			break;
		}
	}
	return 0.f;
}

/////////////////////////////////////////////////////
// FAnimNotifyEvent

void FAnimNotifyEvent::RefreshTriggerOffset(EAnimEventTriggerOffsets::Type PredictedOffsetType)
{
	if(PredictedOffsetType == EAnimEventTriggerOffsets::NoOffset || TriggerTimeOffset == 0.f)
	{
		TriggerTimeOffset = GetTriggerTimeOffsetForType(PredictedOffsetType);
	}
}

void FAnimNotifyEvent::RefreshEndTriggerOffset( EAnimEventTriggerOffsets::Type PredictedOffsetType )
{
	if(PredictedOffsetType == EAnimEventTriggerOffsets::NoOffset || EndTriggerTimeOffset == 0.f)
	{
		EndTriggerTimeOffset = GetTriggerTimeOffsetForType(PredictedOffsetType);
	}
}

float FAnimNotifyEvent::GetTriggerTime() const
{
	return GetTime() + TriggerTimeOffset;
}

float FAnimNotifyEvent::GetEndTriggerTime() const
{
	if (!NotifyStateClass && (EndTriggerTimeOffset != 0.f))
	{
		UE_LOG(LogAnimNotify, Log, TEXT("Anim Notify %s is non state, but has an EndTriggerTimeOffset %f!"), *NotifyName.ToString(), EndTriggerTimeOffset);
	}

	return NotifyStateClass ? (GetTriggerTime() + GetDuration() + EndTriggerTimeOffset) : GetTriggerTime();
}

float FAnimNotifyEvent::GetDuration() const
{
	return NotifyStateClass ? EndLink.GetTime() - GetTime() : 0.0f;
}

void FAnimNotifyEvent::SetDuration(float NewDuration)
{
	Duration = NewDuration;
	EndLink.SetTime(GetTime() + Duration);
}

bool FAnimNotifyEvent::IsBranchingPoint() const
{
	return GetLinkedMontage() && ((MontageTickType == EMontageNotifyTickType::BranchingPoint) || (Notify && Notify->bIsNativeBranchingPoint) || (NotifyStateClass && NotifyStateClass->bIsNativeBranchingPoint));
}


void FAnimNotifyEvent::SetTime(float NewTime, EAnimLinkMethod::Type ReferenceFrame /*= EAnimLinkMethod::Absolute*/)
{
	FAnimLinkableElement::SetTime(NewTime, ReferenceFrame);
	SetDuration(Duration);
}

////////////////////////////
//
// FMarkerSyncData
// 
////////////////////////////

void FMarkerSyncData::GetMarkerIndicesForTime(float CurrentTime, bool bLooping, const TArray<FName>& ValidMarkerNames, FMarkerPair& OutPrevMarker, FMarkerPair& OutNextMarker, float SequenceLength) const
{
	const int LoopModStart = bLooping ? -1 : 0;
	const int LoopModEnd = bLooping ? 2 : 1;

	OutPrevMarker.MarkerIndex = -1;
	OutPrevMarker.TimeToMarker = -CurrentTime;
	OutNextMarker.MarkerIndex = -1;
	OutNextMarker.TimeToMarker = SequenceLength - CurrentTime;

	for (int32 LoopMod = LoopModStart; LoopMod < LoopModEnd; ++LoopMod)
	{
		const float LoopModTime = LoopMod * SequenceLength;
		for (int Idx = 0; Idx < AuthoredSyncMarkers.Num(); ++Idx)
		{
			const FAnimSyncMarker& Marker = AuthoredSyncMarkers[Idx];
			if (ValidMarkerNames.Contains(Marker.MarkerName))
			{
				const float MarkerTime = Marker.Time + LoopModTime;
				if (MarkerTime < CurrentTime)
				{
					OutPrevMarker.MarkerIndex = Idx;
					OutPrevMarker.TimeToMarker = MarkerTime - CurrentTime;
				}
				else if (MarkerTime >= CurrentTime)
				{
					OutNextMarker.MarkerIndex = Idx;
					OutNextMarker.TimeToMarker = MarkerTime - CurrentTime;
					break; // Done
				}
			}
		}
		if (OutNextMarker.MarkerIndex != -1)
		{
			break; // Done
		}
	}
}

FMarkerSyncAnimPosition FMarkerSyncData::GetMarkerSyncPositionfromMarkerIndicies(int32 PrevMarker, int32 NextMarker, float CurrentTime, float SequenceLength) const
{
	FMarkerSyncAnimPosition SyncPosition;
	float PrevTime, NextTime;

	if (PrevMarker != -1)
	{
		PrevTime = AuthoredSyncMarkers[PrevMarker].Time;
		SyncPosition.PreviousMarkerName = AuthoredSyncMarkers[PrevMarker].MarkerName;
	}
	else
	{
		PrevTime = 0.f;
	}

	if (NextMarker != -1)
	{
		NextTime = AuthoredSyncMarkers[NextMarker].Time;
		SyncPosition.NextMarkerName = AuthoredSyncMarkers[NextMarker].MarkerName;
	}
	else
	{
		NextTime = SequenceLength;
	}

	// Account for looping
	PrevTime = (PrevTime > CurrentTime) ? PrevTime - SequenceLength : PrevTime;
	NextTime = (NextTime < CurrentTime) ? NextTime + SequenceLength : NextTime;

	if (PrevTime == NextTime)
	{
		PrevTime -= SequenceLength;
	}

	check(NextTime > PrevTime);

	SyncPosition.PositionBetweenMarkers = (CurrentTime - PrevTime) / (NextTime - PrevTime);
	return SyncPosition;
}

void FMarkerSyncData::CollectUniqueNames()
{
	if (AuthoredSyncMarkers.Num() > 0)
	{
		AuthoredSyncMarkers.Sort();
		UniqueMarkerNames.Reset();
		UniqueMarkerNames.Reserve(AuthoredSyncMarkers.Num());

		const FAnimSyncMarker* PreviousMarker = nullptr;
		for (const FAnimSyncMarker& Marker : AuthoredSyncMarkers)
		{
			UniqueMarkerNames.AddUnique(Marker.MarkerName);
			PreviousMarker = &Marker;
		}
	}
	else
	{
		UniqueMarkerNames.Empty();
	}
}

void FMarkerSyncData::CollectMarkersInRange(float PrevPosition, float NewPosition, 	TArray<FPassedMarker>& OutMarkersPassedThisTick, float TotalDeltaMove)
{
	for (const auto& Marker : AuthoredSyncMarkers)
	{
		if (Marker.Time >= PrevPosition && Marker.Time < NewPosition)
		{
			float TimeToMarker = Marker.Time - PrevPosition;
			int32 PassedMarker = OutMarkersPassedThisTick.Add(FPassedMarker());
			OutMarkersPassedThisTick[PassedMarker].PassedMarkerName = Marker.MarkerName;
			OutMarkersPassedThisTick[PassedMarker].DeltaTimeWhenPassed = TotalDeltaMove - TimeToMarker;
		}
	}
}
