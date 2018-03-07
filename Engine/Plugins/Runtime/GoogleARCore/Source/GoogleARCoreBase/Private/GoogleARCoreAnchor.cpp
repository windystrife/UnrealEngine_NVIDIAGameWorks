// Copyright 2017 Google Inc.
#include "GoogleARCoreAnchor.h"

// UGoogleARCoreAnchor
UGoogleARCoreAnchor::UGoogleARCoreAnchor()
{
	ARAnchorId = FGuid::NewGuid().ToString();
}

void UGoogleARCoreAnchor::InitARAnchorPose(const FTransform& ARAnchorWorldTransform, const FGoogleARCorePose& CurrentDevicePose)
{
	LatestARAnchorDevicePose = CurrentDevicePose;
	RelativeTransformToARDevicePose = ARAnchorWorldTransform.GetRelativeTransform(LatestARAnchorDevicePose.Pose);
	LatestPose.Pose = ARAnchorWorldTransform;
	LatestPose.Timestamp = LatestARAnchorDevicePose.Timestamp;
	TrackingState = EGoogleARCoreAnchorTrackingState::Tracking;

#if ENABLE_GOOGLEARANCHOR_DEBUG_LOG
	UE_LOG(LogGoogleARAnchor, Log, TEXT("Creating ARAnchor id:%s at transform: %s "), *ARAnchorId, *LatestPose.Pose.ToString());
#endif
}

FGoogleARCoreTimestamp UGoogleARCoreAnchor::GetARAnchorCreationTimestamp()
{
	return LatestARAnchorDevicePose.Timestamp;
}

void UGoogleARCoreAnchor::UpdatePose(FGoogleARCorePose NewARAnchorCameraPose, FGoogleARCoreTimestamp CurrentTimestamp)
{
#if ENABLE_GOOGLEARANCHOR_DEBUG_LOG
	UE_LOG(LogGoogleARAnchor, Log, TEXT("Anchor id=%s got updated! NewARAnchorCameraPose:%s, OldAnchorCameraPose:%s"), *ARAnchorId, *NewARAnchorCameraPose.Pose.ToString(), *LatestARAnchorDevicePose.Pose.ToString());
#endif
	// Camera pose at the anchor creation timestamp has been updated.
	// Recalculate anchor pose.
	LatestARAnchorDevicePose = NewARAnchorCameraPose;
	LatestPose.Pose = RelativeTransformToARDevicePose * LatestARAnchorDevicePose.Pose;
	LatestPose.Timestamp = CurrentTimestamp;
	TrackingState = EGoogleARCoreAnchorTrackingState::Tracking;

#if ENABLE_GOOGLEARANCHOR_DEBUG_LOG
	UE_LOG(LogGoogleARAnchor, Log, TEXT("Anchor id=%s got updated! New Transform:%s"), *ARAnchorId, *LatestPose.Pose.ToString());
#endif
}

