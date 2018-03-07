// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

class AMatineeActor;
class SWindow;

#if (WITH_DEV_AUTOMATION_TESTS || WITH_PERF_AUTOMATION_TESTS)

///////////////////////////////////////////////////////////////////////
// Common Latent commands which are used across test type. I.e. Engine, Network, etc...

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogEditorAutomationTests, Log, All);
ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogEngineAutomationTests, Log, All);

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnEditorAutomationMapLoad, const FString&, FString*);

#endif

/** Common automation functions */
namespace AutomationCommon
{
	/** Get a string contains the render mode we are currently in */
	ENGINE_API FString GetRenderDetailsString();

#if (WITH_DEV_AUTOMATION_TESTS || WITH_PERF_AUTOMATION_TESTS)

	/** Gets a path used for automation testing (PNG sent to the AutomationTest folder) */
	ENGINE_API void GetScreenshotPath(const FString& TestName, FString& OutScreenshotName);

	ENGINE_API FAutomationScreenshotData BuildScreenshotData(const FString& MapOrContext, const FString& TestName, int32 Width, int32 Height);

	ENGINE_API extern FOnEditorAutomationMapLoad OnEditorAutomationMapLoad;
	static FOnEditorAutomationMapLoad& OnEditorAutomationMapLoadDelegate()
	{
		return OnEditorAutomationMapLoad;
	}

#endif
}

#if (WITH_DEV_AUTOMATION_TESTS || WITH_PERF_AUTOMATION_TESTS)

/**
 * Parameters to the Latent Automation command FTakeEditorScreenshotCommand
 */
struct WindowScreenshotParameters
{
	FString ScreenshotName;
	TSharedPtr<SWindow> CurrentWindow;
};

/**
 * If Editor, Opens map and PIES.  If Game, transitions to map and waits for load
 */
ENGINE_API bool AutomationOpenMap(const FString& MapName);

/**
 * Wait for the given amount of time
 */
DEFINE_ENGINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FWaitLatentCommand, float, Duration);

/**
 * Take a screenshot of the active window
 */
DEFINE_ENGINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FTakeActiveEditorScreenshotCommand, FString, ScreenshotName);

/**
 * Take a screenshot of the active window
 */
DEFINE_ENGINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FTakeEditorScreenshotCommand, WindowScreenshotParameters, ScreenshotParameters);

/**
 * Latent command to load a map in game
 */
DEFINE_ENGINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FLoadGameMapCommand, FString, MapName);

/**
 * Latent command to exit the current game
 */
DEFINE_ENGINE_LATENT_AUTOMATION_COMMAND(FExitGameCommand);

/**
 * Latent command that requests exit
 */
DEFINE_ENGINE_LATENT_AUTOMATION_COMMAND( FRequestExitCommand );

/**
* Latent command to wait for map to complete loading
*/
DEFINE_ENGINE_LATENT_AUTOMATION_COMMAND(FWaitForMapToLoadCommand);

/**
* Latent command to wait for map to complete loading
*/
DEFINE_ENGINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FWaitForSpecifiedMapToLoadCommand, FString, MapName);

/**
* Force a matinee to not loop and request that it play
*/
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FPlayMatineeLatentCommand, AMatineeActor*, MatineeActor);


/**
* Wait for a particular matinee actor to finish playing
*/
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FWaitForMatineeToCompleteLatentCommand, AMatineeActor*, MatineeActor);


/**
* Execute command string
*/
DEFINE_ENGINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FExecStringLatentCommand, FString, ExecCommand);


/**
* Wait for the given amount of time
*/
DEFINE_ENGINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FEngineWaitLatentCommand, float, Duration);

/**
* Wait until data is streamed in
*/
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FStreamAllResourcesLatentCommand, float, Duration);

/**
* Enqueue performance capture commands after a map has been loaded
*/
DEFINE_ENGINE_LATENT_AUTOMATION_COMMAND(FEnqueuePerformanceCaptureCommands);


/**
* Run FPS chart command using for the actual duration of the matinee.
*/
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FMatineePerformanceCaptureCommand, FString, MatineeName);

/**
* Latent command to run an exec command that also requires a UWorld.
*/
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FExecWorldStringLatentCommand, FString, ExecCommand);


/**
* Waits for shaders to finish compiling before moving on to the next thing.
*/
DEFINE_ENGINE_LATENT_AUTOMATION_COMMAND(FWaitForShadersToFinishCompilingInGame);

#endif
