// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AppleARKitSystem.h"
#include "DefaultXRCamera.h"
#include "AppleARKitSessionDelegate.h"
#include "ScopeLock.h"
#include "AppleARKitModule.h"
#include "AppleARKitTransform.h"
#include "AppleARKitVideoOverlay.h"
#include "AppleARKitFrame.h"
#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000
#include "IOSAppDelegate.h"
#endif
#include "ARHitTestingSupport.h"
#include "AppleARKitAnchor.h"
#include "AppleARKitPlaneAnchor.h"
#include "GeneralProjectSettings.h"
#include "IOSRuntimeSettings.h"

// For orientation changed
#include "Misc/CoreDelegates.h"

//
//  FAppleARKitXRCamera
//

class FAppleARKitXRCamera : public FDefaultXRCamera
{
public:
	FAppleARKitXRCamera(const FAutoRegister& AutoRegister, FAppleARKitSystem& InTrackingSystem, int32 InDeviceId)
	: FDefaultXRCamera( AutoRegister, &InTrackingSystem, InDeviceId )
	, ARKitSystem( InTrackingSystem )
	{}
	
private:
	//~ FDefaultXRCamera
	void OverrideFOV(float& InOutFOV)
	{
		// @todo arkit : is it safe not to lock here? Theoretically this should only be called on the game thread.
		ensure(IsInGameThread());
		if (ARKitSystem.GameThreadFrame.IsValid())
		{
			if (ARKitSystem.DeviceOrientation == EScreenOrientation::Portrait || ARKitSystem.DeviceOrientation == EScreenOrientation::PortraitUpsideDown)
			{
				// Portrait
				InOutFOV = ARKitSystem.GameThreadFrame->Camera.GetVerticalFieldOfViewForScreen(EAppleARKitBackgroundFitMode::Fill);
			}
			else
			{
				// Landscape
				InOutFOV = ARKitSystem.GameThreadFrame->Camera.GetHorizontalFieldOfViewForScreen(EAppleARKitBackgroundFitMode::Fill);
			}
		}
	}
	
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override
	{
		FDefaultXRCamera::SetupView(InViewFamily, InView);
	}
	
	virtual void SetupViewProjectionMatrix(FSceneViewProjectionData& InOutProjectionData) override
	{
		FDefaultXRCamera::SetupViewProjectionMatrix(InOutProjectionData);
	}
	
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override
	{
		FDefaultXRCamera::BeginRenderViewFamily(InViewFamily);
	}
	
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override
	{
		FDefaultXRCamera::PreRenderView_RenderThread(RHICmdList, InView);
	}
	
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override
	{
		// Grab the latest frame from ARKit
		{
			FScopeLock ScopeLock(&ARKitSystem.FrameLock);
			ARKitSystem.RenderThreadFrame = ARKitSystem.LastReceivedFrame;
		}
		
		// @todo arkit: Camera late update?
		
		if (ARKitSystem.RenderThreadFrame.IsValid())
		{
			VideoOverlay.UpdateVideoTexture_RenderThread(RHICmdList, *ARKitSystem.RenderThreadFrame, InViewFamily);
		}
		
		FDefaultXRCamera::PreRenderViewFamily_RenderThread(RHICmdList, InViewFamily);
	}
	
	virtual void PostRenderMobileBasePass_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override
	{
		VideoOverlay.RenderVideoOverlay_RenderThread(RHICmdList, InView, ARKitSystem.DeviceOrientation);
		
		FDefaultXRCamera::PostRenderMobileBasePass_RenderThread(RHICmdList, InView);
	}
	
	virtual bool IsActiveThisFrame(class FViewport* InViewport) const override
	{
#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000
		if ([IOSAppDelegate GetDelegate].OSVersion >= 11.0f)
		{
			return GetDefault<UGeneralProjectSettings>()->bStartInAR;
		}
		else
		{
			return false;
		}
#else
		return false;
#endif //ARKIT_SUPPORT
	}
	//~ FDefaultXRCamera
	
private:
	FAppleARKitSystem& ARKitSystem;
	FAppleARKitVideoOverlay VideoOverlay;
};




//
//  FAppleARKitSystem
//

FAppleARKitSystem::FAppleARKitSystem()
: DeviceOrientation(EScreenOrientation::Unknown)
, DerivedTrackingToUnrealRotation(FRotator::ZeroRotator)
{
	// See Initialize(), as we have access to SharedThis()
}

void FAppleARKitSystem::Initialize()
{
	// Register our ability to hit-test in AR with Unreal
	IModularFeatures::Get().RegisterModularFeature(IARHitTestingSupport::GetModularFeatureName(), static_cast<IARHitTestingSupport*>(this));
	IModularFeatures::Get().RegisterModularFeature(IARTrackingQuality::GetModularFeatureName(), static_cast<IARTrackingQuality*>(this));
	
	// Register for device orientation changes
	FCoreDelegates::ApplicationReceivedScreenOrientationChangedNotificationDelegate.AddThreadSafeSP(this, &FAppleARKitSystem::OrientationChanged);
	
	Run();
}

FAppleARKitSystem::~FAppleARKitSystem()
{
	// Unregister our ability to hit-test in AR with Unreal
	IModularFeatures::Get().UnregisterModularFeature(IARHitTestingSupport::GetModularFeatureName(), static_cast<IARHitTestingSupport*>(this));
	IModularFeatures::Get().UnregisterModularFeature(IARTrackingQuality::GetModularFeatureName(), static_cast<IARTrackingQuality*>(this));
}

TMap< FGuid, UAppleARKitAnchor* > FAppleARKitSystem::GetAnchors() const
{
	FScopeLock ScopeLock( &AnchorsLock );
	
	return Anchors;
}

FName FAppleARKitSystem::GetSystemName() const
{
	static const FName AppleARKitSystemName(TEXT("AppleARKit"));
	return AppleARKitSystemName;
}


bool FAppleARKitSystem::GetCurrentPose(int32 DeviceId, FQuat& OutOrientation, FVector& OutPosition)
{
	if (DeviceId == IXRTrackingSystem::HMDDeviceId && GameThreadFrame.IsValid())
	{
		// Do not have to lock here, because we are on the game
		// thread and GameThreadFrame is only written to from the game thread.
		OutOrientation = GameThreadFrame->Camera.Orientation * DerivedTrackingToUnrealRotation.Quaternion();
		OutPosition = GameThreadFrame->Camera.Translation;
		
		return true;
	}
	else
	{
		return false;
	}
}

FString FAppleARKitSystem::GetVersionString() const
{
	return TEXT("AppleARKit - V1.0");
}


bool FAppleARKitSystem::EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type)
{
	if (Type == EXRTrackedDeviceType::Any || Type == EXRTrackedDeviceType::HeadMountedDisplay)
	{
		static const int32 DeviceId = IXRTrackingSystem::HMDDeviceId;
		OutDevices.Add(DeviceId);
		return true;
	}
	return false;
}

FRotator DeriveTrackingToWorldRotation( EScreenOrientation::Type DeviceOrientation )
{
	// We rotate the camera to counteract the portrait vs. landscape viewport rotation
	FRotator DeviceRot = FRotator::ZeroRotator;
	switch (DeviceOrientation)
	{
		case EScreenOrientation::Portrait:
			DeviceRot = FRotator(0.0f, 0.0f, -90.0f);
			break;
			
		case EScreenOrientation::PortraitUpsideDown:
			DeviceRot = FRotator(0.0f, 0.0f, 90.0f);
			break;
			
		default:
		case EScreenOrientation::LandscapeLeft:
			break;
			
		case EScreenOrientation::LandscapeRight:
			DeviceRot = FRotator(0.0f, 0.0f, 180.0f);
			break;
	};
	
	return DeviceRot;
}

void FAppleARKitSystem::RefreshPoses()
{
	if (DeviceOrientation == EScreenOrientation::Unknown)
	{
		SetDeviceOrientation( static_cast<EScreenOrientation::Type>(FPlatformMisc::GetDeviceOrientation()) );
	}
	
	{
		FScopeLock ScopeLock( &FrameLock );
		GameThreadFrame = LastReceivedFrame;
	}
}


void FAppleARKitSystem::ResetOrientationAndPosition(float Yaw)
{
	// @todo arkit implement FAppleARKitSystem::ResetOrientationAndPosition
}

bool FAppleARKitSystem::IsHeadTrackingAllowed() const
{
#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000
	if ([IOSAppDelegate GetDelegate].OSVersion >= 11.0f)
	{
        return GetDefault<UGeneralProjectSettings>()->bStartInAR;
	}
	else
	{
		return false;
	}
#else
	return false;
#endif //ARKIT_SUPPORT
}

TSharedPtr<class IXRCamera, ESPMode::ThreadSafe> FAppleARKitSystem::GetXRCamera(int32 DeviceId)
{
	if (!XRCamera.IsValid())
	{
		TSharedRef<FAppleARKitXRCamera, ESPMode::ThreadSafe> NewCamera = FSceneViewExtensions::NewExtension<FAppleARKitXRCamera>(*this, DeviceId);
		XRCamera = NewCamera;
	}
	
	return XRCamera;
}

float FAppleARKitSystem::GetWorldToMetersScale() const
{
	// @todo arkit FAppleARKitSystem::GetWorldToMetersScale needs a real scale somehow
	return 100.0f;
}

//bool FAppleARKitSystem::ARLineTraceFromScreenPoint(const FVector2D ScreenPosition, TArray<FARHitTestResult>& OutHitResults)
//{
//	const bool bSuccess = HitTestAtScreenPosition(ScreenPosition, EAppleARKitHitTestResultType::ExistingPlaneUsingExtent, OutHitResults);
//	return bSuccess;
//}

EARTrackingQuality FAppleARKitSystem::ARGetTrackingQuality() const
{
	return GameThreadFrame.IsValid()
		? GameThreadFrame->Camera.TrackingQuality
		: EARTrackingQuality::NotAvailable;
}

bool FAppleARKitSystem::GetCurrentFrame(FAppleARKitFrame& OutCurrentFrame) const
{
	if( GameThreadFrame.IsValid() )
	{
		OutCurrentFrame = *GameThreadFrame;
		return true;
	}
	else
	{
		return false;
	}
}


#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000
// @todo arkit : are the default params OK?
FARHitTestResult ToARHitTestResult( ARHitTestResult* InARHitTestResult, class UAppleARKitAnchor* InAnchor = nullptr, float WorldToMetersScale = 100.0f )
{
	// Sanity check
	check( InARHitTestResult );
	
	FARHitTestResult Result;
	
	// Convert properties
	// @todo arkit Result.Type = ToEAppleARKitHitTestResultType( InARHitTestResult.type );
	Result.Distance = InARHitTestResult.distance * WorldToMetersScale;
	Result.Transform = FAppleARKitTransform::ToFTransform( InARHitTestResult.worldTransform, WorldToMetersScale );
	// @todo arkit Anchor = InAnchor;
	
	return Result;
}
#endif//ARKIT_SUPPORT


bool FAppleARKitSystem::HitTestAtScreenPosition(const FVector2D ScreenPosition, EAppleARKitHitTestResultType InTypes, TArray< FAppleARKitHitTestResult >& OutResults)
{
	// Sanity check
	if (!IsRunning())
	{
		return false;
	}
	
	// Clear the HitResults
	OutResults.Empty();
	
#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000
	
	@autoreleasepool {
		
		// Perform a hit test on the Session's last frame
		ARFrame* HitTestFrame = Session.currentFrame;
		if (!HitTestFrame)
		{
			return false;
		}
		
		// Convert the screen position to normalised coordinates in the capture image space
		FVector2D NormalizedImagePosition = FAppleARKitCamera( HitTestFrame.camera ).GetImageCoordinateForScreenPosition( ScreenPosition, EAppleARKitBackgroundFitMode::Fill );
		switch (DeviceOrientation)
		{
			case EScreenOrientation::Portrait:
				NormalizedImagePosition = FVector2D( NormalizedImagePosition.Y, 1.0f - NormalizedImagePosition.X );
				break;
				
			case EScreenOrientation::PortraitUpsideDown:
				NormalizedImagePosition = FVector2D( 1.0f - NormalizedImagePosition.Y, NormalizedImagePosition.X );
				break;
				
			default:
			case EScreenOrientation::LandscapeLeft:
				break;
				
			case EScreenOrientation::LandscapeRight:
				NormalizedImagePosition = FVector2D(1.0f, 1.0f) - NormalizedImagePosition;
				break;
		};
		
		// GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString::Printf(TEXT("Hit Test At Screen Position: x: %f, y: %f"), NormalizedImagePosition.X, NormalizedImagePosition.Y));
		
		// Convert the types flags
		//ARHitTestResultType Types = ToARHitTestResultType( InTypes );
		
		// First run hit test against existing planes with extents (converting & filtering results as we go)
		NSArray< ARHitTestResult* >* PlaneHitTestResults = [HitTestFrame hitTest:CGPointMake(NormalizedImagePosition.X, NormalizedImagePosition.Y) types:ARHitTestResultTypeExistingPlaneUsingExtent];
		for ( ARHitTestResult* HitTestResult in PlaneHitTestResults )
		{
			// Convert to Unreal's Hit Test result format
			FAppleARKitHitTestResult OutResult( HitTestResult );
			
			// Skip results further than 5m or closer that 20cm from camera
			if (OutResult.Distance > 500.0f || OutResult.Distance < 20.0f)
			{
				continue;
			}
			
			// Apply BaseTransform
			// @todo arkit OutResult.Transform *= BaseTransform;
			
			// Hit result has passed and above filtering, add it to the list
			OutResults.Add( OutResult );
		}
		
		// If there were no valid results, fall back to hit testing against one shot plane
		if (!OutResults.Num())
		{
			PlaneHitTestResults = [HitTestFrame hitTest:CGPointMake(NormalizedImagePosition.X, NormalizedImagePosition.Y) types:ARHitTestResultTypeEstimatedHorizontalPlane];
			for ( ARHitTestResult* HitTestResult in PlaneHitTestResults )
			{
				// Convert to Unreal's Hit Test result format
				FAppleARKitHitTestResult OutResult( HitTestResult );
				
				// Skip results further than 5m or closer that 20cm from camera
				if (OutResult.Distance > 500.0f || OutResult.Distance < 20.0f)
				{
					continue;
				}
				
				// Apply BaseTransform
				// @todo arkit OutResult.Transform *= BaseTransform;
				
				// Hit result has passed and above filtering, add it to the list
				OutResults.Add( OutResult );
			}
		}
		
		// If there were no valid results, fall back further to hit testing against feature points
		if (!OutResults.Num())
		{
			// GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("No results for plane hit test - reverting to feature points"), NormalizedImagePosition.X, NormalizedImagePosition.Y));
			
			NSArray< ARHitTestResult* >* FeatureHitTestResults = [HitTestFrame hitTest:CGPointMake(NormalizedImagePosition.X, NormalizedImagePosition.Y) types:ARHitTestResultTypeFeaturePoint];
			for ( ARHitTestResult* HitTestResult in FeatureHitTestResults )
			{
				// Convert to Unreal's Hit Test result format
				FAppleARKitHitTestResult OutResult( HitTestResult );
				
				// Skip results further than 5m or closer that 20cm from camera
				if (OutResult.Distance > 500.0f || OutResult.Distance < 20.0f)
				{
					continue;
				}
				
				// Apply BaseTransform
				// @todo arkit OutResult.Transform *= BaseTransform;
				
				// Hit result has passed and above filtering, add it to the list
				OutResults.Add( OutResult );
			}
		}
		
		// if (!OutResults.Num())
		// {
		// 	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("No results for feature points either!"), NormalizedImagePosition.X, NormalizedImagePosition.Y));
		// }
		// else
		// {
		// 	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("Found %d hit results, first at a distance of %fcm"), OutResults[0].Distance));
		// }
	}
	
#endif // ARKIT_SUPPORT
	
	return (OutResults.Num() > 0);
}

static TOptional<EScreenOrientation::Type> PickAllowedDeviceOrientation( EScreenOrientation::Type InOrientation )
{
#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000
	const UIOSRuntimeSettings* IOSSettings = GetDefault<UIOSRuntimeSettings>();
	
	const bool bOrientationSupported[] =
	{
		true, // Unknown
		IOSSettings->bSupportsPortraitOrientation != 0, // Portait
		IOSSettings->bSupportsUpsideDownOrientation != 0, // PortraitUpsideDown
		IOSSettings->bSupportsLandscapeRightOrientation != 0, // LandscapeLeft; These are flipped vs the enum name?
		IOSSettings->bSupportsLandscapeLeftOrientation != 0, // LandscapeRight; These are flipped vs the enum name?
		false, // FaceUp
		false // FaceDown
	};
	
	if (bOrientationSupported[static_cast<int32>(InOrientation)])
	{
		return InOrientation;
	}
	else
	{
		return TOptional<EScreenOrientation::Type>();
	}
#else
	return TOptional<EScreenOrientation::Type>();
#endif
}

void FAppleARKitSystem::SetDeviceOrientation( EScreenOrientation::Type InOrientation )
{
	TOptional<EScreenOrientation::Type> NewOrientation = PickAllowedDeviceOrientation(InOrientation);

	if (!NewOrientation.IsSet() && DeviceOrientation == EScreenOrientation::Unknown)
	{
		// We do not currently have a valid orientation, nor did the device provide one.
		// So pick ANY ALLOWED default.
		// This only realy happens if the device is face down on something or
		// in another "useless" state for AR.
		
		if (!NewOrientation.IsSet())
		{
			NewOrientation = PickAllowedDeviceOrientation(EScreenOrientation::Portrait);
		}
		
		if (!NewOrientation.IsSet())
		{
			NewOrientation = PickAllowedDeviceOrientation(EScreenOrientation::LandscapeLeft);
		}
		
		if (!NewOrientation.IsSet())
		{
			NewOrientation = PickAllowedDeviceOrientation(EScreenOrientation::PortraitUpsideDown);
		}
		
		if (!NewOrientation.IsSet())
		{
			NewOrientation = PickAllowedDeviceOrientation(EScreenOrientation::LandscapeRight);
		}
		
		check(NewOrientation.IsSet());
	}
	
	if (NewOrientation.IsSet() && DeviceOrientation != NewOrientation.GetValue())
	{
		DeviceOrientation = NewOrientation.GetValue();
		DerivedTrackingToUnrealRotation = DeriveTrackingToWorldRotation( DeviceOrientation );
	}
}


void FAppleARKitSystem::Run()
{
	// @todo arkit FAppleARKitSystem::GetWorldToMetersScale needs a real scale somehow
	FAppleARKitConfiguration Config;
	RunWithConfiguration( Config );
}

bool FAppleARKitSystem::RunWithConfiguration(const FAppleARKitConfiguration& InConfiguration)
{

	if (IsRunning())
	{
		UE_LOG(LogAppleARKit, Log, TEXT("Session already running"), this);

		return true;
	}

#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000
	if ([IOSAppDelegate GetDelegate].OSVersion >= 11.0f)
	{

		ARSessionRunOptions options = 0;

		// Create our ARSessionDelegate
		if (Delegate == nullptr)
		{
			Delegate = [[FAppleARKitSessionDelegate alloc] initWithAppleARKitSystem:this];
		}

		if (Session == nullptr)
		{
			// Start a new ARSession
			Session = [ARSession new];
			Session.delegate = Delegate;
			Session.delegateQueue = dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0);
		}
		else
		{
			// pause and start with new options
			options = ARSessionRunOptionResetTracking | ARSessionRunOptionRemoveExistingAnchors;
			[Session pause];
		}

		// Create MetalTextureCache
		if (IsMetalPlatform(GMaxRHIShaderPlatform))
		{
			id<MTLDevice> Device = (id<MTLDevice>)GDynamicRHI->RHIGetNativeDevice();
			check(Device);

			CVReturn Return = CVMetalTextureCacheCreate(nullptr, nullptr, Device, nullptr, &MetalTextureCache);
			check(Return == kCVReturnSuccess);
			check(MetalTextureCache);

			// Pass to session delegate to use for Metal texture creation
			[Delegate setMetalTextureCache : MetalTextureCache];
		}

		// Convert to native ARWorldTrackingSessionConfiguration
		ARConfiguration* Configuration = FAppleARKitConfiguration::ToARConfiguration(InConfiguration);

		UE_LOG(LogAppleARKit, Log, TEXT("Starting session: %p with options %d"), this, options);

		// Start the session with the configuration
		[Session runWithConfiguration : Configuration options : options];
	}
	
#endif // ARKIT_SUPPORT
	
	// @todo arkit Add support for relocating ARKit space to Unreal World Origin? BaseTransform = FTransform::Identity;
	
	// Set running state
	bIsRunning = true;
	
	return true;
}

bool FAppleARKitSystem::IsRunning() const
{
	return bIsRunning;
}

bool FAppleARKitSystem::Pause()
{
	// Already stopped?
	if (!IsRunning())
	{
		return true;
	}
	
	UE_LOG(LogAppleARKit, Log, TEXT("Stopping session: %p"), this);
	
#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000
	if ([IOSAppDelegate GetDelegate].OSVersion >= 11.0f)
	{
		// Suspend the session
		[Session pause];
	
		// Release MetalTextureCache created in Start
		if (MetalTextureCache)
		{
			// Tell delegate to release it
			[Delegate setMetalTextureCache:nullptr];
		
			CFRelease(MetalTextureCache);
			MetalTextureCache = nullptr;
		}
	}
	
#endif // ARKIT_SUPPORT
	
	// Set running state
	bIsRunning = false;
	
	return true;
}

void FAppleARKitSystem::OrientationChanged(const int32 NewOrientationRaw)
{
	const EScreenOrientation::Type NewOrientation = static_cast<EScreenOrientation::Type>(NewOrientationRaw);
	SetDeviceOrientation(NewOrientation);
}
						
void FAppleARKitSystem::SessionDidUpdateFrame_DelegateThread(TSharedPtr< FAppleARKitFrame, ESPMode::ThreadSafe > Frame)
{
	// Thread safe swap buffered frame
	FScopeLock ScopeLock(&FrameLock);
	LastReceivedFrame = Frame;
}
			
void FAppleARKitSystem::SessionDidFailWithError_DelegateThread(const FString& Error)
{
	UE_LOG(LogAppleARKit, Warning, TEXT("Session failed with error: %s"), *Error);
}

#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000

FORCEINLINE void ToFGuid( uuid_t UUID, FGuid& OutGuid )
{
	// Set FGuid parts
	OutGuid.A = *(uint32*)UUID;
	OutGuid.B = *((uint32*)UUID)+1;
	OutGuid.C = *((uint32*)UUID)+2;
	OutGuid.D = *((uint32*)UUID)+3;
}

FORCEINLINE void ToFGuid( NSUUID* Identifier, FGuid& OutGuid )
{
	// Get bytes
	uuid_t UUID;
	[Identifier getUUIDBytes:UUID];
	
	// Set FGuid parts
	ToFGuid( UUID, OutGuid );
}

void FAppleARKitSystem::SessionDidAddAnchors_DelegateThread( NSArray<ARAnchor*>* anchors )
{
	FScopeLock ScopeLock( &AnchorsLock );

	for (ARAnchor* anchor in anchors)
	{
		// Construct appropriate UAppleARKitAnchor subclass
		UAppleARKitAnchor* Anchor;
		if ([anchor isKindOfClass:[ARPlaneAnchor class]])
		{
			Anchor = NewObject< UAppleARKitPlaneAnchor >();
		}
		else
		{
			Anchor = NewObject< UAppleARKitAnchor >();
		}

		// Set UUID
		ToFGuid( anchor.identifier, Anchor->Identifier );

		// Update fields
		Anchor->Update_DelegateThread( anchor );

		// Map to UUID
		Anchors.Add( Anchor->Identifier, Anchor );
	}
}

void FAppleARKitSystem::SessionDidUpdateAnchors_DelegateThread( NSArray<ARAnchor*>* anchors )
{
	FScopeLock ScopeLock( &AnchorsLock );

	for (ARAnchor* anchor in anchors)
	{
		// Convert to FGuid
		FGuid Identifier;
		ToFGuid( anchor.identifier, Identifier );


		// Lookup in map
		if ( UAppleARKitAnchor** Anchor = Anchors.Find( Identifier ) )
		{
			// Update fields
			(*Anchor)->Update_DelegateThread( anchor );
		}
	}
}

void FAppleARKitSystem::SessionDidRemoveAnchors_DelegateThread( NSArray<ARAnchor*>* anchors )
{
	FScopeLock ScopeLock( &AnchorsLock );

	for (ARAnchor* anchor in anchors)
	{
		// Convert to FGuid
		FGuid Identifier;
		ToFGuid( anchor.identifier, Identifier );

		// Remove from map (allowing anchor to be garbage collected)
		Anchors.Remove( Identifier );
	}
}

#endif //ARKIT_SUPPORT






namespace AppleARKitSupport
{
	TSharedPtr<class FAppleARKitSystem, ESPMode::ThreadSafe> CreateAppleARKitSystem()
	{
#if ARKIT_SUPPORT && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000
		const bool bIsARApp = GetDefault<UGeneralProjectSettings>()->bStartInAR;
		if (bIsARApp)
		{
			auto NewARKitSystem = TSharedPtr<class FAppleARKitSystem, ESPMode::ThreadSafe>(new FAppleARKitSystem());
			NewARKitSystem->Initialize();
			return NewARKitSystem;;
		}
#endif
		return TSharedPtr<class FAppleARKitSystem, ESPMode::ThreadSafe>();
	}
}
