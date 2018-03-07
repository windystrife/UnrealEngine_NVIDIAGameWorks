// Copyright 2017 Google Inc.
#include "GoogleARCoreAnchorActor.h"
#include "GoogleARCoreFunctionLibrary.h"

void AGoogleARCoreAnchorActor::Tick(float DeltaSeconds)
{
	if (bTrackingEnabled && ARAnchorObject != nullptr && ARAnchorObject->GetTrackingState() == EGoogleARCoreAnchorTrackingState::Tracking)
	{
		SetActorTransform(ARAnchorObject->GetLatestPose().Pose);
	}

	if ((bHideWhenNotCurrentlyTracking || bDestroyWhenStoppedTracking) && ARAnchorObject != nullptr)
	{
		switch (ARAnchorObject->GetTrackingState())
		{
		case EGoogleARCoreAnchorTrackingState::Tracking:
			SetActorHiddenInGame(false);
			break;
		case EGoogleARCoreAnchorTrackingState::NotCurrentlyTracking:
			SetActorHiddenInGame(bHideWhenNotCurrentlyTracking);
			break;
		case EGoogleARCoreAnchorTrackingState::StoppedTracking:
			if (bDestroyWhenStoppedTracking)
			{
				Destroy();
			}
			else
			{
				SetActorHiddenInGame(bHideWhenNotCurrentlyTracking);
			}
			break;
		default:
			// This case should never be reached, do nothing.
			break;
		}
	}

	Super::Tick(DeltaSeconds);
}

void AGoogleARCoreAnchorActor::BeginDestroy()
{
	if (bRemoveAnchorObjectWhenDestroyed && ARAnchorObject)
	{
		UGoogleARCoreSessionFunctionLibrary::RemoveGoogleARAnchorObject(ARAnchorObject);
	}

	Super::BeginDestroy();
}

void AGoogleARCoreAnchorActor::SetARAnchor(UGoogleARCoreAnchorBase* InARAnchorObject)
{
	if (ARAnchorObject != nullptr && bRemoveAnchorObjectWhenAnchorChanged)
	{
		UGoogleARCoreSessionFunctionLibrary::RemoveGoogleARAnchorObject(ARAnchorObject);
	}

	ARAnchorObject = InARAnchorObject;
	SetActorTransform(ARAnchorObject->GetLatestPose().Pose);
}

UGoogleARCoreAnchorBase* AGoogleARCoreAnchorActor::GetARAnchor()
{
	return ARAnchorObject;
}