// Copyright 2017 Google Inc.

#include "GoogleVRHMD.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"
#include "Misc/EngineVersion.h"
#include "Misc/CoreDelegates.h"
#include "HAL/PlatformApplicationMisc.h"
#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS
#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#endif // GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS
#if GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS
#include "IOS/IOSApplication.h"
#include "IOS/IOSWindow.h"
#include "IOSAppDelegate.h"
#include "IOSView.h"
#endif
#if GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS
#include "GlobalShader.h"
#include "ScreenRendering.h"
#include "PipelineStateCache.h"

#include "instant_preview_server.h"
#include "GoogleVRInstantPreviewGetServer.h"
#endif  // GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS
#include "UObject/Package.h"
#include "EngineAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"

///////////////////////////////////////////
// Begin GoogleVR Api Console Variables //
///////////////////////////////////////////

static TAutoConsoleVariable<int32> CVarViewerPreview(
	TEXT("vr.googlevr.ViewerPreview"),
	3,
	TEXT("Change which viewer data is used for VR previewing.\n")
	TEXT(" 0: No viewer or distortion\n")
	TEXT(" 1: Google Cardboard 1.0\n")
	TEXT(" 2: Google Cardboard 2.0\n")
	TEXT(" 3: ViewMaster (default)\n")
	TEXT(" 4: SnailVR\n")
	TEXT(" 5: RiTech 2.0\n")
	TEXT(" 6: Go4D C1-Glass")
);

static TAutoConsoleVariable<float> CVarPreviewSensitivity(
	TEXT("vr.googlevr.PreviewSensitivity"),
	1.0f,
	TEXT("Change preview sensitivity of Yaw and Pitch multiplier.\n")
	TEXT("Values are clamped between 0.1 and 10.0\n")
);

///////////////////////////////////
// Begin GoogleVR Api Reference //
///////////////////////////////////

// GoogleVR Api Reference
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
gvr_context* GVRAPI = nullptr;
const gvr_user_prefs* GVRUserPrefs = nullptr;
static const int64_t kPredictionTime = 50 * 1000000; //50 millisecond
static const float kDefaultRenderTargetScaleFactor = 1.0f;
#endif // GOOGLEVRHMD_SUPPORTED_PLATFORMS

// Only one HMD can be active at a time, so using file static to track this for transferring to game thread
static bool bBackDetected = false;
static bool bTriggerDetected = false;
static double BackbuttonPressTime;
static const double BACK_BUTTON_SHORT_PRESS_TIME = 1.0f;

//static variable for debugging;
static bool bDebugShowGVRSplash = false;

#if GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS
namespace InstantPreviewConstants
{
	const float kCameraZOffsetMeters = 0.0f;
	const int kBitrateKbps = 8000;
	const FRotator pre_rotator(180.f, 0.f, 0.f);
	const FRotator post_rotator(90.f, 0.f, 0.f);
	const int kRefPoseTextureSize = 32;
	const float kFrameSendPeriodMultiple = 0.05f;
	const float kFrameSendToCapturePeriod = 1.33333f;
}
#endif  // GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS

////////////////////////////////////////////////
// Begin Misc Helper Functions Implementation //
////////////////////////////////////////////////

void OnTriggerEvent(void* UserParam)
{
	UE_LOG(LogHMD, Log, TEXT("Trigger event detected"));

	bTriggerDetected = true;
}

////////////////////////////////////////
// Begin Android JNI Helper Functions //
////////////////////////////////////////

#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS

// Note: Should probably be moved into AndroidJNI class
int64 CallLongMethod(JNIEnv* Env, jobject Object, jmethodID Method, ...)
{
	if (Method == NULL || Object == NULL)
	{
		return false;
	}

	va_list Args;
	va_start(Args, Method);
	jlong Return = Env->CallLongMethodV(Object, Method, Args);
	va_end(Args);

	return (int64)Return;
}

JNI_METHOD void Java_com_epicgames_ue4_GameActivity_nativeOnUiLayerBack(JNIEnv* jenv, jobject thiz)
{
	// Need to be on game thread to dispatch handler
	bBackDetected = true;
}

void AndroidThunkCpp_UiLayer_SetEnabled(bool bEnable)
{
 	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
 	{
		static jmethodID UiLayerMethod = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_UiLayer_SetEnabled", "(Z)V", false);
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, UiLayerMethod, bEnable);
 	}
}
void AndroidThunkCpp_UiLayer_SetViewerName(const FString& ViewerName)
{
	if(ViewerName.Len() == 0)
	{
		return;
	}

 	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
 	{
		static jmethodID UiLayerMethod = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_UiLayer_SetViewerName", "(Ljava/lang/String;)V", false);
		jstring NameJava = Env->NewStringUTF(TCHAR_TO_UTF8(*ViewerName));
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, UiLayerMethod, NameJava);
 	}
}

gvr_context* AndroidThunkCpp_GetNativeGVRApi()
{
 	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
 	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_GetNativeGVRApi", "()J", false);
		return reinterpret_cast<gvr_context*>(CallLongMethod(Env, FJavaWrapper::GameActivityThis, Method));
 	}

	return nullptr;
}

void AndroidThunkCpp_GvrLayout_SetFixedPresentationSurfaceSizeToCurrent()
{
 	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
 	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_GvrLayout_SetFixedPresentationSurfaceSizeToCurrent", "()V", false);
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method);
 	}
}

bool AndroidThunkCpp_ProjectWantsCardboardOnlyMode()
{
 	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
 	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_ProjectWantsCardboardOnlyMode", "()Z", false);
		return FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, Method);
 	}

	return false;
}

bool AndroidThunkCpp_IsVrLaunch()
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_IsVrLaunch", "()Z", false);
		return FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, Method);
	}

	return true;
}

void AndroidThunkCpp_QuitDaydreamApplication()
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_QuitDaydreamApplication", "()V", false);
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method);
	}
}

void AndroidThunkCpp_EnableSPM()
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_EnableSPM", "()V", false);
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method);
	}
}

void AndroidThunkCpp_DisableSPM()
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_DisableSPM", "()V", false);
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method);
	}
}

FString AndroidThunkCpp_GetDataString()
{
	FString Result = FString("");
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_GetDataString", "()Z", false);
		jstring JavaString = (jstring)FJavaWrapper::CallObjectMethod(Env, FJavaWrapper::GameActivityThis, Method);
		if (JavaString != NULL)
		{
			const char* JavaChars = Env->GetStringUTFChars(JavaString, 0);
			Result = FString(UTF8_TO_TCHAR(JavaChars));
			Env->ReleaseStringUTFChars(JavaString, JavaChars);
			Env->DeleteLocalRef(JavaString);
		}
	}

	return Result;
}

#endif // GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS

/////////////////////////////////////
// Begin IOS Class Implementations //
/////////////////////////////////////

#if GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS

// Helped function to get global access
FGoogleVRHMD* GetGoogleVRHMD()
{
	if (GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetVersionString().Contains(TEXT("GoogleVR")) )
	{
		return static_cast<FGoogleVRHMD*>(GEngine->XRSystem.Get());
	}

	return nullptr;
}

@implementation FOverlayViewDelegate

- (void)didChangeViewerProfile
{
	FGoogleVRHMD* HMD = GetGoogleVRHMD();
	if(HMD)
	{
		HMD->RefreshViewerProfile();
	}
}

- (void)didTapBackButton
{
	bBackDetected = true;
}

@end

#endif

/////////////////////////////////////////////////
// Begin FGoogleVRHMDPlugin Implementation     //
/////////////////////////////////////////////////

class FGoogleVRHMDPlugin : public IGoogleVRHMDPlugin
{
public:

	/** Returns the key into the HMDPluginPriority section of the config file for this module */
	virtual FString GetModuleKeyName() const override
	{
		return TEXT("GoogleVRHMD");
	}

	/**
	 * Attempts to create a new head tracking device interface
	 *
	 * @return	Interface to the new head tracking device, if we were able to successfully create one
	 */
	virtual TSharedPtr< class IXRTrackingSystem, ESPMode::ThreadSafe > CreateTrackingSystem() override;

	/**
	 * Always return true for GoogleVR, when enabled, to allow HMD Priority to sort it out
	 */
	virtual bool IsHMDConnected() { return true; }
};

IMPLEMENT_MODULE( FGoogleVRHMDPlugin, GoogleVRHMD );

TSharedPtr< class IXRTrackingSystem, ESPMode::ThreadSafe > FGoogleVRHMDPlugin::CreateTrackingSystem()
{
	TSharedRef< FGoogleVRHMD, ESPMode::ThreadSafe > HMD = FSceneViewExtensions::NewExtension<FGoogleVRHMD>();
	if (HMD->IsInitialized())
	{
		return HMD;
	}

	return nullptr;
}

/////////////////////////////////////
// Begin FGoogleVRHMD Self API     //
/////////////////////////////////////
FGoogleVRHMD::FGoogleVRHMD(const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	, CustomPresent(nullptr)
#endif
	, bStereoEnabled(false)
	, bHMDEnabled(false)
	, bDistortionCorrectionEnabled(true)
	, bUseGVRApiDistortionCorrection(false)
	, bUseOffscreenFramebuffers(false)
	, bIsInDaydreamMode(false)
	, bForceStopPresentScene(false)
	, bIsMobileMultiViewDirect(false)
	, NeckModelScale(1.0f)
	, BaseOrientation(FQuat::Identity)
	, RendererModule(nullptr)
	, DistortionMeshIndices(nullptr)
	, DistortionMeshVerticesLeftEye(nullptr)
	, DistortionMeshVerticesRightEye(nullptr)
#if GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS
	, OverlayView(nil)
#endif
	, LastUpdatedCacheFrame(0)
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	, CachedFinalHeadRotation(EForceInit::ForceInit)
	, CachedFinalHeadPosition(EForceInit::ForceInitToZero)
	, DistortedBufferViewportList(nullptr)
	, NonDistortedBufferViewportList(nullptr)
	, ActiveViewportList(nullptr)
	, ScratchViewport(nullptr)
#endif
	, PosePitch(0.0f)
	, PoseYaw(0.0f)
	, DistortionPointsX(40)
	, DistortionPointsY(40)
	, NumVerts(0)
	, NumTris(0)
	, NumIndices(0)
	, DistortEnableCommand(TEXT("vr.googlevr.DistortionCorrection.bEnable"),
		*NSLOCTEXT("GoogleVR", "CCommandText_DistortEnable",
			"Gogle VR specific extension.\n"
			"Enable or disable lens distortion correction.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(this, &FGoogleVRHMD::DistortEnableCommandHandler))
	, DistortMethodCommand(TEXT("vr.googlevr.DistortionCorrection.Method"),
		*NSLOCTEXT("GoogleVR", "CCommandText_DistortMethod",
			"Gogle VR specific extension.\n"
			"Set the lens distortion method.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(this, &FGoogleVRHMD::DistortMethodCommandHandler))
	, RenderTargetSizeCommand(TEXT("vr.googlevr.RenderTargetSize"),
		*NSLOCTEXT("GoogleVR", "CCommandText_RenderTargetSize",
			"Gogle VR specific extension.\n"
			"Set or reset render target size.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(this, &FGoogleVRHMD::RenderTargetSizeCommandHandler))
	, NeckModelScaleCommand(TEXT("vr.googlevr.NeckModelScale"),
		*NSLOCTEXT("GoogleVR", "CCommandText_NeckModelScale",
			"Gogle VR specific extension.\n"
			"Set the neck model scale.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(this, &FGoogleVRHMD::NeckModelScaleCommandHandler))
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	, DistortMeshSizeCommand(TEXT("vr.googlevr.DistortionMesh"),
		*NSLOCTEXT("GoogleVR", "CCommandText_DistortMeshSizee",
			"Gogle VR specific extension.\n"
			"Set the size of the distortion mesh.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(this, &FGoogleVRHMD::DistortMeshSizeCommandHandler))
	, ShowSplashCommand(TEXT("vr.googlevr.bShowSplash"),
		*NSLOCTEXT("GoogleVR", "CCommandText_ShowSplash",
			"Gogle VR specific extension.\n"
			"Show or hide the splash screen").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(this, &FGoogleVRHMD::ShowSplashCommandHandler))
	, SplashScreenDistanceCommand(TEXT("vr.googlevr.SplashScreenDistance"),
		*NSLOCTEXT("GoogleVR", "CCommandText_SplashScreenDistance",
			"Gogle VR specific extension.\n"
			"Set the distance to the splash screen").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(this, &FGoogleVRHMD::SplashScreenDistanceCommandHandler))
	, SplashScreenRenderScaleCommand(TEXT("vr.googlevr.SplashScreenRenderScale"),
		*NSLOCTEXT("GoogleVR", "CCommandText_SplashScreenRenderScale",
			"Gogle VR specific extension.\n"
			"Set the scale at which the splash screen is rendered").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(this, &FGoogleVRHMD::SplashScreenRenderScaleCommandHandler))
	, EnableSustainedPerformanceModeCommand(TEXT("vr.googlevr.bEnableSustainedPerformanceMode"),
		*NSLOCTEXT("GoogleVR", "CCommandText_EnableSustainedPerformanceMode",
			"Gogle VR specific extension.\n"
			"Enable or Disable Sustained Performance Mode").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(this, &FGoogleVRHMD::EnableSustainedPerformanceModeHandler))
	, CVarSink(FConsoleCommandDelegate::CreateRaw(this, &FGoogleVRHMD::CVarSinkHandler))
#endif
	, TrackingOrigin(EHMDTrackingOrigin::Eye)
	, bIs6DoFSupported(false)

{
	FPlatformMisc::LowLevelOutputDebugString(TEXT("Initializing FGoogleVRHMD"));

#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS

	// Get GVRAPI from java
	GVRAPI = AndroidThunkCpp_GetNativeGVRApi();

#elif GOOGLEVRHMD_SUPPORTED_PLATFORMS

	GVRAPI = gvr_create();

#endif

#if GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS
	IpServerHandle = InstantPreviewGetServerHandle();

	for (int i = 0; i < kReadbackTextureCount; i++) {
		ReadbackTextures[i] = nullptr;
		ReadbackBuffers[i] = nullptr;
		ReadbackTextureSizes[i] = FIntPoint();
	}

	ReadbackTextureCount = 0;
	SentTextureCount = 0;
	bIsInstantPreviewActive = false;
#endif

	if(IsInitialized())
	{
		UE_LOG(LogHMD, Log, TEXT("GoogleVR API created"));

		// Get renderer module
		static const FName RendererModuleName("Renderer");
		RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
		check(RendererModule);

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS

#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS
		// Initialize OpenGL resources for API
		gvr_initialize_gl(GVRAPI);
#endif

		// Log the current viewer
		UE_LOG(LogHMD, Log, TEXT("The current viewer is %s"), UTF8_TO_TCHAR(gvr_get_viewer_model(GVRAPI)));

		// Get gvr user prefs
		GVRUserPrefs = gvr_get_user_prefs(GVRAPI);

#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS
		//Set the flag when async reprojection is enabled
		bUseOffscreenFramebuffers = gvr_get_async_reprojection_enabled(GVRAPI);
		//We are in Daydream Mode when async reprojection is enabled.
		bIsInDaydreamMode = bUseOffscreenFramebuffers;

		// Only use gvr api distortion when async reprojection is enabled
		// And by default we use Unreal's PostProcessing Distortion for Cardboard
		bUseGVRApiDistortionCorrection = bUseOffscreenFramebuffers;
		//bUseGVRApiDistortionCorrection = true;  //Uncomment this line is you want to use GVR distortion when async reprojection is not enabled.

		// Query for direct multi-view
		GSupportsMobileMultiView = gvr_is_feature_supported(GVRAPI, GVR_FEATURE_MULTIVIEW);
		const auto CVarMobileMultiView = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.MobileMultiView"));
		const auto CVarMobileMultiViewDirect = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.MobileMultiView.Direct"));
		const bool bIsMobileMultiViewEnabled = (CVarMobileMultiView && CVarMobileMultiView->GetValueOnAnyThread() != 0);
		const bool bIsMobileMultiViewDirectEnabled = (CVarMobileMultiViewDirect && CVarMobileMultiViewDirect->GetValueOnAnyThread() != 0);
		bIsMobileMultiViewDirect = GSupportsMobileMultiView && bIsMobileMultiViewEnabled && bIsMobileMultiViewDirectEnabled;

		if(bUseOffscreenFramebuffers)
		{
			// Create custom present class
			CustomPresent = new FGoogleVRHMDCustomPresent(this);
			GVRSplash = MakeShareable(new FGoogleVRSplash(this));
			GVRSplash->Init();
		}

		// Show ui on Android
		AndroidThunkCpp_UiLayer_SetEnabled(true);
		AndroidThunkCpp_UiLayer_SetViewerName(UTF8_TO_TCHAR(gvr_get_viewer_model(GVRAPI)));

		// Set Hardware Scaling in GvrLayout
		AndroidThunkCpp_GvrLayout_SetFixedPresentationSurfaceSizeToCurrent();

#elif GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS

		// We will use Unreal's PostProcessing Distortion for iOS
		bUseGVRApiDistortionCorrection = false;
		bIsInDaydreamMode = false;

		// Setup and show ui on iOS
		dispatch_async(dispatch_get_main_queue(), ^
		{
			OverlayViewDelegate = [[FOverlayViewDelegate alloc] init];
			OverlayView = [[GVROverlayView alloc] initWithFrame:[IOSAppDelegate GetDelegate].IOSView.bounds];
			OverlayView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
			OverlayView.delegate = OverlayViewDelegate;
			[[IOSAppDelegate GetDelegate].IOSView addSubview:OverlayView];
		});
#endif // GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS

		// By default, go ahead and disable the screen saver. The user can still change this freely
		FPlatformApplicationMisc::ControlScreensaver(FPlatformApplicationMisc::Disable);

		// TODO: Get trigger event handler working again
		// Setup GVRAPI delegates
		//gvr_set_trigger_event_handler(GVRAPI, OnTriggerEvent, nullptr);

		// Refresh delegate
		FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FGoogleVRHMD::ApplicationResumeDelegate);

		UpdateGVRViewportList();

		bIs6DoFSupported = gvr_is_feature_supported(GVRAPI, GVR_FEATURE_HEAD_POSE_6DOF);
#endif // GOOGLEVRHMD_SUPPORTED_PLATFORMS

		// Set the default rendertarget size to the default size in UE4
		SetRenderTargetSizeToDefault();

		// Enabled by default
		EnableHMD(true);
		EnableStereo(true);

		// Initialize distortion mesh and indices
		SetNumOfDistortionPoints(DistortionPointsX, DistortionPointsY);

		// Register LoadMap Delegate
		FCoreUObjectDelegates::PreLoadMap.AddRaw(this, &FGoogleVRHMD::OnPreLoadMap);
	}
	else
	{
		FPlatformMisc::LowLevelOutputDebugString(TEXT("GoogleVR context failed to create successfully."));
	}
}

FGoogleVRHMD::~FGoogleVRHMD()
{
	delete[] DistortionMeshIndices;
	DistortionMeshIndices = nullptr;
	delete[] DistortionMeshVerticesLeftEye;
	DistortionMeshVerticesLeftEye = nullptr;
	delete[] DistortionMeshVerticesRightEye;
	DistortionMeshVerticesRightEye = nullptr;

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	if (DistortedBufferViewportList)
	{
		// gvr destroy function will set the pointer to nullptr
		gvr_buffer_viewport_list_destroy(&DistortedBufferViewportList);
	}
	if (NonDistortedBufferViewportList)
	{
		gvr_buffer_viewport_list_destroy(&NonDistortedBufferViewportList);
	}
	if (ScratchViewport)
	{
		gvr_buffer_viewport_destroy(&ScratchViewport);
	}

	if (CustomPresent)
	{
		CustomPresent->Shutdown();
		CustomPresent = nullptr;
	}
#endif

	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
}

bool FGoogleVRHMD::IsInitialized() const
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS

	return GVRAPI != nullptr;

#endif

	// Non-supported platform will be PC editor which will always succeed
	return true;
}

void FGoogleVRHMD::UpdateGVRViewportList() const
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	// Allocate if necessary!
	if (!DistortedBufferViewportList)
	{
		DistortedBufferViewportList = gvr_buffer_viewport_list_create(GVRAPI);
	}
	if (!NonDistortedBufferViewportList)
	{
		NonDistortedBufferViewportList = gvr_buffer_viewport_list_create(GVRAPI);
	}
	if (!ScratchViewport)
	{
		ScratchViewport = gvr_buffer_viewport_create(GVRAPI);
	}

	gvr_get_recommended_buffer_viewports(GVRAPI, DistortedBufferViewportList);
	gvr_get_screen_buffer_viewports(GVRAPI, NonDistortedBufferViewportList);

	ActiveViewportList = bDistortionCorrectionEnabled ? DistortedBufferViewportList : NonDistortedBufferViewportList;

	if (IsMobileMultiViewDirect())
	{
		check(gvr_buffer_viewport_list_get_size(ActiveViewportList) == 2);
		for (uint32 EyeIndex = 0; EyeIndex < 2; ++EyeIndex)
		{
			gvr_buffer_viewport_list_get_item(ActiveViewportList, EyeIndex, ScratchViewport);
			const gvr_rectf ViewportRect = { 0, 1.0f, 0.0f, 1.0f };
			gvr_buffer_viewport_set_source_uv(ScratchViewport, ViewportRect);
			gvr_buffer_viewport_set_source_layer(ScratchViewport, EyeIndex);
			gvr_buffer_viewport_list_set_item(ActiveViewportList, EyeIndex, ScratchViewport);
		}
	}

	// Pass the viewport list used for rendering to CustomPresent for async reprojection
	if(CustomPresent)
	{
		CustomPresent->UpdateRenderingViewportList(ActiveViewportList);
	}
#endif  // GOOGLEVRHMD_SUPPORTED_PLATFORMS
}

bool FGoogleVRHMD::GetCurrentPose(int32 DeviceId, FQuat& CurrentOrientation, FVector& CurrentPosition)
{
	if (DeviceId != HMDDeviceId)
	{
		return false;
	}

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	// Update camera pose using cached head pose which we updated at the beginning of a frame.
	CurrentOrientation = BaseOrientation * CachedFinalHeadRotation;
	CurrentPosition = BaseOrientation.RotateVector(CachedFinalHeadPosition);
#else
#if GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS
	if (bIsInstantPreviewActive) {
		GetCurrentReferencePose(CurrentOrientation, CurrentPosition);
	}
	else
#endif  // GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS
	{
		// Simulating head rotation using mouse in Editor.
		ULocalPlayer* Player = GEngine->GetDebugLocalPlayer();
		float PreviewSensitivity = FMath::Clamp(CVarPreviewSensitivity.GetValueOnAnyThread(), 0.1f, 10.0f);

		if (Player != NULL && Player->PlayerController != NULL && GWorld)
		{
			float MouseX = 0.0f;
			float MouseY = 0.0f;
			Player->PlayerController->GetInputMouseDelta(MouseX, MouseY);

			double CurrentTime = FApp::GetCurrentTime();
			double DeltaTime = GWorld->GetDeltaSeconds();

			PoseYaw += (FMath::RadiansToDegrees(MouseX * DeltaTime * 4.0f) * PreviewSensitivity);
			PosePitch += (FMath::RadiansToDegrees(MouseY * DeltaTime * 4.0f) * PreviewSensitivity);
			PosePitch = FMath::Clamp(PosePitch, -90.0f + KINDA_SMALL_NUMBER, 90.0f - KINDA_SMALL_NUMBER);

			CurrentOrientation = BaseOrientation * FQuat(FRotator(PosePitch, PoseYaw, 0.0f));
		}
		else
		{
			CurrentOrientation = FQuat(FRotator(0.0f, 0.0f, 0.0f));
		}

		// TODO: Move this functionality into the AUX library so that it doesn't need to be duplicated.
		// Between the SDK and here.

		const float NeckHorizontalOffset = 0.080f;  // meters in Z
		const float NeckVerticalOffset = 0.075f;     // meters in Y

		// Rotate eyes around neck pivot point.
		CurrentPosition = CurrentOrientation * FVector(NeckHorizontalOffset, 0.0f, NeckVerticalOffset);

		// Measure new position relative to original center of head, because
		// applying a neck model should not elevate the camera.
		CurrentPosition -= FVector(0.0f, 0.0f, NeckVerticalOffset);

		// Apply the Neck Model Scale
		CurrentPosition *= NeckModelScale;

		// Number of Unreal Units per meter.
		const float WorldToMetersScale = GetWorldToMetersScale();
		CurrentPosition *= WorldToMetersScale;

		CurrentPosition = BaseOrientation.RotateVector(CurrentPosition);

		if (GetTrackingOrigin() == EHMDTrackingOrigin::Floor)
		{
			float floorHeight;
			if (GetFloorHeight(&floorHeight))
			{
				CurrentPosition.Z -= floorHeight;
			}
		}
	}
#endif
	return true;
}

IRendererModule* FGoogleVRHMD::GetRendererModule()
{
	return RendererModule;
}

void FGoogleVRHMD::RefreshViewerProfile()
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	gvr_refresh_viewer_profile(GVRAPI);
#endif

	// Re-Initialize distortion meshes, as the viewer may have changed
	SetNumOfDistortionPoints(DistortionPointsX, DistortionPointsY);
}

FIntPoint FGoogleVRHMD::GetUnrealMobileContentSize()
{
	FIntPoint Size = FIntPoint::ZeroValue;
#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS
	FPlatformRect Rect = FAndroidWindow::GetScreenRect();
	Size.X = Rect.Right;
	Size.Y = Rect.Bottom;
#elif GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS
	FPlatformRect Rect = FIOSWindow::GetScreenRect();
	Size.X = Rect.Right;
	Size.Y = Rect.Bottom;
#endif
	return Size;
}

FIntPoint FGoogleVRHMD::GetGVRHMDRenderTargetSize()
{
	return GVRRenderTargetSize;
}

FIntPoint FGoogleVRHMD::GetGVRMaxRenderTargetSize()
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	gvr_sizei MaxSize = gvr_get_maximum_effective_render_target_size(GVRAPI);
	UE_LOG(LogHMD, Log, TEXT("GVR Recommended RenderTargetSize: %d x %d"), MaxSize.width, MaxSize.height);
	return FIntPoint{ static_cast<int>(MaxSize.width), static_cast<int>(MaxSize.height) };
#else
	return FIntPoint{ 0, 0 };
#endif
}

FIntPoint FGoogleVRHMD::SetRenderTargetSizeToDefault()
{
	GVRRenderTargetSize = FIntPoint::ZeroValue;
#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS
	if (bUseGVRApiDistortionCorrection)
	{
		FIntPoint RenderTargetSize = FIntPoint::ZeroValue;
		SetGVRHMDRenderTargetSize(kDefaultRenderTargetScaleFactor, RenderTargetSize);
	}
	else
	{
		FPlatformRect Rect = FAndroidWindow::GetScreenRect();
		GVRRenderTargetSize.X = Rect.Right;
		GVRRenderTargetSize.Y = Rect.Bottom;
	}
#elif GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS
	FPlatformRect Rect = FIOSWindow::GetScreenRect();
	GVRRenderTargetSize.X = Rect.Right;
	GVRRenderTargetSize.Y = Rect.Bottom;
#endif
	return GVRRenderTargetSize;
}

bool FGoogleVRHMD::SetGVRHMDRenderTargetSize(float ScaleFactor, FIntPoint& OutRenderTargetSize)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	if (ScaleFactor < 0.1 || ScaleFactor > 1)
	{
        	ScaleFactor = FMath::Clamp(ScaleFactor, 0.1f, 1.0f);
		UE_LOG(LogHMD, Warning, TEXT("Invalid RenderTexture Scale Factor. The valid value should be within [0.1, 1.0]. Clamping the value to %f"), ScaleFactor);
	}

	// For now, only allow change the render texture size when using gvr distortion.
	// Since when use PPHMD, the render texture size need to match the surface size.
	if (!bUseGVRApiDistortionCorrection)
	{
		return false;
	}
	UE_LOG(LogHMD, Log, TEXT("Setting render target size using scale factor: %f"), ScaleFactor);
	FIntPoint DesiredRenderTargetSize = GetGVRMaxRenderTargetSize();
	DesiredRenderTargetSize.X *= ScaleFactor;
	DesiredRenderTargetSize.Y *= ScaleFactor;
	return SetGVRHMDRenderTargetSize(DesiredRenderTargetSize.X, DesiredRenderTargetSize.Y, OutRenderTargetSize);
#else
	return false;
#endif
}

bool FGoogleVRHMD::SetGVRHMDRenderTargetSize(int DesiredWidth, int DesiredHeight, FIntPoint& OutRenderTargetSize)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	// For now, only allow change the render texture size when using gvr distortion.
	// Since when use PPHMD, the render texture size need to match the surface size.
	if (!bUseGVRApiDistortionCorrection)
	{
		return false;
	}

	const uint32 AdjustedDesiredWidth = (IsMobileMultiViewDirect()) ? DesiredWidth / 2 : DesiredWidth;
	
	// Ensure sizes are dividable by DividableBy to get post processing effects with lower resolution working well
	const uint32 DividableBy = 4;

	const uint32 Mask = ~(DividableBy - 1);
	GVRRenderTargetSize.X = (AdjustedDesiredWidth + DividableBy - 1) & Mask;
	GVRRenderTargetSize.Y = (DesiredHeight + DividableBy - 1) & Mask;

	OutRenderTargetSize = GVRRenderTargetSize;
	UE_LOG(LogHMD, Log, TEXT("Set Render Target Size to %d x %d, the deired size is %d x %d"), GVRRenderTargetSize.X, GVRRenderTargetSize.Y, AdjustedDesiredWidth, DesiredHeight);
	return true;
#else
	return false;
#endif
}

void FGoogleVRHMD::OnPreLoadMap(const FString &)
{
	// Force not to present the scene when start loading a map.
	bForceStopPresentScene = true;
}

void FGoogleVRHMD::ApplicationResumeDelegate()
{
	RefreshViewerProfile();
}

void FGoogleVRHMD::HandleGVRBackEvent()
{
	if( GEngine &&
		GEngine->GameViewport &&
		GEngine->GameViewport->Viewport &&
		GEngine->GameViewport->Viewport->GetClient() )
	{
#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS
		GEngine->GameViewport->Viewport->GetClient()->InputKey(GEngine->GameViewport->Viewport, 0, EKeys::Android_Back, EInputEvent::IE_Pressed);
		GEngine->GameViewport->Viewport->GetClient()->InputKey(GEngine->GameViewport->Viewport, 0, EKeys::Android_Back, EInputEvent::IE_Released);
#elif GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS
		// TODO: iOS doesn't have hardware back buttons, so what should be fired?
		GEngine->GameViewport->Viewport->GetClient()->InputKey(GEngine->GameViewport->Viewport, 0, EKeys::Global_Back, EInputEvent::IE_Pressed);
		GEngine->GameViewport->Viewport->GetClient()->InputKey(GEngine->GameViewport->Viewport, 0, EKeys::Global_Back, EInputEvent::IE_Released);
#endif
	}
}

void FGoogleVRHMD::SetDistortionCorrectionEnabled(bool bEnable)
{
	// Can't turn off distortion correction if using async reprojection;
	if(bUseOffscreenFramebuffers)
	{
		bDistortionCorrectionEnabled = true;
	}
	else
	{
		bDistortionCorrectionEnabled = bEnable;
	}
}

void FGoogleVRHMD::SetDistortionCorrectionMethod(bool bInUseGVRApiDistortionCorrection)
{
	//Cannot change distortion method when use async reprojection
	if(bUseOffscreenFramebuffers)
	{
		bUseGVRApiDistortionCorrection = true;
	}
	else
	{
		bUseGVRApiDistortionCorrection = bInUseGVRApiDistortionCorrection;
	}
}

bool FGoogleVRHMD::SetDefaultViewerProfile(const FString& ViewerProfileURL)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	if(gvr_set_default_viewer_profile(GVRAPI, TCHAR_TO_ANSI(*ViewerProfileURL)))
	{
		gvr_refresh_viewer_profile(GVRAPI);

		// Re-Initialize distortion meshes, as the viewer may have changed
		SetNumOfDistortionPoints(DistortionPointsX, DistortionPointsY);

		return true;
	}
#endif

	return false;
}

void FGoogleVRHMD::SetNumOfDistortionPoints(int32 XPoints, int32 YPoints)
{
	// Force non supported platform distortion mesh be 40 x 40
#if !GOOGLEVRHMD_SUPPORTED_PLATFORMS
	XPoints = 40;
	YPoints = 40;
#endif

	// clamp values
	if (XPoints < 2)
	{
		XPoints = 2;
	}
	else if (XPoints > 200)
	{
		XPoints = 200;
	}

	if (YPoints < 2)
	{
		YPoints = 2;
	}
	else if (YPoints > 200)
	{
		YPoints = 200;
	}

	// calculate our values
	DistortionPointsX = XPoints;
	DistortionPointsY = YPoints;
	NumVerts = DistortionPointsX * DistortionPointsY;
	NumTris = (DistortionPointsX - 1) * (DistortionPointsY - 1) * 2;
	NumIndices = NumTris * 3;

	// generate the distortion mesh
	GenerateDistortionCorrectionIndexBuffer();
	GenerateDistortionCorrectionVertexBuffer(eSSP_LEFT_EYE);
	GenerateDistortionCorrectionVertexBuffer(eSSP_RIGHT_EYE);
}

void FGoogleVRHMD::SetDistortionMeshSize(EDistortionMeshSizeEnum MeshSize)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	switch(MeshSize)
	{
	case EDistortionMeshSizeEnum::DMS_VERYSMALL:
		SetNumOfDistortionPoints(20, 20);
		break;
	case EDistortionMeshSizeEnum::DMS_SMALL:
		SetNumOfDistortionPoints(40, 40);
		break;
	case EDistortionMeshSizeEnum::DMS_MEDIUM:
		SetNumOfDistortionPoints(60, 60);
		break;
	case EDistortionMeshSizeEnum::DMS_LARGE:
		SetNumOfDistortionPoints(80, 80);
		break;
	case EDistortionMeshSizeEnum::DMS_VERYLARGE:
		SetNumOfDistortionPoints(100, 100);
		break;
	}
#endif
}

void FGoogleVRHMD::SetNeckModelScale(float ScaleFactor)
{
	NeckModelScale = FMath::Clamp(ScaleFactor, 0.0f, 1.0f);
}

bool FGoogleVRHMD::GetDistortionCorrectionEnabled() const
{
	return bDistortionCorrectionEnabled;
}

bool FGoogleVRHMD::IsUsingGVRApiDistortionCorrection() const
{
	return bUseGVRApiDistortionCorrection;
}

float FGoogleVRHMD::GetNeckModelScale() const
{
	return NeckModelScale;
}

float FGoogleVRHMD::GetWorldToMetersScale() const
{
	if (IsInGameThread() && GWorld != nullptr)
	{
		return GWorld->GetWorldSettings()->WorldToMeters;
	}

	// Default value, assume Unreal units are in centimeters
	return 100.0f;
}

bool FGoogleVRHMD::IsVrLaunch() const
{
#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS
	return AndroidThunkCpp_IsVrLaunch();
#endif
	return false;
}

bool FGoogleVRHMD::IsInDaydreamMode() const
{
	return bIsInDaydreamMode;
}

void FGoogleVRHMD::SetSPMEnable(bool bEnable) const
{
#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS
	if (bEnable)
	{
		AndroidThunkCpp_EnableSPM();
	}
	else
	{
		AndroidThunkCpp_DisableSPM();
	}
#endif
}

FString FGoogleVRHMD::GetIntentData() const
{
#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS
	return AndroidThunkCpp_GetDataString();
#endif
	return TEXT("");
}

FString FGoogleVRHMD::GetViewerModel()
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	return UTF8_TO_TCHAR(gvr_get_viewer_model(GVRAPI));
#endif

	return TEXT("");
}

FString FGoogleVRHMD::GetViewerVendor()
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	return UTF8_TO_TCHAR(gvr_get_viewer_vendor(GVRAPI));
#endif

	return TEXT("");
}

FGoogleVRHMD::EViewerPreview FGoogleVRHMD::GetPreviewViewerType()
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	return EVP_None;
#else
	int32 Val = FMath::Clamp(CVarViewerPreview.GetValueOnAnyThread(), 0, 6);
	return EViewerPreview(Val);
#endif
}

float FGoogleVRHMD::GetPreviewViewerInterpupillaryDistance()
{
	switch(GetPreviewViewerType())
	{
#if !GOOGLEVRHMD_SUPPORTED_PLATFORMS

	case EVP_GoogleCardboard1:
		return GoogleCardboardViewerPreviews::GoogleCardboard1::InterpupillaryDistance;
	case EVP_GoogleCardboard2:
		return GoogleCardboardViewerPreviews::GoogleCardboard2::InterpupillaryDistance;
	case EVP_ViewMaster:
		return GoogleCardboardViewerPreviews::ViewMaster::InterpupillaryDistance;
	case EVP_SnailVR:
		return GoogleCardboardViewerPreviews::SnailVR::InterpupillaryDistance;
	case EVP_RiTech2:
		return GoogleCardboardViewerPreviews::RiTech2::InterpupillaryDistance;
	case EVP_Go4DC1Glass:
		return GoogleCardboardViewerPreviews::Go4DC1Glass::InterpupillaryDistance;

#endif

	case EVP_None:
	default:
		return 0.064f;
	}
}

FMatrix FGoogleVRHMD::GetPreviewViewerStereoProjectionMatrix(enum EStereoscopicPass StereoPass)
{
	switch(GetPreviewViewerType())
	{
#if !GOOGLEVRHMD_SUPPORTED_PLATFORMS

	case EVP_GoogleCardboard1:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::GoogleCardboard1::LeftStereoProjectionMatrix : GoogleCardboardViewerPreviews::GoogleCardboard1::RightStereoProjectionMatrix;
	case EVP_GoogleCardboard2:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::GoogleCardboard2::LeftStereoProjectionMatrix : GoogleCardboardViewerPreviews::GoogleCardboard2::RightStereoProjectionMatrix;
	case EVP_ViewMaster:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::ViewMaster::LeftStereoProjectionMatrix : GoogleCardboardViewerPreviews::ViewMaster::RightStereoProjectionMatrix;
	case EVP_SnailVR:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::SnailVR::LeftStereoProjectionMatrix : GoogleCardboardViewerPreviews::SnailVR::RightStereoProjectionMatrix;
	case EVP_RiTech2:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::RiTech2::LeftStereoProjectionMatrix : GoogleCardboardViewerPreviews::RiTech2::RightStereoProjectionMatrix;
	case EVP_Go4DC1Glass:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::Go4DC1Glass::LeftStereoProjectionMatrix : GoogleCardboardViewerPreviews::Go4DC1Glass::RightStereoProjectionMatrix;

#endif

	case EVP_None:
	default:
		check(0);
		return FMatrix();
	}
}

uint32 FGoogleVRHMD::GetPreviewViewerNumVertices(enum EStereoscopicPass StereoPass)
{
	switch(GetPreviewViewerType())
	{
#if !GOOGLEVRHMD_SUPPORTED_PLATFORMS

	case EVP_GoogleCardboard1:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::GoogleCardboard1::NumLeftVertices : GoogleCardboardViewerPreviews::GoogleCardboard1::NumRightVertices;
	case EVP_GoogleCardboard2:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::GoogleCardboard2::NumLeftVertices : GoogleCardboardViewerPreviews::GoogleCardboard2::NumRightVertices;
	case EVP_ViewMaster:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::ViewMaster::NumLeftVertices : GoogleCardboardViewerPreviews::ViewMaster::NumRightVertices;
	case EVP_SnailVR:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::SnailVR::NumLeftVertices : GoogleCardboardViewerPreviews::SnailVR::NumRightVertices;
	case EVP_RiTech2:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::RiTech2::NumLeftVertices : GoogleCardboardViewerPreviews::RiTech2::NumRightVertices;
	case EVP_Go4DC1Glass:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::Go4DC1Glass::NumLeftVertices : GoogleCardboardViewerPreviews::Go4DC1Glass::NumRightVertices;

#endif

	case EVP_None:
	default:
		check(0);
		return 0;
	}
}

const FDistortionVertex* FGoogleVRHMD::GetPreviewViewerVertices(enum EStereoscopicPass StereoPass)
{
	switch(GetPreviewViewerType())
	{
#if !GOOGLEVRHMD_SUPPORTED_PLATFORMS

	case EVP_GoogleCardboard1:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::GoogleCardboard1::LeftVertices : GoogleCardboardViewerPreviews::GoogleCardboard1::RightVertices;
	case EVP_GoogleCardboard2:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::GoogleCardboard2::LeftVertices : GoogleCardboardViewerPreviews::GoogleCardboard2::RightVertices;
	case EVP_ViewMaster:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::ViewMaster::LeftVertices : GoogleCardboardViewerPreviews::ViewMaster::RightVertices;
	case EVP_SnailVR:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::SnailVR::LeftVertices : GoogleCardboardViewerPreviews::SnailVR::RightVertices;
	case EVP_RiTech2:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::RiTech2::LeftVertices : GoogleCardboardViewerPreviews::RiTech2::RightVertices;
	case EVP_Go4DC1Glass:
		return StereoPass == eSSP_LEFT_EYE ? GoogleCardboardViewerPreviews::Go4DC1Glass::LeftVertices : GoogleCardboardViewerPreviews::Go4DC1Glass::RightVertices;

#endif

	case EVP_None:
	default:
		check(0);
		return nullptr;
	}
}

// ------  Called on Game Thread ------
bool FGoogleVRHMD::GetHMDDistortionEnabled() const
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	// Enable Unreal PostProcessing Distortion when not using GVR Distortion.
	return bDistortionCorrectionEnabled && !IsUsingGVRApiDistortionCorrection();
#else
	return bDistortionCorrectionEnabled && (GetPreviewViewerType() != EVP_None);
#endif
}

void FGoogleVRHMD::AdjustViewRect(enum EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	// We should have a valid GVRRenderTargetSize here.
	check(GVRRenderTargetSize.X != 0 && GVRRenderTargetSize.Y != 0);
	check(ActiveViewportList);
	check(gvr_buffer_viewport_list_get_size(ActiveViewportList) == 2);
	switch (StereoPass)
	{
	case EStereoscopicPass::eSSP_LEFT_EYE:
		gvr_buffer_viewport_list_get_item(ActiveViewportList, 0, ScratchViewport);
		break;
	case EStereoscopicPass::eSSP_RIGHT_EYE:
		gvr_buffer_viewport_list_get_item(ActiveViewportList, 1, ScratchViewport);
		break;
	default:
		// We shouldn't got here.
		check(false);
		break;
	}
	gvr_rectf GVRRect = gvr_buffer_viewport_get_source_uv(ScratchViewport);
	int Left = static_cast<int>(GVRRect.left * GVRRenderTargetSize.X);
	int Bottom = static_cast<int>(GVRRect.bottom * GVRRenderTargetSize.Y);
	int Right = static_cast<int>(GVRRect.right * GVRRenderTargetSize.X);
	int Top = static_cast<int>(GVRRect.top * GVRRenderTargetSize.Y);

	//UE_LOG(LogTemp, Log, TEXT("Set Viewport Rect to %d, %d, %d, %d for render target size %d x %d"), Left, Bottom, Right, Top, GVRRenderTargetSize.X, GVRRenderTargetSize.Y);
	X = Left;
	Y = Bottom;
	SizeX = Right - Left;
	SizeY = Top - Bottom;
#else
	SizeX = SizeX / 2;
	if (StereoPass == eSSP_RIGHT_EYE)
	{
		X += SizeX;
	}
#endif
}

void FGoogleVRHMD::BeginRendering_GameThread()
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	// Note that we force not enqueue the CachedHeadPose when start loading a map until a new game frame started.
	// This is for solving the one frame flickering issue when load another level due to that there is one frame
	// the scene is rendered before the camera is updated.
	// TODO: We need to investigate a better solution here.
	if(CustomPresent && !bForceStopPresentScene && !bDebugShowGVRSplash)
	{
		CustomPresent->UpdateRenderingPose(CachedHeadPose);
	}
#endif
#if GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS
	struct FQueueRenderPoseContext {
		instant_preview::ReferencePose currentPose;
		instant_preview::ReferencePose *renderPose;
	};
	struct FQueueRenderPoseContext context {
		CurrentReferencePose,
		&RenderReferencePose,
	};
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		QueueRenderPose,
		struct FQueueRenderPoseContext, Context, context,
		{
			*Context.renderPose = Context.currentPose;
		});
#endif  // GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS
}

// ------  Called on Render Thread ------
void FGoogleVRHMD::BeginRendering_RenderThread(const FTransform& NewRelativeTransform, FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily)
{
	FHeadMountedDisplayBase::BeginRendering_RenderThread(NewRelativeTransform, RHICmdList, ViewFamily);
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	if(CustomPresent)
	{
		CustomPresent->BeginRendering();
	}
#endif
}

bool FGoogleVRHMD::IsActiveThisFrame(class FViewport* InViewport) const
{
#if GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS
	return GEngine && GEngine->IsStereoscopic3D(InViewport);
#else
	return false;
#endif
}

#if GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS
void FGoogleVRHMD::PostRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
	if (ReadbackTextureCount < SentTextureCount + kReadbackTextureCount) {
		int textureIndex = ReadbackTextureCount % kReadbackTextureCount;
		FIntPoint renderSize = InViewFamily.RenderTarget->GetSizeXY();
		if (ReadbackTextures[textureIndex] == nullptr ||
			ReadbackTextureSizes[textureIndex].X != renderSize.X ||
			ReadbackTextureSizes[textureIndex].Y != renderSize.Y) {
			if (ReadbackTextures[textureIndex] != nullptr) {
				ReadbackTextures[textureIndex].SafeRelease();
				ReadbackTextures[textureIndex] = nullptr;
			}
			FRHIResourceCreateInfo CreateInfo;
			ReadbackTextures[textureIndex] = RHICreateTexture2D(
				renderSize.X,
				renderSize.Y,
				PF_B8G8R8A8,
				1,
				1,
				TexCreate_CPUReadback,
				CreateInfo);
			check(ReadbackTextures[textureIndex].GetReference());
			ReadbackTextureSizes[textureIndex] = renderSize;
		}
		ReadbackCopyQueries[ReadbackTextureCount % kReadbackTextureCount] = RHICmdList.CreateRenderQuery(ERenderQueryType::RQT_AbsoluteTime);
		
		// Absolute time query creation can fail on AMD hardware due to driver support
		if (!ReadbackCopyQueries[ReadbackTextureCount % kReadbackTextureCount])
		{
			return;
		}
		
		// copy and map the texture.
		FPooledRenderTargetDesc OutputDesc(FPooledRenderTargetDesc::Create2DDesc(ReadbackTextureSizes[textureIndex], PF_B8G8R8A8, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable, false));
		const auto FeatureLevel = GMaxRHIFeatureLevel;
		TRefCountPtr<IPooledRenderTarget> ResampleTexturePooledRenderTarget;
		RendererModule->RenderTargetPoolFindFreeElement(RHICmdList, OutputDesc, ResampleTexturePooledRenderTarget, TEXT("ResampleTexture"));
		check(ResampleTexturePooledRenderTarget);
		const FSceneRenderTargetItem& DestRenderTarget = ResampleTexturePooledRenderTarget->GetRenderTargetItem();
		SetRenderTarget(RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
		RHICmdList.SetViewport(0, 0, 0.0f, ReadbackTextureSizes[textureIndex].X, ReadbackTextureSizes[textureIndex].Y, 1.0f);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

		auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
		TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
		TShaderMapRef<FScreenPS> PixelShader(ShaderMap);
		
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = RendererModule->GetFilterVertexDeclaration().VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;
		
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);


		PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Point>::GetRHI(),
			InViewFamily.RenderTarget->GetRenderTargetTexture());
		RendererModule->DrawRectangle(
			RHICmdList,
			0, 0,		// Dest X, Y
			ReadbackTextureSizes[textureIndex].X, ReadbackTextureSizes[textureIndex].Y,	// Dest Width, Height
			0, 0,		// Source U, V
			1, 1,		// Source USize, VSize
			ReadbackTextureSizes[textureIndex],		// Target buffer size
			FIntPoint(1, 1),		// Source texture size
			*VertexShader,
			EDRF_Default);
		// Asynchronously copy delayed render target from GPU to CPU
		const bool bKeepOriginalSurface = false;
		RHICmdList.CopyToResolveTarget(
			DestRenderTarget.TargetableTexture,
			ReadbackTextures[ReadbackTextureCount % kReadbackTextureCount],
			bKeepOriginalSurface,
			FResolveParams());
		ReadbackReferencePoses[ReadbackTextureCount % kReadbackTextureCount] = RenderReferencePose;
		RHICmdList.EndRenderQuery(ReadbackCopyQueries[ReadbackTextureCount % kReadbackTextureCount]);

		ReadbackTextureCount++;
	}
	
	uint64 result = 0;
	bool isTextureReadyForReadback = false;
	while (SentTextureCount < ReadbackTextureCount && RHICmdList.GetRenderQueryResult(ReadbackCopyQueries[SentTextureCount % kReadbackTextureCount], result, false)) {
		isTextureReadyForReadback = true;
		SentTextureCount++;
	}

	if (isTextureReadyForReadback) {
		int latestReadbackTextureIndex = (SentTextureCount - 1) % kReadbackTextureCount;
		GDynamicRHI->RHIReadSurfaceData(
			ReadbackTextures[latestReadbackTextureIndex],
			FIntRect(FIntPoint(0, 0),
			ReadbackTextureSizes[latestReadbackTextureIndex]),
			ReadbackData,
			FReadSurfaceDataFlags());

		PushVideoFrame(ReadbackData.GetData(),
			ReadbackTextureSizes[latestReadbackTextureIndex].X,
			ReadbackTextureSizes[latestReadbackTextureIndex].Y,
			ReadbackTextureSizes[latestReadbackTextureIndex].X * 4,
			instant_preview::PIXEL_FORMAT_BGRA,
			ReadbackReferencePoses[latestReadbackTextureIndex]);
	}
}
#endif  // GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS

#if GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS
bool FGoogleVRHMD::GetCurrentReferencePose(FQuat& CurrentOrientation, FVector& CurrentPosition) const 
{
	FMatrix TransposeHeadPoseUnreal;
	memcpy(&TransposeHeadPoseUnreal.M, CurrentReferencePose.pose.transform, 4 * 4 * sizeof(float));
	FMatrix FinalHeadPoseUnreal = TransposeHeadPoseUnreal.GetTransposed();
	FMatrix FinalHeadPoseInverseUnreal = FinalHeadPoseUnreal.Inverse();
	const float WorldToMetersScale = GetWorldToMetersScale();
	CurrentPosition = FVector(
		-FinalHeadPoseInverseUnreal.M[2][3] * WorldToMetersScale,
		FinalHeadPoseInverseUnreal.M[0][3] * WorldToMetersScale,
		FinalHeadPoseInverseUnreal.M[1][3] * WorldToMetersScale
	);
	CurrentOrientation = FQuat(FinalHeadPoseUnreal);
	CurrentOrientation = FQuat(-CurrentOrientation.Z, CurrentOrientation.X, CurrentOrientation.Y, -CurrentOrientation.W);
	return true;
}

FVector FGoogleVRHMD::GetLocalEyePos(const instant_preview::EyeView& EyeView) 
{
	const float* mat = EyeView.eye_pose.transform;
	FMatrix PoseMatrix(
		FPlane(mat[0], mat[1], mat[2], mat[3]),
		FPlane(mat[4], mat[5], mat[6], mat[7]),
		FPlane(mat[8], mat[9], mat[10], mat[11]),
		FPlane(mat[12], mat[13], mat[14], mat[15]));
	return PoseMatrix.TransformPosition(FVector(0, 0, 0));
}

void FGoogleVRHMD::PushVideoFrame(const FColor* VideoFrameBuffer, int width, int height, int stride, instant_preview::PixelFormat pixel_format, instant_preview::ReferencePose reference_pose) 
{
	instant_preview::Session *session = ip_static_server_acquire_active_session(IpServerHandle);
	if (session != NULL && width > 0 && height > 0)
	{
		session->send_frame(reinterpret_cast<const uint8_t *>(VideoFrameBuffer), pixel_format, width, height, stride, reference_pose, InstantPreviewConstants::kBitrateKbps);
	}
	ip_static_server_release_active_session(IpServerHandle, session);
}
#endif  // GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS

bool FGoogleVRHMD::IsStereoEnabled() const
{
	return bStereoEnabled && bHMDEnabled;
}

bool FGoogleVRHMD::EnableStereo(bool stereo)
{
#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS
	// We will not allow stereo rendering to be disabled when using async reprojection
	if(bUseOffscreenFramebuffers && !stereo)
	{
		UE_LOG(LogHMD, Warning, TEXT("Attemp to disable stereo rendering when using async reprojection. This is not support so the operation will be ignored!"));
		return true;
	}
	AndroidThunkCpp_UiLayer_SetEnabled(stereo);
#endif

	bStereoEnabled = stereo;
	GEngine->bForceDisableFrameRateSmoothing = bStereoEnabled;
	return bStereoEnabled;
}

FMatrix FGoogleVRHMD::GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType) const
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS

	check(ActiveViewportList);
	check(gvr_buffer_viewport_list_get_size(ActiveViewportList) == 2);
	switch(StereoPassType)
	{
		case EStereoscopicPass::eSSP_LEFT_EYE:
			gvr_buffer_viewport_list_get_item(ActiveViewportList, 0, ScratchViewport);
			break;
		case EStereoscopicPass::eSSP_RIGHT_EYE:
			gvr_buffer_viewport_list_get_item(ActiveViewportList, 1, ScratchViewport);
			break;
		default:
			// We shouldn't got here.
			check(false);
			break;
	}

	gvr_rectf EyeFov = gvr_buffer_viewport_get_source_fov(ScratchViewport);
	//UE_LOG(LogHMD, Log, TEXT("Eye %d FOV: Left - %f, Right - %f, Top - %f, Botton - %f"), StereoPassType, EyeFov.left, EyeFov.right, EyeFov.top, EyeFov.bottom);

	// Have to flip left/right and top/bottom to match UE4 expectations
	float Right = FPlatformMath::Tan(FMath::DegreesToRadians(EyeFov.left));
	float Left = -FPlatformMath::Tan(FMath::DegreesToRadians(EyeFov.right));
	float Bottom = -FPlatformMath::Tan(FMath::DegreesToRadians(EyeFov.top));
	float Top = FPlatformMath::Tan(FMath::DegreesToRadians(EyeFov.bottom));

	float ZNear = GNearClippingPlane;

	float SumRL = (Right + Left);
	float SumTB = (Top + Bottom);
	float InvRL = (1.0f / (Right - Left));
	float InvTB = (1.0f / (Top - Bottom));

#if LOG_VIEWER_DATA_FOR_GENERATION

	FPlane Plane0 = FPlane((2.0f * InvRL), 0.0f, 0.0f, 0.0f);
	FPlane Plane1 = FPlane(0.0f, (2.0f * InvTB), 0.0f, 0.0f);
	FPlane Plane2 = FPlane((SumRL * InvRL), (SumTB * InvTB), 0.0f, 1.0f);
	FPlane Plane3 = FPlane(0.0f, 0.0f, ZNear, 0.0f);

	const TCHAR* EyeString = StereoPassType == eSSP_LEFT_EYE ? TEXT("Left") : TEXT("Right");
	UE_LOG(LogHMD, Log, TEXT("===== Begin Projection Matrix Eye %s"), EyeString);
	UE_LOG(LogHMD, Log, TEXT("const FMatrix %sStereoProjectionMatrix = FMatrix("), EyeString);
	UE_LOG(LogHMD, Log, TEXT("FPlane(%ff,  0.0f, 0.0f, 0.0f),"), Plane0.X);
	UE_LOG(LogHMD, Log, TEXT("FPlane(0.0f, %ff,  0.0f, 0.0f),"), Plane1.Y);
	UE_LOG(LogHMD, Log, TEXT("FPlane(%ff,  %ff,  0.0f, 1.0f),"), Plane2.X, Plane2.Y);
	UE_LOG(LogHMD, Log, TEXT("FPlane(0.0f, 0.0f, %ff,  0.0f)"), Plane3.Z);
	UE_LOG(LogHMD, Log, TEXT(");"));
	UE_LOG(LogHMD, Log, TEXT("===== End Projection Matrix Eye %s"));

	return FMatrix(
		Plane0,
		Plane1,
		Plane2,
		Plane3
		);

#else

	return FMatrix(
		FPlane((2.0f * InvRL), 0.0f, 0.0f, 0.0f),
		FPlane(0.0f, (2.0f * InvTB), 0.0f, 0.0f),
		FPlane((SumRL * InvRL), (SumTB * InvTB), 0.0f, 1.0f),
		FPlane(0.0f, 0.0f, ZNear, 0.0f)
		);

#endif // LOG_VIEWER_DATA_FOR_GENERATION

#else //!GOOGLEVRHMD_SUPPORTED_PLATFORMS
#if GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS
	if (bIsInstantPreviewActive) {
		int index = (StereoPassType == eSSP_LEFT_EYE) ? 0 : 1;
		// Have to flip left/right and top/bottom to match UE4 expectations
		float Right = FPlatformMath::Tan(FMath::DegreesToRadians(EyeViews.eye_views[index].eye_fov.left));
		float Left = -FPlatformMath::Tan(FMath::DegreesToRadians(EyeViews.eye_views[index].eye_fov.right));
		float Bottom = -FPlatformMath::Tan(FMath::DegreesToRadians(EyeViews.eye_views[index].eye_fov.top));
		float Top = FPlatformMath::Tan(FMath::DegreesToRadians(EyeViews.eye_views[index].eye_fov.bottom));
		float ZNear = GNearClippingPlane;
		float SumRL = (Right + Left);
		float SumTB = (Top + Bottom);
		float InvRL = (1.0f / (Right - Left));
		float InvTB = (1.0f / (Top - Bottom));
		return FMatrix(
			FPlane((2.0f * InvRL), 0.0f, 0.0f, 0.0f),
			FPlane(0.0f, (2.0f * InvTB), 0.0f, 0.0f),
			FPlane((SumRL * InvRL), (SumTB * InvTB), 0.0f, 1.0f),
			FPlane(0.0f, 0.0f, ZNear, 0.0f)
		);
	}
	else
#endif  // GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS
	if(GetPreviewViewerType() == EVP_None)
	{
		// Test data copied from SimpleHMD
		const float ProjectionCenterOffset = 0.151976421f;
		const float PassProjectionOffset = (StereoPassType == eSSP_LEFT_EYE) ? ProjectionCenterOffset : -ProjectionCenterOffset;

		const float HalfFov = 2.19686294f / 2.f;
		const float InWidth = 640.f;
		const float InHeight = 480.f;
		const float XS = 1.0f / tan(HalfFov);
		const float YS = InWidth / tan(HalfFov) / InHeight;

		const float InNearZ = GNearClippingPlane;
		return FMatrix(
			FPlane(XS,                      0.0f,								    0.0f,							0.0f),
			FPlane(0.0f,					YS,	                                    0.0f,							0.0f),
			FPlane(0.0f,	                0.0f,								    0.0f,							1.0f),
			FPlane(0.0f,					0.0f,								    InNearZ,						0.0f))

			* FTranslationMatrix(FVector(PassProjectionOffset,0,0));
	}
	else
	{
		return GetPreviewViewerStereoProjectionMatrix(StereoPassType);
	}

#endif // GOOGLEVRHMD_SUPPORTED_PLATFORMS
}

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
gvr_rectf FGoogleVRHMD::GetGVREyeFOV(int EyeIndex) const
{
	gvr_buffer_viewport_list_get_item(ActiveViewportList, EyeIndex, ScratchViewport);
	gvr_rectf EyeFov = gvr_buffer_viewport_get_source_fov(ScratchViewport);

	return EyeFov;
}
#endif

void FGoogleVRHMD::GetEyeRenderParams_RenderThread(const struct FRenderingCompositePassContext& Context, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const
{
	if (Context.View.StereoPass == eSSP_LEFT_EYE)
	{
		EyeToSrcUVOffsetValue.X = 0.0f;
		EyeToSrcUVOffsetValue.Y = 0.0f;

		EyeToSrcUVScaleValue.X = 0.5f;
		EyeToSrcUVScaleValue.Y = 1.0f;
	}
	else
	{
		EyeToSrcUVOffsetValue.X = 0.5f;
		EyeToSrcUVOffsetValue.Y = 0.0f;

		EyeToSrcUVScaleValue.X = 0.5f;
		EyeToSrcUVScaleValue.Y = 1.0f;
	}
}

void FGoogleVRHMD::UpdateViewportRHIBridge(bool bUseSeparateRenderTarget, const class FViewport& InViewport, FRHIViewport* const ViewportRHI)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS

	check(CustomPresent);
	CustomPresent->UpdateViewport(InViewport, ViewportRHI);

#endif // GOOGLEVRHMD_SUPPORTED_PLATFORMS
}

void FGoogleVRHMD::CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY)
{
	check(IsInGameThread());

	// Change the render target size when it is valid
	if (GVRRenderTargetSize.X != 0 && GVRRenderTargetSize.Y != 0)
	{
		InOutSizeX = GVRRenderTargetSize.X;
		InOutSizeY = GVRRenderTargetSize.Y;
	}
}

bool FGoogleVRHMD::ShouldUseSeparateRenderTarget() const
{
	check(IsInGameThread());
	return IsStereoEnabled() && bUseGVRApiDistortionCorrection;
}

bool FGoogleVRHMD::IsHMDConnected()
{
	// Just uses regular screen, so this is always true!
	return true;
}

bool FGoogleVRHMD::IsHMDEnabled() const
{
	return bHMDEnabled;
}

void FGoogleVRHMD::EnableHMD(bool bEnable)
{
#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS
	// We will not allow stereo rendering to be disabled when using async reprojection
	if(bUseOffscreenFramebuffers && !bEnable)
	{
		UE_LOG(LogHMD, Warning, TEXT("Attemp to disable HMD when using async reprojection. This is not support so the operation will be ignored!"));
		return;
	}
#endif
	bHMDEnabled = bEnable;
	if(!bHMDEnabled)
	{
		EnableStereo(false);
	}
}

EHMDDeviceType::Type FGoogleVRHMD::GetHMDDeviceType() const
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	return EHMDDeviceType::DT_ES2GenericStereoMesh;
#else
	return EHMDDeviceType::DT_GoogleVR; // Workaround needed for non-es2 post processing to call PostProcessHMD
#endif
}

bool FGoogleVRHMD::GetHMDMonitorInfo(MonitorInfo& OutMonitorInfo)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	if(!IsStereoEnabled())
	{
		return false;
	}

	OutMonitorInfo.MonitorName = FString::Printf(TEXT("%s - %s"), UTF8_TO_TCHAR(gvr_get_viewer_vendor(GVRAPI)), UTF8_TO_TCHAR(gvr_get_viewer_model(GVRAPI)));
	OutMonitorInfo.MonitorId = 0;
	OutMonitorInfo.DesktopX = OutMonitorInfo.DesktopY = OutMonitorInfo.ResolutionX = OutMonitorInfo.ResolutionY = 0;
	OutMonitorInfo.WindowSizeX = OutMonitorInfo.WindowSizeY = 0;

	// For proper scaling, and since hardware scaling is used, return the calculated size and not the actual device size
	// TODO: We are using the screen resolution to tune the rendering scale. Revisit here if we want to hook up the gvr
	// gvr_get_recommended_render_target_size function.
#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS
	FPlatformRect Rect = FAndroidWindow::GetScreenRect();
	OutMonitorInfo.ResolutionX = Rect.Right - Rect.Left;
	OutMonitorInfo.ResolutionY = Rect.Bottom - Rect.Top;
#elif GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS
	FPlatformRect Rect = FIOSWindow::GetScreenRect();
	OutMonitorInfo.ResolutionX = Rect.Right - Rect.Left;
	OutMonitorInfo.ResolutionY = Rect.Bottom - Rect.Top;
#else
	gvr_recti Bounds = gvr_get_window_bounds(GVRAPI);
	OutMonitorInfo.ResolutionX = Bounds.right - Bounds.left;
	OutMonitorInfo.ResolutionY = Bounds.top - Bounds.bottom;
#endif

	return true;
#else
	OutMonitorInfo.MonitorName = "UnsupportedGoogleVRHMDPlatform";
	OutMonitorInfo.MonitorId = 0;
	OutMonitorInfo.DesktopX = OutMonitorInfo.DesktopY = OutMonitorInfo.ResolutionX = OutMonitorInfo.ResolutionY = 0;
	OutMonitorInfo.WindowSizeX = OutMonitorInfo.WindowSizeY = 0;

	return false;
#endif
}

void FGoogleVRHMD::GetFieldOfView(float& InOutHFOVInDegrees, float& InOutVFOVInDegrees) const
{
	InOutHFOVInDegrees = 0.0f;
	InOutVFOVInDegrees = 0.0f;
}

void FGoogleVRHMD::SetInterpupillaryDistance(float NewInterpupillaryDistance)
{
	// Nothing
}

float FGoogleVRHMD::GetInterpupillaryDistance() const
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS || GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS
	// For simplicity, the interpupillary distance is the distance to the left eye, doubled.
	FQuat Unused;
	FVector Offset;

	GetRelativeHMDEyePose(EStereoscopicPass::eSSP_LEFT_EYE, Unused, Offset);
	return Offset.Size() * 2.0f;
#else  // GOOGLEVRHMD_SUPPORTED_PLATFORMS || GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS
	return GetPreviewViewerInterpupillaryDistance();
#endif  // GOOGLEVRHMD_SUPPORTED_PLATFORMS
}

bool FGoogleVRHMD::GetRelativeEyePose(int32 DeviceId, EStereoscopicPass Eye, FQuat& OutOrientation, FVector& OutPosition)
{
	if (DeviceId != IXRTrackingSystem::HMDDeviceId || !(Eye == eSSP_LEFT_EYE || Eye == eSSP_RIGHT_EYE))
	{
		return false;
	}
	else
	{
		GetRelativeHMDEyePose(Eye, OutOrientation, OutPosition);
		return true;
	}
}

void FGoogleVRHMD::GetRelativeHMDEyePose(EStereoscopicPass Eye, FQuat& OutOrientation, FVector& OutPosition) const
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	gvr_mat4f EyeMat = gvr_get_eye_from_head_matrix(GVRAPI, (Eye == eSSP_LEFT_EYE?GVR_LEFT_EYE:GVR_RIGHT_EYE));
	OutPosition = FVector(-EyeMat.m[2][3], -EyeMat.m[0][3], EyeMat.m[1][3]) * GetWorldToMetersScale();
	FQuat Orientation(ToFMatrix(EyeMat));

	OutOrientation.X = -Orientation.Z;
	OutOrientation.Y = Orientation.X;
	OutOrientation.Z = Orientation.Y;
	OutOrientation.W = -Orientation.W;
#elif  GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS
	instant_preview::Pose EyePose = EyeViews.eye_views[(Eye == eSSP_LEFT_EYE ? 0 : 1)].eye_pose;
	OutPosition = FVector(-EyePose.transform[14], -EyePose.transform[12], EyePose.transform[13]) * GetWorldToMetersScale();
	OutOrientation = FQuat::Identity; // TODO: extract orientation from transform?
#else  // GOOGLEVRHMD_SUPPORTED_PLATFORMS || GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS
	OutPosition = FVector(0, (Eye == eSSP_LEFT_EYE ? .5 : -.5) * GetPreviewViewerInterpupillaryDistance() * GetWorldToMetersScale(), 0);
	OutOrientation = FQuat::Identity; // TODO: extract orientation from transform?
#endif  // GOOGLEVRHMD_SUPPORTED_PLATFORMS
}


bool FGoogleVRHMD::IsChromaAbCorrectionEnabled() const
{
	return false;
}

bool FGoogleVRHMD::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	const TCHAR* OrigCmd = Cmd;
	FString AliasedCommand;

	if (FParse::Command(&Cmd, TEXT("googlevr.ViewerPreview")) || FParse::Command(&Cmd, TEXT("googlevr.PreviewSensitivity")))
	{
		AliasedCommand = FString::Printf(TEXT("vr.%s"), OrigCmd);
	}
	else if (FParse::Command(&Cmd, TEXT("DISTORT")))
	{
		FString Value = FParse::Token(Cmd, 0);
		if (Value.Equals(TEXT("ON"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("OFF"), ESearchCase::IgnoreCase))
		{
			AliasedCommand = FString::Printf(TEXT("vr.googlevr.DistortionCorrection.bEnable %s"), *Value.ToLower());
		}
		else if (Value.Equals(TEXT("PPHMD"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("GVRAPI"), ESearchCase::IgnoreCase))
		{
			AliasedCommand = FString::Printf(TEXT("vr.googlevr.DistortionCorrection.Method %s"), *Value.ToLower());
		}

		return false;
	}
	else if (FParse::Command(&Cmd, TEXT("GVRRENDERSIZE")))
	{
		int Width, Height;
		float ScaleFactor;
		FIntPoint ActualSize;
		if (FParse::Value(Cmd, TEXT("W="), Width) && FParse::Value(Cmd, TEXT("H="), Height))
		{
			AliasedCommand = FString::Printf(TEXT("vr.googlevr.RenderTargetSize %d %d"), Width, Height);
		}
		else if (FParse::Value(Cmd, TEXT("S="), ScaleFactor))
		{
			AliasedCommand = FString::Printf(TEXT("r.ScreenPercentage %.0f"), ScaleFactor*100.f);
		}
		else if (FParse::Command(&Cmd, TEXT("RESET")))
		{
			AliasedCommand = TEXT("vr.googlevr.RenderTargetSize reset");
		}

		return false;
	}
	else if (FParse::Command(&Cmd, TEXT("GVRNECKMODELSCALE")))
	{
		FString ScaleFactor;
		if (FParse::Value(Cmd, TEXT("Factor="), ScaleFactor))
		{
			AliasedCommand = FString::Printf(TEXT("vr.googlevr.NeckModelScale %s"), *ScaleFactor);
		}

		return false;
	}
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	// Tune the distortion mesh vert count when use Unreal's PostProcessing Distortion
	else if (FParse::Command(&Cmd, TEXT("DISTORTMESH")))
	{
		const static UEnum* MeshSizeEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EDistortionMeshSizeEnum"));
		FString Value = FParse::Token(Cmd, 0);

		if (!Value.IsEmpty() && MeshSizeEnum->GetIndexByName(*FString::Printf(TEXT("DMS_%s"), *Value.ToUpper())) != INDEX_NONE)
		{
			AliasedCommand = FString::Printf(TEXT("vr.googlevr.DistortionMesh %s"), *Value.ToLower());
		}
		return false;
	}
	else if (FParse::Command(&Cmd, TEXT("GVRSPLASH")))
	{
		AliasedCommand = FString::Printf(TEXT("vr.googlevr.bShowSplash %s"), FParse::Command(&Cmd, TEXT("SHOW"))?TEXT("True"):TEXT("False") );
	}
#endif

	if (!AliasedCommand.IsEmpty())
	{
		Ar.Logf(ELogVerbosity::Warning, TEXT("%s is deprecated. Use %s instead"), OrigCmd, *AliasedCommand);
		return IConsoleManager::Get().ProcessUserConsoleInput(*AliasedCommand, Ar, InWorld);
	}
	return false;
}

void FGoogleVRHMD::DistortEnableCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	if (Args.Num())
	{
		bool bShouldEnable = FCString::ToBool(*Args[0]);
		SetDistortionCorrectionEnabled(bShouldEnable);
	}
}

void FGoogleVRHMD::DistortMethodCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	if (Args.Num())
	{
		if (Args[0].Equals(TEXT("PPHMD"), ESearchCase::IgnoreCase))
		{
			SetDistortionCorrectionMethod(false);
		}
		else if (Args[0].Equals(TEXT("GVRAPI"), ESearchCase::IgnoreCase))
		{
			SetDistortionCorrectionMethod(true);
		}
		else
		{
			Ar.Logf(ELogVerbosity::Error, TEXT("Invalid argument '%s'. Use gvrapi or pphmd"), *Args[0]);
		}
	}
}

void FGoogleVRHMD::RenderTargetSizeCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	FIntPoint ActualSize;
	if (Args.Num())
	{
		if (Args.Num() == 1 && Args[0].Equals(TEXT("reset"),ESearchCase::IgnoreCase))
		{
			SetRenderTargetSizeToDefault();
			ActualSize = GVRRenderTargetSize;
		}
		else if (Args.Num() == 2 && FCString::IsNumeric(*Args[0]) && FCString::IsNumeric(*Args[1]))
		{
			SetGVRHMDRenderTargetSize(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1]), ActualSize);
		}
		else
		{
			Ar.Logf(ELogVerbosity::Error, TEXT("Usage: vr.googlevr.RenderTargetSize [reset|<width> <height>]"));
			return;
		}
	}
	else
	{
		ActualSize = GVRRenderTargetSize;
	}
	Ar.Logf(ELogVerbosity::Display, TEXT("vr.googlevr.RenderTargetSize = %d %d"), ActualSize.X, ActualSize.Y);
}

void FGoogleVRHMD::NeckModelScaleCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	if (Args.Num())
	{
		float ScaleFactor = FCString::Atof(*Args[0]);
		SetNeckModelScale(ScaleFactor);
	}
}

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
void FGoogleVRHMD::DistortMeshSizeCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	const static UEnum* MeshSizeEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EDistortionMeshSizeEnum"));
	int EnumIndex = INDEX_NONE;

	if (Args.Num())
	{
		if (FCString::IsNumeric(*Args[0]))
		{
			EnumIndex = FCString::Atoi(*Args[0]);
		}
		else
		{
			EnumIndex = MeshSizeEnum->GetIndexByName(*Args[0]);
		}
		if (!MeshSizeEnum->IsValidEnumValue(EnumIndex))
		{
			Ar.Logf(ELogVerbosity::Error, TEXT("Invalid distort mesh size, %s"), *Args[0]);
			return;
		}
		else
		{
			SetDistortionMeshSize(EDistortionMeshSizeEnum(EnumIndex));
		}
	}
}

void FGoogleVRHMD::ShowSplashCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	static FDelegateHandle Handle;
	if (Args.Num() && GVRSplash.IsValid())
	{
		bDebugShowGVRSplash = FCString::ToBool(*Args[0]);
		if (bDebugShowGVRSplash)
		{
			GVRSplash->Show();
		}
		else
		{
			GVRSplash->Hide();
		}
	}
}

void FGoogleVRHMD::SplashScreenDistanceCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	static FDelegateHandle Handle;
	if (Args.Num() && GVRSplash.IsValid())
	{
		const float distance = FCString::Atof(*Args[0]);
		if (distance >=0)
		{
			GVRSplash->RenderDistanceInMeter = distance;
		}
		else
		{
			Ar.Logf(ELogVerbosity::Error, TEXT("Invalid SplashScreenDistance, %s"), *Args[0]);
		}
	}
}

void FGoogleVRHMD::SplashScreenRenderScaleCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	static FDelegateHandle Handle;
	if (Args.Num() && GVRSplash.IsValid())
	{
		const float scale = FCString::Atof(*Args[0]);
		if (scale > 0)
		{
			GVRSplash->RenderScale = scale;
		}
		else
		{
			Ar.Logf(ELogVerbosity::Error, TEXT("Invalid SplashScreenRenderScale, %s"), *Args[0]);
		}
	}
}

void FGoogleVRHMD::EnableSustainedPerformanceModeHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	static FDelegateHandle Handle;
	if (Args.Num() && GVRSplash.IsValid())
	{
		const bool Enabled = FCString::ToBool(*Args[0]);
		SetSPMEnable(Enabled);
	}
}

void FGoogleVRHMD::CVarSinkHandler()
{
	static const auto ScreenPercentageCVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.ScreenPercentage"));
	static float PreviousValue = ScreenPercentageCVar->GetValueOnAnyThread();

	float CurrentValue = ScreenPercentageCVar->GetValueOnAnyThread();
	if (CurrentValue != PreviousValue)
	{
		FIntPoint ActualSize;
		SetGVRHMDRenderTargetSize(CurrentValue / 100.f, ActualSize);
		PreviousValue = CurrentValue;
	}
}
#endif

void FGoogleVRHMD::ResetOrientationAndPosition(float Yaw)
{
	ResetOrientation(Yaw);
	ResetPosition();
}

bool FGoogleVRHMD::DoesSupportPositionalTracking() const
{
	// Does not support position tracking, only pose
	return Is6DOFSupported();
}

void FGoogleVRHMD::ResetOrientation(float Yaw)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	gvr_reset_tracking(GVRAPI);
#else
	PoseYaw = 0;
#endif
	SetBaseOrientation(FRotator(0.0f, Yaw, 0.0f).Quaternion());
}

void FGoogleVRHMD::SetBaseRotation(const FRotator& BaseRot)
{
	SetBaseOrientation(FRotator(0.0f, BaseRot.Yaw, 0.0f).Quaternion());
}

FRotator FGoogleVRHMD::GetBaseRotation() const
{
	return GetBaseOrientation().Rotator();
}

void FGoogleVRHMD::SetBaseOrientation(const FQuat& BaseOrient)
{
	BaseOrientation = BaseOrient;
}

FQuat FGoogleVRHMD::GetBaseOrientation() const
{
	return BaseOrientation;
}

bool FGoogleVRHMD::HandleInputKey(UPlayerInput *, const FKey & Key, EInputEvent EventType, float AmountDepressed, bool bGamepad)
{
#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS
	if (Key == EKeys::Android_Back)
	{
		if (EventType == IE_Pressed)
		{
			BackbuttonPressTime = FPlatformTime::Seconds();
		}
		else if (EventType == IE_Released)
		{
			if (FPlatformTime::Seconds() - BackbuttonPressTime < BACK_BUTTON_SHORT_PRESS_TIME)
			{
				// Add default back button behavior in Daydream Mode
				if (bIsInDaydreamMode)
				{
					AndroidThunkCpp_QuitDaydreamApplication();
					return true;
				}
			}
		}
	}
#endif
	return false;
}

bool FGoogleVRHMD::HandleInputTouch(uint32 Handle, ETouchType::Type Type, const FVector2D& TouchLocation, FDateTime DeviceTimestamp, uint32 TouchpadIndex)
{
	return false;
}

bool FGoogleVRHMD::EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type /*= EXRTrackedDeviceType::Any*/)
{
	if (Type == EXRTrackedDeviceType::Any || Type == EXRTrackedDeviceType::HeadMountedDisplay)
	{
		OutDevices.Add(IXRTrackingSystem::HMDDeviceId);
		return true;
	}
	return false;
}

void FGoogleVRHMD::RefreshPoses()
{
	if (IsInRenderingThread())
	{
		// Currently, attempting to update the pose on the render thread is a no-op.
		return;
	}
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	// Update CachedHeadPose
	CachedFuturePoseTime = gvr_get_time_point_now();
	CachedFuturePoseTime.monotonic_system_time_nanos += kPredictionTime;

	if (Is6DOFSupported())
	{
		CachedHeadPose = gvr_get_head_space_from_start_space_transform(GVRAPI, CachedFuturePoseTime);
	}
	else
	{
		gvr_mat4f HeadRotation = gvr_get_head_space_from_start_space_rotation(GVRAPI, CachedFuturePoseTime);

		// Apply the neck model to calculate the final pose
		CachedHeadPose = gvr_apply_neck_model(GVRAPI, HeadRotation, NeckModelScale);
	}

	// Convert the final pose into Unreal data type
	FMatrix FinalHeadPoseUnreal;
	memcpy(FinalHeadPoseUnreal.M[0], CachedHeadPose.m[0], 4 * 4 * sizeof(float));

	// Inverse the view matrix so we can get the world position of the pose
	FMatrix FinalHeadPoseInverseUnreal = FinalHeadPoseUnreal.Inverse();

	// Number of Unreal Units per meter.
	const float WorldToMetersScale = GetWorldToMetersScale();

	// Gvr is using a openGl Right Handed coordinate system, UE is left handed.
	// The following code is converting the gvr coordinate system to UE coordinates.

	// Gvr: Negative Z is Forward, UE: Positive X is Forward.
	CachedFinalHeadPosition.X = -FinalHeadPoseInverseUnreal.M[2][3] * WorldToMetersScale;

	// Gvr: Positive X is Right, UE: Positive Y is Right.
	CachedFinalHeadPosition.Y = FinalHeadPoseInverseUnreal.M[0][3] * WorldToMetersScale;

	// Gvr: Positive Y is Up, UE: Positive Z is Up
	CachedFinalHeadPosition.Z = FinalHeadPoseInverseUnreal.M[1][3] * WorldToMetersScale;

	// Convert Gvr right handed coordinate system rotation into UE left handed coordinate system.
	CachedFinalHeadRotation = FQuat(FinalHeadPoseUnreal);
	CachedFinalHeadRotation = FQuat(-CachedFinalHeadRotation.Z, CachedFinalHeadRotation.X, CachedFinalHeadRotation.Y, -CachedFinalHeadRotation.W);
#elif GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS
	instant_preview::Session* session = ip_static_server_acquire_active_session(IpServerHandle);
	if (NULL != session && instant_preview::RESULT_SUCCESS == session->get_latest_pose(&CurrentReferencePose)) {
		session->get_eye_views(&EyeViews);
		bIsInstantPreviewActive = true;
	} else {
		bIsInstantPreviewActive = false;
	}
	ip_static_server_release_active_session(IpServerHandle, session);
#endif
}

bool FGoogleVRHMD::OnStartGameFrame( FWorldContext& WorldContext )
{
	//UE_LOG(LogTemp, Log, TEXT("Start Game Frame!!!"));

	// Handle back coming from viewer magnet clickers or ui layer
	if(bBackDetected)
	{
		HandleGVRBackEvent();
		bBackDetected = false;
	}

	if(bTriggerDetected)
	{
		if( GEngine &&
			GEngine->GameViewport &&
			GEngine->GameViewport->Viewport &&
			GEngine->GameViewport->Viewport->GetClient() )
		{
			GEngine->GameViewport->Viewport->GetClient()->InputTouch(GEngine->GameViewport->Viewport, 0, 0, ETouchType::Began, FVector2D(-1, -1), FDateTime::Now(), 0);
			GEngine->GameViewport->Viewport->GetClient()->InputTouch(GEngine->GameViewport->Viewport, 0, 0, ETouchType::Ended, FVector2D(-1, -1), FDateTime::Now(), 0);
		}
		bTriggerDetected = false;
	}

	//Update the head pose at the begnning of a frame. This headpose will be used for both simulation and rendering.
	RefreshPoses();

	// Update ViewportList from GVR API
	UpdateGVRViewportList();

	// Enable scene present after OnStartGameFrame get called.
	bForceStopPresentScene = false;
	return false;
}

FString FGoogleVRHMD::GetVersionString() const
{
	FString s = FString::Printf(TEXT("GoogleVR - %s, VrLib: %s, built %s, %s"), *FEngineVersion::Current().ToString(), TEXT("GVR"),
		UTF8_TO_TCHAR(__DATE__), UTF8_TO_TCHAR(__TIME__));
	return s;
}

bool FGoogleVRHMD::GetFloorHeight(float* FloorHeight)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	gvr::Value value_out = gvr::Value();
	if (TryReadProperty(GVR_PROPERTY_TRACKING_FLOOR_HEIGHT, &value_out))
	{
		*FloorHeight = value_out.f;
		return true;
	}
#endif
	return false;
}

bool FGoogleVRHMD::GetSafetyCylinderInnerRadius(float* InnerRadius)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	gvr::Value value_out = gvr::Value();
	if (TryReadProperty(GVR_PROPERTY_SAFETY_CYLINDER_INNER_RADIUS, &value_out))
	{
		*InnerRadius = value_out.f;
		return true;
	}
#endif
	return false;
}

bool FGoogleVRHMD::GetSafetyCylinderOuterRadius(float* OuterRadius)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	gvr::Value value_out = gvr::Value();
	if (TryReadProperty(GVR_PROPERTY_SAFETY_CYLINDER_OUTER_RADIUS, &value_out))
	{
		*OuterRadius = value_out.f;
		return true;
	}
#endif
	return false;
}

bool FGoogleVRHMD::GetSafetyRegion(ESafetyRegionType* RegionType)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	gvr::Value value_out = gvr::Value();
	if (TryReadProperty(GVR_PROPERTY_SAFETY_REGION, &value_out))
	{
		*RegionType = ESafetyRegionType::INVALID;
		if (value_out.i == gvr_safety_region_type::GVR_SAFETY_REGION_CYLINDER)
		{
			*RegionType = ESafetyRegionType::CYLINDER;
		}
		return true;
	}
#endif
	return false;
}

bool FGoogleVRHMD::GetRecenterTransform(FQuat& RecenterOrientation, FVector& RecenterPosition)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	gvr::Value value_out = gvr::Value();
	if (TryReadProperty(GVR_PROPERTY_RECENTER_TRANSFORM, &value_out))
	{
		FMatrix RecenterUnreal;
		memcpy(RecenterUnreal.M[0], value_out.m4f.m[0], 4 * 4 * sizeof(float));

		// Inverse the view matrix so we can get the world position of the pose
		FMatrix RecenterInverseUnreal = RecenterUnreal.Inverse();

		// Number of Unreal Units per meter.
		const float WorldToMetersScale = GetWorldToMetersScale();

		// Gvr is using a openGl Right Handed coordinate system, UE is left handed.
		// The following code is converting the gvr coordinate system to UE coordinates.

		// Gvr: Negative Z is Forward, UE: Positive X is Forward.
		RecenterPosition.X = -RecenterInverseUnreal.M[2][3] * WorldToMetersScale;

		// Gvr: Positive X is Right, UE: Positive Y is Right.
		RecenterPosition.Y = RecenterInverseUnreal.M[0][3] * WorldToMetersScale;

		// Gvr: Positive Y is Up, UE: Positive Z is Up
		RecenterPosition.Z = RecenterInverseUnreal.M[1][3] * WorldToMetersScale;

		// Convert Gvr right handed coordinate system rotation into UE left handed coordinate system.
		RecenterOrientation = FQuat(RecenterUnreal);
		RecenterOrientation = FQuat(-RecenterOrientation.Z, RecenterOrientation.X, RecenterOrientation.Y, -RecenterOrientation.W);
		return true;
}
#endif
	return false;
}

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
bool FGoogleVRHMD::TryReadProperty(int32_t PropertyKey, gvr_value* ValueOut)
{
	const gvr_properties* props = gvr_get_current_properties(GVRAPI);
	return gvr_properties_get(props, PropertyKey, ValueOut) == GVR_ERROR_NONE;
}
#endif

void FGoogleVRHMD::SetTrackingOrigin(EHMDTrackingOrigin::Type InOrigin)
{
	if (InOrigin == EHMDTrackingOrigin::Floor && !Is6DOFSupported())
	{
		UE_LOG(LogHMD, Log, TEXT("EHMDTrackingOrigin::Floor not set. Positional Tracking is not supported."));
		return;
	}
	TrackingOrigin = InOrigin;
}

EHMDTrackingOrigin::Type FGoogleVRHMD::GetTrackingOrigin()
{
	return TrackingOrigin;
}

bool FGoogleVRHMD::Is6DOFSupported() const
{
	return bIs6DoFSupported;
}
