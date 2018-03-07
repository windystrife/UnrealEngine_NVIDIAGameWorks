// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimationBlueprintLibrary.h" 

#include "Animation/AnimSequence.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimMetaData.h"
#include "Animation/AnimNotifies/AnimNotifyState.h" 
#include "Animation/Skeleton.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "BonePose.h" 

#include "AnimationRuntime.h"

DEFINE_LOG_CATEGORY_STATIC(LogAnimationBlueprintLibrary, Verbose, All);

const FName UAnimationBlueprintLibrary::SmartContainerNames[(int32)ESmartNameContainerType::SNCT_MAX] = { USkeleton::AnimCurveMappingName, USkeleton::AnimTrackCurveMappingName };

void UAnimationBlueprintLibrary::GetNumFrames(const UAnimSequence* AnimationSequence, int32& NumFrames)
{
	NumFrames = 0;
	if (AnimationSequence)
	{
		NumFrames = AnimationSequence->NumFrames;
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for GetNumFrames"));
	}
}

void UAnimationBlueprintLibrary::GetAnimationTrackNames(const UAnimSequence* AnimationSequence, TArray<FName>& TrackNames)
{
	TrackNames.Empty();
	if (AnimationSequence)
	{
		TrackNames.Append(AnimationSequence->AnimationTrackNames);
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for GetAnimationTrackNames"));
	}	
}

void UAnimationBlueprintLibrary::GetRawTrackPositionData(const UAnimSequence* AnimationSequence, const FName TrackName, TArray<FVector>& PositionData)
{
	PositionData.Empty();
	if (IsValidRawAnimationTrackName(AnimationSequence, TrackName))
	{
		const FRawAnimSequenceTrack& RawTrack =	GetRawAnimationTrackByName(AnimationSequence, TrackName);
		PositionData.Append(RawTrack.PosKeys);
	}
	else
	{
		const FString AnimSequenceName = (AnimationSequence != nullptr) ? AnimationSequence->GetName() : "Invalid Animation sequence";
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Raw Animation Track name %s does not exist in Animation Sequence %s"), *TrackName.ToString(), *AnimSequenceName );
	}	
}

void UAnimationBlueprintLibrary::GetRawTrackRotationData(const UAnimSequence* AnimationSequence, const FName TrackName, TArray<FQuat>& RotationData)
{
	RotationData.Empty();
	if (IsValidRawAnimationTrackName(AnimationSequence, TrackName))
	{
		const FRawAnimSequenceTrack& RawTrack = GetRawAnimationTrackByName(AnimationSequence, TrackName);
		RotationData.Append(RawTrack.RotKeys);
	}
	else
	{	
		const FString AnimSequenceName = (AnimationSequence != nullptr) ? AnimationSequence->GetName() : "Invalid Animation sequence";
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Raw Animation Track name %s does not exist in Animation Sequence %s"), *TrackName.ToString(), *AnimSequenceName);
	}
}

void UAnimationBlueprintLibrary::GetRawTrackScaleData(const UAnimSequence* AnimationSequence, const FName TrackName, TArray<FVector>& ScaleData)
{
	ScaleData.Empty();
	if (IsValidRawAnimationTrackName(AnimationSequence, TrackName))
	{
		const FRawAnimSequenceTrack& RawTrack = GetRawAnimationTrackByName(AnimationSequence, TrackName);
		ScaleData.Append(RawTrack.ScaleKeys);
	}
	else
	{
		const FString AnimSequenceName = (AnimationSequence != nullptr) ? AnimationSequence->GetName() : "Invalid Animation sequence";
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Raw Animation Track name %s does not exist in Animation Sequence %s"), *TrackName.ToString(), *AnimSequenceName);
	}
}

void UAnimationBlueprintLibrary::GetRawTrackData(const UAnimSequence* AnimationSequence, const FName TrackName, TArray<FVector>& PositionKeys, TArray<FQuat>& RotationKeys, TArray<FVector>& ScalingKeys)
{
	PositionKeys.Empty();
	RotationKeys.Empty();
	ScalingKeys.Empty();
	if (IsValidRawAnimationTrackName(AnimationSequence, TrackName))
	{		
		const FRawAnimSequenceTrack& RawTrack = GetRawAnimationTrackByName(AnimationSequence, TrackName);
		PositionKeys.Append(RawTrack.PosKeys);
		RotationKeys.Append(RawTrack.RotKeys);
		ScalingKeys.Append(RawTrack.ScaleKeys);
	}
	else
	{
		const FString AnimSequenceName = (AnimationSequence != nullptr) ? AnimationSequence->GetName() : "Invalid Animation sequence";
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Raw Animation Track name %s does not exist in Animation Sequence %s"), *TrackName.ToString(), *AnimSequenceName);
	}
}

bool UAnimationBlueprintLibrary::IsValidRawAnimationTrackName(const UAnimSequence* AnimationSequence, const FName TrackName)
{
	bool bValidName = false;

	if (AnimationSequence)
	{
		const int32 TrackIndex = AnimationSequence->AnimationTrackNames.IndexOfByKey(TrackName);
		bValidName = (TrackIndex != INDEX_NONE) && AnimationSequence->RawAnimationData.IsValidIndex(TrackIndex);
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for IsValidRawAnimationTrackName"));
	}

	return bValidName;
}

const FRawAnimSequenceTrack&  UAnimationBlueprintLibrary::GetRawAnimationTrackByName(const UAnimSequence* AnimationSequence, const FName TrackName)
{
	checkf(AnimationSequence, TEXT("Invalid Animation Sequence supplied for GetRawAnimationTrackByName"));

	const int32 TrackIndex = AnimationSequence->AnimationTrackNames.IndexOfByKey(TrackName);
	checkf(TrackIndex != INDEX_NONE, TEXT("Raw Animation Track %s does not exist in Animation Sequence %s"), *TrackName.ToString(), *AnimationSequence->GetName()); 
	return AnimationSequence->GetRawAnimationTrack(TrackIndex);	
}

void UAnimationBlueprintLibrary::GetCompressionScheme(const UAnimSequence* AnimationSequence, UAnimCompress*& CompressionScheme)
{
	if (AnimationSequence)
	{
		CompressionScheme = AnimationSequence->CompressionScheme;
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for GetCompressionScheme"));
	}
}

void UAnimationBlueprintLibrary::SetCompressionScheme(UAnimSequence* AnimationSequence, UAnimCompress* CompressionScheme)
{
	if (AnimationSequence)
	{
		AnimationSequence->CompressionScheme = CompressionScheme;
	}
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for SetCompressionScheme"));
	}
}

void UAnimationBlueprintLibrary::GetAdditiveAnimationType(const UAnimSequence* AnimationSequence, TEnumAsByte<enum EAdditiveAnimationType>& AdditiveAnimationType)
{
	if (AnimationSequence)
	{
		AdditiveAnimationType = AnimationSequence->AdditiveAnimType;
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for GetAdditiveAnimationType"));
	}
}

void UAnimationBlueprintLibrary::SetAdditiveAnimationType(UAnimSequence* AnimationSequence, const TEnumAsByte<enum EAdditiveAnimationType> AdditiveAnimationType)
{
	if (AnimationSequence)
	{
		AnimationSequence->AdditiveAnimType = AdditiveAnimationType;
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for SetAdditiveAnimationType"));
	}
}

void UAnimationBlueprintLibrary::GetAdditiveBasePoseType(const UAnimSequence* AnimationSequence, TEnumAsByte<enum EAdditiveBasePoseType>& AdditiveBasePoseType)
{
	if (AnimationSequence)
	{
		AdditiveBasePoseType = AnimationSequence->RefPoseType;
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for GetAdditiveBasePoseType"));
	}
}

void UAnimationBlueprintLibrary::SetAdditiveBasePoseType(UAnimSequence* AnimationSequence, const TEnumAsByte<enum EAdditiveBasePoseType> AdditiveBasePoseType)
{
	if (AnimationSequence)
	{
		AnimationSequence->RefPoseType = AdditiveBasePoseType;
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for SetAdditiveBasePoseType"));
	}
}

void UAnimationBlueprintLibrary::GetAnimationInterpolationType(const UAnimSequence* AnimationSequence, EAnimInterpolationType& InterpolationType)
{
	if (AnimationSequence)
	{
		InterpolationType = AnimationSequence->Interpolation;
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for GetAnimationInterpolationType"));
	}
}

void UAnimationBlueprintLibrary::SetAnimationInterpolationType(UAnimSequence* AnimationSequence, EAnimInterpolationType Type)
{
	if (AnimationSequence)
	{
		AnimationSequence->Interpolation = Type;
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for SetAnimationInterpolationType"));
	}
}

bool UAnimationBlueprintLibrary::IsRootMotionEnabled(const UAnimSequence* AnimationSequence)
{
	bool bEnabled = false;
	if (AnimationSequence)
	{
		bEnabled = AnimationSequence->bEnableRootMotion;
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for IsRootMotionEnabled"));
	}

	return bEnabled;
}

void UAnimationBlueprintLibrary::SetRootMotionEnabled(UAnimSequence* AnimationSequence, bool bEnabled)
{
	bool bIsEnabled = false;

	if (AnimationSequence)
	{
		bIsEnabled = AnimationSequence->bEnableRootMotion;
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for SetRootMotionEnabled"));
	}
}

void  UAnimationBlueprintLibrary::GetRootMotionLockType(const UAnimSequence* AnimationSequence, TEnumAsByte<ERootMotionRootLock::Type>& LockType)
{
	if (AnimationSequence)
	{
		LockType = AnimationSequence->RootMotionRootLock;
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for GetRootMotionLockType"));
	}
}

void UAnimationBlueprintLibrary::SetRootMotionLockType(UAnimSequence* AnimationSequence, TEnumAsByte<ERootMotionRootLock::Type> RootMotionLockType)
{
	if (AnimationSequence)
	{
		AnimationSequence->RootMotionRootLock = RootMotionLockType;
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for SetRootMotionLockType"));
	}	
}

bool UAnimationBlueprintLibrary::IsRootMotionLockForced(const UAnimSequence* AnimationSequence)
{
	bool bIsLocked = false;
	if (AnimationSequence)
	{
		bIsLocked = AnimationSequence->bForceRootLock;
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for IsRootMotionLockForced"));	
	}

	return bIsLocked;
}

void UAnimationBlueprintLibrary::SetIsRootMotionLockForced(UAnimSequence* AnimationSequence, bool bForced)
{
	if (AnimationSequence)
	{
		AnimationSequence->bForceRootLock = bForced;
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for SetIsRootMotionLockForced"));
	}
}

void UAnimationBlueprintLibrary::GetAnimationSyncMarkers(const UAnimSequence* AnimationSequence, TArray<FAnimSyncMarker>& Markers)
{
	Markers.Empty();
	if (AnimationSequence)
	{
		Markers = AnimationSequence->AuthoredSyncMarkers;;
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for GetAnimationSyncMarkers"));
	}
}

void UAnimationBlueprintLibrary::GetUniqueMarkerNames(const UAnimSequence* AnimationSequence, TArray<FName>& MarkerNames)
{
	MarkerNames.Empty();
	if (AnimationSequence)
	{
		MarkerNames = AnimationSequence->UniqueMarkerNames;
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for GetUniqueMarkerNames"));
	}
}

void UAnimationBlueprintLibrary::AddAnimationSyncMarker(UAnimSequence* AnimationSequence, FName MarkerName, float Time, FName TrackName)
{
	if (AnimationSequence)
	{
		const bool bIsValidTrackName = IsValidAnimNotifyTrackName(AnimationSequence, TrackName);
		const bool bIsValidTime = IsValidTimeInternal(AnimationSequence, Time);

		if (bIsValidTrackName && bIsValidTime)
		{
			FAnimSyncMarker NewMarker;
			NewMarker.MarkerName = MarkerName;
			NewMarker.Time = Time;
			NewMarker.TrackIndex = GetTrackIndexForAnimationNotifyTrackName(AnimationSequence, TrackName);

			AnimationSequence->AuthoredSyncMarkers.Add(NewMarker);
			AnimationSequence->AnimNotifyTracks[NewMarker.TrackIndex].SyncMarkers.Add(&AnimationSequence->AuthoredSyncMarkers.Last());
						
			AnimationSequence->RefreshSyncMarkerDataFromAuthored();

			// Refresh all cached data
			AnimationSequence->RefreshCacheData();
		}
		else
		{
			if (!bIsValidTrackName)
			{
				UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Animation Notify Track %s does not exist in Animation Sequence %s"), *TrackName.ToString(), *AnimationSequence->GetName());
			}

			if (!bIsValidTime)
			{
				UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("%f is side of Animation Sequence %s range"), Time, *AnimationSequence->GetName());
			}
		}
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for AddAnimationSyncMarker"));
	}
	
}

bool UAnimationBlueprintLibrary::IsValidAnimationSyncMarkerName(const UAnimSequence* AnimationSequence, FName MarkerName)
{
	bool bIsValid = false;
	if (AnimationSequence)
	{
		bIsValid = AnimationSequence->UniqueMarkerNames.Contains(MarkerName);
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for IsValidAnimationSyncMarkerName"));
	}

	return bIsValid;	
}

int32 UAnimationBlueprintLibrary::RemoveAnimationSyncMarkersByName(UAnimSequence* AnimationSequence, FName MarkerName)
{
	int32 NumRemovedMarkers = 0;
	if (AnimationSequence)
	{
		NumRemovedMarkers = AnimationSequence->AuthoredSyncMarkers.RemoveAll(
			[&](const FAnimSyncMarker& Marker)
		{
			return Marker.MarkerName == MarkerName;
		});

		AnimationSequence->RefreshSyncMarkerDataFromAuthored();

		// Refresh all cached data
		AnimationSequence->RefreshCacheData();
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for RemoveAnimationSyncMarkersByName"));
	}
	
	return NumRemovedMarkers;
}

int32 UAnimationBlueprintLibrary::RemoveAnimationSyncMarkersByTrack(UAnimSequence* AnimationSequence, FName NotifyTrackName)
{
	int32 NumRemovedMarkers = 0;
	if (AnimationSequence)
	{
		const int32 TrackIndex = GetTrackIndexForAnimationNotifyTrackName(AnimationSequence, NotifyTrackName);
		if (TrackIndex != INDEX_NONE)
		{
			NumRemovedMarkers = AnimationSequence->AuthoredSyncMarkers.RemoveAll(
				[&](const FAnimSyncMarker& Marker)
			{
				return Marker.TrackIndex == TrackIndex;
			});

			AnimationSequence->RefreshSyncMarkerDataFromAuthored();

			// Refresh all cached data
			AnimationSequence->RefreshCacheData();
		}
		else
		{
			UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Animation Notify Track %s does not exist on Animation Sequence %s"), *NotifyTrackName.ToString(), *AnimationSequence->GetName());
		}
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for RemoveAnimationSyncMarkersByTrack"));
	}	
	
	return NumRemovedMarkers;
}

void UAnimationBlueprintLibrary::RemoveAllAnimationSyncMarkers(UAnimSequence* AnimationSequence)
{
	if (AnimationSequence)
	{
		AnimationSequence->AuthoredSyncMarkers.Empty();
		AnimationSequence->RefreshSyncMarkerDataFromAuthored();

		// Refresh all cached data
		AnimationSequence->RefreshCacheData();
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for RemoveAllAnimationSyncMarkers"));
	}
}

void UAnimationBlueprintLibrary::GetAnimationNotifyEvents(const UAnimSequence* AnimationSequence, TArray<FAnimNotifyEvent>& NotifyEvents)
{
	NotifyEvents.Empty();
	if (AnimationSequence)
	{
		NotifyEvents = AnimationSequence->Notifies;
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for GetAnimationNotifyEvents"));
	}	
}

void UAnimationBlueprintLibrary::GetAnimationNotifyEventNames(const UAnimSequence* AnimationSequence, TArray<FName>& EventNames)
{
	EventNames.Empty();
	if (AnimationSequence)
	{
		for (const FAnimNotifyEvent& Event : AnimationSequence->Notifies)
		{
			EventNames.AddUnique(Event.NotifyName);
		}
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for GetAnimationNotifyEventNames"));
	}	
}

UAnimNotify* UAnimationBlueprintLibrary::AddAnimationNotifyEvent(UAnimSequence* AnimationSequence, FName NotifyTrackName, float StartTime, float Duration, TSubclassOf<UAnimNotifyState> NotifyClass)
{
	UAnimNotify* Notify = nullptr;
	if (AnimationSequence)
	{
		const bool bIsValidTrackName = IsValidAnimNotifyTrackName(AnimationSequence, NotifyTrackName);
		const bool bIsValidTime = IsValidTimeInternal(AnimationSequence, StartTime);

		if (bIsValidTrackName && bIsValidTime)
		{
			AnimationSequence->Notifies.AddDefaulted();
			FAnimNotifyEvent& NewEvent = AnimationSequence->Notifies.Last();

			NewEvent.NotifyName = NAME_None;
			NewEvent.Link(AnimationSequence, StartTime);
			NewEvent.TriggerTimeOffset = GetTriggerTimeOffsetForType(AnimationSequence->CalculateOffsetForNotify(StartTime));
			NewEvent.TrackIndex = GetTrackIndexForAnimationNotifyTrackName(AnimationSequence, NotifyTrackName);

			if (NotifyClass)
			{
				class UObject* AnimNotifyClass = NewObject<UObject>(AnimationSequence, NotifyClass, NAME_None, RF_Transactional);
				NewEvent.NotifyStateClass = Cast<UAnimNotifyState>(AnimNotifyClass);
				NewEvent.Notify = Cast<UAnimNotify>(AnimNotifyClass);

				// Setup name and duration for new event
				if (NewEvent.NotifyStateClass)
				{
					NewEvent.NotifyName = FName(*NewEvent.NotifyStateClass->GetNotifyName());
					NewEvent.SetDuration(Duration);
					NewEvent.EndLink.Link(AnimationSequence, NewEvent.EndLink.GetTime());
				}
				else
				{
					NewEvent.NotifyName = FName(*NewEvent.Notify->GetNotifyName());
				}
			}
			else
			{
				NewEvent.Notify = NULL;
				NewEvent.NotifyStateClass = NULL;
			}

			// Refresh all cached data
			AnimationSequence->RefreshCacheData();			

			Notify = NewEvent.Notify;
		}
		else
		{
			if (!bIsValidTrackName)
			{
				UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Animation Notify Track %s does not exist on Animation Sequence %s"), *NotifyTrackName.ToString(), *AnimationSequence->GetName());
			}

			if (!bIsValidTime)
			{
				UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("%f is side of Animation Sequence %s range"), StartTime, *AnimationSequence->GetName());
			}
		}
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for AddAnimationNotifyEvent"));
	}

	return Notify;
}

void UAnimationBlueprintLibrary::AddAnimationNotifyEventObject(UAnimSequence* AnimationSequence, float StartTime, UAnimNotify* Notify, FName NotifyTrackName)
{
	if (AnimationSequence)
	{
		const bool bValidNotify = Notify != nullptr;
		const bool bValidOuter = bValidNotify && Notify->GetOuter() == AnimationSequence;
		const bool bIsValidTrackName = IsValidAnimNotifyTrackName(AnimationSequence, NotifyTrackName);
		const bool bIsValidTime = IsValidTimeInternal(AnimationSequence, StartTime);

		if (bValidNotify && bValidOuter && bIsValidTrackName && bIsValidTime)
		{
			AnimationSequence->Notifies.AddDefaulted();
			FAnimNotifyEvent& NewEvent = AnimationSequence->Notifies.Last();

			NewEvent.NotifyName = NAME_None;
			NewEvent.Link(AnimationSequence, StartTime);
			NewEvent.TriggerTimeOffset = GetTriggerTimeOffsetForType(AnimationSequence->CalculateOffsetForNotify(StartTime));
			NewEvent.TrackIndex = GetTrackIndexForAnimationNotifyTrackName(AnimationSequence, NotifyTrackName);
			NewEvent.NotifyStateClass = Cast<UAnimNotifyState>(Notify);
			NewEvent.Notify = Notify;

			// Refresh all cached data
			AnimationSequence->RefreshCacheData();
		}
		else
		{
			if (!bValidNotify)
			{
				UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Notify in AddAnimationNotifyEventObject"));
			}

			if (!bValidOuter)
			{
				const FString NotifyName = bValidNotify ? Notify->GetName() : "Invalid Notify";
				UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Animation Notify %s Outer is not %s"), *NotifyName, *AnimationSequence->GetName());
			}

			if (!bIsValidTrackName)
			{
				UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Animation Notify Track %s does not exist on Animation Sequence %s"), *NotifyTrackName.ToString(), *AnimationSequence->GetName());
			}

			if (!bIsValidTime)
			{
				UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("%f is side of Animation Sequence %s range"), StartTime, *AnimationSequence->GetName());
			}
		}
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for AddAnimationNotifyEventObject"));
	}
	
}

int32 UAnimationBlueprintLibrary::RemoveAnimationNotifyEventsByName(UAnimSequence* AnimationSequence, FName NotifyName)
{
	int32 NumRemovedEvents = 0;
	if (AnimationSequence)
	{
		NumRemovedEvents = AnimationSequence->Notifies.RemoveAll(
			[&](const FAnimNotifyEvent& Event)
		{
			return Event.NotifyName == NotifyName;
		});

		// Refresh all cached data
		AnimationSequence->RefreshCacheData();
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for RemoveAnimationNotifyEventsByName"));
	}	

	return NumRemovedEvents;
}

int32 UAnimationBlueprintLibrary::RemoveAnimationNotifyEventsByTrack(UAnimSequence* AnimationSequence, FName NotifyTrackName)
{
	int32 NumRemovedEvents = 0;
	if (AnimationSequence)
	{
		const bool bIsValidTrackName = IsValidAnimNotifyTrackName(AnimationSequence, NotifyTrackName);
		if (bIsValidTrackName)
		{
			const int32 TrackIndex = GetTrackIndexForAnimationNotifyTrackName(AnimationSequence, NotifyTrackName);
			NumRemovedEvents = AnimationSequence->Notifies.RemoveAll(
				[&](const FAnimNotifyEvent& Event)
			{
				return Event.TrackIndex == TrackIndex;
			});

			// Refresh all cached data
			AnimationSequence->RefreshCacheData();
		}
		else
		{
			UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Animation Notify Track %s does not exist on Animation Sequence %s"), *NotifyTrackName.ToString(), *AnimationSequence->GetName());
		}
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for RemoveAnimationNotifyEventsByTrack"));
	}
	

	return NumRemovedEvents;
}

void UAnimationBlueprintLibrary::GetAnimationNotifyTrackNames(const UAnimSequence* AnimationSequence, TArray<FName>& TrackNames)
{
	TrackNames.Empty();
	if (AnimationSequence)
	{
		for (const FAnimNotifyTrack& Track : AnimationSequence->AnimNotifyTracks)
		{
			TrackNames.AddUnique(Track.TrackName);
		}
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for GetAnimationNotifyTrackNames"));
	}
}

void UAnimationBlueprintLibrary::AddAnimationNotifyTrack(UAnimSequence* AnimationSequence, FName NotifyTrackName, FLinearColor TrackColor /*= FLinearColor::White*/)
{
	if (AnimationSequence)
	{
		const bool bExistingTrackName = IsValidAnimNotifyTrackName(AnimationSequence, NotifyTrackName);
		if (!bExistingTrackName)
		{
			FAnimNotifyTrack NewTrack;
			NewTrack.TrackName = NotifyTrackName;
			NewTrack.TrackColor = TrackColor;
			AnimationSequence->AnimNotifyTracks.Add(NewTrack);

			// Refresh all cached data
			AnimationSequence->RefreshCacheData();
		}
		else
		{
			UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Animation Notify Track %s already exists on Animation Sequence %s"), *NotifyTrackName.ToString(), *AnimationSequence->GetName());
		}
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for AddAnimationNotifyTrack"));
	}
	
}

void UAnimationBlueprintLibrary::RemoveAnimationNotifyTrack(UAnimSequence* AnimationSequence, FName NotifyTrackName)
{
	if (AnimationSequence)
	{
		const int32 TrackIndexToDelete = GetTrackIndexForAnimationNotifyTrackName(AnimationSequence, NotifyTrackName);
		if (TrackIndexToDelete != INDEX_NONE)
		{	
			// Remove all notifies and sync markers on the to-delete-track
			AnimationSequence->Notifies.RemoveAll([&](const FAnimNotifyEvent& Notify) { return Notify.TrackIndex == TrackIndexToDelete; });
			AnimationSequence->AuthoredSyncMarkers.RemoveAll([&](const FAnimSyncMarker& Marker) { return Marker.TrackIndex == TrackIndexToDelete; });

			// Before track removal, make sure everything behind is fixed
			for (FAnimNotifyEvent& Notify : AnimationSequence->Notifies)
			{
				if (Notify.TrackIndex > TrackIndexToDelete)
				{
					Notify.TrackIndex = Notify.TrackIndex - 1;
				}				
			}
			for (FAnimSyncMarker& SyncMarker : AnimationSequence->AuthoredSyncMarkers)
			{
				if (SyncMarker.TrackIndex > TrackIndexToDelete)
				{
					SyncMarker.TrackIndex = SyncMarker.TrackIndex - 1;
				}
			}
			
			// Delete the track itself
			AnimationSequence->AnimNotifyTracks.RemoveAt(TrackIndexToDelete);

			// Refresh all cached data
			AnimationSequence->RefreshCacheData();
		}		
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for RemoveAnimationNotifyTrack"));
	}
}

void UAnimationBlueprintLibrary::RemoveAllAnimationNotifyTracks(UAnimSequence* AnimationSequence)
{
	if (AnimationSequence)
	{
		AnimationSequence->Notifies.Empty();
		AnimationSequence->AuthoredSyncMarkers.Empty();

		// Remove all but one notify tracks
		AnimationSequence->AnimNotifyTracks.SetNum(1);

		// Also remove all stale notifies and sync markers from only track 
		AnimationSequence->AnimNotifyTracks[0].Notifies.Empty();
		AnimationSequence->AnimNotifyTracks[0].SyncMarkers.Empty();

		// Refresh all cached data
		AnimationSequence->RefreshCacheData();
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for RemoveAllAnimationNotifyTracks"));
	}	
}

bool UAnimationBlueprintLibrary::IsValidAnimNotifyTrackName(const UAnimSequence* AnimationSequence, FName NotifyTrackName)
{
	bool bIsValid = false;
	if (AnimationSequence)
	{
		bIsValid = GetTrackIndexForAnimationNotifyTrackName(AnimationSequence, NotifyTrackName) != INDEX_NONE;
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for IsValidAnimNotifyTrackName"));
	}

	return bIsValid;	
}

int32 UAnimationBlueprintLibrary::GetTrackIndexForAnimationNotifyTrackName(const UAnimSequence* AnimationSequence, FName NotifyTrackName)
{
	return AnimationSequence->AnimNotifyTracks.IndexOfByPredicate(
		[&](const FAnimNotifyTrack& Track)
	{
		return Track.TrackName == NotifyTrackName;
	});
}

const FAnimNotifyTrack& UAnimationBlueprintLibrary::GetNotifyTrackByName(const UAnimSequence* AnimationSequence, FName NotifyTrackName)
{
	const int32 TrackIndex = GetTrackIndexForAnimationNotifyTrackName(AnimationSequence, NotifyTrackName);
	checkf(TrackIndex != INDEX_NONE, TEXT("Notify Track %s does not exist on %s"), *NotifyTrackName.ToString(), *AnimationSequence->GetName());
	return AnimationSequence->AnimNotifyTracks[TrackIndex];
}

void UAnimationBlueprintLibrary::GetAnimationSyncMarkersForTrack(const UAnimSequence* AnimationSequence, FName NotifyTrackName, TArray<FAnimSyncMarker>& Markers)
{
	Markers.Empty();
	if (AnimationSequence)
	{
		const bool bIsValidTrackName = IsValidAnimNotifyTrackName(AnimationSequence, NotifyTrackName);

		if (bIsValidTrackName)
		{
			const FAnimNotifyTrack& Track = GetNotifyTrackByName(AnimationSequence, NotifyTrackName);
			Markers.Empty(Track.SyncMarkers.Num());
			for (FAnimSyncMarker* Marker : Track.SyncMarkers)
			{
				Markers.Add(*Marker);
			}
		}
		else
		{
			UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Animation Notify Track %s does not exist on Animation Sequence %s"), *NotifyTrackName.ToString(), *AnimationSequence->GetName());
		}
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for AddVectorCurveKey"));
	}
}

void UAnimationBlueprintLibrary::GetAnimationNotifyEventsForTrack(const UAnimSequence* AnimationSequence, FName NotifyTrackName, TArray<FAnimNotifyEvent>& Events)
{
	Events.Empty();
	if (AnimationSequence)
	{
		const bool bIsValidTrackName = IsValidAnimNotifyTrackName(AnimationSequence, NotifyTrackName);

		if (bIsValidTrackName)
		{
			const FAnimNotifyTrack& Track = GetNotifyTrackByName(AnimationSequence, NotifyTrackName);
			Events.Empty(Track.Notifies.Num());
			for (FAnimNotifyEvent* Event : Track.Notifies)
			{
				Events.Add(*Event);
			}
		}
		else
		{
			UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Animation Notify Track %s does not exist on Animation Sequence %s"), *NotifyTrackName.ToString(), *AnimationSequence->GetName());
		}
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for AddVectorCurveKey"));
	}
}

void UAnimationBlueprintLibrary::AddCurve(UAnimSequence* AnimationSequence, FName CurveName, ERawCurveTrackTypes CurveType /*= RCT_Float*/, bool bMetaDataCurve /*= false*/)
{
	if (AnimationSequence)
	{
		static const ESmartNameContainerType ContainerForCurveType[(int32)ERawCurveTrackTypes::RCT_MAX] = { ESmartNameContainerType::SNCT_CurveMapping, ESmartNameContainerType::SNCT_CurveMapping, ESmartNameContainerType::SNCT_TrackCurveMapping };
		const ESmartNameContainerType CurveContainer = ContainerForCurveType[(int32)CurveType];
		const int32 CurveFlags = bMetaDataCurve ? AACF_Metadata : AACF_DefaultCurve;
		
		// Validate combination of curve types

		// Only Float metadata curves are valid
		const bool bValidMetaData = !bMetaDataCurve || (bMetaDataCurve && CurveType == ERawCurveTrackTypes::RCT_Float);
		// Transform curves can only be added if the curve name exists as a bone on the skeleton
		const bool bValidTransformCurveData = CurveType != ERawCurveTrackTypes::RCT_Transform || (AnimationSequence->GetSkeleton() && DoesBoneNameExistInternal(AnimationSequence->GetSkeleton(), CurveName));

		if (bValidMetaData && bValidTransformCurveData )
		{
			// Add or retrieve the smartname
			const bool bCurveAdded = AddCurveInternal(AnimationSequence, CurveName, SmartContainerNames[(int32)CurveContainer], CurveFlags, CurveType);

			if (!bCurveAdded)
			{
				// Curve already existed
				UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Curve %s already exists on the Skeleton %s."), *CurveName.ToString(), *AnimationSequence->GetSkeleton()->GetName());
			}
		}
		else
		{
			if (!bValidMetaData)
			{
				UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Curve type to be create as metadata, currently only float curves are supported as metadata."));
			}
			
			if (!bValidTransformCurveData)
			{
				UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Transform Curve name, the supplied name %s does not exist on the Skeleton %s."), *CurveName.ToString(), AnimationSequence->GetSkeleton() ? *AnimationSequence->GetSkeleton()->GetName() : TEXT("Invalid Skeleton"));
			}
		}
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for AddCurve"));
	}	
}

void UAnimationBlueprintLibrary::RemoveCurve(UAnimSequence* AnimationSequence, FName CurveName, bool bRemoveNameFromSkeleton /*= false*/)
{
	if (AnimationSequence)
	{
		const FName ContainerName = RetrieveContainerNameForCurve(AnimationSequence, CurveName);
		if (ContainerName != NAME_None)
		{
			const bool bCurveRemoved = RemoveCurveInternal(AnimationSequence, CurveName, ContainerName, bRemoveNameFromSkeleton);
		}
		else
		{
			UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Could not find SmartName Container for Curve Name %s while trying to remove the curve"), *CurveName.ToString());
		}
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for RemoveCurve"));
	}
}

void UAnimationBlueprintLibrary::RemoveAllCurveData(UAnimSequence* AnimationSequence)
{
	if (AnimationSequence)
	{
		AnimationSequence->RawCurveData.DeleteAllCurveData(ERawCurveTrackTypes::RCT_Float);
		AnimationSequence->RawCurveData.DeleteAllCurveData(ERawCurveTrackTypes::RCT_Vector);
		AnimationSequence->RawCurveData.DeleteAllCurveData(ERawCurveTrackTypes::RCT_Transform);

		AnimationSequence->bNeedsRebake = true;
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for RemoveAllCurveData"));
	}
}

void UAnimationBlueprintLibrary::AddTransformationCurveKey(UAnimSequence* AnimationSequence, FName CurveName, const float Time, const FTransform& Transform)
{
	if (AnimationSequence)
	{
		TArray<float> TimeArray;
		TArray<FTransform> TransformArray;

		TimeArray.Add(Time);
		TransformArray.Add(Transform);

		AddCurveKeysInternal<FTransform, FTransformCurve>(AnimationSequence, CurveName, TimeArray, TransformArray, ERawCurveTrackTypes::RCT_Transform);
		AnimationSequence->bNeedsRebake = true;
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for AddTransformationCurveKey"));
	}

}

void UAnimationBlueprintLibrary::AddTransformationCurveKeys(UAnimSequence* AnimationSequence, FName CurveName, const TArray<float>& Times, const TArray<FTransform>& Transforms)
{
	if (AnimationSequence)
	{
		if (Times.Num() == Transforms.Num())
		{
			AddCurveKeysInternal<FTransform, FTransformCurve>(AnimationSequence, CurveName, Times, Transforms, ERawCurveTrackTypes::RCT_Transform);
			AnimationSequence->bNeedsRebake = true;
		}
		else
		{
			UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Number of Time values %i does not match the number of Transforms %i in AddTransformationCurveKeys"), Times.Num(), Transforms.Num());
		}	
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for AddTransformationCurveKeys"));
	}
}


void UAnimationBlueprintLibrary::AddFloatCurveKey(UAnimSequence* AnimationSequence, FName CurveName, const float Time, const float Value)
{
	if (AnimationSequence)
	{
		TArray<float> TimeArray;
		TArray<float> ValueArray;

		TimeArray.Add(Time);
		ValueArray.Add(Value);

		AddCurveKeysInternal<float, FFloatCurve>(AnimationSequence, CurveName, TimeArray, ValueArray, ERawCurveTrackTypes::RCT_Float);
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for AddFloatCurveKey"));
	}

}

void UAnimationBlueprintLibrary::AddFloatCurveKeys(UAnimSequence* AnimationSequence, FName CurveName, const TArray<float>& Times, const TArray<float>& Values)
{
	if (AnimationSequence)
	{
		if (Times.Num() == Values.Num())
		{
			AddCurveKeysInternal<float, FFloatCurve>(AnimationSequence, CurveName, Times, Values, ERawCurveTrackTypes::RCT_Float);
		}
		else
		{
			UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Number of Time values %i does not match the number of Values %i in AddFloatCurveKeys"), Times.Num(), Values.Num());			
		}
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for AddFloatCurveKeys"));
	}

	
}

void UAnimationBlueprintLibrary::AddVectorCurveKey(UAnimSequence* AnimationSequence, FName CurveName, const float Time, const FVector Vector)
{
	if (AnimationSequence)
	{
		TArray<float> TimeArray;
		TArray<FVector> VectorArray;

		TimeArray.Add(Time);
		VectorArray.Add(Vector);

		AddCurveKeysInternal<FVector, FVectorCurve>(AnimationSequence, CurveName, TimeArray, VectorArray, ERawCurveTrackTypes::RCT_Vector);
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for AddVectorCurveKey"));
	}

}

void UAnimationBlueprintLibrary::AddVectorCurveKeys(UAnimSequence* AnimationSequence, FName CurveName, const TArray<float>& Times, const TArray<FVector>& Vectors)
{
	if (AnimationSequence)
	{
		if (Times.Num() == Vectors.Num())
		{
			AddCurveKeysInternal<FVector, FVectorCurve>(AnimationSequence, CurveName, Times, Vectors, ERawCurveTrackTypes::RCT_Vector);
		}
		else
		{
			UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Number of Time values %i does not match the number of Vectors %i in AddVectorCurveKeys"), Times.Num(), Vectors.Num());
		}
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for AddVectorCurveKeys"));
	}

	
}

template <typename DataType, typename CurveClass>
void UAnimationBlueprintLibrary::AddCurveKeysInternal(UAnimSequence* AnimationSequence, FName CurveName, const TArray<float>& Times, const TArray<DataType>& KeyData, ERawCurveTrackTypes CurveType)
{
	checkf(Times.Num() == KeyData.Num(), TEXT("Not enough key data supplied"));

	const FName ContainerName = RetrieveContainerNameForCurve(AnimationSequence, CurveName);

	if (ContainerName != NAME_None)
	{
		// Retrieve smart name for curve
		const FSmartName CurveSmartName = RetrieveSmartNameForCurve(AnimationSequence, CurveName, ContainerName);

		// Retrieve the curve by name
		CurveClass* Curve = static_cast<CurveClass*>(AnimationSequence->RawCurveData.GetCurveData(CurveSmartName.UID, CurveType));
		if (Curve)
		{
			const int32 NumKeys = KeyData.Num();
			for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
			{
				Curve->UpdateOrAddKey(KeyData[KeyIndex], Times[KeyIndex]);
			}

			AnimationSequence->BakeTrackCurvesToRawAnimation();
		}
	}
}

bool UAnimationBlueprintLibrary::AddCurveInternal(UAnimSequence* AnimationSequence, FName CurveName, FName ContainerName, int32 CurveFlags, ERawCurveTrackTypes SupportedCurveType)
{
	// Add or retrieve the smart name
	FSmartName SmartCurveName;
	AnimationSequence->GetSkeleton()->AddSmartNameAndModify(ContainerName, CurveName, SmartCurveName);	

	bool bCurveAdded = false;

	if (AnimationSequence->RawCurveData.GetCurveData(SmartCurveName.UID) == nullptr)
	{
		bCurveAdded = AnimationSequence->RawCurveData.AddCurveData(SmartCurveName, CurveFlags, SupportedCurveType);		
	}
	else
	{
		// Curve already exists
	}

	return bCurveAdded;
}

bool UAnimationBlueprintLibrary::RemoveCurveInternal(UAnimSequence* AnimationSequence, FName CurveName, FName ContainerName, bool bRemoveNameFromSkeleton)
{
	checkf(AnimationSequence != nullptr, TEXT("Invalid Animation Sequence ptr"));
	bool bRemoved = false;
	SmartName::UID_Type UID = AnimationSequence->GetSkeleton()->GetUIDByName(ContainerName, CurveName);
	if (UID != SmartName::MaxUID)
	{
		FSmartName SmartCurveName;
		USkeleton* Skeleton = AnimationSequence->GetSkeleton();
		checkf(Skeleton != nullptr, TEXT("Invalid Skeleton ptr"));
		if (Skeleton->GetSmartNameByUID(ContainerName, UID, SmartCurveName))
		{
			if (ContainerName == USkeleton::AnimTrackCurveMappingName)
			{
				bRemoved = AnimationSequence->RawCurveData.DeleteCurveData(SmartCurveName, ERawCurveTrackTypes::RCT_Transform);
				AnimationSequence->bNeedsRebake = true;
			}
			else
			{
				bRemoved = AnimationSequence->RawCurveData.DeleteCurveData(SmartCurveName, ERawCurveTrackTypes::RCT_Float);
				bRemoved |= AnimationSequence->RawCurveData.DeleteCurveData(SmartCurveName, ERawCurveTrackTypes::RCT_Vector);
			}			

			if (bRemoveNameFromSkeleton)
			{
				// Ensure we are eligible to do this
				bool bValidToRemove = true;

				if (ContainerName == USkeleton::AnimTrackCurveMappingName)
				{
					// Make sure we do not remove bone names
					bValidToRemove = DoesBoneNameExistInternal(Skeleton, CurveName);
				}

				if (bValidToRemove)
				{
					Skeleton->RemoveSmartnameAndModify(ContainerName, UID);
				}
				else
				{
					UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Cannot remove Curve Name %s from Skeleton %s"), *CurveName.ToString(), *AnimationSequence->GetSkeleton()->GetName());
				}
			}
		}
		else
		{
			UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Could not retrieve Smart Name for Curve Name %s from Skeleton %s"), *CurveName.ToString(), *AnimationSequence->GetSkeleton()->GetName());
		}
	}
	else
	{
		// Name does not exist on skeleton
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Could find for Curve Name %s in Skeleton %s"), *CurveName.ToString(), *AnimationSequence->GetSkeleton()->GetName());
	}

	return bRemoved;
}

void UAnimationBlueprintLibrary::DoesBoneNameExist(UAnimSequence* AnimationSequence, FName BoneName, bool& bExists)
{
	bExists = false;
	if (AnimationSequence)
	{
		USkeleton* Skeleton = AnimationSequence->GetSkeleton();
		if (Skeleton)
		{
			bExists = DoesBoneNameExistInternal(Skeleton, BoneName);
		}
		else
		{
			UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("No Skeleton found for Animation Sequence %s"), *AnimationSequence->GetName());
		}
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for DoesBoneNameExist"));
	}
}

bool UAnimationBlueprintLibrary::DoesBoneNameExistInternal(USkeleton* Skeleton, FName BoneName)
{
	checkf(Skeleton != nullptr, TEXT("Invalid Skeleton ptr"));
	return Skeleton->GetUIDByName(USkeleton::AnimTrackCurveMappingName, BoneName) != SmartName::MaxUID;
}

void UAnimationBlueprintLibrary::GetFloatKeys(UAnimSequence* AnimationSequence, FName CurveName, TArray<float>& Times, TArray<float>& Values)
{
	if (AnimationSequence)
	{
		GetCurveKeysInternal<float, FFloatCurve>(AnimationSequence, CurveName, Times, Values, ERawCurveTrackTypes::RCT_Float);
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for GetFloatKeys"));
	}
}

void UAnimationBlueprintLibrary::GetVectorKeys(UAnimSequence* AnimationSequence, FName CurveName, TArray<float>& Times, TArray<FVector>& Values)
{
	if (AnimationSequence)
	{
		GetCurveKeysInternal<FVector, FVectorCurve>(AnimationSequence, CurveName, Times, Values, ERawCurveTrackTypes::RCT_Vector);
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for GetVectorKeys"));
	}
}

void UAnimationBlueprintLibrary::GetTransformationKeys(UAnimSequence* AnimationSequence, FName CurveName, TArray<float>& Times, TArray<FTransform>& Values)
{
	if (AnimationSequence)
	{
		GetCurveKeysInternal<FTransform, FTransformCurve>(AnimationSequence, CurveName, Times, Values, ERawCurveTrackTypes::RCT_Transform);
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for GetTransformationKeys"));
	}
}

template <typename DataType, typename CurveClass>
void UAnimationBlueprintLibrary::GetCurveKeysInternal(UAnimSequence* AnimationSequence, FName CurveName, TArray<float>& Times, TArray<DataType>& KeyData, ERawCurveTrackTypes CurveType)
{
	checkf(AnimationSequence != nullptr, TEXT("Invalid Animation Sequence ptr"));
	const FName ContainerName = RetrieveContainerNameForCurve(AnimationSequence, CurveName);

	if (ContainerName != NAME_None)
	{
		// Retrieve smart name for curve
		const FSmartName CurveSmartName = RetrieveSmartNameForCurve(AnimationSequence, CurveName, ContainerName);

		// Retrieve the curve by name
		CurveClass* Curve = static_cast<CurveClass*>(AnimationSequence->RawCurveData.GetCurveData(CurveSmartName.UID, CurveType));
		if (Curve)
		{
			Curve->GetKeys(Times, KeyData);
			checkf(Times.Num() == KeyData.Num(), TEXT("Invalid key data retrieved from curve"));
		}
	}
}

bool UAnimationBlueprintLibrary::DoesCurveExist(UAnimSequence* AnimationSequence, FName CurveName, ERawCurveTrackTypes CurveType)
{
	bool bExistingCurve = false;

	if (AnimationSequence)
	{
		FSmartName SmartName;
		if (RetrieveSmartNameForCurve(AnimationSequence, CurveName, USkeleton::AnimTrackCurveMappingName, SmartName))
		{
			FAnimCurveBase* Curve = AnimationSequence->RawCurveData.GetCurveData(SmartName.UID, CurveType);
			bExistingCurve = Curve != nullptr;
		}

		if (RetrieveSmartNameForCurve(AnimationSequence, CurveName, USkeleton::AnimCurveMappingName, SmartName))
		{
			FAnimCurveBase* Curve = AnimationSequence->RawCurveData.GetCurveData(SmartName.UID, CurveType);
			bExistingCurve |= Curve != nullptr;
		}
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for DoesCurveExist"));
	}

	return bExistingCurve;
}

bool UAnimationBlueprintLibrary::DoesSmartNameExist(UAnimSequence* AnimationSequence, FName Name)
{
	checkf(AnimationSequence != nullptr, TEXT("Invalid Animation Sequence ptr"));
	FSmartName SmartName;
	return AnimationSequence->GetSkeleton()->GetSmartNameByName(USkeleton::AnimTrackCurveMappingName, Name, SmartName) ||
		AnimationSequence->GetSkeleton()->GetSmartNameByName(USkeleton::AnimCurveMappingName, Name, SmartName);
}

bool UAnimationBlueprintLibrary::RetrieveSmartNameForCurve(const UAnimSequence* AnimationSequence, FName CurveName, FName ContainerName, FSmartName& SmartName)
{
	checkf(AnimationSequence != nullptr, TEXT("Invalid Animation Sequence ptr"));
	return AnimationSequence->GetSkeleton()->GetSmartNameByName(ContainerName, CurveName, SmartName);
}

FSmartName UAnimationBlueprintLibrary::RetrieveSmartNameForCurve(const UAnimSequence* AnimationSequence, FName CurveName, FName ContainerName)
{
	checkf(AnimationSequence != nullptr, TEXT("Invalid Animation Sequence ptr"));
	FSmartName SmartCurveName;
	AnimationSequence->GetSkeleton()->GetSmartNameByName(ContainerName, CurveName, SmartCurveName);
	return SmartCurveName;
}

FName UAnimationBlueprintLibrary::RetrieveContainerNameForCurve(const UAnimSequence* AnimationSequence, FName CurveName)
{
	checkf(AnimationSequence != nullptr, TEXT("Invalid Animation Sequence ptr"));
	for (int32 Index = 0; Index < (int32)ESmartNameContainerType::SNCT_MAX; ++Index)
	{
		const FSmartNameMapping* CurveMapping = AnimationSequence->GetSkeleton()->GetSmartNameContainer(SmartContainerNames[Index]);
		if (CurveMapping->Exists(CurveName))
		{
			return SmartContainerNames[Index];
		}
	}

	return NAME_None;
}

void UAnimationBlueprintLibrary::AddMetaData(UAnimSequence* AnimationSequence, TSubclassOf<UAnimMetaData> MetaDataClass, UAnimMetaData*& MetaDataInstance)
{
	if (AnimationSequence)
	{
		MetaDataInstance = NewObject<UAnimMetaData>(AnimationSequence, MetaDataClass, NAME_None, RF_Transactional);
		if (MetaDataInstance)
		{
			AnimationSequence->AddMetaData(MetaDataInstance);
		}
		else
		{
			UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Failed to create instance for %s"), *MetaDataClass->GetName());
		}
		
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for AddMetaData"));
	}
}

void UAnimationBlueprintLibrary::AddMetaDataObject(UAnimSequence* AnimationSequence, UAnimMetaData* MetaDataObject)
{
	if (AnimationSequence && MetaDataObject)
	{
		if (MetaDataObject->GetOuter() == AnimationSequence)
		{
			AnimationSequence->AddMetaData(MetaDataObject);
		}
		else
		{
			UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Outer for MetaData Instance %s is not Animation Sequence %s"), *MetaDataObject->GetName(), *AnimationSequence->GetName());
		}		
	}
	else 
	{
		if (!AnimationSequence)
		{
			UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for AddMetaDataObject"));
		}
		
		if (!MetaDataObject)
		{
			UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid MetaDataObject for AddMetaDataObject"));
		}
	}
}

void UAnimationBlueprintLibrary::RemoveAllMetaData(UAnimSequence* AnimationSequence)
{
	if (AnimationSequence)
	{
		AnimationSequence->EmptyMetaData();
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for RemoveAllMetaData"));
	}
}

void UAnimationBlueprintLibrary::RemoveMetaData(UAnimSequence* AnimationSequence, UAnimMetaData* MetaDataObject)
{
	if (AnimationSequence)
	{
		AnimationSequence->RemoveMetaData(MetaDataObject);
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for RemoveMetaData"));
	}
}

void UAnimationBlueprintLibrary::RemoveMetaDataOfClass(UAnimSequence* AnimationSequence, TSubclassOf<UAnimMetaData> MetaDataClass)
{
	if (AnimationSequence)
	{
		TArray<UAnimMetaData*> MetaDataOfClass;
		GetMetaDataOfClass(AnimationSequence, MetaDataClass, MetaDataOfClass);
		AnimationSequence->RemoveMetaData(MetaDataOfClass);
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for RemoveMetaDataOfClass"));
	}	
}

void UAnimationBlueprintLibrary::GetMetaData(const UAnimSequence* AnimationSequence, TArray<UAnimMetaData*>& MetaData)
{
	MetaData.Empty();
	if (AnimationSequence)
	{
		MetaData = AnimationSequence->GetMetaData();
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for GetMetaData"));
	}
}

void UAnimationBlueprintLibrary::GetMetaDataOfClass(const UAnimSequence* AnimationSequence, TSubclassOf<UAnimMetaData> MetaDataClass, TArray<UAnimMetaData*>& MetaDataOfClass)
{
	MetaDataOfClass.Empty();
	if (AnimationSequence)
	{
		const TArray<UAnimMetaData*>& MetaData = AnimationSequence->GetMetaData();
		for (UAnimMetaData* MetaDataInstance : MetaData)
		{
			if (MetaDataInstance->GetClass() == *MetaDataClass)
			{
				MetaDataOfClass.Add(MetaDataInstance);
			}
		}
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for GetMetaDataOfClass"));
	}
}

bool UAnimationBlueprintLibrary::ContainsMetaDataOfClass(const UAnimSequence* AnimationSequence, TSubclassOf<UAnimMetaData> MetaDataClass)
{
	bool bContainsMetaData = false;
	if (AnimationSequence)
	{
		TArray<UAnimMetaData*> MetaData;
		GetMetaData(AnimationSequence, MetaData);
		bContainsMetaData = MetaData.FindByPredicate(
			[&](UAnimMetaData* MetaDataObject)
		{
			return (MetaDataObject->GetClass() == *MetaDataClass);
		}) != nullptr;
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence for ContainsMetaDataOfClass"));
	}

	return bContainsMetaData;	
}

void UAnimationBlueprintLibrary::GetBonePoseForTime(const UAnimSequence* AnimationSequence, FName BoneName, float Time, bool bExtractRootMotion, FTransform& Pose)
{
	Pose.SetIdentity();
	if (AnimationSequence)
	{
		TArray<FName> BoneNameArray;
		TArray<FTransform> PoseArray;
		BoneNameArray.Add(BoneName);
		GetBonePosesForTime(AnimationSequence, BoneNameArray, Time, bExtractRootMotion, PoseArray);
		Pose = PoseArray[0];
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for GetBonePoseForTime"));
	}
}

void UAnimationBlueprintLibrary::GetBonePoseForFrame(const UAnimSequence* AnimationSequence, FName BoneName, int32 Frame, bool bExtractRootMotion, FTransform& Pose)
{
	Pose.SetIdentity();
	if (AnimationSequence)
	{
		GetBonePoseForTime(AnimationSequence, BoneName, GetTimeAtFrameInternal(AnimationSequence, Frame), bExtractRootMotion, Pose);
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for GetBonePoseForFrame"));
	}
}

void UAnimationBlueprintLibrary::GetBonePosesForTime(const UAnimSequence* AnimationSequence, TArray<FName> BoneNames, float Time, bool bExtractRootMotion, TArray<FTransform>& Poses)
{
	Poses.Empty(BoneNames.Num());
	if (AnimationSequence)
	{
		Poses.AddDefaulted(BoneNames.Num());

		// Need this for FCompactPose
		FMemMark Mark(FMemStack::Get());

		if (IsValidTimeInternal(AnimationSequence, Time))
		{
			TArray<FBoneIndexType> RequiredBones;
			TArray<int32> FoundBoneIndices;

			FoundBoneIndices.AddZeroed(BoneNames.Num());

			for (int32 BoneNameIndex = 0; BoneNameIndex < BoneNames.Num(); ++BoneNameIndex)
			{
				const FName& BoneName = BoneNames[BoneNameIndex];
				const int32 BoneIndex = AnimationSequence->GetSkeleton()->GetReferenceSkeleton().FindRawBoneIndex(BoneName);

				FoundBoneIndices[BoneNameIndex] = INDEX_NONE;
				if (BoneIndex != INDEX_NONE)
				{
					FoundBoneIndices[BoneNameIndex] = RequiredBones.Add(BoneIndex);
				}
				else
				{
					UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid bone name %s for Animation Sequence %s in GetBonePosesForTime"), *BoneName.ToString(), *AnimationSequence->GetName());
				}
			}

			if (RequiredBones.Num())
			{
				FBoneContainer BoneContainer(RequiredBones, FCurveEvaluationOption(true), *AnimationSequence->GetSkeleton());
				BoneContainer.SetUseSourceData(true);
				BoneContainer.SetDisableRetargeting(true);
				FCompactPose Pose;
				Pose.SetBoneContainer(&BoneContainer);

				FBlendedCurve Curve;
				FAnimExtractContext Context;
				Context.bExtractRootMotion = bExtractRootMotion;
				Context.CurrentTime = Time;
				const bool bForceUseRawData = true;
				Curve.InitFrom(BoneContainer);

				AnimationSequence->GetBonePose(Pose, Curve, Context, bForceUseRawData);

				for (int32 BoneNameIndex = 0; BoneNameIndex < BoneNames.Num(); ++BoneNameIndex)
				{
					const int32 BoneContainerIndex = FoundBoneIndices[BoneNameIndex];
					Poses[BoneNameIndex] = BoneContainerIndex != INDEX_NONE ? Pose.GetBones()[BoneContainerIndex] : FTransform::Identity;
				}
			}
			else
			{
				UE_LOG(LogAnimationBlueprintLibrary, Error, TEXT("Invalid or no bone names specified to retrieve poses given  Animation Sequence %s in GetBonePosesForTime"), *AnimationSequence->GetName());
			}
		}
		else
		{
			UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid time value %f for Animation Sequence %s supplied for GetBonePosesForTime"), Time, *AnimationSequence->GetName());
		}
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for GetBonePosesForTime"));
	}
}

void UAnimationBlueprintLibrary::GetBonePosesForFrame(const UAnimSequence* AnimationSequence, TArray<FName> BoneNames, int32 Frame, bool bExtractRootMotion, TArray<FTransform>& Poses)
{
	Poses.Empty(BoneNames.Num());
	if (AnimationSequence)
	{
		GetBonePosesForTime(AnimationSequence, BoneNames, GetTimeAtFrameInternal(AnimationSequence, Frame), bExtractRootMotion, Poses);
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for GetBonePosesForFrame"));
	}
}

void UAnimationBlueprintLibrary::AddVirtualBone(const UAnimSequence* AnimationSequence, FName SourceBoneName, FName TargetBoneName, FName& VirtualBoneName)
{
	if (AnimationSequence)
	{
		USkeleton* Skeleton = AnimationSequence->GetSkeleton();
		if (Skeleton)
		{
			const bool bSourceBoneExists = DoesBoneNameExistInternal(Skeleton, SourceBoneName);
			const bool bTargetBoneExists = DoesBoneNameExistInternal(Skeleton, TargetBoneName);
			const bool bVirtualBoneDoesNotExist = !DoesVirtualBoneNameExistInternal(Skeleton, VirtualBoneName);
			
			if (bSourceBoneExists && bTargetBoneExists && bVirtualBoneDoesNotExist)
			{
				const bool bAdded = Skeleton->AddNewVirtualBone(SourceBoneName, TargetBoneName, VirtualBoneName);
				if (bAdded)
				{
					Skeleton->HandleSkeletonHierarchyChange();
				}
				else
				{
					UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Virtual bone between %s and %s already exists on Skeleton %s"), *SourceBoneName.ToString(), *TargetBoneName.ToString(), *Skeleton->GetName());
				}
			}
			else
			{
				if (!bSourceBoneExists)
				{
					UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Bone Name %s does not exist on Skeleton %s"), *SourceBoneName.ToString(), *Skeleton->GetName());
				}

				if (!bTargetBoneExists)
				{
					UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Bone Name %s does not exist on Skeleton %s"), *TargetBoneName.ToString(), *Skeleton->GetName());
				}			
			}		
		}
		else
		{
			UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("No Skeleton found for Animation Sequence %s"), *AnimationSequence->GetName());
		}
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for AddVirtualBone"));
	}
}

void UAnimationBlueprintLibrary::RemoveVirtualBone(const UAnimSequence* AnimationSequence, FName VirtualBoneName)
{
	if (AnimationSequence)
	{
		USkeleton* Skeleton = AnimationSequence->GetSkeleton();
		if (Skeleton)
		{
			if (DoesVirtualBoneNameExistInternal(Skeleton, VirtualBoneName))
			{
				TArray<FName> BoneNameArray;
				BoneNameArray.Add(VirtualBoneName);
				Skeleton->RemoveVirtualBones(BoneNameArray);
				Skeleton->HandleSkeletonHierarchyChange();
			}
			else
			{
				UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Virtual Bone Name %s already exists on Skeleton %s"), *VirtualBoneName.ToString(), *Skeleton->GetName());
			}
		}
		else
		{
			UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("No Skeleton found for Animation Sequence %s"), *AnimationSequence->GetName());
		}
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for RemoveVirtualBone"));
	}
}

void UAnimationBlueprintLibrary::RemoveVirtualBones(const UAnimSequence* AnimationSequence, TArray<FName> VirtualBoneNames)
{
	if (AnimationSequence)
	{
		USkeleton* Skeleton = AnimationSequence->GetSkeleton();
		if (Skeleton)
		{
			for (FName& VirtualBoneName : VirtualBoneNames)
			{
				if (!DoesVirtualBoneNameExistInternal(Skeleton, VirtualBoneName))
				{
					UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Virtual Bone Name %s already exists on Skeleton %s"), *VirtualBoneName.ToString(), *Skeleton->GetName());
				}
			}			

			Skeleton->RemoveVirtualBones(VirtualBoneNames);
			Skeleton->HandleSkeletonHierarchyChange();
		}
		else
		{
			UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("No Skeleton found for Animation Sequence %s"), *AnimationSequence->GetName());
		}
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for RemoveVirtualBones"));
	}
}

void UAnimationBlueprintLibrary::RemoveAllVirtualBones(const UAnimSequence* AnimationSequence)
{
	if (AnimationSequence)
	{
		USkeleton* Skeleton = AnimationSequence->GetSkeleton();
		if (Skeleton)
		{
			TArray<FName> VirtualBoneNames;
			for (const FVirtualBone& VirtualBone : Skeleton->VirtualBones)
			{
				VirtualBoneNames.Add(VirtualBone.VirtualBoneName);
			}

			RemoveVirtualBones(AnimationSequence, VirtualBoneNames);
		}
		else
		{
			UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("No Skeleton found for Animation Sequence %s"), *AnimationSequence->GetName());
		}
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for RemoveAllVirtualBones"));
	}
}

bool UAnimationBlueprintLibrary::DoesVirtualBoneNameExistInternal(USkeleton* Skeleton, FName BoneName)
{
	checkf(Skeleton != nullptr, TEXT("Invalid Skeleton ptr"));
	return Skeleton->VirtualBones.IndexOfByPredicate([&](const FVirtualBone& VirtualBone) { return VirtualBone.VirtualBoneName == BoneName; }) != INDEX_NONE;
}

void UAnimationBlueprintLibrary::GetSequenceLength(const UAnimSequence* AnimationSequence, float& Length)
{
	Length = 0.0f;
	if (AnimationSequence)
	{
		Length = AnimationSequence->SequenceLength;
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for GetSequenceLength"));
	}
}

void UAnimationBlueprintLibrary::GetRateScale(const UAnimSequence* AnimationSequence, float& RateScale)
{
	RateScale = 0.0f;
	if (AnimationSequence)
	{
		RateScale = AnimationSequence->RateScale;
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for GetRateScale"));
	}
}

void UAnimationBlueprintLibrary::SetRateScale(UAnimSequence* AnimationSequence, float RateScale)
{	
	if (AnimationSequence)
	{
		AnimationSequence->RateScale = RateScale;
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for SetRateScale"));
	}
}

void UAnimationBlueprintLibrary::GetFrameAtTime(const UAnimSequence* AnimationSequence, const float Time, int32& Frame)
{
	Frame = 0;
	if (AnimationSequence)
	{
		Frame = AnimationSequence->GetFrameAtTime(Time);
	}
	else
	{		
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for GetFrameAtTime"));
	}
}

void UAnimationBlueprintLibrary::GetTimeAtFrame(const UAnimSequence* AnimationSequence, const int32 Frame, float& Time)
{
	Time = 0.0f;
	if (AnimationSequence)
	{
		Time = GetTimeAtFrameInternal(AnimationSequence, Frame);
	}
	else
	{		
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for GetTimeAtFrame"));
	}
}

float UAnimationBlueprintLibrary::GetTimeAtFrameInternal(const UAnimSequence* AnimationSequence, const int32 Frame)
{
	return AnimationSequence->GetTimeAtFrame(Frame);
}

void UAnimationBlueprintLibrary::IsValidTime(const UAnimSequence* AnimationSequence, const float Time, bool& IsValid)
{
	IsValid = false;
	if (AnimationSequence)
	{
		IsValid = IsValidTimeInternal(AnimationSequence, Time);
	}
	else
	{		
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for IsValidTime"));
	}
}

bool UAnimationBlueprintLibrary::IsValidTimeInternal(const UAnimSequence* AnimationSequence, const float Time)
{
	return FMath::IsWithinInclusive(Time, 0.0f, AnimationSequence->SequenceLength);
}

void UAnimationBlueprintLibrary::FindBonePathToRoot(const UAnimSequence* AnimationSequence, FName BoneName, TArray<FName>& BonePath)
{
	BonePath.Empty();
	if (AnimationSequence)
	{
		BonePath.Add(BoneName);
		int32 BoneIndex = AnimationSequence->GetSkeleton()->GetReferenceSkeleton().FindRawBoneIndex(BoneName);		
		if (BoneIndex != INDEX_NONE)
		{
			while (BoneIndex != INDEX_NONE)
			{
				const int32 ParentBoneIndex = AnimationSequence->GetSkeleton()->GetReferenceSkeleton().GetRawParentIndex(BoneIndex);
				if (ParentBoneIndex != INDEX_NONE)
				{
					BonePath.Add(AnimationSequence->GetSkeleton()->GetReferenceSkeleton().GetBoneName(ParentBoneIndex));
				}

				BoneIndex = ParentBoneIndex;
			}
		}
		else
		{
			UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Bone name %s not found in Skeleton %s"), *BoneName.ToString(), *AnimationSequence->GetSkeleton()->GetName());
		}
	}
	else
	{
		UE_LOG(LogAnimationBlueprintLibrary, Warning, TEXT("Invalid Animation Sequence supplied for FindBonePathToRoot"));
	}
}

template void UAnimationBlueprintLibrary::AddCurveKeysInternal<float, FFloatCurve>(UAnimSequence* AnimationSequence, FName CurveName, const TArray<float>& Times, const TArray<float>& KeyData, ERawCurveTrackTypes CurveType);
template void UAnimationBlueprintLibrary::AddCurveKeysInternal<FVector, FVectorCurve>(UAnimSequence* AnimationSequence, FName CurveName, const TArray<float>& Times, const TArray<FVector>& KeyData, ERawCurveTrackTypes CurveType);
template void UAnimationBlueprintLibrary::AddCurveKeysInternal<FTransform, FTransformCurve>(UAnimSequence* AnimationSequence, FName CurveName, const TArray<float>& Times, const TArray<FTransform>& KeyData, ERawCurveTrackTypes CurveType);

template void UAnimationBlueprintLibrary::GetCurveKeysInternal<float, FFloatCurve>(UAnimSequence* AnimationSequence, FName CurveName, TArray<float>& Times, TArray<float>& KeyData, ERawCurveTrackTypes CurveType);
template void UAnimationBlueprintLibrary::GetCurveKeysInternal<FVector, FVectorCurve>(UAnimSequence* AnimationSequence, FName CurveName, TArray<float>& Times, TArray<FVector>& KeyData, ERawCurveTrackTypes CurveType);
template void UAnimationBlueprintLibrary::GetCurveKeysInternal<FTransform, FTransformCurve>(UAnimSequence* AnimationSequence, FName CurveName, TArray<float>& Times, TArray<FTransform>& KeyData, ERawCurveTrackTypes CurveType);

