// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Tests/AutomationCommon.h"
#include "Misc/Paths.h"
#include "Misc/EngineVersion.h"
#include "EngineGlobals.h"
#include "Widgets/SWidget.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "HardwareInfo.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "Kismet/GameplayStatics.h"
#include "ContentStreaming.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "ShaderCompiler.h"
#include "GameFramework/GameStateBase.h"
#include "Scalability.h"
#include "Matinee/MatineeActor.h"
#include "StereoRendering.h"
#include "PackageName.h"

#if (WITH_DEV_AUTOMATION_TESTS || WITH_PERF_AUTOMATION_TESTS)

DEFINE_LOG_CATEGORY_STATIC(LogEngineAutomationLatentCommand, Log, All);
DEFINE_LOG_CATEGORY(LogEditorAutomationTests);
DEFINE_LOG_CATEGORY(LogEngineAutomationTests);

//declare static variable
FOnEditorAutomationMapLoad AutomationCommon::OnEditorAutomationMapLoad;

///////////////////////////////////////////////////////////////////////
// Common Latent commands

namespace AutomationCommon
{
	FString GetRenderDetailsString()
	{
		FString HardwareDetailsString;

		// Create the folder name based on the hardware specs we have been provided
		FString HardwareDetails = FHardwareInfo::GetHardwareDetailsString();

		FString RHIString;
		FString RHILookup = NAME_RHI.ToString() + TEXT("=");
		if ( FParse::Value(*HardwareDetails, *RHILookup, RHIString) )
		{
			HardwareDetailsString = ( HardwareDetailsString + TEXT("_") ) + RHIString;
		}

		FString TextureFormatString;
		FString TextureFormatLookup = NAME_TextureFormat.ToString() + TEXT("=");
		if ( FParse::Value(*HardwareDetails, *TextureFormatLookup, TextureFormatString) )
		{
			HardwareDetailsString = ( HardwareDetailsString + TEXT("_") ) + TextureFormatString;
		}

		FString DeviceTypeString;
		FString DeviceTypeLookup = NAME_DeviceType.ToString() + TEXT("=");
		if ( FParse::Value(*HardwareDetails, *DeviceTypeLookup, DeviceTypeString) )
		{
			HardwareDetailsString = ( HardwareDetailsString + TEXT("_") ) + DeviceTypeString;
		}

		FString FeatureLevelString;
		GetFeatureLevelName(GMaxRHIFeatureLevel, FeatureLevelString);
		{
			HardwareDetailsString = ( HardwareDetailsString + TEXT("_") ) + FeatureLevelString;
		}

		if ( HardwareDetailsString.Len() > 0 )
		{
			//Get rid of the leading "_"
			HardwareDetailsString = HardwareDetailsString.RightChop(1);
		}

		return HardwareDetailsString;
	}

#if (WITH_DEV_AUTOMATION_TESTS || WITH_PERF_AUTOMATION_TESTS)

	/** Gets a path used for automation testing (PNG sent to the AutomationTest folder) */
	void GetScreenshotPath(const FString& TestName, FString& OutScreenshotName)
	{
		FString PathName = FPaths::AutomationDir() + TestName / FPlatformProperties::IniPlatformName();
		PathName = PathName + TEXT("_") + GetRenderDetailsString();

		FPaths::MakePathRelativeTo(PathName, *FPaths::RootDir());

		OutScreenshotName = FString::Printf(TEXT("%s/%s.png"), *PathName, *FPlatformMisc::GetDeviceId());
	}

	FAutomationScreenshotData BuildScreenshotData(const FString& MapOrContext, const FString& TestName, int32 Width, int32 Height)
	{
		FAutomationScreenshotData Data;

		Data.Name = TestName;
		Data.Context = MapOrContext;
		Data.Id = FGuid::NewGuid();
		Data.Commit = FEngineVersion::Current().HasChangelist() ? FString::FromInt(FEngineVersion::Current().GetChangelist()) : FString(TEXT(""));

		Data.Width = Width;
		Data.Height = Height;
		Data.Platform = FPlatformProperties::IniPlatformName();
		Data.Rhi = FHardwareInfo::GetHardwareInfo(NAME_RHI);
		GetFeatureLevelName(GMaxRHIFeatureLevel, Data.FeatureLevel);
		Data.bIsStereo = GEngine->StereoRenderingDevice.IsValid() ? GEngine->StereoRenderingDevice->IsStereoEnabled() : false;
		Data.Vendor = RHIVendorIdToString();
		Data.AdapterName = GRHIAdapterName;
		Data.AdapterInternalDriverVersion = GRHIAdapterInternalDriverVersion;
		Data.AdapterUserDriverVersion = GRHIAdapterUserDriverVersion;
		Data.UniqueDeviceId = FPlatformMisc::GetDeviceId();

		Scalability::FQualityLevels QualityLevels = Scalability::GetQualityLevels();

		Data.ResolutionQuality = QualityLevels.ResolutionQuality;
		Data.ViewDistanceQuality = QualityLevels.ViewDistanceQuality;
		Data.AntiAliasingQuality = QualityLevels.AntiAliasingQuality;
		Data.ShadowQuality = QualityLevels.ShadowQuality;
		Data.PostProcessQuality = QualityLevels.PostProcessQuality;
		Data.TextureQuality = QualityLevels.TextureQuality;
		Data.EffectsQuality = QualityLevels.EffectsQuality;
		Data.FoliageQuality = QualityLevels.FoliageQuality;
		
		//GRHIDeviceId

		// TBD - 
		// Device's native resolution (we want to use a hardware dump of the frontbuffer at the native resolution so we compare what we actually output rather than what we think we rendered)

		const FString MapAndTest = MapOrContext + TEXT("_") + TestName;
		AutomationCommon::GetScreenshotPath(MapAndTest, Data.Path);

		return Data;
	}
#endif

	/** These save a PNG and get sent over the network */
	static void SaveWindowAsScreenshot(TSharedRef<SWindow> Window, const FString& FileName)
	{
		TSharedRef<SWidget> WindowRef = Window;

		TArray<FColor> OutImageData;
		FIntVector OutImageSize;
		if (FSlateApplication::Get().TakeScreenshot(WindowRef, OutImageData, OutImageSize))
		{
			FAutomationScreenshotData Data;
			Data.Width = OutImageSize.X;
			Data.Height = OutImageSize.Y;
			Data.Path = FileName;
			FAutomationTestFramework::Get().OnScreenshotCaptured().ExecuteIfBound(OutImageData, Data);
		}
	}

	// @todo this is a temporary solution. Once we know how to get test's hands on a proper world
	// this function should be redone/removed
	UWorld* GetAnyGameWorld()
	{
		UWorld* TestWorld = nullptr;
		const TIndirectArray<FWorldContext>& WorldContexts = GEngine->GetWorldContexts();
		for ( const FWorldContext& Context : WorldContexts )
		{
			if ( ( ( Context.WorldType == EWorldType::PIE ) || ( Context.WorldType == EWorldType::Game ) ) && ( Context.World() != NULL ) )
			{
				TestWorld = Context.World();
				break;
			}
		}

		return TestWorld;
	}
}

bool AutomationOpenMap(const FString& MapName)
{
	bool bCanProceed = true;
	FString OutString = TEXT("");
#if WITH_EDITOR
	if (GIsEditor && AutomationCommon::OnEditorAutomationMapLoad.IsBound())
	{
		AutomationCommon::OnEditorAutomationMapLoad.Broadcast(MapName, &OutString);
	}
	else
#endif
	{
		UWorld* TestWorld = AutomationCommon::GetAnyGameWorld();

		// Convert both to short names and strip PIE prefix
		FString ShortMapName = FPackageName::GetShortName(MapName);
		FString ShortWorldMapName = FPackageName::GetShortName(TestWorld->GetMapName());

		if (TestWorld->GetOutermost()->PIEInstanceID != INDEX_NONE)
		{
			FString PIEPrefix = FString::Printf(PLAYWORLD_PACKAGE_PREFIX TEXT("_%d_"), TestWorld->GetOutermost()->PIEInstanceID);
			ShortWorldMapName.ReplaceInline(*PIEPrefix, TEXT(""));
		}
		if (ShortMapName != ShortWorldMapName)
		{
			FString OpenCommand = FString::Printf(TEXT("Open %s"), *MapName);
			GEngine->Exec(TestWorld, *OpenCommand);
		}

		ADD_LATENT_AUTOMATION_COMMAND(FWaitForMapToLoadCommand());
	}

	return (OutString.IsEmpty());
}


bool FWaitLatentCommand::Update()
{
	const double NewTime = FPlatformTime::Seconds();
	if (NewTime - StartTime >= Duration)
	{
		return true;
	}
	return false;
}

bool FTakeActiveEditorScreenshotCommand::Update()
{
	AutomationCommon::SaveWindowAsScreenshot(FSlateApplication::Get().GetActiveTopLevelWindow().ToSharedRef(),ScreenshotName);
	return true;
}

bool FTakeEditorScreenshotCommand::Update()
{
	AutomationCommon::SaveWindowAsScreenshot(ScreenshotParameters.CurrentWindow.ToSharedRef(), ScreenshotParameters.ScreenshotName);
	return true;
}

bool FLoadGameMapCommand::Update()
{
	check(GEngine->GetWorldContexts().Num() == 1);
	check(GEngine->GetWorldContexts()[0].WorldType == EWorldType::Game);

	UE_LOG(LogEngineAutomationTests, Log, TEXT("Loading Map Now. '%s'"), *MapName);
	GEngine->Exec(GEngine->GetWorldContexts()[0].World(), *FString::Printf(TEXT("Open %s"), *MapName));
	return true;
}

bool FExitGameCommand::Update()
{
	UWorld* TestWorld = AutomationCommon::GetAnyGameWorld();

	if ( APlayerController* TargetPC = UGameplayStatics::GetPlayerController(TestWorld, 0) )
	{
		TargetPC->ConsoleCommand(TEXT("Exit"), true);
	}

	return true;
}

bool FRequestExitCommand::Update()
{
	GIsRequestingExit = true;
	return true;
}

bool FWaitForMapToLoadCommand::Update()
{
	UWorld* TestWorld = AutomationCommon::GetAnyGameWorld();

	if ( TestWorld && TestWorld->AreActorsInitialized() )
	{
		AGameStateBase* GameState = TestWorld->GetGameState();
		if (GameState && GameState->HasMatchStarted() )
		{
			return true;
		}
	}

	return false;
}

bool FWaitForSpecifiedMapToLoadCommand::Update()
{
	UWorld* TestWorld = AutomationCommon::GetAnyGameWorld();

	if ( TestWorld && TestWorld->GetName() == MapName && TestWorld->AreActorsInitialized() )
	{
		AGameStateBase* GameState = TestWorld->GetGameState();
		if (GameState && GameState->HasMatchStarted() )
		{
			return true;
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////
// Common Latent commands which are used across test type. I.e. Engine, Network, etc...


bool FPlayMatineeLatentCommand::Update()
{
	if (MatineeActor)
	{
		UE_LOG(LogEngineAutomationLatentCommand, Log, TEXT("Triggering the matinee named: '%s'"), *MatineeActor->GetName())

		//force this matinee to not be looping so it doesn't infinitely loop
		MatineeActor->bLooping = false;
		MatineeActor->Play();
	}
	return true;
}


bool FWaitForMatineeToCompleteLatentCommand::Update()
{
	bool bTestComplete = true;
	if (MatineeActor)
	{
		bTestComplete = !MatineeActor->bIsPlaying;
	}
	return bTestComplete;
}


bool FExecStringLatentCommand::Update()
{
	UE_LOG(LogEngineAutomationLatentCommand, Log, TEXT("Executing the console command: '%s'"), *ExecCommand);

	if (GEngine->GameViewport)
	{
		GEngine->GameViewport->Exec(NULL, *ExecCommand, *GLog);
	}
	else
	{
		GEngine->Exec(NULL, *ExecCommand);
	}
	return true;
}


bool FEngineWaitLatentCommand::Update()
{
	float NewTime = FPlatformTime::Seconds();
	if (NewTime - StartTime >= Duration)
	{
		return true;
	}
	return false;
}

ENGINE_API uint32 GStreamAllResourcesStillInFlight = -1;
bool FStreamAllResourcesLatentCommand::Update()
{
	float LocalStartTime = FPlatformTime::Seconds();

	GStreamAllResourcesStillInFlight = IStreamingManager::Get().StreamAllResources(Duration);

	float Time = FPlatformTime::Seconds();

	if(GStreamAllResourcesStillInFlight)
	{
		UE_LOG(LogEngineAutomationLatentCommand, Warning, TEXT("StreamAllResources() waited for %.2fs but %d resources are still in flight."), Time - LocalStartTime, GStreamAllResourcesStillInFlight);
	}
	else
	{
		UE_LOG(LogEngineAutomationLatentCommand, Log, TEXT("StreamAllResources() waited for %.2fs (max duration: %.2f)."), Time - LocalStartTime, Duration);
	}

	return true;
}

bool FEnqueuePerformanceCaptureCommands::Update()
{
	//for every matinee actor in the level
	for (TObjectIterator<AMatineeActor> It; It; ++It)
	{
		AMatineeActor* MatineeActor = *It;
		if (MatineeActor && MatineeActor->GetName().Contains(TEXT("Automation")))
		{
			//add latent action to execute this matinee
			ADD_LATENT_AUTOMATION_COMMAND(FPlayMatineeLatentCommand(MatineeActor));

			//add action to wait until matinee is complete
			ADD_LATENT_AUTOMATION_COMMAND(FWaitForMatineeToCompleteLatentCommand(MatineeActor));
		}
	}

	return true;
}


bool FMatineePerformanceCaptureCommand::Update()
{
	//for every matinee actor in the level
	for (TObjectIterator<AMatineeActor> It; It; ++It)
	{
		AMatineeActor* MatineeActor = *It;
		FString MatineeFOOName = MatineeActor->GetName();
		if (MatineeActor->GetName().Equals(MatineeName, ESearchCase::IgnoreCase))
		{


			//add latent action to execute this matinee
			ADD_LATENT_AUTOMATION_COMMAND(FPlayMatineeLatentCommand(MatineeActor));

			//Run the Stat FPS Chart command
			ADD_LATENT_AUTOMATION_COMMAND(FExecWorldStringLatentCommand(TEXT("StartFPSChart")));

			//add action to wait until matinee is complete
			ADD_LATENT_AUTOMATION_COMMAND(FWaitForMatineeToCompleteLatentCommand(MatineeActor));

			//Stop the Stat FPS Chart command
			ADD_LATENT_AUTOMATION_COMMAND(FExecWorldStringLatentCommand(TEXT("StopFPSChart")));
		}
		else
		{
			UE_LOG(LogEngineAutomationLatentCommand, Log, TEXT("'%s' is not the matinee name that is being searched for."), *MatineeActor->GetName())
		}
	}

	return true;

}


bool FExecWorldStringLatentCommand::Update()
{
	check(GEngine->GetWorldContexts().Num() == 1);
	check(GEngine->GetWorldContexts()[0].WorldType == EWorldType::Game);

	UE_LOG(LogEngineAutomationLatentCommand, Log, TEXT("Running Exec Command. '%s'"), *ExecCommand);
	GEngine->Exec(GEngine->GetWorldContexts()[0].World(), *ExecCommand);
	return true;
}


/**
* This will cause the test to wait for the shaders to finish compiling before moving on.
*/
bool FWaitForShadersToFinishCompilingInGame::Update()
{
	if (GShaderCompilingManager)
	{
		UE_LOG(LogEditorAutomationTests, Log, TEXT("Waiting for %i shaders to finish."), GShaderCompilingManager->GetNumRemainingJobs());
		GShaderCompilingManager->FinishAllCompilation();
		UE_LOG(LogEditorAutomationTests, Log, TEXT("Done waiting for shaders to finish."));
	}
	return true;
}
#endif //WITH_DEV_AUTOMATION_TESTS
