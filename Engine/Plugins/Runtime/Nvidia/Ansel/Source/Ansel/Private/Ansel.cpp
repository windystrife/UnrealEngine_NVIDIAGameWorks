// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "IAnselPlugin.h"
#include "Camera/CameraTypes.h"
#include "Camera/CameraPhotography.h"
#include "Camera/PlayerCameraManager.h"
#include "ConsoleManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/ViewportSplitScreen.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/SWindow.h"
#include <AnselSDK.h>

DEFINE_LOG_CATEGORY_STATIC(LogAnsel, Log, All);

/////////////////////////////////////////////////
// All the NVIDIA Ansel-specific details

class FNVAnselCameraPhotographyPrivate : public ICameraPhotography
{
public:
	FNVAnselCameraPhotographyPrivate();
	virtual ~FNVAnselCameraPhotographyPrivate() override;
	virtual bool UpdateCamera(FMinimalViewInfo& InOutPOV, APlayerCameraManager* PCMgr) override;
	virtual bool IsSupported() override;
	virtual const TCHAR* const GetProviderName() override { return TEXT("NVIDIA Ansel"); };

private:
	void ReconfigureAnsel();
	void DeconfigureAnsel();

	static ansel::StartSessionStatus AnselStartSessionCallback(ansel::SessionConfiguration& settings, void* userPointer);
	static void AnselStopSessionCallback(void* userPointer);
	static void AnselStartCaptureCallback(const ansel::CaptureConfiguration& CaptureType, void* userPointer);
	static void AnselStopCaptureCallback(void* userPointer);

	static bool AnselCamerasMatch(ansel::Camera& a, ansel::Camera& b);

	void AnselCameraToFMinimalView(FMinimalViewInfo& InOutPOV, ansel::Camera& AnselCam);
	void FMinimalViewToAnselCamera(ansel::Camera& InOutAnselCam, FMinimalViewInfo& POV);

	bool BlueprintModifyCamera(ansel::Camera& InOutAnselCam, APlayerCameraManager* PCMgr); // returns whether modified cam is in original (session-start) position

	ansel::Configuration* AnselConfig;
	ansel::Camera AnselCamera;
	ansel::Camera AnselCameraOriginal;
	ansel::Camera AnselCameraPrevious;

	FMinimalViewInfo UECameraOriginal;
	FMinimalViewInfo UECameraPrevious;

	bool bAnselSessionActive;
	bool bAnselSessionNewlyActive;
	bool bAnselSessionWantDeactivate;
	bool bAnselCaptureActive;
	bool bAnselCaptureNewlyActive;
	bool bAnselCaptureNewlyFinished;

	bool bForceDisallow;
	bool bIsOrthoProjection;

	bool bWasMovableCameraBeforeSession;
	bool bWasPausedBeforeSession;
	bool bWasShowingHUDBeforeSession;
	bool bWereSubtitlesEnabledBeforeSession;
	bool bWasFadingEnabledBeforeSession;

	bool bAutoPostprocess;
	bool bAutoPause;

	/** Console variable delegate for checking when the console variables have changed */
	FConsoleCommandDelegate CVarDelegate;
	FConsoleVariableSinkHandle CVarDelegateHandle;
};

static void* AnselSDKDLLHandle = 0;
static bool bAnselDLLLoaded = false;

FNVAnselCameraPhotographyPrivate::FNVAnselCameraPhotographyPrivate()
	: ICameraPhotography()
	, bAnselSessionActive(false)
	, bAnselSessionNewlyActive(false)
	, bAnselSessionWantDeactivate(false)
	, bAnselCaptureActive(false)
	, bAnselCaptureNewlyActive(false)
	, bAnselCaptureNewlyFinished(false)
	, bForceDisallow(false)
	, bIsOrthoProjection(false)
{
	if (bAnselDLLLoaded)
	{
		AnselConfig = new ansel::Configuration();

		CVarDelegate = FConsoleCommandDelegate::CreateLambda([this] {
			static float LastTranslationSpeed = -1.0f;
			static int32 LastSettleFrames = -1;
			static int32 LastPersistEffects = -1;
			
			static IConsoleVariable* CVarTranslationSpeed = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Photography.TranslationSpeed"));
			static IConsoleVariable* CVarSettleFrames = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Photography.SettleFrames"));
			static IConsoleVariable* CVarPersistEffects = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Photography.PersistEffects"));
			
			float ThisTranslationSpeed = CVarTranslationSpeed->GetFloat();
			int32 ThisSettleFrames = CVarSettleFrames->GetInt();
			int32 ThisPersistEffects = CVarPersistEffects->GetInt();

			if (ThisTranslationSpeed != LastTranslationSpeed ||
				ThisSettleFrames != LastSettleFrames ||
				ThisPersistEffects != LastPersistEffects)
			{
				ReconfigureAnsel();
				LastTranslationSpeed = ThisTranslationSpeed;
				LastSettleFrames = ThisSettleFrames;
				LastPersistEffects = ThisPersistEffects;
			}
		});

		CVarDelegateHandle = IConsoleManager::Get().RegisterConsoleVariableSink_Handle(CVarDelegate);
		ReconfigureAnsel();
	}
	else
	{
		UE_LOG(LogAnsel, Log, TEXT("Ansel DLL was not successfully loaded."));
	}
}


FNVAnselCameraPhotographyPrivate::~FNVAnselCameraPhotographyPrivate()
{	
	if (bAnselDLLLoaded)
	{
		IConsoleManager::Get().UnregisterConsoleVariableSink_Handle(CVarDelegateHandle);
		DeconfigureAnsel();
		delete AnselConfig;
	}
}


bool FNVAnselCameraPhotographyPrivate::IsSupported()
{
	return bAnselDLLLoaded && ansel::isAnselAvailable();
}

bool FNVAnselCameraPhotographyPrivate::AnselCamerasMatch(ansel::Camera& a, ansel::Camera& b)
{
	return a.position.x == b.position.x &&
		a.position.y == b.position.y &&
		a.position.z == b.position.z &&
		a.rotation.x == b.rotation.x &&
		a.rotation.y == b.rotation.y &&
		a.rotation.z == b.rotation.z &&
		a.rotation.w == b.rotation.w &&
		a.fov == b.fov &&
		a.projectionOffsetX == b.projectionOffsetX &&
		a.projectionOffsetY == b.projectionOffsetY;
}

void FNVAnselCameraPhotographyPrivate::AnselCameraToFMinimalView(FMinimalViewInfo& InOutPOV, ansel::Camera& AnselCam)
{
	InOutPOV.FOV = AnselCam.fov;
	InOutPOV.Location.X = AnselCam.position.x;
	InOutPOV.Location.Y = AnselCam.position.y;
	InOutPOV.Location.Z = AnselCam.position.z;
	FQuat rotq(AnselCam.rotation.x, AnselCam.rotation.y, AnselCam.rotation.z, AnselCam.rotation.w);
	InOutPOV.Rotation = FRotator(rotq);
	InOutPOV.OffCenterProjectionOffset.Set(AnselCam.projectionOffsetX, AnselCam.projectionOffsetY);
}

void FNVAnselCameraPhotographyPrivate::FMinimalViewToAnselCamera(ansel::Camera& InOutAnselCam, FMinimalViewInfo& POV)
{
	InOutAnselCam.fov = POV.FOV;
	InOutAnselCam.position = { POV.Location.X, POV.Location.Y, POV.Location.Z };
	FQuat rotq = POV.Rotation.Quaternion();
	InOutAnselCam.rotation = { rotq.X, rotq.Y, rotq.Z, rotq.W };
	InOutAnselCam.projectionOffsetX = 0.f; // Ansel only writes these, doesn't read
	InOutAnselCam.projectionOffsetY = 0.f;
}

bool FNVAnselCameraPhotographyPrivate::BlueprintModifyCamera(ansel::Camera& InOutAnselCam, APlayerCameraManager* PCMgr)
{
	FMinimalViewInfo Proposed;

	AnselCameraToFMinimalView(Proposed, InOutAnselCam);
	PCMgr->PhotographyCameraModify(Proposed.Location, UECameraPrevious.Location, UECameraOriginal.Location, Proposed.Location/*out by ref*/);
	// only position has possibly changed
	InOutAnselCam.position.x = Proposed.Location.X;
	InOutAnselCam.position.y = Proposed.Location.Y;
	InOutAnselCam.position.z = Proposed.Location.Z;

	UECameraPrevious = Proposed;

	bool bIsCameraInOriginalTransform =
		Proposed.Location.Equals(UECameraOriginal.Location) &&
		Proposed.Rotation.Equals(UECameraOriginal.Rotation) &&
		Proposed.FOV == UECameraOriginal.FOV;
	return bIsCameraInOriginalTransform;
}

bool FNVAnselCameraPhotographyPrivate::UpdateCamera(FMinimalViewInfo& InOutPOV, APlayerCameraManager* PCMgr)
{
	check(PCMgr != nullptr);
	bool bGameCameraCutThisFrame = false;

	bForceDisallow = false;
	if (!bAnselSessionActive)
	{
		// grab & store some view details that effect Ansel session setup but which it could be
		// unsafe to access from the Ansel callbacks (which aren't necessarily on render
		// or game thread).
		bIsOrthoProjection = (InOutPOV.ProjectionMode == ECameraProjectionMode::Orthographic);
		if (UGameViewportClient* ViewportClient = PCMgr->GetWorld()->GetGameViewport())
		{
			bForceDisallow = bForceDisallow || (ViewportClient->GetCurrentSplitscreenConfiguration() != ESplitScreenType::None); // forbid if in splitscreen.
		}
		// forbid if in stereoscopic/VR mode
		bForceDisallow = bForceDisallow || (GEngine->IsStereoscopic3D());
	}

	if (bAnselSessionActive)
	{
		APlayerController* PCOwner = PCMgr->GetOwningPlayerController();
		check(PCOwner != nullptr);

		if (bAnselCaptureNewlyActive)
		{
			PCMgr->OnPhotographyMultiPartCaptureStart();
			bGameCameraCutThisFrame = true;
			bAnselCaptureNewlyActive = false;

			// check for Panini projection & disable it?

			// force sync texture loading and/or boost LODs?
			// -> r.Streaming.FullyLoadUsedTextures for 4.13
			// -> r.Streaming.?? for 4.12
		}

		if (bAnselCaptureNewlyFinished)
		{
			bGameCameraCutThisFrame = true;
			bAnselCaptureNewlyFinished = false;
			PCMgr->OnPhotographyMultiPartCaptureEnd();
		}

		if (bAnselSessionWantDeactivate)
		{
			bAnselSessionActive = false;
			bAnselSessionWantDeactivate = false;

			// auto-restore state

			if (bAutoPostprocess)
			{
				if (bWasShowingHUDBeforeSession)
				{
					PCOwner->MyHUD->ShowHUD(); // toggle on
				}
				if (bWereSubtitlesEnabledBeforeSession)
				{
					UGameplayStatics::SetSubtitlesEnabled(true);
				}
				if (bWasFadingEnabledBeforeSession)
				{
					PCMgr->bEnableFading = true;
				}
			}

			if (bAutoPause && !bWasPausedBeforeSession)
			{
				PCOwner->SetPause(false);
			}

			PCMgr->GetWorld()->bIsCameraMoveableWhenPaused = bWasMovableCameraBeforeSession;

			PCMgr->OnPhotographySessionEnd(); // after unpausing

											  // no need to restore original camera params; re-clobbered every frame
		}
		else
		{
			bool bIsCameraInOriginalState = false;

			if (bAnselSessionNewlyActive)
			{
				PCMgr->OnPhotographySessionStart(); // before pausing
													// copy these values to avoid mixup if the CVars are changed during capture callbacks

				static IConsoleVariable* CVarAutoPause = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Photography.AutoPause"));
				static IConsoleVariable* CVarAutoPostProcess = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Photography.AutoPostprocess"));

				bAutoPause = !!CVarAutoPause->GetInt();
				bAutoPostprocess = !!CVarAutoPostProcess->GetInt();

				// attempt to pause game
				bWasPausedBeforeSession = PCOwner->IsPaused();
				bWasMovableCameraBeforeSession = PCMgr->GetWorld()->bIsCameraMoveableWhenPaused;
				if (bAutoPause && !bWasPausedBeforeSession)
				{
					PCOwner->SetPause(true); // should we bother to set delegate to enforce pausedness until session end?  probably over-engineering.
				}

				bWasFadingEnabledBeforeSession = PCMgr->bEnableFading;
				bWasShowingHUDBeforeSession = PCOwner->MyHUD &&
					PCOwner->MyHUD->bShowHUD;
				bWereSubtitlesEnabledBeforeSession = UGameplayStatics::AreSubtitlesEnabled();
				if (bAutoPostprocess)
				{
					if (bWasShowingHUDBeforeSession)
					{
						PCOwner->MyHUD->ShowHUD(); // toggle off
					}
					UGameplayStatics::SetSubtitlesEnabled(false);
					PCMgr->bEnableFading = false;
				}

				// store initial camera info
				UECameraPrevious = InOutPOV;
				UECameraOriginal = InOutPOV;

				FMinimalViewToAnselCamera(AnselCamera, InOutPOV);
				ansel::updateCamera(AnselCamera);

				AnselCameraOriginal = AnselCamera;
				AnselCameraPrevious = AnselCamera;

				bIsCameraInOriginalState = true;

				bAnselSessionNewlyActive = false;
			}
			else
			{
				ansel::updateCamera(AnselCamera);

				// active session; give Blueprints opportunity to modify camera, unless a capture is in progress
				if (!bAnselCaptureActive)
				{
					bIsCameraInOriginalState = BlueprintModifyCamera(AnselCamera, PCMgr);
				}
			}

			//bool bIsCameraStatic = AnselCamerasMatch(AnselCamera, AnselCameraPrevious);
			//UE_LOG(LogAnsel, Warning, TEXT("Anselcam static=%d orig=%d"), bIsCameraStatic, bIsCameraInOriginalState);
			//if (!bIsCameraInOriginalState) __debugbreak();

			AnselCameraToFMinimalView(InOutPOV, AnselCamera);

			if (!bIsCameraInOriginalState)
			{
				// resume updating sceneview upon first camera move.  we wait for a move so motion blur doesn't reset as soon as we start a session.
				PCMgr->GetWorld()->bIsCameraMoveableWhenPaused = true;
			}

			if (bAnselCaptureActive)
			{
				if (bAutoPostprocess)
				{
					// force-disable the standard postprocessing effects which are known to
					// be problematic in multi-part shots

					// these effects tile poorly
					InOutPOV.PostProcessSettings.bOverride_BloomDirtMaskIntensity = 1;
					InOutPOV.PostProcessSettings.BloomDirtMaskIntensity = 0.f;
					InOutPOV.PostProcessSettings.bOverride_LensFlareIntensity = 1;
					InOutPOV.PostProcessSettings.LensFlareIntensity = 0.f;
					InOutPOV.PostProcessSettings.bOverride_VignetteIntensity = 1;
					InOutPOV.PostProcessSettings.VignetteIntensity = 0.f;
					InOutPOV.PostProcessSettings.bOverride_SceneFringeIntensity = 1;
					InOutPOV.PostProcessSettings.SceneFringeIntensity = 0.f;

					// motion blur doesn't make sense with a teleporting camera
					InOutPOV.PostProcessSettings.bOverride_MotionBlurAmount = 1;
					InOutPOV.PostProcessSettings.MotionBlurAmount = 0.f;

					// DoF can look poor/wrong at high-res, depending on settings.
					InOutPOV.PostProcessSettings.bOverride_DepthOfFieldScale = 1;
					InOutPOV.PostProcessSettings.DepthOfFieldScale = 0.f; // BokehDOF
					InOutPOV.PostProcessSettings.bOverride_DepthOfFieldNearBlurSize = 1;
					InOutPOV.PostProcessSettings.DepthOfFieldNearBlurSize = 0.f; // GaussianDOF
					InOutPOV.PostProcessSettings.bOverride_DepthOfFieldFarBlurSize = 1;
					InOutPOV.PostProcessSettings.DepthOfFieldFarBlurSize = 0.f; // GaussianDOF
					InOutPOV.PostProcessSettings.bOverride_DepthOfFieldDepthBlurRadius = 1;
					InOutPOV.PostProcessSettings.DepthOfFieldDepthBlurRadius = 0.f; // CircleDOF
					InOutPOV.PostProcessSettings.bOverride_DepthOfFieldVignetteSize = 1;
					InOutPOV.PostProcessSettings.DepthOfFieldVignetteSize = 200.f; // Scene.h says 200.0 means 'no effect'

					// freeze auto-exposure adaptation
					InOutPOV.PostProcessSettings.bOverride_AutoExposureSpeedDown = 1;
					InOutPOV.PostProcessSettings.AutoExposureSpeedDown = 0.f;
					InOutPOV.PostProcessSettings.bOverride_AutoExposureSpeedUp = 1;
					InOutPOV.PostProcessSettings.AutoExposureSpeedUp = 0.f;

					// SSR is a quality gamble in multi-part shots; disable
					InOutPOV.PostProcessSettings.bOverride_ScreenSpaceReflectionIntensity = 1;
					InOutPOV.PostProcessSettings.ScreenSpaceReflectionIntensity = 0.f;
				}
			}

			AnselCameraPrevious = AnselCamera;
		}
	}

	return bGameCameraCutThisFrame;
}

ansel::StartSessionStatus FNVAnselCameraPhotographyPrivate::AnselStartSessionCallback(ansel::SessionConfiguration& settings, void* userPointer)
{
	ansel::StartSessionStatus AnselSessionStatus = ansel::kDisallowed;
	FNVAnselCameraPhotographyPrivate* PrivateImpl = static_cast<FNVAnselCameraPhotographyPrivate*>(userPointer);
	check(PrivateImpl != nullptr);

	static IConsoleVariable* CVarAllow = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Photography.Allow"));
	static IConsoleVariable* CVarEnableMultipart = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Photography.EnableMultipart"));
	if (!PrivateImpl->bForceDisallow && CVarAllow->GetInt() && !GIsEditor)
	{
		bool bPauseAllowed = true;
		bool bEnableMultipart = !!CVarEnableMultipart->GetInt();

		settings.isTranslationAllowed = true;
		settings.isFovChangeAllowed = !PrivateImpl->bIsOrthoProjection;
		settings.isRotationAllowed = true;
		settings.isPauseAllowed = bPauseAllowed;
		settings.isHighresAllowed = bEnableMultipart;
		settings.is360MonoAllowed = bEnableMultipart;
		settings.is360StereoAllowed = bEnableMultipart;

		PrivateImpl->bAnselSessionActive = true;
		PrivateImpl->bAnselSessionNewlyActive = true;

		AnselSessionStatus = ansel::kAllowed;
	}

	UE_LOG(LogAnsel, Log, TEXT("Photography camera session attempt started, Allowed=%d, ForceDisallowed=%d"), int(AnselSessionStatus == ansel::kAllowed), int(PrivateImpl->bForceDisallow));

	return AnselSessionStatus;
}

void FNVAnselCameraPhotographyPrivate::AnselStopSessionCallback(void* userPointer)
{
	FNVAnselCameraPhotographyPrivate* PrivateImpl = static_cast<FNVAnselCameraPhotographyPrivate*>(userPointer);
	check(PrivateImpl != nullptr);
	if (PrivateImpl->bAnselSessionActive && PrivateImpl->bAnselSessionNewlyActive)
	{
		// if we've not acted upon the new session at all yet, then just don't.
		PrivateImpl->bAnselSessionActive = false;
	}
	else
	{
		PrivateImpl->bAnselSessionWantDeactivate = true;
	}

	UE_LOG(LogAnsel, Log, TEXT("Photography camera session end"));
}

void FNVAnselCameraPhotographyPrivate::AnselStartCaptureCallback(const ansel::CaptureConfiguration& CaptureType, void* userPointer)
{
	FNVAnselCameraPhotographyPrivate* PrivateImpl = static_cast<FNVAnselCameraPhotographyPrivate*>(userPointer);
	check(PrivateImpl != nullptr);
	PrivateImpl->bAnselCaptureActive = true;
	PrivateImpl->bAnselCaptureNewlyActive = true;

	UE_LOG(LogAnsel, Log, TEXT("Photography camera multi-part capture started"));
}

void FNVAnselCameraPhotographyPrivate::AnselStopCaptureCallback(void* userPointer)
{
	FNVAnselCameraPhotographyPrivate* PrivateImpl = static_cast<FNVAnselCameraPhotographyPrivate*>(userPointer);
	check(PrivateImpl != nullptr);
	PrivateImpl->bAnselCaptureActive = false;
	PrivateImpl->bAnselCaptureNewlyFinished = true;

	UE_LOG(LogAnsel, Log, TEXT("Photography camera multi-part capture end"));
}

void FNVAnselCameraPhotographyPrivate::ReconfigureAnsel()
{
	check(AnselConfig != nullptr);
	AnselConfig->userPointer = this;
	AnselConfig->startSessionCallback = AnselStartSessionCallback;
	AnselConfig->stopSessionCallback = AnselStopSessionCallback;
	AnselConfig->startCaptureCallback = AnselStartCaptureCallback;
	AnselConfig->stopCaptureCallback = AnselStopCaptureCallback;

	AnselConfig->gameWindowHandle = GEngine->GameViewport->GetWindow()->GetNativeWindow()->GetOSWindowHandle();

	static IConsoleVariable* CVarTranslationSpeed = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Photography.TranslationSpeed"));
	AnselConfig->translationalSpeedInWorldUnitsPerSecond = CVarTranslationSpeed->GetFloat();

	AnselConfig->metersInWorldUnit = 1.0f / 100.0f;
	AWorldSettings* WorldSettings = nullptr;
	if (GEngine->GetWorld() != nullptr)
	{
		WorldSettings = GEngine->GetWorld()->GetWorldSettings();
	}
	if (WorldSettings != nullptr && WorldSettings->WorldToMeters != 0.f)
	{
		AnselConfig->metersInWorldUnit = 1.0f / WorldSettings->WorldToMeters;
	}
	UE_LOG(LogAnsel, Log, TEXT("We reckon %f meters to 1 world unit"), AnselConfig->metersInWorldUnit);

	AnselConfig->isCameraOffcenteredProjectionSupported = true;

	static IConsoleVariable* CVarPersistEffects = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Photography.PersistEffects"));
	AnselConfig->isFilterOutsideSessionAllowed = !!CVarPersistEffects->GetInt();

	AnselConfig->captureLatency = 0; // important

	static IConsoleVariable* CVarSettleFrames = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Photography.SettleFrames"));
	AnselConfig->captureSettleLatency = CVarSettleFrames->GetInt();

	ansel::setConfiguration(*AnselConfig);
}

void FNVAnselCameraPhotographyPrivate::DeconfigureAnsel()
{
	check(AnselConfig != nullptr);
	AnselConfig->userPointer = nullptr;
	AnselConfig->startSessionCallback = nullptr;
	AnselConfig->stopSessionCallback = nullptr;
	AnselConfig->startCaptureCallback = nullptr;
	AnselConfig->stopCaptureCallback = nullptr;
	AnselConfig->gameWindowHandle = nullptr;
	ansel::setConfiguration(*AnselConfig);
}

class FAnselModule : public IAnselModule
{
public:
	virtual void StartupModule() override
	{
		ICameraPhotographyModule::StartupModule();
		check(!bAnselDLLLoaded);

		// Late-load Ansel DLL.  DLL name has been worked out by the build scripts as ANSEL_DLL
		FString AnselDLLName;
		FString AnselBinariesRoot = FPaths::EngineDir() / TEXT("Plugins/Runtime/Nvidia/Ansel/Binaries/ThirdParty/");
		// common preprocessor fudge to convert macro expansion into string
#define STRINGIFY(X) STRINGIFY2(X)
#define STRINGIFY2(X) #X
			AnselDLLName = AnselBinariesRoot + TEXT(STRINGIFY(ANSEL_DLL));
			AnselSDKDLLHandle = FPlatformProcess::GetDllHandle(*(AnselDLLName));

		bAnselDLLLoaded = AnselSDKDLLHandle != 0;
		
		UE_LOG(LogAnsel, Log, TEXT("Tried to load %s : success=%d"), *AnselDLLName, int(bAnselDLLLoaded));
	}

	virtual void ShutdownModule() override
	{		
		if (bAnselDLLLoaded)
		{
			FPlatformProcess::FreeDllHandle(AnselSDKDLLHandle);
			AnselSDKDLLHandle = 0;
			bAnselDLLLoaded = false;
		}
		ICameraPhotographyModule::ShutdownModule();
	}
private:

	virtual TSharedPtr< class ICameraPhotography > CreateCameraPhotography() override
	{
		TSharedPtr<ICameraPhotography> Photography = nullptr;

		FNVAnselCameraPhotographyPrivate* PhotographyPrivate = new FNVAnselCameraPhotographyPrivate();
		if (PhotographyPrivate->IsSupported())
		{
			Photography = TSharedPtr<ICameraPhotography>(PhotographyPrivate);
		}
		else
		{
			delete PhotographyPrivate;
		}

		return Photography;
	}
};

IMPLEMENT_MODULE(FAnselModule, Ansel)
