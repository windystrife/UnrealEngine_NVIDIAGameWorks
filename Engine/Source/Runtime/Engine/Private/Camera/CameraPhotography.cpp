// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Camera/CameraPhotography.h"

#include "HAL/IConsoleManager.h"
#include "Misc/CoreDelegates.h"
#include "Features/IModularFeatures.h"
#include "CameraPhotographyModule.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogCameraPhotography, Log, All);

/////////////////////////////////////////////////

static TAutoConsoleVariable<int32> CVarPhotographyAvailable(
	TEXT("r.Photography.Available"),
	1,
	TEXT("(Read-only) If 1, the photography system is potentially available to the user.\n")
	TEXT("Otherwise, a functioning back-end is not available."), 
	ECVF_ReadOnly);

/////////////////////////////////////////////////
// FCameraPhotography internals

static TAutoConsoleVariable<int32> CVarPhotographyAllow(
	TEXT("r.Photography.Allow"),
	1,
	TEXT("If 1, allow the user to freeze the scene and potentially use a roaming camera to\n")
	TEXT("take screenshots.  Set this dynamically to permit or forbid photography per-level,\n")
	TEXT("per-cutscene, etc.  (Default: 1)"));

static TAutoConsoleVariable<int32> CVarPhotographyEnableMultipart(
	TEXT("r.Photography.EnableMultipart"),
	1,
	TEXT("If 1, allow the photography system to take high-resolution shots that need to be rendered in tiles which are later stitched together.  (Default: 1)"));

static TAutoConsoleVariable<int32> CVarPhotographySettleFrames(
	TEXT("r.Photography.SettleFrames"),
	10,
	TEXT("The number of frames to let the rendering 'settle' before taking a photo.  Useful to allow temporal AA/smoothing to work well; if not using any temporal effects, can be lowered for faster capture.  (Default: 10)"));

static TAutoConsoleVariable<float> CVarPhotographyTranslationSpeed(
	TEXT("r.Photography.TranslationSpeed"),
	100.0f,
	TEXT("Normal speed (in unreal units per second) at which to move the roaming photography camera. (Default: 100.0)"));

static TAutoConsoleVariable<int32> CVarPhotographyAutoPostprocess(
	TEXT("r.Photography.AutoPostprocess"),
	1,
	TEXT("If 1, the photography system will attempt to automatically disable HUD, subtitles, and some standard postprocessing effects during photography sessions/captures which are known to give poor photography results.  Set to 0 to manage all postprocessing tweaks manually from the PlayerCameraManager Blueprint callbacks.  Note: Blueprint callbacks will be called regardless of AutoPostprocess value.  (Default: auto-disable (1)"));

static TAutoConsoleVariable<int32> CVarPhotographyAutoPause(
	TEXT("r.Photography.AutoPause"),
	1,
	TEXT("If 1, the photography system will attempt to ensure that the level is paused while in photography mode.  Set to 0 to manage pausing and unpausing manually from the PlayerCameraManager Blueprint callbacks.    Note: Blueprint callbacks will be called regardless of AutoPause value.  (Default: auto-pause (1)"));

static TAutoConsoleVariable<int32> CVarPhotographyPersistEffects(
	TEXT("r.Photography.PersistEffects"),
	0,
	TEXT("If 1, custom postprocessing effects enabled in photography mode are permitted to persist in the game after a photography session has ended.  Changes to this value might not be applied until the next photography session starts.  (Default: Disable (0)"));

FCameraPhotographyManager::FCameraPhotographyManager()
	: ActiveImpl(nullptr)
{
	bool bIsSupported = false;

	// initialize any externally-implemented photography implementations (we delay load initialize the array so any plugins have had time to load)
	TArray<ICameraPhotographyModule*> PluginImplementations = IModularFeatures::Get().GetModularFeatureImplementations<ICameraPhotographyModule>(ICameraPhotographyModule::GetModularFeatureName());

	//we take the first one since we don't have a runtime prioritization scheme for multiple photography implementations.
	for (auto CameraPhotoIt = PluginImplementations.CreateIterator(); CameraPhotoIt && !ActiveImpl.IsValid(); ++CameraPhotoIt)
	{
		ActiveImpl = (*CameraPhotoIt)->CreateCameraPhotography();
	}

	if (ActiveImpl.IsValid())
	{
		UE_LOG(LogCameraPhotography, Log, TEXT("Photography camera created.  Provider=%s, Supported=%d"), ActiveImpl->GetProviderName(), ActiveImpl->IsSupported());
		bIsSupported = ActiveImpl->IsSupported();
	}

	CVarPhotographyAvailable->Set(bIsSupported ? 1 : 0);	
}

FCameraPhotographyManager::~FCameraPhotographyManager()
{
	if (ActiveImpl.IsValid())
	{
		UE_LOG(LogCameraPhotography, Log, TEXT("Photography camera destroyed.  Provider=%s, Supported=%d"), ActiveImpl->GetProviderName(), ActiveImpl->IsSupported());		
		ActiveImpl.Reset();
	}	
}


/////////////////////////////////////////////////
// FCameraPhotography Public API

FCameraPhotographyManager* FCameraPhotographyManager::Singleton = nullptr;

bool FCameraPhotographyManager::IsSupported(UWorld* InWorld)
{
	//we don't want this running on dedicated servers
	if(InWorld && InWorld->GetNetMode() != NM_DedicatedServer)
	{
		if (ICameraPhotography* Impl = Get().ActiveImpl.Get())
		{
			return Impl->IsSupported();
		}
	}
	return false;	
}


FCameraPhotographyManager& FCameraPhotographyManager::Get()
{
	if (nullptr == Singleton)
	{
		Singleton = new FCameraPhotographyManager();
		FCoreDelegates::OnExit.AddStatic(Destroy);
	}

	return *Singleton;
}

void FCameraPhotographyManager::Destroy()
{
	delete Singleton;
	Singleton = nullptr;
}

bool FCameraPhotographyManager::UpdateCamera(FMinimalViewInfo& InOutPOV, APlayerCameraManager* PCMgr)
{
	if (ActiveImpl.IsValid())
	{
		return ActiveImpl->UpdateCamera(InOutPOV, PCMgr);
	}
	return false;
}



