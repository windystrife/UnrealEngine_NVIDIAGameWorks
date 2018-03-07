// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OculusHMDModule.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "OculusHMD_Settings.h"
#include "OculusHMD_GameFrame.h"
#include "OculusHMD_CustomPresent.h"
#include "OculusHMD_Layer.h"
#include "OculusHMD_Splash.h"
#include "OculusHMD_StressTester.h"
#include "OculusHMD_ConsoleCommands.h"
#include "OculusHMD_SpectatorScreenController.h"

#include "HeadMountedDisplayBase.h"
#include "HeadMountedDisplay.h"
#include "XRRenderTargetManager.h"
#include "IStereoLayers.h"
#include "Stats.h"
#include "SceneViewExtension.h"
#include "Engine/Engine.h"
#include "Engine/StaticMeshActor.h"

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FPerformanceStats
//-------------------------------------------------------------------------------------------------

struct FPerformanceStats
{
	uint64 Frames;
	double Seconds;

	FPerformanceStats(uint32 InFrames = 0, double InSeconds = 0.0)
		: Frames(InFrames)
		, Seconds(InSeconds)
	{}

	FPerformanceStats operator - (const FPerformanceStats& PerformanceStats) const
	{
		return FPerformanceStats(
			Frames - PerformanceStats.Frames,
			Seconds - PerformanceStats.Seconds);
	}
};


//-------------------------------------------------------------------------------------------------
// FOculusHMD - Oculus Rift Head Mounted Display
//-------------------------------------------------------------------------------------------------

class FOculusHMD : public FHeadMountedDisplayBase, public FXRRenderTargetManager, public IStereoLayers, public FSceneViewExtensionBase
{
	friend class UOculusFunctionLibrary;
	friend FOculusHMDModule;
	friend class FSplash;
	friend class FConsoleCommands;

public:
	// IXRSystemIdentifier
	virtual FName GetSystemName() const override;

	// IXRTrackingSystem
	virtual FString GetVersionString() const override;
	virtual bool DoesSupportPositionalTracking() const override;
	virtual bool HasValidTrackingPosition() override;
	virtual bool EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type=EXRTrackedDeviceType::Any) override;
	virtual void RefreshPoses() override;
	virtual bool GetCurrentPose(int32 InDeviceId, FQuat& OutOrientation, FVector& OutPosition) override;
	virtual bool GetRelativeEyePose(int32 InDeviceId, EStereoscopicPass InEye, FQuat& OutOrientation, FVector& OutPosition) override;
	virtual bool GetTrackingSensorProperties(int32 InDeviceId, FQuat& OutOrientation, FVector& OutPosition, FXRSensorProperties& OutSensorProperties) override;
	virtual void SetTrackingOrigin(EHMDTrackingOrigin::Type NewOrigin) override;
	virtual EHMDTrackingOrigin::Type GetTrackingOrigin() override;
	//virtual FVector GetAudioListenerOffset(int32 InDeviceId = HMDDeviceId) const override;
	virtual void ResetOrientationAndPosition(float Yaw = 0.f) override;
	virtual void ResetOrientation(float Yaw = 0.f) override;
	virtual void ResetPosition() override;
	virtual void SetBaseRotation(const FRotator& BaseRot) override;
	virtual FRotator GetBaseRotation() const override;
	virtual void SetBaseOrientation(const FQuat& BaseOrient) override;
	virtual FQuat GetBaseOrientation() const override;
	virtual class IHeadMountedDisplay* GetHMDDevice() override { return this; }
	virtual class TSharedPtr< class IStereoRendering, ESPMode::ThreadSafe > GetStereoRenderingDevice() override { return SharedThis(this); }
	//virtual class IXRInput* GetXRInput() override;
	virtual bool IsHeadTrackingAllowed() const override;
	virtual void OnBeginPlay(FWorldContext& InWorldContext) override;
	virtual void OnEndPlay(FWorldContext& InWorldContext) override;
	virtual bool OnStartGameFrame(FWorldContext& WorldContext) override;
	virtual bool OnEndGameFrame(FWorldContext& WorldContext) override;

	// IHeadMountedDisplay
	virtual bool IsHMDConnected() override;
	virtual bool IsHMDEnabled() const override;
	virtual EHMDWornState::Type GetHMDWornState() override;
	virtual void EnableHMD(bool bEnable = true) override;
	virtual EHMDDeviceType::Type GetHMDDeviceType() const override;
	virtual bool GetHMDMonitorInfo(MonitorInfo&) override;
	virtual void GetFieldOfView(float& InOutHFOVInDegrees, float& InOutVFOVInDegrees) const override;
	virtual void SetInterpupillaryDistance(float NewInterpupillaryDistance) override;
	virtual float GetInterpupillaryDistance() const override;
	virtual void SetClippingPlanes(float NCP, float FCP) override;
	//virtual FVector GetAudioListenerOffset() const override;
	virtual bool GetHMDDistortionEnabled() const override;
	//virtual void BeginRendering_RenderThread(const FTransform& NewRelativeTransform, FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily) override;
	//virtual class ISpectatorScreenController* GetSpectatorScreenController() override;
	//virtual class ISpectatorScreenController const* GetSpectatorScreenController() const override;
	//virtual float GetDistortionScalingFactor() const override;
	//virtual float GetLensCenterOffset() const override;
	//virtual void GetDistortionWarpValues(FVector4& K) const override;
	virtual bool IsChromaAbCorrectionEnabled() const override;
	//virtual bool GetChromaAbCorrectionValues(FVector4& K) const override;
	virtual bool HasHiddenAreaMesh() const override;
	virtual bool HasVisibleAreaMesh() const override;
	virtual void DrawHiddenAreaMesh_RenderThread(class FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const override;
	virtual void DrawVisibleAreaMesh_RenderThread(class FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const override;
	//virtual void DrawDistortionMesh_RenderThread(struct FRenderingCompositePassContext& Context, const FIntPoint& TextureSize) override;
	//virtual void UpdateScreenSettings(const FViewport* InViewport) override;
	//virtual void UpdatePostProcessSettings(FPostProcessSettings*) override;
	//virtual FTexture* GetDistortionTextureLeft() const override;
	//virtual FTexture* GetDistortionTextureRight() const override;
	//virtual FVector2D GetTextureOffsetLeft() const override;
	//virtual FVector2D GetTextureOffsetRight() const override;
	//virtual FVector2D GetTextureScaleLeft() const override;
	//virtual FVector2D GetTextureScaleRight() const override;
	//virtual const float* GetRedDistortionParameters() const override;
	//virtual const float* GetGreenDistortionParameters() const override;
	//virtual const float* GetBlueDistortionParameters() const override;
	//virtual bool NeedsUpscalePostProcessPass() override;
	//virtual void RecordAnalytics() override;
	//virtual bool DoesAppUseVRFocus() const override;
	//virtual bool DoesAppHaveVRFocus() const override;

	// IStereoRendering interface
	virtual bool IsStereoEnabled() const override;
	virtual bool IsStereoEnabledOnNextFrame() const override;
	virtual bool EnableStereo(bool stereo = true) override;
	virtual void AdjustViewRect(enum EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const  override;
	//virtual FVector2D GetTextSafeRegionBounds() const override;
	virtual void CalculateStereoViewOffset(const enum EStereoscopicPass StereoPassType, FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation) override;
	virtual FMatrix GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType) const override;
	virtual void InitCanvasFromView(class FSceneView* InView, class UCanvas* Canvas) override;
	//virtual void GetEyeRenderParams_RenderThread(const struct FRenderingCompositePassContext& Context, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const override;
	//virtual bool IsSpectatorScreenActive() const override;
	virtual void RenderTexture_RenderThread(class FRHICommandListImmediate& RHICmdList, class FRHITexture2D* BackBuffer, class FRHITexture2D* SrcTexture, FVector2D WindowSize) const override;
	virtual void GetOrthoProjection(int32 RTWidth, int32 RTHeight, float OrthoDistance, FMatrix OrthoProjection[2]) const override;
	virtual FRHICustomPresent* GetCustomPresent() override { return CustomPresent; }
	virtual IStereoRenderTargetManager* GetRenderTargetManager() override { return this; }
	virtual IStereoLayers* GetStereoLayers() override { return this; }
	//virtual void UseImplicitHmdPosition(bool bInImplicitHmdPosition) override;
	//virtual bool GetUseImplicitHmdPosition() override;

	// FHeadMoundedDisplayBase interface
	virtual FVector2D GetEyeCenterPoint_RenderThread(EStereoscopicPass StereoPassType) const override;
	virtual FIntRect GetFullFlatEyeRect_RenderThread(FTexture2DRHIRef EyeTexture) const override;
	virtual void CopyTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef SrcTexture, FIntRect SrcRect, FTexture2DRHIParamRef DstTexture, FIntRect DstRect, bool bClearBlack) const override;
	virtual bool PopulateAnalyticsAttributes(TArray<struct FAnalyticsEventAttribute>& EventAttributes) override;

	// FXRRenderTargetManager interface
	virtual bool ShouldUseSeparateRenderTarget() const override;
	virtual void CalculateRenderTargetSize(const FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY) override;
	virtual bool NeedReAllocateViewportRenderTarget(const class FViewport& Viewport) override;
	virtual bool NeedReAllocateDepthTexture(const TRefCountPtr<IPooledRenderTarget>& DepthTarget) override;
	virtual bool AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 InTexFlags, uint32 InTargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples = 1) override;
	virtual bool AllocateDepthTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples = 1) override;
	virtual void UpdateViewportWidget(bool bUseSeparateRenderTarget, const class FViewport& Viewport, class SViewport* ViewportWidget) override;
	virtual void UpdateViewportRHIBridge(bool bUseSeparateRenderTarget, const class FViewport& Viewport, FRHIViewport* const ViewportRHI) override;

	// IStereoLayers interface
	virtual uint32 CreateLayer(const IStereoLayers::FLayerDesc& InLayerDesc) override;
	virtual void DestroyLayer(uint32 LayerId) override;
	virtual void SetLayerDesc(uint32 LayerId, const IStereoLayers::FLayerDesc& InLayerDesc) override;
	virtual bool GetLayerDesc(uint32 LayerId, IStereoLayers::FLayerDesc& OutLayerDesc) override;
	virtual void MarkTextureForUpdate(uint32 LayerId) override;
	virtual void UpdateSplashScreen() override;
	virtual IStereoLayers::FLayerDesc GetDebugCanvasLayerDesc(FTextureRHIRef Texture) override;

	// ISceneViewExtension
	virtual bool IsActiveThisFrame(class FViewport* InViewport) const override;
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override;
	virtual void PostRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;
	virtual int32 GetPriority() const override
	{
		return -1; // We want to run after the FDefaultXRCamera's view extension
	}

public:
	FOculusHMD(const FAutoRegister&);
	~FOculusHMD();

protected:
	bool Startup();
	void PreShutdown();
	void Shutdown();
	bool InitializeSession();
	void ShutdownSession();
	bool InitDevice();
	void ReleaseDevice();
	void ApplicationPauseDelegate();
	void ApplicationResumeDelegate();
	void SetupOcclusionMeshes();
	void UpdateStereoRenderingParams();
	void UpdateHmdRenderInfo();
	void InitializeEyeLayer_RenderThread(FRHICommandListImmediate& RHICmdList);
	void ApplySystemOverridesOnStereo(bool force = false);
	bool OnOculusStateChange(bool bIsEnabledNow);
	bool ShouldDisableHiddenAndVisibileAreaMeshForSpectatorScreen_RenderThread() const;
	void RenderPokeAHole(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily);
#if !UE_BUILD_SHIPPING
	void DrawDebug(UCanvas* InCanvas, APlayerController* InPlayerController);
#endif

	class FSceneViewport* FindSceneViewport();

public:
	bool IsHMDActive() const;

	FSplash* GetSplash() const { return Splash.Get(); }
	FCustomPresent* GetCustomPresent_Internal() const { return CustomPresent; }

	float GetWorldToMetersScale() const;
	float GetMonoCullingDistance() const;

	ESpectatorScreenMode GetSpectatorScreenMode_RenderThread() const;

	FVector GetNeckPosition(const FQuat& HeadOrientation, const FVector& HeadPosition);

	/**
	* Sets base position offset (in meters). The base position offset is the distance from the physical (0, 0, 0) position
	* to current HMD position (bringing the (0, 0, 0) point to the current HMD position)
	* Note, this vector is set by ResetPosition call; use this method with care.
	* The axis of the vector are the same as in Unreal: X - forward, Y - right, Z - up.
	*
	* @param BaseOffset			(in) the vector to be set as base offset, in meters.
	*/
	void SetBaseOffsetInMeters(const FVector& BaseOffset);

	/**
	* Returns the currently used base position offset, previously set by the
	* ResetPosition or SetBasePositionOffset calls. It represents a vector that translates the HMD's position
	* into (0,0,0) point, in meters.
	* The axis of the vector are the same as in Unreal: X - forward, Y - right, Z - up.
	*
	* @return Base position offset, in meters.
	*/
	FVector GetBaseOffsetInMeters() const;

	OCULUSHMD_API bool ConvertPose(const ovrpPosef& InPose, FPose& OutPose) const;
	OCULUSHMD_API bool ConvertPose_RenderThread(const ovrpPosef& InPose, FPose& OutPose) const;
	OCULUSHMD_API static bool ConvertPose_Internal(const ovrpPosef& InPose, FPose& OutPose, const FSettings* Settings, float WorldToMetersScale = 100.0f);

	/** Turns ovrVector3f in Unreal World space to a scaled FVector and applies translation and rotation corresponding to player movement */
	FVector ScaleAndMovePointWithPlayer(ovrpVector3f& OculusHMDPoint) const;

	/** Convert dimension of a float (e.g., a distance) from meters to Unreal Units */
	float ConvertFloat_M2U(float OculusFloat) const;
	FVector ConvertVector_M2U(ovrpVector3f OculusPoint) const;

	struct UserProfile
	{
		float IPD;
		float EyeDepth;
		float EyeHeight;
	};

	bool GetUserProfile(UserProfile& OutProfile);
	float GetVsyncToNextVsync() const;
	FPerformanceStats GetPerformanceStats() const;
	void SetPixelDensity(float NewPD);
	bool DoEnableStereo(bool bStereo);
	void ResetStereoRenderingParams();
	void ResetControlRotation() const;


	FSettingsPtr CreateNewSettings() const;
	FGameFramePtr CreateNewGameFrame() const;

	FGameFrame* GetFrame() { CheckInGameThread(); return Frame.Get(); }
	const FGameFrame* GetFrame() const { CheckInGameThread(); return Frame.Get(); }
	FGameFrame* GetFrame_RenderThread() { CheckInRenderThread(); return Frame_RenderThread.Get(); }
	const FGameFrame* GetFrame_RenderThread() const { CheckInRenderThread(); return Frame_RenderThread.Get(); }
	FGameFrame* GetFrame_RHIThread() { CheckInRHIThread(); return Frame_RHIThread.Get(); }
	const FGameFrame* GetFrame_RHIThread() const { CheckInRHIThread(); return Frame_RHIThread.Get(); }

	FSettings* GetSettings() { CheckInGameThread(); return Settings.Get(); }
	const FSettings* GetSettings() const { CheckInGameThread(); return Settings.Get(); }
	FSettings* GetSettings_RenderThread() { CheckInRenderThread(); return Settings_RenderThread.Get(); }
	const FSettings* GetSettings_RenderThread() const { CheckInRenderThread(); return Settings_RenderThread.Get(); }
	FSettings* GetSettings_RHIThread() { CheckInRHIThread(); return Settings_RHIThread.Get(); }
	const FSettings* GetSettings_RHIThread() const { CheckInRHIThread(); return Settings_RHIThread.Get(); }

	FLayer* GetEyeLayer_RenderThread() { CheckInRenderThread(); return EyeLayer_RenderThread.Get(); }
	const FLayer* GetEyeLayer_RenderThread() const { CheckInRenderThread(); return EyeLayer_RenderThread.Get(); }
	FLayer* GetEyeLayer_RHIThread() { CheckInRHIThread(); return EyeLayer_RHIThread.Get(); }
	const FLayer* GetEyeLayer_RHIThread() const { CheckInRHIThread(); return EyeLayer_RHIThread.Get(); }

	void StartGameFrame_GameThread(); // Called from OnStartGameFrame
	void FinishGameFrame_GameThread(); // Called from OnEndGameFrame
	void StartRenderFrame_GameThread(); // Called from BeginRenderViewFamily
	void FinishRenderFrame_RenderThread(FRHICommandListImmediate& RHICmdList); // Called from PostRenderViewFamily_RenderThread
	void StartRHIFrame_RenderThread(); // Called from PreRenderViewFamily_RenderThread
	void FinishRHIFrame_RHIThread(); // Called from FinishRendering_RHIThread


protected:
	FConsoleCommands ConsoleCommands;
	void UpdateOnRenderThreadCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void PixelDensityCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void PixelDensityMinCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void PixelDensityMaxCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void PixelDensityAdaptiveCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void HQBufferCommandHandler(const TArray<FString>& Args, UWorld*, FOutputDevice& Ar);
	void HQDistortionCommandHandler(const TArray<FString>& Args, UWorld*, FOutputDevice& Ar);
	void ShowGlobalMenuCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void ShowQuitMenuCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
#if !UE_BUILD_SHIPPING
	void EnforceHeadTrackingCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void StatsCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void ShowSettingsCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void IPDCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void FCPCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void NCPCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
#endif
	static void CVarSinkHandler();
	static FAutoConsoleVariableSink CVarSink;

	void LoadFromIni();
	void SaveToIni();

protected:
	void UpdateHMDWornState();
	EHMDWornState::Type HMDWornState = EHMDWornState::Unknown;

	union
	{
		struct
		{
			uint64	bApplySystemOverridesOnStereo : 1;

			uint64	bNeedEnableStereo : 1;
			uint64	bNeedDisableStereo : 1;
		};
		uint64 Raw;
	} Flags;

	union
	{
		struct
		{
			// set to true when origin was set while OvrSession == null; the origin will be set ASA OvrSession != null
			uint64 NeedSetTrackingOrigin : 1;
			// enforces exit; used mostly for testing
			uint64 EnforceExit : 1;
			// set if a game is paused by the plug-in
			uint64 AppIsPaused : 1;
			// set to indicate that DisplayLost was detected by game thread.
			uint64 DisplayLostDetected : 1;
			// set to true once new session is created; being handled and reset as soon as session->IsVisible.
			uint64 NeedSetFocusToGameViewport : 1;
		};
		uint64 Raw;
	} OCFlags;

	TRefCountPtr<FCustomPresent> CustomPresent;
	FSplashPtr Splash;
	IRendererModule* RendererModule;

	ovrpTrackingOrigin TrackingOrigin;
	// Stores difference between ViewRotation and EyeOrientation from previous frame
	FQuat LastPlayerOrientation;
	// Stores GetFrame()->PlayerLocation (i.e., ViewLocation) from the previous frame
	FVector LastPlayerLocation;
	FRotator DeltaControlRotation; // used from ApplyHmdRotation
	TWeakPtr<SWidget> CachedViewportWidget;
	TWeakPtr<SWindow> CachedWindow;
	FVector2D CachedWindowSize;
	float CachedWorldToMetersScale;
	float CachedMonoCullingDistance;

	// Game thread
	FSettingsPtr Settings;
	uint32 NextFrameNumber;
	FGameFramePtr Frame; // Valid from OnStartGameFrame to OnEndGameFrame
	FGameFramePtr NextFrameToRender; // Valid from OnStartGameFrame to BeginRenderViewFamily
	FGameFramePtr LastFrameToRender; // Valid from OnStartGameFrame to BeginRenderViewFamily
	uint32 NextLayerId;
	TMap<uint32, FLayerPtr> LayerMap;

	// Render thread
	FSettingsPtr Settings_RenderThread;
	FGameFramePtr Frame_RenderThread; // Valid from BeginRenderViewFamily to PostRenderViewFamily_RenderThread
	TArray<FLayerPtr> Layers_RenderThread;
	FLayerPtr EyeLayer_RenderThread;

	// RHI thread
	FSettingsPtr Settings_RHIThread;
	FGameFramePtr Frame_RHIThread; // Valid from PreRenderViewFamily_RenderThread to FinishRendering_RHIThread
	TArray<FLayerPtr> Layers_RHIThread;
	FLayerPtr EyeLayer_RHIThread;

	FHMDViewMesh HiddenAreaMeshes[2];
	FHMDViewMesh VisibleAreaMeshes[2];

	FPerformanceStats PerformanceStats;

#if !UE_BUILD_SHIPPING
	FDelegateHandle DrawDebugDelegateHandle;
#endif
};

typedef TSharedPtr< FOculusHMD, ESPMode::ThreadSafe > FOculusHMDPtr;


} // namespace OculusHMD

#endif //OCULUS_HMD_SUPPORTED_PLATFORMS
