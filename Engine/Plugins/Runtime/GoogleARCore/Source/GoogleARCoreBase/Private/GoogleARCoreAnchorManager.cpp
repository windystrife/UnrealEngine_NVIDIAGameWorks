// Copyright 2017 Google Inc.
#include "GoogleARCoreAnchorManager.h"
#include "GoogleARCoreDevice.h"

void UGoogleARCoreAnchorManager::OnTrackingSessionStarted()
{
	bIsTracking = false;
	bIsRelocalized = false;
}

void UGoogleARCoreAnchorManager::OnTrackingSessionEnded()
{
	// Mark the Anchor Object as stopped tracking, since other UObject could be referencing it.
	for (auto& Elem : ARAnchorMap)
	{
		if (!Elem.Value->IsPendingKillOrUnreachable())
		{
			Elem.Value->TrackingState = EGoogleARCoreAnchorTrackingState::StoppedTracking;
		}
	}

	ARAnchorMap.Empty();
	AnchorsNotLocalizedSinceReset.Empty();
}

UGoogleARCoreAnchor* UGoogleARCoreAnchorManager::AddARAnchor(const FTransform& ARAnchorWorldTransform)
{
	// The camera pose used for anchor need to ignore device orientation
	FGoogleARCorePose DevicePoseWithoutRotation;
	double CurrentPoseTimestamp = FGoogleARCoreDevice::GetInstance()->TangoMotionManager.GetCurrentPoseTimestamp().TimestampValue;
	bool bHasValidPose = FGoogleARCoreDevice::GetInstance()->TangoMotionManager.GetPoseAtTime(EGoogleARCoreReferenceFrame::DEVICE, CurrentPoseTimestamp, DevicePoseWithoutRotation, true);

	if (!bHasValidPose)
	{
		return nullptr;
	}

	UGoogleARCoreAnchor*  OutARAnchorObject = NewObject<UGoogleARCoreAnchor>();
	OutARAnchorObject->InitARAnchorPose(ARAnchorWorldTransform, DevicePoseWithoutRotation);

	// We need to track the anchor that we created while not localized.
	if (!bIsRelocalized)
	{
		AnchorsNotLocalizedSinceReset.Add(OutARAnchorObject);
	}

#if ENABLE_GOOGLEARANCHOR_DEBUG_LOG
	UE_LOG(LogGoogleARAnchor, Log, TEXT("ARAnchor Id:%s create and add from GoogleARAnchorManager!"), *OutARAnchorObject->GetARAnchorId());
#endif

	ARAnchorMap.Add(OutARAnchorObject->GetARAnchorId(), OutARAnchorObject);

	return OutARAnchorObject;
}

void UGoogleARCoreAnchorManager::RemoveARAnchor(UGoogleARCoreAnchorBase* ARAnchor)
{
	FString Id = ARAnchor->GetARAnchorId();
	if (ARAnchor->IsA(UGoogleARCoreAnchor::StaticClass()))
	{
#if ENABLE_GOOGLEARANCHOR_DEBUG_LOG
		UE_LOG(LogGoogleARAnchor, Log, TEXT("ARAnchor Id:%s removed from GoogleARAnchorManager!"), *ARAnchor->GetARAnchorId());
#endif
		if (ARAnchorMap.Contains(Id))
		{
			AnchorsNotLocalizedSinceReset.Remove(ARAnchorMap[Id]);
			ARAnchorMap[Id]->TrackingState = EGoogleARCoreAnchorTrackingState::StoppedTracking;
			ARAnchorMap.Remove(Id);
		}
	}
}

void UGoogleARCoreAnchorManager::UpdateARAnchors(bool bInIsTracking, bool bInIsRelocalized, double EarliestTimestamp)
{
	for (auto& Elem : ARAnchorMap)
	{
		if (Elem.Value->IsPendingKillOrUnreachable())
		{
			ARAnchorMap.Remove(Elem.Key);
		}
	}

	bool bIsCOMUpdated = EarliestTimestamp > 0;
	bool bIsTrackingLost = bIsTracking && !bInIsTracking;
	bool bRelocalized = !bIsRelocalized && bInIsRelocalized;

	bIsTracking = bInIsTracking;
	bIsRelocalized = bInIsRelocalized;

	if (!bIsCOMUpdated && !bIsTrackingLost && !bRelocalized)
	{
		// No need to update if the tracking state doesn't change.
		return;
	}

	TArray<UGoogleARCoreAnchor*> AllAnchors;
	ARAnchorMap.GenerateValueArray(AllAnchors);
	FGoogleARCoreTimestamp CurrentARCoreTimestamp = FGoogleARCoreDevice::GetInstance()->TangoMotionManager.GetCurrentPoseTimestamp();

	if (bIsCOMUpdated)
	{
#if ENABLE_GOOGLEARANCHOR_DEBUG_LOG
		UE_LOG(LogGoogleARAnchor, Log, TEXT("GoogleARAnchorManager update anchor due to map updated!"));
#endif
		for (UGoogleARCoreAnchor* ARAnchor : AllAnchors)
		{
			double ARAnchorTimestamp = ARAnchor->GetARAnchorCreationTimestamp().TimestampValue;
			if (ARAnchorTimestamp < EarliestTimestamp)
			{
				continue;
			}

			FGoogleARCorePose LatestARAnchorDevicePose;
			bool bHasValidPose = FGoogleARCoreDevice::GetInstance()->TangoMotionManager.GetPoseAtTime(EGoogleARCoreReferenceFrame::DEVICE, ARAnchorTimestamp, LatestARAnchorDevicePose, true);
			if (bHasValidPose)
			{
				ARAnchor->UpdatePose(LatestARAnchorDevicePose, CurrentARCoreTimestamp);
			}
			else
			{
				ARAnchor->TrackingState = EGoogleARCoreAnchorTrackingState::NotCurrentlyTracking;
			}
		}
	}

	if (bIsTrackingLost)
	{
		for (UGoogleARCoreAnchor* UnlocalizedAnchor : AnchorsNotLocalizedSinceReset)
		{
			// Avoid invalid anchor object
			if (UnlocalizedAnchor->IsPendingKillOrUnreachable())
			{
				continue;
			}

			// Remove the anchor that ARCore is not gonna tracked again
			RemoveARAnchor(UnlocalizedAnchor);
		}
#if ENABLE_GOOGLEARANCHOR_DEBUG_LOG
		UE_LOG(LogGoogleARAnchor, Log, TEXT("GoogleARAnchorManager Track lost! Anchors that is not localized will be deleted."));
#endif

		AnchorsNotLocalizedSinceReset.Empty();
		ARAnchorMap.GenerateValueArray(AllAnchors);
		for (UGoogleARCoreAnchor* ARAnchor : AllAnchors)
		{
			ARAnchor->TrackingState = EGoogleARCoreAnchorTrackingState::NotCurrentlyTracking;
		}
	}
	else if (bRelocalized)
	{
#if ENABLE_GOOGLEARANCHOR_DEBUG_LOG
		UE_LOG(LogGoogleARAnchor, Log, TEXT("GoogleARAnchorManager Relocalized!"));
#endif
		// All the non localized anchors can now be updated after a reset. Remove them from the list.
		AnchorsNotLocalizedSinceReset.Empty();
		for (UGoogleARCoreAnchor* ARAnchor : AllAnchors)
		{
			ARAnchor->TrackingState = EGoogleARCoreAnchorTrackingState::Tracking;
		}
	}
}
