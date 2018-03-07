// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "XRTrackingSystemBase.h"
#include "AppleARKitConfiguration.h"
#include "ARHitTestingSupport.h"
#include "ARTrackingQuality.h"
#include "AppleARKitHitTestResult.h"
#include "Kismet/BlueprintPlatformLibrary.h"

// ARKit
#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000
#import <ARKit/ARKit.h>
#include "AppleARKitSessionDelegate.h"
#endif // ARKIT_SUPPORT

//
//  FAppleARKitSystem
//

struct FAppleARKitFrame;

class FAppleARKitSystem : public FXRTrackingSystemBase, public IARHitTestingSupport, public IARTrackingQuality, public TSharedFromThis<FAppleARKitSystem, ESPMode::ThreadSafe>
{
	friend class FAppleARKitXRCamera;
	
public:
	FAppleARKitSystem();
	void Initialize();
	~FAppleARKitSystem();
	
	/** Thread safe anchor map getter */
	TMap< FGuid, UAppleARKitAnchor* > GetAnchors() const;
	
	//~ IXRTrackingSystem
	FName GetSystemName() const override;
	bool GetCurrentPose(int32 DeviceId, FQuat& OutOrientation, FVector& OutPosition) override;
	FString GetVersionString() const override;
	bool EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type) override;
	void RefreshPoses() override;
	void ResetOrientationAndPosition(float Yaw) override;
	bool IsHeadTrackingAllowed() const override;
	TSharedPtr<class IXRCamera, ESPMode::ThreadSafe> GetXRCamera(int32 DeviceId) override;
	float GetWorldToMetersScale() const override;
	//~ IXRTrackingSystem
	
	//~ IARHitTestingSupport
	//virtual bool ARLineTraceFromScreenPoint(const FVector2D ScreenPosition, TArray<FARHitTestResult>& OutHitResults) override;
	//~ IARHitTestingSupport
	
	//~ IARTrackingQuality
	virtual EARTrackingQuality ARGetTrackingQuality() const;
	//~ IARTrackingQuality
	
	// @todo arkit : this is for the blueprint library only; try to get rid of this method
	bool GetCurrentFrame(FAppleARKitFrame& OutCurrentFrame) const;

private:
	void Run();
	bool RunWithConfiguration(const FAppleARKitConfiguration& InConfiguration);
	bool IsRunning() const;
	bool Pause();
	void OrientationChanged(const int32 NewOrientation);
	
public:
	// Session delegate callbacks
	void SessionDidUpdateFrame_DelegateThread( TSharedPtr< FAppleARKitFrame, ESPMode::ThreadSafe > Frame );
	void SessionDidFailWithError_DelegateThread( const FString& Error );
#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000
	void SessionDidAddAnchors_DelegateThread( NSArray<ARAnchor*>* anchors );
	void SessionDidUpdateAnchors_DelegateThread( NSArray<ARAnchor*>* anchors );
	void SessionDidRemoveAnchors_DelegateThread( NSArray<ARAnchor*>* anchors );
#endif // ARKIT_SUPPORT

	
public:
	/**
	 * Searches the last processed frame for anchors corresponding to a point in the captured image.
	 *
	 * A 2D point in the captured image's coordinate space can refer to any point along a line segment
	 * in the 3D coordinate space. Hit-testing is the process of finding anchors of a frame located along this line segment.
	 *
	 * NOTE: The hit test locations are reported in ARKit space. For hit test results
	 * in game world coordinates, you're after UAppleARKitCameraComponent::HitTestAtScreenPosition
	 *
	 * @param ScreenPosition The viewport pixel coordinate of the trace origin.
	 */
	UFUNCTION( BlueprintCallable, Category="AppleARKit|Session" )
	bool HitTestAtScreenPosition( const FVector2D ScreenPosition, EAppleARKitHitTestResultType Types, TArray< FAppleARKitHitTestResult >& OutResults );
	
	
private:
	
	bool bIsRunning = false;
	
	void SetDeviceOrientation( EScreenOrientation::Type InOrientation );
	
	/** The orientation of the device; see EScreenOrientation */
	EScreenOrientation::Type DeviceOrientation;
	
	/** A rotation from ARKit TrackingSpace to Unreal Space. It is re-derived based on other parameters; users should not set it directly. */
	FRotator DerivedTrackingToUnrealRotation;
	
#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000

	// ARKit Session
	ARSession* Session = nullptr;
	
	// ARKit Session Delegate
	FAppleARKitSessionDelegate* Delegate = nullptr;
	
	/** The Metal texture cache for unbuffered texture uploads. */
	CVMetalTextureCacheRef MetalTextureCache = nullptr;
	
#endif // ARKIT_SUPPORT
	
	// Internal list of current known anchors
	mutable FCriticalSection AnchorsLock;
	UPROPERTY( Transient )
	TMap< FGuid, UAppleARKitAnchor* > Anchors;
	
	
	// The frame number when LastReceivedFrame was last updated
	uint32 GameThreadFrameNumber;

	//'threadsafe' sharedptrs merely guaranteee atomicity when adding/removing refs.  You can still have a race
	//with destruction and copying sharedptrs.
	FCriticalSection FrameLock;

	// Last frame grabbed & processed by via the ARKit session delegate
	TSharedPtr< FAppleARKitFrame, ESPMode::ThreadSafe > GameThreadFrame;
	TSharedPtr< FAppleARKitFrame, ESPMode::ThreadSafe > RenderThreadFrame;
	TSharedPtr< FAppleARKitFrame, ESPMode::ThreadSafe > LastReceivedFrame;
};


namespace AppleARKitSupport
{
	APPLEARKIT_API TSharedPtr<class FAppleARKitSystem, ESPMode::ThreadSafe> CreateAppleARKitSystem();
}

