// Copyright 2017 Google Inc.

#pragma once

#include "IGoogleVRHMDPlugin.h"
#include "HeadMountedDisplay.h"
#include "HeadMountedDisplayBase.h"
#include "SceneViewExtension.h"
#include "RHIStaticStates.h"
#include "SceneViewport.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "PostProcess/PostProcessHMD.h"
#include "GoogleVRHMDViewerPreviews.h"
#include "Classes/GoogleVRHMDFunctionLibrary.h"
#include "GoogleVRSplash.h"
#include "Containers/Queue.h"
#include "XRRenderTargetManager.h"
#include "IXRInput.h"


#define LOG_VIEWER_DATA_FOR_GENERATION 0

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS

#include "OpenGLDrvPrivate.h"
#include "OpenGLResources.h"

#include "gvr.h"

// GVR Api Reference
extern gvr_context* GVRAPI;

#if GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS

#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (push,8)
#endif

#include <GLES2/gl2.h>

#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

#endif //GOOGLEVRHMD_SUPPORTED_ANDROID_PLATFORMS

#if GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS

#import <GVRSDK/GVROverlayView.h>

@class FOverlayViewDelegate;
@interface FOverlayViewDelegate : UIResponder<GVROverlayViewDelegate>
{
}
@end

#endif //GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS

// Forward decl
class FGoogleVRHMD;

class FGoogleVRHMDTexture2DSet : public FOpenGLTexture2D
{
public:

	FGoogleVRHMDTexture2DSet(
		class FOpenGLDynamicRHI* InGLRHI,
		GLuint InResource,
		GLenum InTarget,
		GLenum InAttachment,
		uint32 InSizeX,
		uint32 InSizeY,
		uint32 InSizeZ,
		uint32 InNumMips,
		uint32 InNumSamples,
		uint32 InNumSamplesTileMem,
		uint32 InArraySize,
		EPixelFormat InFormat,
		bool bInCubemap,
		bool bInAllocatedStorage,
		uint32 InFlags,
		uint8* InTextureRange
	);

	virtual ~FGoogleVRHMDTexture2DSet();

	static FGoogleVRHMDTexture2DSet* CreateTexture2DSet(
		FOpenGLDynamicRHI* InGLRHI,
		uint32 SizeX, uint32 SizeY,
		uint32 InNumLayers,
		uint32 InNumSamples,
		uint32 InNumSamplesTileMem,
		EPixelFormat InFormat,
		uint32 InFlags);
};

class FGoogleVRHMDCustomPresent : public FRHICustomPresent
{
public:

	FGoogleVRHMDCustomPresent(FGoogleVRHMD* HMD);
	virtual ~FGoogleVRHMDCustomPresent();

    void Shutdown();

	gvr_frame* CurrentFrame;
	TRefCountPtr<FGoogleVRHMDTexture2DSet> TextureSet;
private:

	FGoogleVRHMD* HMD;

	bool bNeedResizeGVRRenderTarget;
	gvr_sizei RenderTargetSize;

	gvr_swap_chain* SwapChain;
	TQueue<gvr_mat4f> RenderingHeadPoseQueue;
	gvr_mat4f CurrentFrameRenderHeadPose;
	const gvr_buffer_viewport_list* CurrentFrameViewportList;
	bool bSkipPresent;

public:

	/**
	 * Allocates a render target texture.
	 *
	 * @param Index			(in) index of the buffer, changing from 0 to GetNumberOfBufferedFrames()
	 * @return				true, if texture was allocated; false, if the default texture allocation should be used.
	 */
	bool AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumLayers, uint32 NumMips, uint32 Flags, uint32 TargetableTextureFlags);

	// Frame operations
	void UpdateRenderingViewportList(const gvr_buffer_viewport_list* BufferViewportList);
	void UpdateRenderingPose(gvr_mat4f InHeadPose);
	void UpdateViewport(const FViewport& Viewport, FRHIViewport* ViewportRHI);

	void BeginRendering();
	void BeginRendering(const gvr_mat4f& RenderingHeadPose);
	void FinishRendering();

public:

	void CreateGVRSwapChain();
	///////////////////////////////////////
	// Begin FRHICustomPresent Interface //
	///////////////////////////////////////

	// Called when viewport is resized.
	virtual void OnBackBufferResize() override;

	// Called from render thread to see if a native present will be requested for this frame.
	// @return	true if native Present will be requested for this frame; false otherwise.  Must
	// match value subsequently returned by Present for this frame.
	virtual bool NeedsNativePresent() override;

	// Called from RHI thread to perform custom present.
	// @param InOutSyncInterval - in out param, indicates if vsync is on (>0) or off (==0).
	// @return	true if native Present should be also be performed; false otherwise. If it returns
	// true, then InOutSyncInterval could be modified to switch between VSync/NoVSync for the normal 
	// Present.  Must match value previously returned by NeedsNormalPresent for this frame.
	virtual bool Present(int32& InOutSyncInterval) override;

	//// Called when rendering thread is acquired
	//virtual void OnAcquireThreadOwnership() {}

	//// Called when rendering thread is released
	//virtual void OnReleaseThreadOwnership() {}
};
#endif // GOOGLEVRHMD_SUPPORTED_PLATFORMS
#if GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS
#include "instant_preview_server.h"
#include "ip_shared.h"
#endif  // GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS

/**
 * GoogleVR Head Mounted Display
 */
class FGoogleVRHMD : public FHeadMountedDisplayBase, public FXRRenderTargetManager, public IXRInput, public FSelfRegisteringExec, public FSceneViewExtensionBase
{
	friend class FGoogleVRHMDCustomPresent;
	friend class FGoogleVRSplash;
public:

	FGoogleVRHMD(const FAutoRegister&);
	~FGoogleVRHMD();

	virtual FName GetSystemName() const override
	{
		static FName DefaultName(TEXT("FGoogleVRHMD"));
		return DefaultName;
	}

	/** @return	True if the HMD was initialized OK */
	bool IsInitialized() const;

	/** Update viewportlist */
	void UpdateGVRViewportList() const;

	/** Helper method to get renderer module */
	IRendererModule* GetRendererModule();

	/** Refreshes the viewer if it might have changed */
	void RefreshViewerProfile();

	/** Get the Mobile Content rendering size set by Unreal. This value is affected by r.MobileContentScaleFactor.
	 *  On Android, this is also the size of the Surface View. When it is not set to native screen resolution,
	 *  the hardware scaler will be used.
	 */
	FIntPoint GetUnrealMobileContentSize();

	/** Get the RenderTarget size GoogleVRHMD is using for rendering the scene. */
	FIntPoint GetGVRHMDRenderTargetSize();

	/** Get the maximal effective render target size for the current windows size(surface size).
	 *  This value is got from GVR SDK. Which may change based on the viewer.
	 */
	FIntPoint GetGVRMaxRenderTargetSize();

	/** Set RenderTarget size to the default size and return the value. */
	FIntPoint SetRenderTargetSizeToDefault();

	/** Set the RenderTarget size with a scale factor
	 *  The scale factor will be multiplied with the MaxRenderTargetSize.
	 *  The range should be [0.1, 1.0]
	 */
	bool SetGVRHMDRenderTargetSize(float ScaleFactor, FIntPoint& OutRenderTargetSize);

	/** Set the RenderTargetSize with the desired value.
	 *  Note that the size will be rounded up to the next multiple of 4.
	 *  This is because Unreal need the render target size is dividable by 4 for post process.
	 */
	bool SetGVRHMDRenderTargetSize(int DesiredWidth, int DesiredHeight, FIntPoint& OutRenderTargetSize);

	/** Enable/disable distortion correction */
	void SetDistortionCorrectionEnabled(bool bEnable);

	/** Change whether distortion correction is performed by GVR Api, or PostProcessHMD. Only supported on-device. */
	void SetDistortionCorrectionMethod(bool bUseGVRApiDistortionCorrection);

	/** Change the default viewer profile */
	bool SetDefaultViewerProfile(const FString& ViewerProfileURL);

	/** Generates a new distortion mesh of the given size */
	void SetDistortionMeshSize(EDistortionMeshSizeEnum MeshSize);

	/** Change the scaling factor used for applying the neck model offset */
	void SetNeckModelScale(float ScaleFactor);

	/** Check if distortion correction is enabled */
	bool GetDistortionCorrectionEnabled() const;

	/** Check which method distortion correction is using */
	bool IsUsingGVRApiDistortionCorrection() const;

	/** Get the scaling factor used for applying the neck model offset */
	float GetNeckModelScale() const;

	/** Check if application was launched in VR." */
	bool IsVrLaunch() const;

	/** Check if the application is running in Daydream mode*/
	bool IsInDaydreamMode() const;

	/** Check if mobile multi-view direct is enabled */
	bool IsMobileMultiViewDirect() const
	{
		return bIsMobileMultiViewDirect;
	}

	void SetSPMEnable(bool bEnable) const;

	/**
	 * Returns the string representation of the data URI on which this activity's intent is operating.
	 * See Intent.getDataString() in the Android documentation.
	 */
	FString GetIntentData() const;

	/** Get the currently set viewer model */
	FString GetViewerModel();

	/** Get the currently set viewer vendor */
	FString GetViewerVendor();

private:

	/** Refresh RT so screen isn't black */
	void ApplicationResumeDelegate();

	/** Handle letting application know about GVR back event */
	void HandleGVRBackEvent();

	/** Helper method to generate index buffer for manual distortion rendering */
	void GenerateDistortionCorrectionIndexBuffer();

	/** Helper method to generate vertex buffer for manual distortion rendering */
	void GenerateDistortionCorrectionVertexBuffer(EStereoscopicPass Eye);

	/** Generates Distortion Correction Points*/
	void SetNumOfDistortionPoints(int32 XPoints, int32 YPoints);

	/** Get how many Unreal units correspond to one meter in the real world */
	float GetWorldToMetersScale() const;

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	/** Get the Eye FOV from GVR SDK */
	gvr_rectf GetGVREyeFOV(int EyeIndex) const;
#endif

	/** Helper method implementing GetRelativeEyePose and GetInterpupillaryDistance */
	void GetRelativeHMDEyePose(EStereoscopicPass Eye, FQuat& OutOrientation, FVector& OutPosition) const;

	/** Function get called when start loading a map*/
	void OnPreLoadMap(const FString&);

	/** Console command handlers */
	void DistortEnableCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void DistortMethodCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void RenderTargetSizeCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void NeckModelScaleCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);


#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	void DistortMeshSizeCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void ShowSplashCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void SplashScreenDistanceCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void SplashScreenRenderScaleCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void EnableSustainedPerformanceModeHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);

	/**
	Clutch to ensure that changes in r.ScreenPercentage are reflected in render target size.
	*/
	void CVarSinkHandler();
#endif
public:

	// public function for In editor distortion previews
	enum EViewerPreview
	{
		EVP_None				= 0,
		EVP_GoogleCardboard1	= 1,
		EVP_GoogleCardboard2	= 2,
		EVP_ViewMaster			= 3,
		EVP_SnailVR				= 4,
		EVP_RiTech2				= 5,
		EVP_Go4DC1Glass			= 6
	};

	/** Check which viewer is being used for previewing */
	static EViewerPreview GetPreviewViewerType();

	/** Get preview viewer interpupillary distance */
	static float GetPreviewViewerInterpupillaryDistance();

	/** Get preview viewer stereo projection matrix */
	static FMatrix GetPreviewViewerStereoProjectionMatrix(enum EStereoscopicPass StereoPass);

	/** Get preview viewer num vertices */
	static uint32 GetPreviewViewerNumVertices(enum EStereoscopicPass StereoPass);

	/** Get preview viewer vertices */
	static const FDistortionVertex* GetPreviewViewerVertices(enum EStereoscopicPass StereoPass);

public:
	// Public Components
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	FGoogleVRHMDCustomPresent* CustomPresent;
	TSharedPtr<FGoogleVRSplash> GVRSplash;
#endif

private:
	// Updating Data
	bool		bStereoEnabled;
	bool		bHMDEnabled;
	bool		bDistortionCorrectionEnabled;
	bool		bUseGVRApiDistortionCorrection;
	bool		bUseOffscreenFramebuffers;
	bool		bIsInDaydreamMode;
	bool		bForceStopPresentScene;
	bool		bIsMobileMultiViewDirect;
	float		NeckModelScale;
	FQuat		BaseOrientation;

	// Drawing Data
	FIntPoint GVRRenderTargetSize;
	IRendererModule* RendererModule;
	uint16* DistortionMeshIndices;
	FDistortionVertex* DistortionMeshVerticesLeftEye;
	FDistortionVertex* DistortionMeshVerticesRightEye;

#if GOOGLEVRHMD_SUPPORTED_IOS_PLATFORMS
	GVROverlayView* OverlayView;
	FOverlayViewDelegate* OverlayViewDelegate;
#endif

	// Cached data that should only be updated once per frame
	mutable uint32		LastUpdatedCacheFrame;
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	mutable gvr_clock_time_point CachedFuturePoseTime;
	mutable gvr_mat4f CachedHeadPose;
	mutable FQuat CachedFinalHeadRotation;
	mutable FVector CachedFinalHeadPosition;
	mutable gvr_buffer_viewport_list* DistortedBufferViewportList;
	mutable gvr_buffer_viewport_list* NonDistortedBufferViewportList;
	mutable gvr_buffer_viewport_list* ActiveViewportList;
	mutable gvr_buffer_viewport* ScratchViewport;
#endif
#if GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS
	static const int kReadbackTextureCount = 5;
	FTexture2DRHIRef ReadbackTextures[kReadbackTextureCount];
	FRenderQueryRHIRef ReadbackCopyQueries[kReadbackTextureCount];
	FIntPoint ReadbackTextureSizes[kReadbackTextureCount];
	int ReadbackTextureCount;
	instant_preview::ReferencePose ReadbackReferencePoses[kReadbackTextureCount];
	void* ReadbackBuffers[kReadbackTextureCount];
	int ReadbackBufferWidths[kReadbackTextureCount];
	int SentTextureCount;
	TArray<FColor> ReadbackData;

	ip_static_server_handle IpServerHandle;
	bool bIsInstantPreviewActive;
	mutable instant_preview::EyeViews EyeViews;
	mutable instant_preview::ReferencePose CurrentReferencePose;
	mutable TQueue<instant_preview::ReferencePose> PendingRenderReferencePoses;
	mutable instant_preview::ReferencePose RenderReferencePose;
#endif

	// Simulation data for previewing
	float PosePitch;
	float PoseYaw;

	// distortion mesh
	uint32 DistortionPointsX;
	uint32 DistortionPointsY;
	uint32 NumVerts;
	uint32 NumTris;
	uint32 NumIndices;

	/** Console commands */
	FAutoConsoleCommand DistortEnableCommand;
	FAutoConsoleCommand DistortMethodCommand;
	FAutoConsoleCommand RenderTargetSizeCommand;
	FAutoConsoleCommand NeckModelScaleCommand;

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	FAutoConsoleCommand DistortMeshSizeCommand;
	FAutoConsoleCommand ShowSplashCommand;
	FAutoConsoleCommand SplashScreenDistanceCommand;
	FAutoConsoleCommand SplashScreenRenderScaleCommand;
	FAutoConsoleCommand EnableSustainedPerformanceModeCommand;

	FAutoConsoleVariableSink CVarSink;
#endif

	EHMDTrackingOrigin::Type TrackingOrigin;
	bool bIs6DoFSupported;

public:
	//////////////////////////////////////////////////////
	// Begin ISceneViewExtension Pure-Virtual Interface //
	//////////////////////////////////////////////////////

#if GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS
	/**
	* Called on render thread after rendering. Used to copy instant preview framebuffer to the viewer application.
	*/
	virtual void PostRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;
#endif  // GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS

	// This view extension should only be allowed when stereo is enabled.
	virtual bool IsActiveThisFrame(class FViewport* InViewport) const override;

	// Remaining pure virtual methods are not used
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override {}
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override {}


	///////////////////////////////////////////////////
	// Begin IStereoRendering Pure-Virtual Interface //
	///////////////////////////////////////////////////

	/**
	 * Whether or not stereo rendering is on this frame.
	 */
	virtual bool IsStereoEnabled() const override;

	/**
	 * Switches stereo rendering on / off. Returns current state of stereo.
	 * @return Returns current state of stereo (true / false).
	 */
	virtual bool EnableStereo(bool stereo = true) override;

	/**
	 * Adjusts the viewport rectangle for stereo, based on which eye pass is being rendered.
	 */
	virtual void AdjustViewRect(enum EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const override;

	/**
	 * Gets a projection matrix for the device, given the specified eye setup
	 */
	virtual FMatrix GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType) const override;

	//////////////////////////////////////////////
	// Begin IStereoRendering Virtual Interface //
	//////////////////////////////////////////////

	/**
	 * Returns eye render params, used from PostProcessHMD, RenderThread.
	 */
	virtual void GetEyeRenderParams_RenderThread(const struct FRenderingCompositePassContext& Context, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const override;

	// Renders texture into a backbuffer. Could be empty if no rendertarget texture is used, or if direct-rendering
	// through RHI bridge is implemented.
	virtual void RenderTexture_RenderThread(class FRHICommandListImmediate& RHICmdList, class FRHITexture2D* BackBuffer, class FRHITexture2D* SrcTexture, FVector2D WindowSize) const override;

	/**
	 * Returns currently active custom present.
	 */
	virtual FRHICustomPresent* GetCustomPresent() override;

	virtual IStereoRenderTargetManager* GetRenderTargetManager() override { return this; }

	////////////////////////////////////////////
	// Begin FXRRenderTargetManager Interface //
	////////////////////////////////////////////

	/**
	* Updates viewport for direct rendering of distortion. Should be called on a game thread.
	 */
	virtual void UpdateViewportRHIBridge(bool bUseSeparateRenderTarget, const class FViewport& Viewport, FRHIViewport* const ViewportRHI) override;

	/**
	* Calculates dimensions of the render target texture for direct rendering of distortion.
	*/
	virtual void CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY) override;

	// Whether separate render target should be used or not.
	virtual bool ShouldUseSeparateRenderTarget() const override;

	/**
	 * Allocates a render target texture.
	 *
	 * @param Index			(in) index of the buffer, changing from 0 to GetNumberOfBufferedFrames()
	 * @return				true, if texture was allocated; false, if the default texture allocation should be used.
	 */
	virtual bool AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples = 1) override;

	////////////////////////////////////////////////////
	// Begin IXRTrackingSystem Pure-Virtual Interface //
	////////////////////////////////////////////////////

	/**
	 * Returns version string.
	 */
	virtual FString GetVersionString() const override;

	/**
	 * Reports all devices currently available to the system, optionally limiting the result to a given class of devices.
	 *
	 * @param OutDevices The device ids of available devices will be appended to this array.
	 * @param Type Optionally limit the list of devices to a certain type.
	 */
	virtual bool EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type = EXRTrackedDeviceType::Any) override;

	/**
	 * Refresh poses. Tells the system to update the poses for its tracked devices.
	 * May be called both from the game and the render thread.
	 */
	virtual void RefreshPoses() override;
	
    /**
	 * Get the current pose for a device.
	 * This method must be callable both on the render thread and the game thread.
	 * For devices that don't support positional tracking, OutPosition will be at the base position.
	 *
	 * @param DeviceId the device to request the pose for.
	 * @param OutOrientation The current orientation of the device
	 * @param OutPosition The current position of the device
	 * @return true if the pose is valid or not.
     */
	virtual bool GetCurrentPose(int32 DeviceId, FQuat& OutOrientation, FVector& OutPosition) override;

    /**
	 * If the device id represents a head mounted display, fetches the relative position of the given eye relative to the eye.
	 * If the device is does not represent a stereoscopic tracked camera, orientation and position should be identity and zero and the return value should be false.
	 *
	 * @param DeviceId the device to request the eye pose for.
	 * @param Eye the eye the pose should be requested for, if passing in any other value than eSSP_LEFT_EYE or eSSP_RIGHT_EYE, the method should return a zero offset.
	 * @param OutOrientation The orientation of the eye relative to the device orientation.
	 * @param OutPosition The position of the eye relative to the tracked device
	 * @return true if the pose is valid or not. If the device is not a stereoscopic device, return false.
	 */
	virtual bool GetRelativeEyePose(int32 DeviceId, EStereoscopicPass Eye, FQuat& OutOrientation, FVector& OutPosition) override;

	/**
	 * Resets orientation by setting roll and pitch to 0, assuming that current yaw is forward direction and assuming
	 * current position as a 'zero-point' (for positional tracking).
	 *
	 * @param Yaw				(in) the desired yaw to be set after orientation reset.
	 */
	virtual void ResetOrientationAndPosition(float Yaw = 0.f) override;

	/**
	* Whether or not the system supports positional tracking (either via sensor or other means).
	* The default implementation always returns false, indicating that only rotational tracking is supported.
	*/
	virtual bool DoesSupportPositionalTracking() const override;

	///////////////////////////////////////////////
	// Begin IXRTrackingSystem Virtual Interface //
	///////////////////////////////////////////////

	/**
	 * Resets orientation by setting roll and pitch to 0, assuming that current yaw is forward direction. Position is not changed.
	 *
	 * @param Yaw				(in) the desired yaw to be set after orientation reset.
	 */
	virtual void ResetOrientation(float Yaw = 0.f) override;

	/**
	 * Sets base orientation by setting yaw, pitch, roll, assuming that this is forward direction.
	 * Position is not changed.
	 *
	 * @param BaseRot			(in) the desired orientation to be treated as a base orientation.
	 */
	virtual void SetBaseRotation(const FRotator& BaseRot) override;

	/**
	 * Returns current base orientation of HMD as yaw-pitch-roll combination.
	 */
	virtual FRotator GetBaseRotation() const override;

	/**
	 * Sets base orientation, assuming that this is forward direction.
	 * Position is not changed.
	 *
	 * @param BaseOrient		(in) the desired orientation to be treated as a base orientation.
	 */
	virtual void SetBaseOrientation(const FQuat& BaseOrient) override;

	/**
	 * Returns current base orientation of HMD as a quaternion.
	 */
	virtual FQuat GetBaseOrientation() const override;

	/**
	 * This method is called when new game frame begins (called on a game thread).
	*/
	virtual bool OnStartGameFrame(FWorldContext& WorldContext) override;

	/**
	 * Access optional HMD input override interface.
	 *
	 * @return a IXRInput pointer or a nullptr if not supported
	*/
	virtual class IXRInput* GetXRInput() { return this; }

	virtual class IHeadMountedDisplay* GetHMDDevice() override { return this; }

	virtual class TSharedPtr< class IStereoRendering, ESPMode::ThreadSafe > GetStereoRenderingDevice() override { return SharedThis(this); }

	//////////////////////////////////////
	// Begin IXRInput Virtual Interface //
	//////////////////////////////////////

	/**
	 * Passing key events to HMD.
	 * If returns 'false' then key will be handled by PlayerController;
	 * otherwise, key won't be handled by the PlayerController.
	*/
	virtual bool HandleInputKey(class UPlayerInput*, const struct FKey& Key, enum EInputEvent EventType, float AmountDepressed, bool bGamepad) override;

	/**
	 * Passing touch events to HMD.
	 * If returns 'false' then touch will be handled by PlayerController;
	 * otherwise, touch won't be handled by the PlayerController.
	*/
	virtual bool HandleInputTouch(uint32 Handle, ETouchType::Type Type, const FVector2D& TouchLocation, FDateTime DeviceTimestamp, uint32 TouchpadIndex) override;


	//////////////////////////////////////////////////////
	// Begin IHeadMountedDisplay Pure-Virtual Interface //
	//////////////////////////////////////////////////////

	/**
	 * Called on the game thread when view family is about to be rendered.
	 */

	virtual void BeginRendering_GameThread() override;
	
	/**
	 * Called on the render thread at the start of rendering.
	 */
	
	virtual void BeginRendering_RenderThread(const FTransform& NewRelativeTransform, FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily) override;

	/**
	 * Returns whether HMDDistortion post processing should be enabled or not.
	 */
	virtual bool GetHMDDistortionEnabled() const override;

	/**
	 * Returns true if HMD is currently connected.
	 */
	virtual bool IsHMDConnected() override;

	/**
	 * Whether or not switching to stereo is enabled; if it is false, then EnableStereo(true) will do nothing.
	 */
	virtual bool IsHMDEnabled() const override;

	/**
	 * Enables or disables switching to stereo.
	 */
	virtual void EnableHMD(bool bEnable = true) override;

	/**
	 * Returns the family of HMD device implemented
	 */
	virtual EHMDDeviceType::Type GetHMDDeviceType() const override;

    /**
     * Get the name or id of the display to output for this HMD.
     */
	virtual bool	GetHMDMonitorInfo(MonitorInfo&) override;

	/**
	 * Calculates the FOV, based on the screen dimensions of the device. Original FOV is passed as params.
	 */
	virtual void	GetFieldOfView(float& InOutHFOVInDegrees, float& InOutVFOVInDegrees) const override;

	/**
	 * Accessors to modify the interpupillary distance (meters)
	 */
	virtual void	SetInterpupillaryDistance(float NewInterpupillaryDistance) override;
	virtual float	GetInterpupillaryDistance() const override;

	/**
	 * Returns 'false' if chromatic aberration correction is off.
	 */
	virtual bool IsChromaAbCorrectionEnabled() const override;

	/////////////////////////////////////////////////
	// Begin IHeadMountedDisplay Virtual Interface //
	/////////////////////////////////////////////////
	
	virtual void DrawDistortionMesh_RenderThread(struct FRenderingCompositePassContext& Context, const FIntPoint& TextureSize) override;

	///////////////////////////////////////////////////////
	// Begin FSelfRegisteringExec Pure-Virtual Interface //
	///////////////////////////////////////////////////////

	/**
	* Exec handler to allow console commands to be passed through to the HMD for debugging
	 */
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	static FORCEINLINE FMatrix ToFMatrix(const gvr_mat4f& tm)
	{
		// Rows and columns are swapped between gvr_mat4f and FMatrix
		return FMatrix(
			FPlane(tm.m[0][0], tm.m[1][0], tm.m[2][0], tm.m[3][0]),
			FPlane(tm.m[0][1], tm.m[1][1], tm.m[2][1], tm.m[3][1]),
			FPlane(tm.m[0][2], tm.m[1][2], tm.m[2][2], tm.m[3][2]),
			FPlane(tm.m[0][3], tm.m[1][3], tm.m[2][3], tm.m[3][3]));
	}
#endif

	virtual void SetTrackingOrigin(EHMDTrackingOrigin::Type) override;
	virtual EHMDTrackingOrigin::Type GetTrackingOrigin() override;


#if GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS
	bool GetCurrentReferencePose(FQuat& CurrentOrientation, FVector& CurrentPosition) const;
	static FVector GetLocalEyePos(const instant_preview::EyeView& EyeView);
	void PushVideoFrame(const FColor* VideoFrameBuffer, int width, int height, int stride, instant_preview::PixelFormat pixel_format, instant_preview::ReferencePose reference_pose);
#endif  // GOOGLEVRHMD_SUPPORTED_INSTANT_PREVIEW_PLATFORMS

	/**
	* Tries to get the floor height if available
	* @param FloorHeight where the floor height read will get stored
	* returns true is the read was successful, false otherwise
	*/
	bool GetFloorHeight(float* FloorHeight);

	/**
	* Tries to get the Safety Cylinder Inner Radius if available
	* @param InnerRadius where the Safety Cylinder Inner Radius read will get stored
	* returns true is the read was successful, false otherwise
	*/
	bool GetSafetyCylinderInnerRadius(float* InnerRadius);

	/**
	* Tries to get the Safety Cylinder Outer Radius if available
	* @param OuterRadius where the Safety Cylinder Outer Radius read will get stored
	* returns true is the read was successful, false otherwise
	*/
	bool GetSafetyCylinderOuterRadius(float* OuterRadius);

	/**
	* Tries to get the Safety Region Type if available
	* @param RegionType where the Safety Region Type read will get stored
	* returns true is the read was successful, false otherwise
	*/
	bool GetSafetyRegion(ESafetyRegionType* RegionType);

	/**
	* Tries to get the Recenter Transform if available
	* @param RecenterOrientation where the Recenter Orientation read will get stored
	* @param RecenterPosition where the Recenter Position read will get stored
	* returns true is the read was successful, false otherwise
	*/
	bool GetRecenterTransform(FQuat& RecenterOrientation, FVector& RecenterPosition);

private :

#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	bool TryReadProperty(int32_t PropertyKey, gvr_value* ValueOut);
#endif

	bool Is6DOFSupported() const;
};
