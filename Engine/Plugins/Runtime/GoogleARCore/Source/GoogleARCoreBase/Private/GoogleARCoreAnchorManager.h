// Copyright 2017 Google Inc.

#pragma once
#include "GoogleARCoreAnchor.h"
#include "GoogleARCoreAnchorManager.generated.h"

UCLASS()
class UGoogleARCoreAnchorManager : public UObject
{
	GENERATED_BODY()

public:

	// Add the ARAnchor object to the anchor manager to get updated
	UGoogleARCoreAnchor* AddARAnchor(const FTransform& ARAnchorWorldTransform);
	// Remove the ARAnchor object for update
	void RemoveARAnchor(UGoogleARCoreAnchorBase* ARAnchorObject);
	// Update the ARAnchor pose. Called from the FGoogleARCoreDevice::OnWorldTickStart
	void UpdateARAnchors(bool bIsTracking, bool bIsRelocalized, double EarliestTimestamp);

	// Tracking session start callback
	void OnTrackingSessionStarted();

	//  Tracking session end callback
	void OnTrackingSessionEnded();

private:
	UPROPERTY()
	TMap<FString, UGoogleARCoreAnchor*> ARAnchorMap;
	UPROPERTY()
	TArray<UGoogleARCoreAnchor*> AnchorsNotLocalizedSinceReset;

	bool bIsTracking;
	bool bIsRelocalized;
};
