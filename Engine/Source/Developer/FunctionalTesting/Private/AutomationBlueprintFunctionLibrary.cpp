// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AutomationBlueprintFunctionLibrary.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "EngineGlobals.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Texture.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Tests/AutomationCommon.h"
#include "Logging/MessageLog.h"
#include "TakeScreenshotAfterTimeLatentAction.h"
#include "HighResScreenshot.h"
#include "Slate/SceneViewport.h"
#include "Tests/AutomationTestSettings.h"
#include "Slate/WidgetRenderer.h"
#include "DelayAction.h"
#include "Widgets/SViewport.h"
#include "Framework/Application/SlateApplication.h"
#include "ShaderCompiler.h"
#include "AutomationBlueprintFunctionLibrary.h"
#include "BufferVisualizationData.h"
#include "Engine/LocalPlayer.h"
#include "ContentStreaming.h"
#include "Stats/StatsData.h"
#include "HAL/PlatformProperties.h"
#include "IAutomationControllerModule.h"

#define LOCTEXT_NAMESPACE "Automation"

DEFINE_LOG_CATEGORY_STATIC(BlueprintAssertion, Error, Error)
DEFINE_LOG_CATEGORY_STATIC(AutomationFunctionLibrary, Log, Log)

static TAutoConsoleVariable<int32> CVarAutomationScreenshotResolutionWidth(
	TEXT("AutomationScreenshotResolutionWidth"),
	0,
	TEXT("The width of automation screenshots."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarAutomationScreenshotResolutionHeight(
	TEXT("AutomationScreenshotResolutionHeight"),
	0,
	TEXT("The height of automation screenshots."),
	ECVF_Default);

#if (WITH_DEV_AUTOMATION_TESTS || WITH_PERF_AUTOMATION_TESTS)

template<typename T>
FConsoleVariableSwapperTempl<T>::FConsoleVariableSwapperTempl(FString InConsoleVariableName)
	: bModified(false)
	, ConsoleVariableName(InConsoleVariableName)
{
}

template<typename T>
void FConsoleVariableSwapperTempl<T>::Set(T Value)
{
	IConsoleVariable* ConsoleVariable = IConsoleManager::Get().FindConsoleVariable(*ConsoleVariableName);
	if (ensure(ConsoleVariable))
	{
		if (bModified == false)
		{
			bModified = true;
			OriginalValue = ConsoleVariable->GetInt();
		}

		ConsoleVariable->Set(Value);
	}
}

template<typename T>
void FConsoleVariableSwapperTempl<T>::Restore()
{
	if (bModified)
	{
		IConsoleVariable* ConsoleVariable = IConsoleManager::Get().FindConsoleVariable(*ConsoleVariableName);
		if (ensure(ConsoleVariable))
		{
			ConsoleVariable->Set(OriginalValue);
		}

		bModified = false;
	}
}

template<>
void FConsoleVariableSwapperTempl<float>::Set(float Value)
{
	IConsoleVariable* ConsoleVariable = IConsoleManager::Get().FindConsoleVariable(*ConsoleVariableName);
	if (ensure(ConsoleVariable))
	{
		if (bModified == false)
		{
			bModified = true;
			OriginalValue = ConsoleVariable->GetFloat();
		}

		ConsoleVariable->Set(Value);
	}
}

FAutomationTestScreenshotEnvSetup::FAutomationTestScreenshotEnvSetup()
	: DefaultFeature_AntiAliasing(TEXT("r.DefaultFeature.AntiAliasing"))
	, DefaultFeature_AutoExposure(TEXT("r.DefaultFeature.AutoExposure"))
	, DefaultFeature_MotionBlur(TEXT("r.DefaultFeature.MotionBlur"))
	, PostProcessAAQuality(TEXT("r.PostProcessAAQuality"))
	, MotionBlurQuality(TEXT("r.MotionBlurQuality"))
	, ScreenSpaceReflectionQuality(TEXT("r.SSR.Quality"))
	, EyeAdaptationQuality(TEXT("r.EyeAdaptationQuality"))
	, ContactShadows(TEXT("r.ContactShadows"))
{
}

void FAutomationTestScreenshotEnvSetup::Setup(FAutomationScreenshotOptions& InOutOptions)
{
	check(IsInGameThread());

	if (InOutOptions.bDisableNoisyRenderingFeatures)
	{
		DefaultFeature_AntiAliasing.Set(0);
		DefaultFeature_AutoExposure.Set(0);
		DefaultFeature_MotionBlur.Set(0);
		PostProcessAAQuality.Set(0);
		MotionBlurQuality.Set(0);
		ScreenSpaceReflectionQuality.Set(0);
		EyeAdaptationQuality.Set(0);
		ContactShadows.Set(0);
	}

	InOutOptions.SetToleranceAmounts(InOutOptions.Tolerance);

	if (UGameViewportClient* ViewportClient = GEngine->GameViewport)
	{
		static IConsoleVariable* ICVar = IConsoleManager::Get().FindConsoleVariable(FBufferVisualizationData::GetVisualizationTargetConsoleCommandName());
		if (ICVar)
		{
			if (ViewportClient->GetEngineShowFlags())
			{
				ViewportClient->GetEngineShowFlags()->SetVisualizeBuffer(InOutOptions.VisualizeBuffer == NAME_None ? false : true);
				ViewportClient->GetEngineShowFlags()->SetTonemapper(InOutOptions.VisualizeBuffer == NAME_None ? true : false);
				ICVar->Set(*InOutOptions.VisualizeBuffer.ToString());
			}
		}
	}
}

void FAutomationTestScreenshotEnvSetup::Restore()
{
	check(IsInGameThread());

	DefaultFeature_AntiAliasing.Restore();
	DefaultFeature_AutoExposure.Restore();
	DefaultFeature_MotionBlur.Restore();
	PostProcessAAQuality.Restore();
	MotionBlurQuality.Restore();
	ScreenSpaceReflectionQuality.Restore();
	EyeAdaptationQuality.Restore();
	ContactShadows.Restore();

	if (UGameViewportClient* ViewportClient = GEngine->GameViewport)
	{
		static IConsoleVariable* ICVar = IConsoleManager::Get().FindConsoleVariable(FBufferVisualizationData::GetVisualizationTargetConsoleCommandName());
		if (ICVar)
		{
			if (ViewportClient->GetEngineShowFlags())
			{
				ViewportClient->GetEngineShowFlags()->SetVisualizeBuffer(false);
				ViewportClient->GetEngineShowFlags()->SetTonemapper(true);
				ICVar->Set(TEXT(""));
			}
		}
	}
}

class FAutomationScreenshotTaker
{
public:
	FAutomationScreenshotTaker(UWorld* InWorld, const FString& InName, FAutomationScreenshotOptions InOptions)
		: World(InWorld)
		, Name(InName)
		, Options(InOptions)
	{
		GEngine->GameViewport->OnScreenshotCaptured().AddRaw(this, &FAutomationScreenshotTaker::GrabScreenShot);

		EnvSetup.Setup(Options);
	}

	virtual ~FAutomationScreenshotTaker()
	{
		EnvSetup.Restore();

		GEngine->GameViewport->OnScreenshotCaptured().RemoveAll(this);

		FAutomationTestFramework::Get().NotifyScreenshotTakenAndCompared();
	}

	void GrabScreenShot(int32 InSizeX, int32 InSizeY, const TArray<FColor>& InImageData)
	{
		check(IsInGameThread());

		FAutomationScreenshotData Data = AutomationCommon::BuildScreenshotData(GWorld->GetName(), Name, InSizeX, InSizeY);

		// Copy the relevant data into the metadata for the screenshot.
		Data.bHasComparisonRules = true;
		Data.ToleranceRed = Options.ToleranceAmount.Red;
		Data.ToleranceGreen = Options.ToleranceAmount.Green;
		Data.ToleranceBlue = Options.ToleranceAmount.Blue;
		Data.ToleranceAlpha = Options.ToleranceAmount.Alpha;
		Data.ToleranceMinBrightness = Options.ToleranceAmount.MinBrightness;
		Data.ToleranceMaxBrightness = Options.ToleranceAmount.MaxBrightness;
		Data.bIgnoreAntiAliasing = Options.bIgnoreAntiAliasing;
		Data.bIgnoreColors = Options.bIgnoreColors;
		Data.MaximumLocalError = Options.MaximumLocalError;
		Data.MaximumGlobalError = Options.MaximumGlobalError;

		FAutomationTestFramework::Get().OnScreenshotCaptured().ExecuteIfBound(InImageData, Data);

		UE_LOG(AutomationFunctionLibrary, Log, TEXT("Screenshot captured as %s"), *Data.Path);

		if ( GIsAutomationTesting )
		{
			FAutomationTestFramework::Get().OnScreenshotCompared.AddRaw(this, &FAutomationScreenshotTaker::OnComparisonComplete);
		}
		else
		{
			delete this;
		}
	}

	void OnComparisonComplete(bool bWasNew, bool bWasSimilar, double MaxLocalDifference, double GlobalDifference, FString ErrorMessage)
	{
		FAutomationTestFramework::Get().OnScreenshotCompared.RemoveAll(this);

		if ( bWasNew )
		{
			UE_LOG(AutomationFunctionLibrary, Warning, TEXT("New Screenshot '%s' was discovered!  Please add a ground truth version of it."), *Name);
		}
		else
		{
			if ( bWasSimilar )
			{
				UE_LOG(AutomationFunctionLibrary, Display, TEXT("Screenshot '%s' was similar!  Global Difference = %f, Max Local Difference = %f"), *Name, GlobalDifference, MaxLocalDifference);
			}
			else
			{
				if ( ErrorMessage.IsEmpty() )
				{
					UE_LOG(AutomationFunctionLibrary, Error, TEXT("Screenshot '%s' test failed, Screnshots were different!  Global Difference = %f, Max Local Difference = %f"), *Name, GlobalDifference, MaxLocalDifference);
				}
				else
				{
					UE_LOG(AutomationFunctionLibrary, Error, TEXT("Screenshot '%s' test failed;  Error = %s"), *Name, *ErrorMessage);
				}
			}
		}

		delete this;
	}

private:

	TWeakObjectPtr<UWorld> World;
	
	FString	Name;
	FAutomationScreenshotOptions Options;

	FAutomationTestScreenshotEnvSetup EnvSetup;
};

#endif

UAutomationBlueprintFunctionLibrary::UAutomationBlueprintFunctionLibrary(const class FObjectInitializer& Initializer)
	: Super(Initializer)
{
}

void UAutomationBlueprintFunctionLibrary::FinishLoadingBeforeScreenshot()
{
	// Finish compiling the shaders if the platform doesn't require cooked data.
	if (!FPlatformProperties::RequiresCookedData())
	{
		GShaderCompilingManager->FinishAllCompilation();
		FModuleManager::GetModuleChecked<IAutomationControllerModule>("AutomationController").GetAutomationController()->ResetAutomationTestTimeout(TEXT("shader compilation"));
	}

	// Force all mip maps to load before taking the screenshot.
	UTexture::ForceUpdateTextureStreaming();

	IStreamingManager::Get().StreamAllResources(0.0f);
}

FIntPoint UAutomationBlueprintFunctionLibrary::GetAutomationScreenshotSize(const FAutomationScreenshotOptions& Options)
{
	// Fallback resolution if all else fails for screenshots.
	uint32 ResolutionX = 1280;
	uint32 ResolutionY = 720;

	// First get the default set for the project.
	UAutomationTestSettings const* AutomationTestSettings = GetDefault<UAutomationTestSettings>();
	if (AutomationTestSettings->DefaultScreenshotResolution.GetMin() > 0)
	{
		ResolutionX = (uint32)AutomationTestSettings->DefaultScreenshotResolution.X;
		ResolutionY = (uint32)AutomationTestSettings->DefaultScreenshotResolution.Y;
	}

	// If there's an override resolution, use that instead.
	if (Options.Resolution.GetMin() > 0)
	{
		ResolutionX = (uint32)Options.Resolution.X;
		ResolutionY = (uint32)Options.Resolution.Y;
	}
	else
	{
		// Failing to find an override, look for a platform override that may have been provided through the
		// device profiles setup, to configure the CVars for controlling the automation screenshot size.
		int32 OverrideWidth = CVarAutomationScreenshotResolutionWidth.GetValueOnGameThread();
		int32 OverrideHeight = CVarAutomationScreenshotResolutionHeight.GetValueOnGameThread();

		if (OverrideWidth > 0)
		{
			ResolutionX = (uint32)OverrideWidth;
		}

		if (OverrideHeight > 0)
		{
			ResolutionY = (uint32)OverrideHeight;
		}
	}

	return FIntPoint(ResolutionX, ResolutionY);
}

bool UAutomationBlueprintFunctionLibrary::TakeAutomationScreenshotInternal(UObject* WorldContextObject, const FString& Name, FAutomationScreenshotOptions Options)
{
	UAutomationBlueprintFunctionLibrary::FinishLoadingBeforeScreenshot();

	FIntPoint ScreenshotRes = GetAutomationScreenshotSize(Options);

#if (WITH_DEV_AUTOMATION_TESTS || WITH_PERF_AUTOMATION_TESTS)
	FAutomationScreenshotTaker* TempObject = new FAutomationScreenshotTaker(WorldContextObject ? WorldContextObject->GetWorld() : nullptr, Name, Options);
#endif

	//static IConsoleVariable* HighResScreenshotDelay = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HighResScreenshotDelay"));
	//check(HighResScreenshotDelay);
	//HighResScreenshotDelay->Set(10);

    if ( FPlatformProperties::HasFixedResolution() )
    {
	    FScreenshotRequest::RequestScreenshot(false);
	    return true;
    }
	else
	{
	    FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();

	    if ( Config.SetResolution(ScreenshotRes.X, ScreenshotRes.Y, 1.0f) )
	    {
			if ( !GEngine->GameViewport->GetGameViewport()->TakeHighResScreenShot() )
			{
				// If we failed to take the screenshot, we're going to need to cleanup the automation screenshot taker.
#if (WITH_DEV_AUTOMATION_TESTS || WITH_PERF_AUTOMATION_TESTS)
				delete TempObject;
#endif 
				return false;
			}

			return true; //-V773
		}
	}

	return false;
}

void UAutomationBlueprintFunctionLibrary::TakeAutomationScreenshot(UObject* WorldContextObject, FLatentActionInfo LatentInfo, const FString& Name, const FAutomationScreenshotOptions& Options)
{
	if ( GIsAutomationTesting )
	{
		if ( UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) )
		{
			FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
			if ( LatentActionManager.FindExistingAction<FTakeScreenshotAfterTimeLatentAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr )
			{
				LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FTakeScreenshotAfterTimeLatentAction(LatentInfo, Name, Options));
			}
		}
	}
	else
	{
		UE_LOG(AutomationFunctionLibrary, Log, TEXT("Screenshot not captured - screenshots are only taken during automation tests"));
	}
}

void UAutomationBlueprintFunctionLibrary::TakeAutomationScreenshotAtCamera(UObject* WorldContextObject, FLatentActionInfo LatentInfo, ACameraActor* Camera, const FString& NameOverride, const FAutomationScreenshotOptions& Options)
{
	if ( Camera == nullptr )
	{
		FMessageLog("PIE").Error(LOCTEXT("CameraRequired", "A camera is required to TakeAutomationScreenshotAtCamera"));
		return;
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(WorldContextObject, 0);

	if ( PlayerController == nullptr )
	{
		FMessageLog("PIE").Error(LOCTEXT("PlayerRequired", "A player controller is required to TakeAutomationScreenshotAtCamera"));
		return;
	}

	// Move the player, then queue up a screenshot.
	// We need to delay before the screenshot so that the motion blur has time to stop.
	PlayerController->SetViewTarget(Camera, FViewTargetTransitionParams());
	FString ScreenshotName = Camera->GetName();

	if ( !NameOverride.IsEmpty() )
	{
		ScreenshotName = NameOverride;
	}

	if ( UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) )
	{
		ScreenshotName = FString::Printf(TEXT("%s_%s"), *World->GetName(), *ScreenshotName);

		FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
		if ( LatentActionManager.FindExistingAction<FTakeScreenshotAfterTimeLatentAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr )
		{
			LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FTakeScreenshotAfterTimeLatentAction(LatentInfo, ScreenshotName, Options));
		}
	}
}

bool UAutomationBlueprintFunctionLibrary::TakeAutomationScreenshotOfUI_Immediate(UObject* WorldContextObject, const FString& Name, const FAutomationScreenshotOptions& Options)
{
	UAutomationBlueprintFunctionLibrary::FinishLoadingBeforeScreenshot();

	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (UGameViewportClient* GameViewport = WorldContextObject->GetWorld()->GetGameViewport())
		{
			TSharedPtr<SViewport> Viewport = GameViewport->GetGameViewportWidget();
			if (Viewport.IsValid())
			{
				TArray<FColor> OutColorData;
				FIntVector OutSize;
				if (FSlateApplication::Get().TakeScreenshot(Viewport.ToSharedRef(), OutColorData, OutSize))
				{
#if (WITH_DEV_AUTOMATION_TESTS || WITH_PERF_AUTOMATION_TESTS)
					// For UI, we only care about what the final image looks like. So don't compare alpha channel.
					// In editor, scene is rendered into a PF_B8G8R8A8 RT and then copied over to the R10B10G10A2 swapchain back buffer and
					// this copy ignores alpha. In game, however, scene is directly rendered into the back buffer and the alpha values are
					// already meaningless at that stage.
					for (int32 Idx = 0; Idx < OutColorData.Num(); ++Idx)
					{
						OutColorData[Idx].A = 0xff;
					}

					// The screenshot taker deletes itself later.
					FAutomationScreenshotTaker* TempObject = new FAutomationScreenshotTaker(World, Name, Options);

					FAutomationScreenshotData Data = AutomationCommon::BuildScreenshotData(World->GetName(), Name, OutSize.X, OutSize.Y);

					// Copy the relevant data into the metadata for the screenshot.
					Data.bHasComparisonRules = true;
					Data.ToleranceRed = Options.ToleranceAmount.Red;
					Data.ToleranceGreen = Options.ToleranceAmount.Green;
					Data.ToleranceBlue = Options.ToleranceAmount.Blue;
					Data.ToleranceAlpha = Options.ToleranceAmount.Alpha;
					Data.ToleranceMinBrightness = Options.ToleranceAmount.MinBrightness;
					Data.ToleranceMaxBrightness = Options.ToleranceAmount.MaxBrightness;
					Data.bIgnoreAntiAliasing = Options.bIgnoreAntiAliasing;
					Data.bIgnoreColors = Options.bIgnoreColors;
					Data.MaximumLocalError = Options.MaximumLocalError;
					Data.MaximumGlobalError = Options.MaximumGlobalError;

					GEngine->GameViewport->OnScreenshotCaptured().Broadcast(OutSize.X, OutSize.Y, OutColorData);
#endif

					return true; //-V773
				}
			}
		}
	}

	return false;
}

void UAutomationBlueprintFunctionLibrary::TakeAutomationScreenshotOfUI(UObject* WorldContextObject, FLatentActionInfo LatentInfo, const FString& Name, const FAutomationScreenshotOptions& Options)
{
	if (TakeAutomationScreenshotOfUI_Immediate(WorldContextObject, Name, Options))
	{
		FLatentActionManager& LatentActionManager = WorldContextObject->GetWorld()->GetLatentActionManager();
		if ( LatentActionManager.FindExistingAction<FTakeScreenshotAfterTimeLatentAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr )
		{
			LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FWaitForScreenshotComparisonLatentAction(LatentInfo));
		}
	}
}

void UAutomationBlueprintFunctionLibrary::EnableStatGroup(UObject* WorldContextObject, FName GroupName)
{
#if STATS
	if (FGameThreadStatsData* StatsData = FLatestGameThreadStatsData::Get().Latest)
	{
		const FString GroupNameString = FString(TEXT("STATGROUP_")) + GroupName.ToString();
		const FName GroupNameFull = FName(*GroupNameString, EFindName::FNAME_Find);
		if(StatsData->GroupNames.Contains(GroupNameFull))
		{
			return;
		}
	}

	if (APlayerController* TargetPC = UGameplayStatics::GetPlayerController(WorldContextObject, 0))
	{
		TargetPC->ConsoleCommand( FString(TEXT("stat ")) + GroupName.ToString() + FString(TEXT(" -nodisplay")), /*bWriteToLog=*/false);
	}
#endif
}

void UAutomationBlueprintFunctionLibrary::DisableStatGroup(UObject* WorldContextObject, FName GroupName)
{
#if STATS
	if (FGameThreadStatsData* StatsData = FLatestGameThreadStatsData::Get().Latest)
	{
		const FString GroupNameString = FString(TEXT("STATGROUP_")) + GroupName.ToString();
		const FName GroupNameFull = FName(*GroupNameString, EFindName::FNAME_Find);

		if (!StatsData->GroupNames.Contains(GroupNameFull))
		{
			return;
		}
	}

	if (APlayerController* TargetPC = UGameplayStatics::GetPlayerController(WorldContextObject, 0))
	{
		TargetPC->ConsoleCommand(FString(TEXT("stat ")) + GroupName.ToString() + FString(TEXT(" -nodisplay")), /*bWriteToLog=*/false);
	}
#endif
}

#if STATS
template <EComplexStatField::Type ValueType, bool bCallCount = false>
float HelperGetStat(FName StatName)
{
	if (FGameThreadStatsData* StatsData = FLatestGameThreadStatsData::Get().Latest)
	{
		if (const FComplexStatMessage* StatMessage = StatsData->GetStatData(StatName))
		{
			if(bCallCount)
			{
				return StatMessage->GetValue_CallCount(ValueType);	
			}
			else
			{
				return FPlatformTime::ToMilliseconds(StatMessage->GetValue_Duration(ValueType));
			}
		}
	}

#if WITH_EDITOR
	FText WarningOut = FText::Format(LOCTEXT("StatNotFound", "Could not find stat data for {0}, did you call ToggleStatGroup with enough time to capture data?"), FText::FromName(StatName));
	FMessageLog("PIE").Warning(WarningOut);
	UE_LOG(AutomationFunctionLibrary, Warning, TEXT("%s"), *WarningOut.ToString());
#endif

	return 0.f;
}
#endif

float UAutomationBlueprintFunctionLibrary::GetStatIncAverage(FName StatName)
{
#if STATS
	return HelperGetStat<EComplexStatField::IncAve>(StatName);
#else
	return 0.0f;
#endif
}

float UAutomationBlueprintFunctionLibrary::GetStatIncMax(FName StatName)
{
#if STATS
	return HelperGetStat<EComplexStatField::IncMax>(StatName);
#else
	return 0.0f;
#endif
}

float UAutomationBlueprintFunctionLibrary::GetStatExcAverage(FName StatName)
{
#if STATS
	return HelperGetStat<EComplexStatField::ExcAve>(StatName);
#else
	return 0.0f;
#endif
}

float UAutomationBlueprintFunctionLibrary::GetStatExcMax(FName StatName)
{
#if STATS
	return HelperGetStat<EComplexStatField::ExcMax>(StatName);
#else
	return 0.0f;
#endif
}

float UAutomationBlueprintFunctionLibrary::GetStatCallCount(FName StatName)
{
#if STATS
	return HelperGetStat<EComplexStatField::IncAve, /*bCallCount=*/true>(StatName);
#else
	return 0.0f;
#endif
}

bool UAutomationBlueprintFunctionLibrary::AreAutomatedTestsRunning()
{
	return GIsAutomationTesting;
}

FAutomationScreenshotOptions UAutomationBlueprintFunctionLibrary::GetDefaultScreenshotOptionsForGameplay(EComparisonTolerance Tolerance, float Delay)
{
	FAutomationScreenshotOptions Options;
	Options.Delay = Delay;
	Options.Tolerance = Tolerance;
	Options.bDisableNoisyRenderingFeatures = true;
	Options.bIgnoreAntiAliasing = true;
	Options.SetToleranceAmounts(Tolerance);

	return Options;
}

FAutomationScreenshotOptions UAutomationBlueprintFunctionLibrary::GetDefaultScreenshotOptionsForRendering(EComparisonTolerance Tolerance, float Delay)
{
	FAutomationScreenshotOptions Options;
	Options.Delay = Delay;
	Options.Tolerance = Tolerance;
	Options.bDisableNoisyRenderingFeatures = true;
	Options.bIgnoreAntiAliasing = true;
	Options.SetToleranceAmounts(Tolerance);

	return Options;
}

#undef LOCTEXT_NAMESPACE
