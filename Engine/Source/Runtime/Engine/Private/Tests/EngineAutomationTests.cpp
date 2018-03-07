// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "CoreMinimal.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryWriter.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "GameMapsSettings.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/EngineTypes.h"
#include "UnrealClient.h"
#include "Engine/GameViewportClient.h"
#include "UnrealEngine.h"
#include "Serialization/MemoryReader.h"
#include "UObject/UObjectIterator.h"
#include "EngineUtils.h"
#include "Tests/AutomationTestSettings.h"

#if WITH_EDITOR
#include "FileHelpers.h"
#endif

#include "EngineGlobals.h"
#include "Tests/AutomationCommon.h"
#include "PlatformFeatures.h"
#include "SaveGameSystem.h"
#include "GameFramework/DefaultPawn.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	UWorld* GetSimpleEngineAutomationTestGameWorld(const int32 TestFlags)
	{
		// Accessing the game world is only valid for game-only 
		check((TestFlags & EAutomationTestFlags::ApplicationContextMask) == EAutomationTestFlags::ClientContext);
		check(GEngine->GetWorldContexts().Num() == 1);
		check(GEngine->GetWorldContexts()[0].WorldType == EWorldType::Game);

		return GEngine->GetWorldContexts()[0].World();
	}

	/**
	* Populates the test names and commands for complex tests that are ran on all available maps
	*
	* @param OutBeautifiedNames - The list of map names
	* @param OutTestCommands - The list of commands for each test (The file names in this case)
	*/
	void PopulateTestsForAllAvailableMaps(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands)
	{
		TArray<FString> FileList;
#if WITH_EDITOR
		FEditorFileUtils::FindAllPackageFiles(FileList);
#else
		// Look directly on disk. Very slow!
		FPackageName::FindPackagesInDirectory(FileList, *FPaths::ProjectContentDir());
#endif

		// Iterate over all files, adding the ones with the map extension..
		for (int32 FileIndex = 0; FileIndex < FileList.Num(); FileIndex++)
		{
			const FString& Filename = FileList[FileIndex];

			// Disregard filenames that don't have the map extension if we're in MAPSONLY mode.
			if (FPaths::GetExtension(Filename, true) == FPackageName::GetMapPackageExtension())
			{
				if (FAutomationTestFramework::Get().ShouldTestContent(Filename))
				{
					OutBeautifiedNames.Add(FPaths::GetBaseFilename(Filename));
					OutTestCommands.Add(Filename);
				}
			}
		}
	}
}

#if PLATFORM_DESKTOP

/**
 * SetRes Verification - Verify changing resolution works
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSetResTest, "System.Windows.Set Resolution", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

/** 
 * Change resolutions, wait, and change back
 *
 * @param Parameters - Unused for this test
 * @return	TRUE if the test was successful, FALSE otherwise
 */
bool FSetResTest::RunTest(const FString& Parameters)
{

	//Gets the default map that the game uses.
	const UGameMapsSettings* GameMapsSettings = GetDefault<UGameMapsSettings>();
	const FString& MapName = GameMapsSettings->GetGameDefaultMap();

	//Opens the actual default map in game.
	GEngine->Exec(GetSimpleEngineAutomationTestGameWorld(GetTestFlags()), *FString::Printf(TEXT("Open %s"), *MapName));

	//Gets the current resolution.
	int32 ResX = GSystemResolution.ResX;
	int32 ResY = GSystemResolution.ResY;
	FString RestoreResolutionString = FString::Printf(TEXT("setres %dx%d"), ResX, ResY);

	//Change the resolution and then restore it.
	ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("setres 640x480")));
	ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(RestoreResolutionString));
	return true;
}

#endif

/**
 * Stats verification - Toggle various "stats" commands
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStatsVerificationMapTest, "System.Maps.Stats Verification", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

/** 
 * Execute the loading of one map to verify screen captures and performance captures work
 *
 * @param Parameters - Unused for this test
 * @return	TRUE if the test was successful, FALSE otherwise
 */
bool FStatsVerificationMapTest::RunTest(const FString& Parameters)
{
	UAutomationTestSettings const* AutomationTestSettings = GetDefault<UAutomationTestSettings>();
	check(AutomationTestSettings);

	if ( AutomationTestSettings->AutomationTestmap.IsValid() )
	{
		FString MapName = AutomationTestSettings->AutomationTestmap.GetLongPackageName();

		GEngine->Exec(GetSimpleEngineAutomationTestGameWorld(GetTestFlags()), *FString::Printf(TEXT("Open %s"), *MapName));
	}
	else
	{
		UE_LOG(LogEngineAutomationTests, Log, TEXT("Automation test map doesn't exist or is not set: %s.  \nUsing the currently loaded map."), *AutomationTestSettings->AutomationTestmap.GetLongPackageName());
	}

	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("stat game")));
	ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("stat game")));

	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("stat scenerendering")));
	ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("stat scenerendering")));

	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("stat memory")));
	ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("stat memory")));

	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("stat slate")));
	ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("stat slate")));

	return true;
}

/**
 * LoadAutomationMap
 * Verification automation test to make sure features of map loading work (load, screen capture, performance capture)
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPerformanceCaptureTest, "System.Maps.Performance Capture", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

/** 
 * Execute the loading of one map to verify screen captures and performance captures work
 *
 * @param Parameters - Unused for this test
 * @return	TRUE if the test was successful, FALSE otherwise
 */
bool FPerformanceCaptureTest::RunTest(const FString& Parameters)
{
	UAutomationTestSettings const* AutomationTestSettings = GetDefault<UAutomationTestSettings>();
	check(AutomationTestSettings);
	FString MapName = AutomationTestSettings->AutomationTestmap.GetLongPackageName();

	GEngine->Exec(GetSimpleEngineAutomationTestGameWorld(GetTestFlags()), *FString::Printf(TEXT("Open %s"), *MapName));
	ADD_LATENT_AUTOMATION_COMMAND(FEnqueuePerformanceCaptureCommands());

	return true;
}

/**
 * Latent command to take a screenshot of the viewport
 */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FTakeViewportScreenshotCommand, FString, ScreenshotFileName);

bool FTakeViewportScreenshotCommand::Update()
{
	const bool bShowUI = false;
	const bool bAddFilenameSuffix = false;
	FScreenshotRequest::RequestScreenshot( ScreenshotFileName, bShowUI, bAddFilenameSuffix );
	return true;
}

/**
 * LoadAllMapsInGame
 * Verification automation test to make sure loading all maps succeed without crashing AND does performance captures
 */
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FLoadAllMapsInGameTest, "Project.Maps.Load All In Game", EAutomationTestFlags::ClientContext | EAutomationTestFlags::StressFilter)

/** 
 * Requests a enumeration of all maps to be loaded
 */
void FLoadAllMapsInGameTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const
{
	PopulateTestsForAllAvailableMaps(OutBeautifiedNames, OutTestCommands);
}

/** 
 * Execute the loading of each map and performance captures
 *
 * @param Parameters - Should specify which map name to load
 * @return	TRUE if the test was successful, FALSE otherwise
 */
bool FLoadAllMapsInGameTest::RunTest(const FString& Parameters)
{
	FString MapName = Parameters;

	//Open the map
	GEngine->Exec(GetSimpleEngineAutomationTestGameWorld(GetTestFlags()), *FString::Printf(TEXT("Open %s"), *MapName));

	//if( FAutomationTestFramework::Get().IsScreenshotAllowed() )
	{
		//Generate the screen shot name and path
		FString ScreenshotFileName;
		const FString LoadAllMapsTestName = FString::Printf(TEXT("LoadAllMaps_Game/%s"), *FPaths::GetBaseFilename(MapName));
		AutomationCommon::GetScreenshotPath(LoadAllMapsTestName, ScreenshotFileName);

		//Give the map some time to load
		ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.5f));
		//Take the screen shot
		ADD_LATENT_AUTOMATION_COMMAND(FTakeViewportScreenshotCommand(ScreenshotFileName));
		//Give the screen shot a chance to capture the scene
		ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(0.5f));
	}

	//Kick off any Automation matinees that are in this map
	ADD_LATENT_AUTOMATION_COMMAND(FEnqueuePerformanceCaptureCommands());

	return true;
}

/**
 * SaveGameTest
 * Test makes sure a save game (without UI) saves and loads correctly
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveGameTest, "System.Engine.Game.Noninteractive Save", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

/** 
 * Saves and loads a savegame file
 *
 * @param Parameters - Unused for this test
 * @return	TRUE if the test was successful, FALSE otherwise
 */
bool FSaveGameTest::RunTest(const FString& Parameters)
{
	// automation save name
	const TCHAR* SaveName = TEXT("AutomationSaveTest");
	uint32 SavedData = 99;

	// the blob we are going to write out
	TArray<uint8> Blob;
	FMemoryWriter WriteAr(Blob);
	WriteAr << SavedData;

	// get the platform's save system
	ISaveGameSystem* Save = IPlatformFeaturesModule::Get().GetSaveGameSystem();

	// write it out
	if (Save->SaveGame(false, SaveName, 0, Blob) == false)
	{
		return false;
	}

	// make sure it was written
	if (Save->DoesSaveGameExist(SaveName, 0) == false)
	{
		return false;
	}

	// read it back in
	Blob.Empty();
	if (Save->LoadGame(false, SaveName, 0, Blob) == false)
	{
		return false;
	}

	// make sure it's the same data
	FMemoryReader ReadAr(Blob);
	uint32 LoadedData;
	ReadAr << LoadedData;

	// try to delete it (not all platforms can)
	if (Save->DeleteGame(false, SaveName, 0))
	{
		// make sure it's no longer there
		if (Save->DoesSaveGameExist(SaveName, 0) == true)
		{
			return false;
		}
	}

	return LoadedData == SavedData;
}

/**
 * Automation test to load a map and capture FPS performance charts
 */
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FCinematicFPSPerfTest, "Project.Maps.Cinematic FPS Perf Capture", (EAutomationTestFlags::ClientContext | EAutomationTestFlags::NonNullRHI | EAutomationTestFlags::StressFilter));

void FCinematicFPSPerfTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const
{
	PopulateTestsForAllAvailableMaps(OutBeautifiedNames, OutTestCommands);
}

bool FCinematicFPSPerfTest::RunTest(const FString& Parameters)
{
	//Map to use for this test.
	const FString MapName = Parameters;

	//The name of the matinee actor that will be triggered.
	FString MatineeActorName;

	//Check we are running from commandline
	const FString CommandLine(FCommandLine::Get());
	if (CommandLine.Contains(TEXT("Automation")))
	{
		//Get the name of the matinee to be used.
		//If the game was not launched with the -MatineeName argument then this test will be ran based on time.
		if (FParse::Value(*CommandLine, TEXT("MatineeName="), MatineeActorName))
		{
			//Load map
			ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0f));
			ADD_LATENT_AUTOMATION_COMMAND(FLoadGameMapCommand(MapName));
			ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0f));

			//Start the matinee and perform the FPS Chart
			ADD_LATENT_AUTOMATION_COMMAND(FMatineePerformanceCaptureCommand(MatineeActorName));
			ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0f));

			return true;
		}
		else
		{
			UE_LOG(LogEngineAutomationTests, Log, TEXT("The matinee name was not specified.  Run the game with -MatineeName=\"Name of the matinee actor\"."));

			//Get the name of the console event to trigger the cinematic
			FString CinematicEventCommand;
			if (!FParse::Value(*CommandLine, TEXT("CE="), CinematicEventCommand))
			{
				UE_LOG(LogEngineAutomationTests, Log, TEXT("A console event command was not specified. Defaults to CE START.  Run the game with -CE=\"Command\"."));
				CinematicEventCommand = TEXT("CE Start");
			}

			//Get the length of time the cinematic will run
			float RunTime;
			if (!FParse::Value(*CommandLine, TEXT("RunTime="), RunTime))
			{
				UE_LOG(LogEngineAutomationTests, Log, TEXT("A run time length in seconds was not specified. Defaults to 60 seconds. Run the game with -RunTime=###."));
				RunTime = 60.f;
			}

			//Load map
			ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0f));
			ADD_LATENT_AUTOMATION_COMMAND(FLoadGameMapCommand(MapName));
			ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0f));

			//Start the matinee and perform the FPS Chart
			ADD_LATENT_AUTOMATION_COMMAND(FExecWorldStringLatentCommand(CinematicEventCommand));
			ADD_LATENT_AUTOMATION_COMMAND(FExecWorldStringLatentCommand(TEXT("StartFPSChart")));
			ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(RunTime));
			ADD_LATENT_AUTOMATION_COMMAND(FExecWorldStringLatentCommand(TEXT("StopFPSChart")));
			ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0f));

			return true;

		}
	}
	//If the user is running from the UFE then we'll use the default values.
	//@todo Give the end user a way to specify the values for this test.

	UE_LOG(LogEngineAutomationTests, Log, TEXT("Running the FPS chart performance capturing for 60 seconds while in '%s'.\nThe default CE command won't be used at this time."), *MapName);

	//Load map
	ADD_LATENT_AUTOMATION_COMMAND(FLoadGameMapCommand(MapName));
	ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0f));

	//Start the matinee and perform the FPS Chart
	ADD_LATENT_AUTOMATION_COMMAND(FExecWorldStringLatentCommand(TEXT("StartFPSChart")));
	ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(60.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FExecWorldStringLatentCommand(TEXT("StopFPSChart")));
	ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(1.0f));

	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutomationLogAddMessage, "System.Automation.Log.Add Log Message", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FAutomationLogAddMessage::RunTest(const FString& Parameters)
{
	//** TEST **//
	AddInfo(TEXT("Test log message."));

	//** VERIFY **//
	TestEqual<FString>(TEXT("Test log message was not added to the ExecutionInfo.Log array."), ExecutionInfo.GetEvents().Last().Message, TEXT("Test log message."));
	
	//** TEARDOWN **//
	// We have to empty this log array so that it doesn't show in the automation results window as it may cause confusion.
	ExecutionInfo.RemoveAllEvents(EAutomationEventType::Info);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutomationLogAddWarning, "System.Automation.Log.Add Warning Message", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutomationLogAddWarning::RunTest(const FString& Parameters)
{
	//** TEST **//
	AddWarning(TEXT("Test warning message."));

	//** VERIFY **//
	FString CurrentWarningMessage = ExecutionInfo.GetEvents().Last().Message;
	// The warnings array is emptied so that it doesn't cause a false positive warning for this test.
	ExecutionInfo.RemoveAllEvents(EAutomationEventType::Warning);

	TestEqual<FString>(TEXT("Test warning message was not added to the ExecutionInfo.Warning array."), CurrentWarningMessage, TEXT("Test warning message."));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutomationLogAddError, "System.Automation.Log.Add Error Message", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAutomationLogAddError::RunTest(const FString& Parameters)
{
	//** TEST **//
	AddError(TEXT("Test error message"));
	
	//** VERIFY **//
	FString CurrentErrorMessage = ExecutionInfo.GetEvents().Last().Message;
	// The errors array is emptied so that this doesn't cause a false positive failure for this test.
	ExecutionInfo.RemoveAllEvents(EAutomationEventType::Error);

	TestEqual<FString>(TEXT("Test error message was not added to the ExecutionInfo.Error array."), CurrentErrorMessage, TEXT("Test error message"));
	
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutomationAttachment, "System.Engine.Attachment", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

#define DUMP_EXPECTED_TRANSFORMS 0
#define TEST_NEW_ATTACHMENTS 1
#define TEST_DEPRECATED_ATTACHMENTS 0

namespace AttachTestConstants
{
	const FVector ParentLocation(1.0f, -2.0f, 4.0f);
	const FQuat ParentRotation(FRotator(0.0f, 45.0f, 45.0f).Quaternion());
	const FVector ParentScale(1.25f, 1.25f, 1.25f);
	const FVector ChildLocation(2.0f, -8.0f, -4.0f);
	const FQuat ChildRotation(FRotator(0.0f, 45.0f, 20.0f).Quaternion());
	const FVector ChildScale(1.25f, 1.25f, 1.25f);
}

void AttachmentTest_CommonTests(AActor* ParentActor, AActor* ChildActor, FAutomationTestBase* Test)
{
#if TEST_DEPRECATED_ATTACHMENTS
	static const FTransform LegacyExpectedChildTransforms[4][2] =
	{
		{
			FTransform(
				FQuat(-0.49031073f, -0.11344112f, 0.64335662f, 0.57690436f),
				FVector(10.26776695f, -7.73223305f, 7.53553343f),
				FVector(1.56250000f, 1.56250000f, 1.56250000f)
			),
			FTransform(
				FQuat(-0.49031067f, -0.11344092f, 0.64335656f, 0.57690459f),
				FVector(10.26776695f, -7.73223305f, 7.53553343f),
				FVector(1.56250000f, 1.56250000f, 1.56250000f)
			),
		},
		{
			FTransform(
				FQuat(-0.49031067f, -0.11344092f, 0.64335662f, 0.57690459f),
				FVector(10.26776695f, -7.73223305f, 7.53553343f),
				FVector(1.56250000f, 1.56250000f, 1.56250000f)
			),
			FTransform(
				FQuat(-0.49031061f, -0.11344086f, 0.64335656f, 0.57690465f),
				FVector(10.26776695f, -7.73223305f, 7.53553343f),
				FVector(1.56250000f, 1.56250000f, 1.56250000f)
			),
		},
		{
			FTransform(
				FQuat(-0.35355338f, -0.14644660f, 0.35355338f, 0.85355335f),
				FVector(1.00000000f, -2.00000000f, 4.00000000f),
				FVector(1.56250000f, 1.56250000f, 1.56250000f)
			),
			FTransform(
				FQuat(-0.35355335f, -0.14644656f, 0.35355335f, 0.85355347f),
				FVector(1.00000000f, -2.00000000f, 4.00000000f),
				FVector(1.56250000f, 1.56250000f, 1.56250000f)
			),
		},
		{
			FTransform(
				FQuat(-0.35355338f, -0.14644660f, 0.35355338f, 0.85355335f),
				FVector(1.00000000f, -2.00000000f, 4.00000000f),
				FVector(1.25000000f, 1.25000000f, 1.25000000f)
			),
			FTransform(
				FQuat(-0.35355335f, -0.14644656f, 0.35355335f, 0.85355347f),
				FVector(1.00000000f, -2.00000000f, 4.00000000f),
				FVector(1.25000000f, 1.25000000f, 1.25000000f)
			),
		},
	};

	for (uint8 LocationInteger = (uint8)EAttachLocation::KeepRelativeOffset; LocationInteger <= (uint8)EAttachLocation::SnapToTargetIncludingScale; ++LocationInteger)
	{
#if DUMP_EXPECTED_TRANSFORMS
		UE_LOG(LogTemp, Log, TEXT("{"));
#endif
		EAttachLocation::Type Location = (EAttachLocation::Type)LocationInteger;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
		ChildActor->AttachRootComponentToActor(ParentActor, NAME_None, Location, true);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

		// check parent actor is unaffected by attachment
		Test->TestEqual<FVector>(TEXT("Parent location was affected by attachment"), ParentActor->GetActorLocation(), AttachTestConstants::ParentLocation);
		Test->TestEqual<FQuat>(TEXT("Parent rotation was affected by attachment"), ParentActor->GetActorQuat(), AttachTestConstants::ParentRotation);
		Test->TestEqual<FVector>(TEXT("Parent scale was affected by attachment"), ParentActor->GetActorScale3D(), AttachTestConstants::ParentScale);

		// check we have expected transforms for each mode
#if DUMP_EXPECTED_TRANSFORMS
		UE_LOG(LogTemp, Log, TEXT("\tFTransform("));
		UE_LOG(LogTemp, Log, TEXT("\t\tFQuat(%.8ff, %.8ff, %.8ff, %.8ff),"), ChildActor->GetActorQuat().X, ChildActor->GetActorQuat().Y, ChildActor->GetActorQuat().Z, ChildActor->GetActorQuat().W);
		UE_LOG(LogTemp, Log, TEXT("\t\tFVector(%.8ff, %.8ff, %.8ff),"), ChildActor->GetActorLocation().X, ChildActor->GetActorLocation().Y, ChildActor->GetActorLocation().Z);
		UE_LOG(LogTemp, Log, TEXT("\t\tFVector(%.8ff, %.8ff, %.8ff)"), ChildActor->GetActorScale3D().X, ChildActor->GetActorScale3D().Y, ChildActor->GetActorScale3D().Z);
		UE_LOG(LogTemp, Log, TEXT("\t),"));
#endif

		Test->TestTrue(FString::Printf(TEXT("Child world location was incorrect after attachment (was %s, should be %s)"), *ChildActor->GetActorLocation().ToString(), *LegacyExpectedChildTransforms[LocationInteger][0].GetLocation().ToString()), ChildActor->GetActorLocation().Equals(LegacyExpectedChildTransforms[LocationInteger][0].GetLocation(), KINDA_SMALL_NUMBER));
		Test->TestTrue(FString::Printf(TEXT("Child world rotation was incorrect after attachment (was %s, should be %s)"), *ChildActor->GetActorQuat().ToString(), *LegacyExpectedChildTransforms[LocationInteger][0].GetRotation().ToString()), ChildActor->GetActorQuat().Equals(LegacyExpectedChildTransforms[LocationInteger][0].GetRotation(), KINDA_SMALL_NUMBER));
		Test->TestTrue(FString::Printf(TEXT("Child world scale was incorrect after attachment (was %s, should be %s)"), *ChildActor->GetActorScale3D().ToString(), *LegacyExpectedChildTransforms[LocationInteger][0].GetScale3D().ToString()), ChildActor->GetActorScale3D().Equals(LegacyExpectedChildTransforms[LocationInteger][0].GetScale3D(), KINDA_SMALL_NUMBER));

		ChildActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

		// check we have expected values after detachment
		Test->TestEqual<FVector>(TEXT("Parent location was affected by detachment"), ParentActor->GetActorLocation(), AttachTestConstants::ParentLocation);
		Test->TestEqual<FQuat>(TEXT("Parent rotation was affected by detachment"), ParentActor->GetActorQuat(), AttachTestConstants::ParentRotation);
		Test->TestEqual<FVector>(TEXT("Parent scale was affected by detachment"), ParentActor->GetActorScale3D(), AttachTestConstants::ParentScale);

		// check we have expected transforms for each mode
#if DUMP_EXPECTED_TRANSFORMS
		UE_LOG(LogTemp, Log, TEXT("\tFTransform("));
		UE_LOG(LogTemp, Log, TEXT("\t\tFQuat(%.8ff, %.8ff, %.8ff, %.8ff),"), ChildActor->GetActorQuat().X, ChildActor->GetActorQuat().Y, ChildActor->GetActorQuat().Z, ChildActor->GetActorQuat().W);
		UE_LOG(LogTemp, Log, TEXT("\t\tFVector(%.8ff, %.8ff, %.8ff),"), ChildActor->GetActorLocation().X, ChildActor->GetActorLocation().Y, ChildActor->GetActorLocation().Z);
		UE_LOG(LogTemp, Log, TEXT("\t\tFVector(%.8ff, %.8ff, %.8ff)"), ChildActor->GetActorScale3D().X, ChildActor->GetActorScale3D().Y, ChildActor->GetActorScale3D().Z);
		UE_LOG(LogTemp, Log, TEXT("\t),"));
#endif

		Test->TestTrue(FString::Printf(TEXT("Child relative location was incorrect after detachment (was %s, should be %s)"), *ChildActor->GetActorLocation().ToString(), *LegacyExpectedChildTransforms[LocationInteger][1].GetLocation().ToString()), ChildActor->GetActorLocation().Equals(LegacyExpectedChildTransforms[LocationInteger][1].GetLocation(), KINDA_SMALL_NUMBER));
		Test->TestTrue(FString::Printf(TEXT("Child relative rotation was incorrect after detachment (was %s, should be %s)"), *ChildActor->GetActorQuat().ToString(), *LegacyExpectedChildTransforms[LocationInteger][1].GetRotation().ToString()), ChildActor->GetActorQuat().Equals(LegacyExpectedChildTransforms[LocationInteger][1].GetRotation(), KINDA_SMALL_NUMBER));
		Test->TestTrue(FString::Printf(TEXT("Child relative scale was incorrect after detachment (was %s, should be %s)"), *ChildActor->GetActorScale3D().ToString(), *LegacyExpectedChildTransforms[LocationInteger][1].GetScale3D().ToString()), ChildActor->GetActorScale3D().Equals(LegacyExpectedChildTransforms[LocationInteger][1].GetScale3D(), KINDA_SMALL_NUMBER));
#if DUMP_EXPECTED_TRANSFORMS
		UE_LOG(LogTemp, Log, TEXT("},"));
#endif
	}
#endif

#if TEST_NEW_ATTACHMENTS
	// Check each component against each rule in all combinations, pre and post-detachment
	static const FTransform ExpectedChildTransforms[3][3][3][2] =
	{
		{
			{
				{
					FTransform(
						FQuat(-0.49031073f, -0.11344108f, 0.64335668f, 0.57690459f),
						FVector(10.26776695f, -7.73223495f, 7.53553295f),
						FVector(1.56250000f, 1.56250000f, 1.56250000f)
					),
					FTransform(
						FQuat(-0.16042995f, -0.06645225f, 0.37686956f, 0.90984380f),
						FVector(2.00000000f, -8.00000000f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.49031073f, -0.11344108f, 0.64335668f, 0.57690459f),
						FVector(10.26776695f, -7.73223495f, 7.53553295f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.16042995f, -0.06645225f, 0.37686956f, 0.90984380f),
						FVector(2.00000000f, -8.00000000f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.49031073f, -0.11344108f, 0.64335668f, 0.57690459f),
						FVector(10.26776695f, -7.73223495f, 7.53553295f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.16042995f, -0.06645225f, 0.37686956f, 0.90984380f),
						FVector(2.00000000f, -8.00000000f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
			},
			{
				{
					FTransform(
						FQuat(-0.16042994f, -0.06645226f, 0.37686956f, 0.90984380f),
						FVector(10.26776695f, -7.73223495f, 7.53553295f),
						FVector(1.56250000f, 1.56250000f, 1.56250000f)
					),
					FTransform(
						FQuat(-0.16042991f, -0.06645230f, 0.37686959f, 0.90984380f),
						FVector(2.00000000f, -8.00000000f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.16042991f, -0.06645229f, 0.37686959f, 0.90984380f),
						FVector(10.26776695f, -7.73223495f, 7.53553295f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.16042989f, -0.06645229f, 0.37686959f, 0.90984380f),
						FVector(2.00000000f, -8.00000000f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.16042989f, -0.06645229f, 0.37686956f, 0.90984380f),
						FVector(10.26776695f, -7.73223495f, 7.53553295f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.16042989f, -0.06645229f, 0.37686959f, 0.90984380f),
						FVector(2.00000000f, -8.00000000f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
			},
			{
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644656f, 0.35355335f, 0.85355347f),
						FVector(10.26776695f, -7.73223495f, 7.53553295f),
						FVector(1.56250000f, 1.56250000f, 1.56250000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(2.00000000f, -8.00000000f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644656f, 0.35355335f, 0.85355347f),
						FVector(10.26776695f, -7.73223495f, 7.53553295f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(2.00000000f, -8.00000000f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644656f, 0.35355335f, 0.85355347f),
						FVector(10.26776695f, -7.73223495f, 7.53553295f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(2.00000000f, -8.00000000f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
			},
		},
		{
			{
				{
					FTransform(
						FQuat(-0.60355335f, -0.24999997f, 0.60355341f, 0.45710698f),
						FVector(1.99999976f, -8.00000000f, -4.00000095f),
						FVector(1.56250000f, 1.56250000f, 1.56250000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.99999976f, -8.00000000f, -4.00000095f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.60355335f, -0.24999997f, 0.60355341f, 0.45710698f),
						FVector(1.99999881f, -8.00000095f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.99999881f, -8.00000095f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.60355335f, -0.24999997f, 0.60355341f, 0.45710698f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
			},
			{
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644657f, 0.35355335f, 0.85355347f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.56250000f, 1.56250000f, 1.56250000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644657f, 0.35355335f, 0.85355347f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644657f, 0.35355335f, 0.85355347f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
			},
			{
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644656f, 0.35355335f, 0.85355347f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.56250000f, 1.56250000f, 1.56250000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644656f, 0.35355335f, 0.85355347f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644656f, 0.35355335f, 0.85355347f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.99999857f, -8.00000191f, -4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
			},
		},
		{
			{
				{
					FTransform(
						FQuat(-0.60355335f, -0.24999997f, 0.60355341f, 0.45710698f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.56250000f, 1.56250000f, 1.56250000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.60355335f, -0.24999997f, 0.60355341f, 0.45710698f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.60355335f, -0.24999997f, 0.60355341f, 0.45710698f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
			},
			{
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644657f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.56250000f, 1.56250000f, 1.56250000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644657f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644657f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
			},
			{
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644656f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.56250000f, 1.56250000f, 1.56250000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644656f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
				{
					FTransform(
						FQuat(-0.35355335f, -0.14644656f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
					FTransform(
						FQuat(-0.35355335f, -0.14644659f, 0.35355335f, 0.85355347f),
						FVector(1.00000000f, -2.00000000f, 4.00000000f),
						FVector(1.25000000f, 1.25000000f, 1.25000000f)
					),
				},
			},
		},
	};

	const FVector ParentPreAttachmentLocation = ParentActor->GetActorLocation();
	const FQuat ParentPreAttachmentRotation = ParentActor->GetActorQuat();
	const FVector ParentPreAttachmentScale = ParentActor->GetActorScale3D();

	for (uint8 RuleInteger0 = (uint8)EAttachmentRule::KeepRelative; RuleInteger0 <= (uint8)EAttachmentRule::SnapToTarget; ++RuleInteger0)
	{
#if DUMP_EXPECTED_TRANSFORMS
		UE_LOG(LogTemp, Log, TEXT("{"));
#endif
		for (uint8 RuleInteger1 = (uint8)EAttachmentRule::KeepRelative; RuleInteger1 <= (uint8)EAttachmentRule::SnapToTarget; ++RuleInteger1)
		{
#if DUMP_EXPECTED_TRANSFORMS
			UE_LOG(LogTemp, Log, TEXT("\t{"));
#endif
			for (uint8 RuleInteger2 = (uint8)EAttachmentRule::KeepRelative; RuleInteger2 <= (uint8)EAttachmentRule::SnapToTarget; ++RuleInteger2)
			{
#if DUMP_EXPECTED_TRANSFORMS
				UE_LOG(LogTemp, Log, TEXT("\t\t{"));
#endif
				EAttachmentRule Rule0 = (EAttachmentRule)RuleInteger0;
				EAttachmentRule Rule1 = (EAttachmentRule)RuleInteger1;
				EAttachmentRule Rule2 = (EAttachmentRule)RuleInteger2;

				FAttachmentTransformRules Rules(Rule0, Rule1, Rule2, false);

				ChildActor->AttachToActor(ParentActor, Rules);

				// check parent actor is unaffected by attachment
				Test->TestEqual<FVector>(TEXT("Parent location was affected by attachment"), ParentActor->GetActorLocation(), ParentPreAttachmentLocation);
				Test->TestEqual<FQuat>(TEXT("Parent rotation was affected by attachment"), ParentActor->GetActorQuat(), ParentPreAttachmentRotation);
				Test->TestEqual<FVector>(TEXT("Parent scale was affected by attachment"), ParentActor->GetActorScale3D(), ParentPreAttachmentScale);

				// check we have expected transforms for each mode
#if DUMP_EXPECTED_TRANSFORMS
				UE_LOG(LogTemp, Log, TEXT("\t\t\tFTransform("));
				UE_LOG(LogTemp, Log, TEXT("\t\t\t\tFQuat(%.8ff, %.8ff, %.8ff, %.8ff),"), ChildActor->GetActorQuat().X, ChildActor->GetActorQuat().Y, ChildActor->GetActorQuat().Z, ChildActor->GetActorQuat().W);
				UE_LOG(LogTemp, Log, TEXT("\t\t\t\tFVector(%.8ff, %.8ff, %.8ff),"), ChildActor->GetActorLocation().X, ChildActor->GetActorLocation().Y, ChildActor->GetActorLocation().Z);
				UE_LOG(LogTemp, Log, TEXT("\t\t\t\tFVector(%.8ff, %.8ff, %.8ff)"), ChildActor->GetActorScale3D().X, ChildActor->GetActorScale3D().Y, ChildActor->GetActorScale3D().Z);
				UE_LOG(LogTemp, Log, TEXT("\t\t\t),"));
#endif

				Test->TestTrue(FString::Printf(TEXT("Child world location was incorrect after attachment (was %s, should be %s)"), *ChildActor->GetActorLocation().ToString(), *ExpectedChildTransforms[RuleInteger0][RuleInteger1][RuleInteger2][0].GetLocation().ToString()), ChildActor->GetActorLocation().Equals(ExpectedChildTransforms[RuleInteger0][RuleInteger1][RuleInteger2][0].GetLocation(), KINDA_SMALL_NUMBER));
				Test->TestTrue(FString::Printf(TEXT("Child world rotation was incorrect after attachment (was %s, should be %s)"), *ChildActor->GetActorQuat().ToString(), *ExpectedChildTransforms[RuleInteger0][RuleInteger1][RuleInteger2][0].GetRotation().ToString()), ChildActor->GetActorQuat().Equals(ExpectedChildTransforms[RuleInteger0][RuleInteger1][RuleInteger2][0].GetRotation(), KINDA_SMALL_NUMBER));
				Test->TestTrue(FString::Printf(TEXT("Child world scale was incorrect after attachment (was %s, should be %s)"), *ChildActor->GetActorScale3D().ToString(), *ExpectedChildTransforms[RuleInteger0][RuleInteger1][RuleInteger2][0].GetScale3D().ToString()), ChildActor->GetActorScale3D().Equals(ExpectedChildTransforms[RuleInteger0][RuleInteger1][RuleInteger2][0].GetScale3D(), KINDA_SMALL_NUMBER));

				ChildActor->DetachFromActor(FDetachmentTransformRules(Rules, true));

				// check we have expected values after detachment
				Test->TestEqual<FVector>(TEXT("Parent location was affected by detachment"), ParentActor->GetActorLocation(), ParentPreAttachmentLocation);
				Test->TestEqual<FQuat>(TEXT("Parent rotation was affected by detachment"), ParentActor->GetActorQuat(), ParentPreAttachmentRotation);
				Test->TestEqual<FVector>(TEXT("Parent scale was affected by detachment"), ParentActor->GetActorScale3D(), ParentPreAttachmentScale);

				// check we have expected transforms for each mode
#if DUMP_EXPECTED_TRANSFORMS
				UE_LOG(LogTemp, Log, TEXT("\t\t\tFTransform("));
				UE_LOG(LogTemp, Log, TEXT("\t\t\t\tFQuat(%.8ff, %.8ff, %.8ff, %.8ff),"), ChildActor->GetActorQuat().X, ChildActor->GetActorQuat().Y, ChildActor->GetActorQuat().Z, ChildActor->GetActorQuat().W);
				UE_LOG(LogTemp, Log, TEXT("\t\t\t\tFVector(%.8ff, %.8ff, %.8ff),"), ChildActor->GetActorLocation().X, ChildActor->GetActorLocation().Y, ChildActor->GetActorLocation().Z);
				UE_LOG(LogTemp, Log, TEXT("\t\t\t\tFVector(%.8ff, %.8ff, %.8ff)"), ChildActor->GetActorScale3D().X, ChildActor->GetActorScale3D().Y, ChildActor->GetActorScale3D().Z);
				UE_LOG(LogTemp, Log, TEXT("\t\t\t),"));
#endif

				Test->TestTrue(FString::Printf(TEXT("Child relative location was incorrect after detachment (was %s, should be %s)"), *ChildActor->GetActorLocation().ToString(), *ExpectedChildTransforms[RuleInteger0][RuleInteger1][RuleInteger2][1].GetLocation().ToString()), ChildActor->GetActorLocation().Equals(ExpectedChildTransforms[RuleInteger0][RuleInteger1][RuleInteger2][1].GetLocation(), KINDA_SMALL_NUMBER));
				Test->TestTrue(FString::Printf(TEXT("Child relative rotation was incorrect after detachment (was %s, should be %s)"), *ChildActor->GetActorQuat().ToString(), *ExpectedChildTransforms[RuleInteger0][RuleInteger1][RuleInteger2][1].GetRotation().ToString()), ChildActor->GetActorQuat().Equals(ExpectedChildTransforms[RuleInteger0][RuleInteger1][RuleInteger2][1].GetRotation(), KINDA_SMALL_NUMBER));
				Test->TestTrue(FString::Printf(TEXT("Child relative scale was incorrect after detachment (was %s, should be %s)"), *ChildActor->GetActorScale3D().ToString(), *ExpectedChildTransforms[RuleInteger0][RuleInteger1][RuleInteger2][1].GetScale3D().ToString()), ChildActor->GetActorScale3D().Equals(ExpectedChildTransforms[RuleInteger0][RuleInteger1][RuleInteger2][1].GetScale3D(), KINDA_SMALL_NUMBER));
#if DUMP_EXPECTED_TRANSFORMS
				UE_LOG(LogTemp, Log, TEXT("\t\t},"));
#endif
			}
#if DUMP_EXPECTED_TRANSFORMS
			UE_LOG(LogTemp, Log, TEXT("\t},"));
#endif
		}
#if DUMP_EXPECTED_TRANSFORMS
		UE_LOG(LogTemp, Log, TEXT("},"));
#endif
	}
#endif // TEST_NEW_ATTACHMENTS
}


void AttachmentTest_SetupParentAndChild(UWorld* World, AActor*& InOutParentActor, AActor*& InOutChildActor)
{
	ADefaultPawn* ParentActor = NewObject<ADefaultPawn>(World->PersistentLevel);
	ParentActor->SetActorLocation(AttachTestConstants::ParentLocation);
	ParentActor->SetActorRotation(AttachTestConstants::ParentRotation);
	ParentActor->SetActorScale3D(AttachTestConstants::ParentScale);

	ADefaultPawn* ChildActor = NewObject<ADefaultPawn>(World->PersistentLevel);
	ChildActor->SetActorLocation(AttachTestConstants::ChildLocation);
	ChildActor->SetActorRotation(AttachTestConstants::ChildRotation);
	ChildActor->SetActorScale3D(AttachTestConstants::ChildScale);

	InOutParentActor = ParentActor;
	InOutChildActor = ChildActor;
}

void AttachmentTest_AttachWhenNotAttached(UWorld* World, FAutomationTestBase* Test)
{
	AActor* ParentActor = nullptr;
	AActor* ChildActor = nullptr;
	AttachmentTest_SetupParentAndChild(World, ParentActor, ChildActor);

	AttachmentTest_CommonTests(ParentActor, ChildActor, Test);
}

void AttachmentTest_AttachWhenAttached(UWorld* World, FAutomationTestBase* Test)
{
	ADefaultPawn* PreviousParentActor = NewObject<ADefaultPawn>(World->PersistentLevel);
	PreviousParentActor->SetActorLocation(FVector::ZeroVector);
	PreviousParentActor->SetActorRotation(FQuat::Identity);
	PreviousParentActor->SetActorScale3D(FVector(1.0f));

	AActor* ParentActor = nullptr;
	AActor* ChildActor = nullptr;
	AttachmentTest_SetupParentAndChild(World, ParentActor, ChildActor);

#if TEST_DEPRECATED_ATTACHMENTS
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
		ChildActor->AttachRootComponentToActor(PreviousParentActor, NAME_None, EAttachLocation::KeepWorldPosition, true);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#else
	ChildActor->AttachToActor(PreviousParentActor, FAttachmentTransformRules(EAttachmentRule::KeepWorld, false));
#endif

	AttachmentTest_CommonTests(ParentActor, ChildActor, Test);
}

bool FAutomationAttachment::RunTest(const FString& Parameters)
{
	UWorld *World = UWorld::CreateWorld(EWorldType::Game, false);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);

	FURL URL;
	World->InitializeActorsForPlay(URL);
	World->BeginPlay();

	AttachmentTest_AttachWhenNotAttached(World, this);
	AttachmentTest_AttachWhenAttached(World, this);

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);

	return true;
}

// ----------------------------------------------------------------------------------------------------------------

#include "Matinee/MatineeActor.h"

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FWaitForMatineeToCompleteAndDoScreenshotsLatentCommand, AMatineeActor*, MatineeActor);

bool FWaitForMatineeToCompleteAndDoScreenshotsLatentCommand::Update()
{
	bool bTestComplete = true;
	if (MatineeActor)
	{
		bTestComplete = !MatineeActor->bIsPlaying;
	}
	return bTestComplete;
}

///**
// * FRenderOutputValidation - render deterministic and compare results of multiple runs
// */
//IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRenderOutputValidation, "Rendering.RenderOutputValidation", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
//
///** 
// * render deterministic and compare results of multiple runs
// *
// * @param Parameters - Unused for this test
// * @return	TRUE if the test was successful, FALSE otherwise
// */
//bool FRenderOutputValidation::RunTest(const FString& Parameters)
//{
//	const bool bChangeResolution = false;
//	const bool bLoadMap = false;			// true doesn't work at the moment (iterate matinees needs to happen after loading the map)
//
//	//Gets the default map that the game uses.
//	const UGameMapsSettings* GameMapsSettings = GetDefault<UGameMapsSettings>();
////	const FString& MapName = GameMapsSettings->GetGameDefaultMap();
//	const FString MapName = TEXT("ScreenshotsTest");
//
//	// request full res screenshots
//	//FAutomationTestFramework::Get().SetScreenshotOptions(true);
//
//	if(bLoadMap)
//	{
//		// Opens the actual default map in game.
//		GEngine->Exec(GetSimpleEngineAutomationTestGameWorld(GetTestFlags()), *FString::Printf(TEXT("Open %s"), *MapName));
//		ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(2.0f));
//	}
//
//	{
//		UConsole* ViewportConsole = (GEngine->GameViewport != nullptr) ? GEngine->GameViewport->ViewportConsole : nullptr;
//		FConsoleOutputDevice StrOut(ViewportConsole);
//
//		StrOut.Logf(TEXT("FRenderOutputValidation:Runtest()"));
//	}
//
//	// Gets the current resolution.
//	FString RestoreResolutionString = FString::Printf(TEXT("setres %dx%d"), GSystemResolution.ResX, GSystemResolution.ResY);
//
//	if(bChangeResolution)
//	{
//		// Change the resolution
//		ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("setres 640x480")));
//		ADD_LATENT_AUTOMATION_COMMAND(FEngineWaitLatentCommand(2.0f));
//	}
//	// 5 sec should be enough, if more is needed - we should investigate
//	ADD_LATENT_AUTOMATION_COMMAND(FStreamAllResourcesLatentCommand(5.0f));
//
////later	ADD_LATENT_AUTOMATION_COMMAND(FBuildLightingCommand);
//
//	// streaming should not be distributing it's work over multiple frames - to be more deterministic
//	ADD_LATENT_AUTOMATION_COMMAND(FExecWorldStringLatentCommand(TEXT("r.Streaming.FramesForFullUpdate 0")));
//
//	// reset all data in the ViewState e.g. TemporalAA, SSR cyclic counter, TemporalAA images
//	ADD_LATENT_AUTOMATION_COMMAND(FExecWorldStringLatentCommand(TEXT("r.ResetViewState")));
//
//	// this needs to be improved: better thank hanging would be to render frames (showing the user that it waits on something)
//	ADD_LATENT_AUTOMATION_COMMAND(FWaitForShadersToFinishCompilingInGame);
//
//	//if( FAutomationTestFramework::Get().IsScreenshotAllowed() )
//	{
//		for (TObjectIterator<AMatineeActor> It; It; ++It)
//		{
//			AMatineeActor* MatineeActor = *It;
//
//			FString Name;
//			MatineeActor->GetName(Name);
//
//			if(Name.Contains(TEXT("Automation")))
//			{
//				UE_LOG(LogEngineAutomationTests, Log, TEXT("Iterated matinee: %p '%s'"), MatineeActor, *MatineeActor->GetName())
//
//				//add latent action to execute this matinee
//				ADD_LATENT_AUTOMATION_COMMAND(FPlayMatineeLatentCommand(MatineeActor));
//
//				//Run the Stat FPS Chart command
//		//			ADD_LATENT_AUTOMATION_COMMAND(FExecWorldStringLatentCommand(TEXT("StartFPSChart")));
//
//				//add action to wait until matinee is complete
//				ADD_LATENT_AUTOMATION_COMMAND(FWaitForMatineeToCompleteAndDoScreenshotsLatentCommand(MatineeActor));
//
//				//Stop the Stat FPS Chart command
//		//			ADD_LATENT_AUTOMATION_COMMAND(FExecWorldStringLatentCommand(TEXT("StopFPSChart")));
//			}
//		}
//	}
//
//	if(bChangeResolution)
//	{
//		// restore the resolution
//		ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(RestoreResolutionString));
//	}
//
//	return true;
//}

#endif //WITH_DEV_AUTOMATION_TESTS

/* UAutomationTestSettings interface
*****************************************************************************/

UAutomationTestSettings::UAutomationTestSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultScreenshotResolution = FIntPoint(1920, 1080);
}
