// Copyright 2017 Google Inc.

#pragma once
#include "GoogleARCorePrimitives.h"

#include "GoogleARCoreAnchor.generated.h"

#define ENABLE_GOOGLEARANCHOR_DEBUG_LOG 0
DEFINE_LOG_CATEGORY_STATIC(LogGoogleARAnchor, Log, All);

/**
 * @ingroup GoogleARCoreBase
 * Describes the state of a GoogleARCoreAnchor's pose.
 */
UENUM(BlueprintType)
enum class EGoogleARCoreAnchorTrackingState : uint8
{
	/* ARCore is tracking this Anchor and its pose is current. */
	Tracking,
	/**
	 * ARCore is not currently tracking this Anchor, but may resume tracking it in the future. This
	 * can happen if device tracking is lost or if the user enters a new space. When in this state
	 * the pose of the anchor may be wildly inaccurate and should generally not be used.
	 */
	NotCurrentlyTracking,
	/**
	 * ARCore has stopped tracking this Anchor and will never resume tracking it.  This happens
	 * either because the anchor was created when the device's tracking state was diminished and
	 * then lost, or because it was removed by calling UGoogleARCoreSessionFunctionLibrary::RemoveGoogleARAnchorObject
	 */
	StoppedTracking
};

/**
 * The abstract base class of any GoogleARCoreAnchor object.
 */
UCLASS(abstract)
class GOOGLEARCOREBASE_API UGoogleARCoreAnchorBase : public UObject
{
	GENERATED_BODY()
public:
	UGoogleARCoreAnchorBase()
	{
		TrackingState = EGoogleARCoreAnchorTrackingState::NotCurrentlyTracking;
	};

	/** Returns a unique identifier of this anchor object.*/
	UFUNCTION(BlueprintPure, Category = "GoogleARAnchor")
	FString GetARAnchorId()
	{
		return ARAnchorId;
	};

	/**
	 * Returns the current state of the pose of this anchor object. If this
	 * state is anything but <code>Tracking</code> the
	 * pose may be dramatically incorrect.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleARAnchor")
	EGoogleARCoreAnchorTrackingState GetTrackingState()
	{
		return TrackingState;
	};

	/**
	 * Returns the pose of the anchor in Unreal world space. This pose
	 * should only be considered valid if GetTrackingState() returns
	 * <code>Tracking</code>.
	 */
	UFUNCTION(BlueprintPure, Category = "GoogleARAnchor")
	FGoogleARCorePose GetLatestPose()
	{
		return LatestPose;
	};

protected:
	/** The unique identifier of this anchor object. */
	FString ARAnchorId;
	/** The anchor's latest pose in Unreal world space. */
	FGoogleARCorePose LatestPose;
	/** The anchor's current tracking state. */
	EGoogleARCoreAnchorTrackingState TrackingState;

	friend class UGoogleARCoreAnchorManager;
};

/**
 * A UObject that describes a fixed location and orientation in the real world.
 * To stay at a fixed location in physical space, the numerical description of this position will update
 * as ARCore's understanding of the space improves. Use GetLatestPose() to get the latest updated numerical
 * location of this anchor.
 */
UCLASS(BlueprintType)
class GOOGLEARCOREBASE_API UGoogleARCoreAnchor : public UGoogleARCoreAnchorBase
{
	GENERATED_BODY()
public:
	UGoogleARCoreAnchor();

	/** Returns the ARCore timestamp when this anchor object was created. */
	UFUNCTION(BlueprintPure, Category = "GoogleARAnchor")
	FGoogleARCoreTimestamp GetARAnchorCreationTimestamp();

private:
	FTransform RelativeTransformToARDevicePose;
	FGoogleARCorePose LatestARAnchorDevicePose;

	friend class UGoogleARCoreAnchorManager;
	friend class UGoogleARCoreSessionFunctionLibrary;

	void InitARAnchorPose(const FTransform& AnchorWorldTransform, const FGoogleARCorePose& CurrentCameraPose);
	void UpdatePose(FGoogleARCorePose NewAnchorCameraPose, FGoogleARCoreTimestamp CurrentTimestamp);
};
