// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "HAL/PlatformStackWalk.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Internationalization/Internationalization.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Internationalization/Regex.h"


DEFINE_LOG_CATEGORY_STATIC(LogAutomationTest, Warning, All);

void FAutomationTestFramework::FAutomationTestFeedbackContext::Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	const int32 STACK_OFFSET = 7;
	// TODO would be nice to search for the first stack frame that isn't in outputdevice or other logging files, would be more robust.

	if (!IsRunningCommandlet() && (Verbosity == ELogVerbosity::SetColor))
	{
		return;
	}

	// Ensure there's a valid unit test associated with the context
	if ( CurTest )
	{
		// Warnings
		if ( Verbosity == ELogVerbosity::Warning )
		{
			// If warnings should be treated as errors, log the warnings as such in the current unit test
			if ( TreatWarningsAsErrors )
			{
				CurTest->AddError(FString(V), STACK_OFFSET);
			}
			else
			{
				CurTest->AddWarning(FString(V), STACK_OFFSET);
			}
		}
		// Errors
		else if ( Verbosity == ELogVerbosity::Error )
		{
			CurTest->AddError(FString(V), STACK_OFFSET);
		}
		// Display
		if ( Verbosity == ELogVerbosity::Display )
		{
			CurTest->AddInfo(FString(V), STACK_OFFSET);
		}
		// Log...etc
		else
		{
			// IMPORTANT NOTE: This code will never be called in a build with NO_LOGGING defined, which means pretty much
			// any Test or Shipping config build.  If you're trying to use the automation test framework for performance
			// data capture in a Test config, you'll want to call the AddAnalyticsItemToCurrentTest() function instead of
			// using this log interception stuff.

			FString LogString = FString(V);
			FString AnalyticsString = TEXT("AUTOMATIONANALYTICS");
			if (LogString.StartsWith(*AnalyticsString))
			{
				//Remove "analytics" from the string
				LogString = LogString.Right(LogString.Len() - (AnalyticsString.Len() + 1));

				CurTest->AddAnalyticsItem(LogString);
			}
			//else
			//{
			//	CurTest->AddInfo(LogString, STACK_OFFSET);
			//}
		}
	}
}

FAutomationTestFramework& FAutomationTestFramework::Get()
{
	static FAutomationTestFramework Framework;
	return Framework;
}

FString FAutomationTestFramework::GetUserAutomationDirectory() const
{
	const FString DefaultAutomationSubFolder = TEXT("Unreal Automation");
	return FString(FPlatformProcess::UserDir()) + DefaultAutomationSubFolder;
}

bool FAutomationTestFramework::RegisterAutomationTest( const FString& InTestNameToRegister, class FAutomationTestBase* InTestToRegister )
{
	const bool bAlreadyRegistered = AutomationTestClassNameToInstanceMap.Contains( InTestNameToRegister );
	if ( !bAlreadyRegistered )
	{
		AutomationTestClassNameToInstanceMap.Add( InTestNameToRegister, InTestToRegister );
	}
	return !bAlreadyRegistered;
}

bool FAutomationTestFramework::UnregisterAutomationTest( const FString& InTestNameToUnregister )
{
	const bool bRegistered = AutomationTestClassNameToInstanceMap.Contains( InTestNameToUnregister );
	if ( bRegistered )
	{
		AutomationTestClassNameToInstanceMap.Remove( InTestNameToUnregister );
	}
	return bRegistered;
}

void FAutomationTestFramework::EnqueueLatentCommand(TSharedPtr<IAutomationLatentCommand> NewCommand)
{
	//ensure latent commands are never used within smoke tests - will only catch when smokes are exclusively requested
	check((RequestedTestFilter & EAutomationTestFlags::FilterMask) != EAutomationTestFlags::SmokeFilter);

	//ensure we are currently "running a test"
	check(GIsAutomationTesting);

	LatentCommands.Enqueue(NewCommand);
}

void FAutomationTestFramework::EnqueueNetworkCommand(TSharedPtr<IAutomationNetworkCommand> NewCommand)
{
	//ensure latent commands are never used within smoke tests
	check((RequestedTestFilter & EAutomationTestFlags::FilterMask) != EAutomationTestFlags::SmokeFilter);

	//ensure we are currently "running a test"
	check(GIsAutomationTesting);

	NetworkCommands.Enqueue(NewCommand);
}

bool FAutomationTestFramework::ContainsTest( const FString& InTestName ) const
{
	return AutomationTestClassNameToInstanceMap.Contains( InTestName );
}

bool FAutomationTestFramework::RunSmokeTests()
{
	bool bAllSuccessful = true;

	uint32 PreviousRequestedTestFilter = RequestedTestFilter;
	//so extra log spam isn't generated
	RequestedTestFilter = EAutomationTestFlags::SmokeFilter;
	
	// Skip running on cooked platforms like mobile
	//@todo - better determination of whether to run than requires cooked data
	// Ensure there isn't another slow task in progress when trying to run unit tests
	const bool bRequiresCookedData = FPlatformProperties::RequiresCookedData();
	if ((!bRequiresCookedData && !GIsSlowTask && !GIsPlayInEditorWorld && !FPlatformProperties::IsProgram()) || bForceSmokeTests)
	{
		TArray<FAutomationTestInfo> TestInfo;

		GetValidTestNames( TestInfo );

		if ( TestInfo.Num() > 0 )
		{
			const double SmokeTestStartTime = FPlatformTime::Seconds();

			// Output the results of running the automation tests
			TMap<FString, FAutomationTestExecutionInfo> OutExecutionInfoMap;

			// Run each valid test
			FScopedSlowTask SlowTask(TestInfo.Num());

			// We disable capturing the stack when running smoke tests, it adds too much overhead to do it at startup.
			FAutomationTestFramework::Get().SetCaptureStack(false);

			for ( int TestIndex = 0; TestIndex < TestInfo.Num(); ++TestIndex )
			{
				SlowTask.EnterProgressFrame(1);
				if (TestInfo[TestIndex].GetTestFlags() & EAutomationTestFlags::SmokeFilter )
				{
					FString TestCommand = TestInfo[TestIndex].GetTestName();
					FAutomationTestExecutionInfo& CurExecutionInfo = OutExecutionInfoMap.Add( TestCommand, FAutomationTestExecutionInfo() );
					
					int32 RoleIndex = 0;  //always default to "local" role index.  Only used for multi-participant tests
					StartTestByName( TestCommand, RoleIndex );
					const bool CurTestSuccessful = StopTest(CurExecutionInfo);

					bAllSuccessful = bAllSuccessful && CurTestSuccessful;
				}
			}

			FAutomationTestFramework::Get().SetCaptureStack(true);

			const double EndTime = FPlatformTime::Seconds();
			const double TimeForTest = static_cast<float>(EndTime - SmokeTestStartTime);
			if (TimeForTest > 2.0f)
			{
				//force a failure if a smoke test takes too long
				UE_LOG(LogAutomationTest, Warning, TEXT("Smoke tests took > 2s to run: %.2fs"), (float)TimeForTest);
			}

			FAutomationTestFramework::DumpAutomationTestExecutionInfo( OutExecutionInfoMap );
		}
	}
	else if( bRequiresCookedData )
	{
		UE_LOG( LogAutomationTest, Log, TEXT( "Skipping unit tests for the cooked build." ) );
	}
	else if (!FPlatformProperties::IsProgram())
	{
		UE_LOG(LogAutomationTest, Error, TEXT("Skipping unit tests.") );
		bAllSuccessful = false;
	}

	//revert to allowing all logs
	RequestedTestFilter = PreviousRequestedTestFilter;

	return bAllSuccessful;
}

void FAutomationTestFramework::ResetTests()
{
	bool bEnsureExists = false;
	bool bDeleteEntireTree = true;
	//make sure all transient files are deleted successfully
	IFileManager::Get().DeleteDirectory(*FPaths::AutomationTransientDir(), bEnsureExists, bDeleteEntireTree);
}

void FAutomationTestFramework::StartTestByName( const FString& InTestToRun, const int32 InRoleIndex )
{
	if (GIsAutomationTesting)
	{
		while(!LatentCommands.IsEmpty())
		{
			TSharedPtr<IAutomationLatentCommand> TempCommand;
			LatentCommands.Dequeue(TempCommand);
		}
		while(!NetworkCommands.IsEmpty())
		{
			TSharedPtr<IAutomationNetworkCommand> TempCommand;
			NetworkCommands.Dequeue(TempCommand);
		}
		FAutomationTestExecutionInfo TempExecutionInfo;
		StopTest(TempExecutionInfo);
	}

	FString TestName;
	FString Params;
	if (!InTestToRun.Split(TEXT(" "), &TestName, &Params, ESearchCase::CaseSensitive))
	{
		TestName = InTestToRun;
	}

	NetworkRoleIndex = InRoleIndex;

	// Ensure there isn't another slow task in progress when trying to run unit tests
	if ( !GIsSlowTask && !GIsPlayInEditorWorld )
	{
		// Ensure the test exists in the framework and is valid to run
		if ( ContainsTest( TestName ) )
		{
			// Make any setting changes that have to occur to support unit testing
			PrepForAutomationTests();

			InternalStartTest( InTestToRun );
		}
		else
		{
			UE_LOG(LogAutomationTest, Error, TEXT("Test %s does not exist and could not be run."), *InTestToRun);
		}
	}
	else
	{
		UE_LOG(LogAutomationTest, Error, TEXT("Test %s is too slow and could not be run."), *InTestToRun);
	}
}

bool FAutomationTestFramework::StopTest( FAutomationTestExecutionInfo& OutExecutionInfo )
{
	check(GIsAutomationTesting);
	
	bool bSuccessful = InternalStopTest(OutExecutionInfo);

	// Restore any changed settings now that unit testing has completed
	ConcludeAutomationTests();

	return bSuccessful;
}


bool FAutomationTestFramework::ExecuteLatentCommands()
{
	check(GIsAutomationTesting);

	bool bHadAnyLatentCommands = !LatentCommands.IsEmpty();
	while (!LatentCommands.IsEmpty())
	{
		//get the next command to execute
		TSharedPtr<IAutomationLatentCommand> NextCommand;
		LatentCommands.Peek(NextCommand);

		bool bComplete = NextCommand->InternalUpdate();
		if (bComplete)
		{
			//all done.  remove from the queue
			LatentCommands.Dequeue(NextCommand);
		}
		else
		{
			break;
		}
	}
	//need more processing on the next frame
	if (bHadAnyLatentCommands)
	{
		return false;
	}

	return true;
}

bool FAutomationTestFramework::ExecuteNetworkCommands()
{
	check(GIsAutomationTesting);
	bool bHadAnyNetworkCommands = !NetworkCommands.IsEmpty();

	if( bHadAnyNetworkCommands )
	{
		// Get the next command to execute
		TSharedPtr<IAutomationNetworkCommand> NextCommand;
		NetworkCommands.Dequeue(NextCommand);
		if (NextCommand->GetRoleIndex() == NetworkRoleIndex)
		{
			NextCommand->Run();
		}
	}

	return !bHadAnyNetworkCommands;
}

void FAutomationTestFramework::LoadTestModules( )
{
	const bool bRunningEditor = GIsEditor && !IsRunningCommandlet();

	bool bRunningSmokeTests = ((RequestedTestFilter & EAutomationTestFlags::FilterMask) == EAutomationTestFlags::SmokeFilter);
	if( !bRunningSmokeTests )
	{
		TArray<FString> EngineTestModules;
		GConfig->GetArray( TEXT("/Script/Engine.AutomationTestSettings"), TEXT("EngineTestModules"), EngineTestModules, GEngineIni);
		//Load any engine level modules.
		for( int32 EngineModuleId = 0; EngineModuleId < EngineTestModules.Num(); ++EngineModuleId)
		{
			const FName ModuleName = FName(*EngineTestModules[EngineModuleId]);
			//Make sure that there is a name available.  This can happen if a name is left blank in the Engine.ini
			if (ModuleName == NAME_None || ModuleName == TEXT("None"))
			{
				UE_LOG(LogAutomationTest, Warning, TEXT("The automation test module ('%s') doesn't have a valid name."), *ModuleName.ToString());
				continue;
			}
			if (!FModuleManager::Get().IsModuleLoaded(ModuleName))
			{
				UE_LOG(LogAutomationTest, Log, TEXT("Loading automation test module: '%s'."), *ModuleName.ToString());
				FModuleManager::Get().LoadModule(ModuleName);
			}
		}
		//Load any editor modules.
		if( bRunningEditor )
		{
			TArray<FString> EditorTestModules;
			GConfig->GetArray( TEXT("/Script/Engine.AutomationTestSettings"), TEXT("EditorTestModules"), EditorTestModules, GEngineIni);
			for( int32 EditorModuleId = 0; EditorModuleId < EditorTestModules.Num(); ++EditorModuleId )
			{
				const FName ModuleName = FName(*EditorTestModules[EditorModuleId]);
				//Make sure that there is a name available.  This can happen if a name is left blank in the Engine.ini
				if (ModuleName == NAME_None || ModuleName == TEXT("None"))
				{
					UE_LOG(LogAutomationTest, Warning, TEXT("The automation test module ('%s') doesn't have a valid name."), *ModuleName.ToString());
					continue;
				}
				if (!FModuleManager::Get().IsModuleLoaded(ModuleName))
				{
					UE_LOG(LogAutomationTest, Log, TEXT("Loading automation test module: '%s'."), *ModuleName.ToString());
					FModuleManager::Get().LoadModule(ModuleName);
				}
			}
		}
	}
}

void FAutomationTestFramework::GetValidTestNames( TArray<FAutomationTestInfo>& TestInfo ) const
{
	TestInfo.Empty();

	// Determine required application type (Editor, Game, or Commandlet)
	const bool bRunningEditor = GIsEditor && !IsRunningCommandlet();
	const bool bRunningGame = !GIsEditor || IsRunningGame();
	const bool bRunningCommandlet = IsRunningCommandlet();

	//application flags
	uint32 ApplicationSupportFlags = 0;
	if ( bRunningEditor )
	{
		ApplicationSupportFlags |= EAutomationTestFlags::EditorContext;
	}
	if ( bRunningGame )
	{
		ApplicationSupportFlags |= EAutomationTestFlags::ClientContext;
	}
	if ( bRunningCommandlet )
	{
		ApplicationSupportFlags |= EAutomationTestFlags::CommandletContext;
	}

	//Feature support - assume valid RHI until told otherwise
	uint32 FeatureSupportFlags = EAutomationTestFlags::FeatureMask;
	// @todo: Handle this correctly. GIsUsingNullRHI is defined at Engine-level, so it can't be used directly here in Core.
	// For now, assume Null RHI is only used for commandlets, servers, and when the command line specifies to use it.
	if (FPlatformProperties::SupportsWindowedMode())
	{
		bool bUsingNullRHI = FParse::Param( FCommandLine::Get(), TEXT("nullrhi") ) || IsRunningCommandlet() || IsRunningDedicatedServer();
		if (bUsingNullRHI)
		{
			FeatureSupportFlags &= (~EAutomationTestFlags::NonNullRHI);
		}
	}
	if (FApp::IsUnattended())
	{
		FeatureSupportFlags &= (~EAutomationTestFlags::RequiresUser);
	}

	for ( TMap<FString, FAutomationTestBase*>::TConstIterator TestIter( AutomationTestClassNameToInstanceMap ); TestIter; ++TestIter )
	{
		const FAutomationTestBase* CurTest = TestIter.Value();
		check( CurTest );

		uint32 CurTestFlags = CurTest->GetTestFlags();

		//filter out full tests when running smoke tests
		const bool bPassesFilterRequirement = ((CurTestFlags & RequestedTestFilter) != 0);

		//Application Tests
		uint32 CurTestApplicationFlags = (CurTestFlags & EAutomationTestFlags::ApplicationContextMask);
		const bool bPassesApplicationRequirements = (CurTestApplicationFlags == 0) || (CurTestApplicationFlags & ApplicationSupportFlags);
		
		//Feature Tests
		uint32 CurTestFeatureFlags = (CurTestFlags & EAutomationTestFlags::FeatureMask);
		const bool bPassesFeatureRequirements = (CurTestFeatureFlags == 0) || (CurTestFeatureFlags & FeatureSupportFlags);

		const bool bEnabled = (CurTestFlags & EAutomationTestFlags::Disabled) == 0;

		if (bEnabled && bPassesApplicationRequirements && bPassesFeatureRequirements && bPassesFilterRequirement)
		{
			CurTest->GenerateTestNames(TestInfo);
		}
	}
}

bool FAutomationTestFramework::ShouldTestContent(const FString& Path) const
{
	static TArray<FString> TestLevelFolders;
	if ( TestLevelFolders.Num() == 0 )
	{
		GConfig->GetArray( TEXT("/Script/Engine.AutomationTestSettings"), TEXT("TestLevelFolders"), TestLevelFolders, GEngineIni);
	}

	bool bMatchingDirectory = false;
	for ( const FString& Folder : TestLevelFolders )
	{
		const FString PatternToCheck = FString::Printf(TEXT("/%s/"), *Folder);
		if ( Path.Contains(*PatternToCheck) )
		{
			bMatchingDirectory = true;
		}
	}
	if (bMatchingDirectory)
	{
		return true;
	}

	FString DevelopersPath = FPaths::GameDevelopersDir().LeftChop(1);
	return bDeveloperDirectoryIncluded || !Path.StartsWith(DevelopersPath);
}

void FAutomationTestFramework::SetDeveloperDirectoryIncluded(const bool bInDeveloperDirectoryIncluded)
{
	bDeveloperDirectoryIncluded = bInDeveloperDirectoryIncluded;
}

void FAutomationTestFramework::SetRequestedTestFilter(const uint32 InRequestedTestFlags)
{
	RequestedTestFilter = InRequestedTestFlags;
}

FOnTestScreenshotCaptured& FAutomationTestFramework::OnScreenshotCaptured()
{
	return TestScreenshotCapturedDelegate;
}

void FAutomationTestFramework::PrepForAutomationTests()
{
	check(!GIsAutomationTesting);

	// Fire off callback signifying that unit testing is about to begin. This allows
	// other systems to prepare themselves as necessary without the unit testing framework having to know
	// about them.
	PreTestingEvent.Broadcast();

	// Cache the contents of GWarn, as unit testing is going to forcibly replace GWarn with a specialized feedback context
	// designed for unit testing
	AutomationTestFeedbackContext.TreatWarningsAsErrors = GWarn->TreatWarningsAsErrors;
	//GWarn = &AutomationTestFeedbackContext;
	GLog->AddOutputDevice(&AutomationTestFeedbackContext);

	// Mark that unit testing has begun
	GIsAutomationTesting = true;
}

void FAutomationTestFramework::ConcludeAutomationTests()
{
	check(GIsAutomationTesting);
	
	// Mark that unit testing is over
	GIsAutomationTesting = false;

	//GWarn = CachedContext;
	GLog->RemoveOutputDevice(&AutomationTestFeedbackContext);

	// Fire off callback signifying that unit testing has concluded.
	PostTestingEvent.Broadcast();
}

/**
 * Helper method to dump the contents of the provided test name to execution info map to the provided feedback context
 *
 * @param	InContext		Context to dump the execution info to
 * @param	InInfoToDump	Execution info that should be dumped to the provided feedback context
 */
void FAutomationTestFramework::DumpAutomationTestExecutionInfo( const TMap<FString, FAutomationTestExecutionInfo>& InInfoToDump )
{
	const FString SuccessMessage = NSLOCTEXT("UnrealEd", "AutomationTest_Success", "Success").ToString();
	const FString FailMessage = NSLOCTEXT("UnrealEd", "AutomationTest_Fail", "Fail").ToString();
	for ( TMap<FString, FAutomationTestExecutionInfo>::TConstIterator MapIter(InInfoToDump); MapIter; ++MapIter )
	{
		const FString& CurTestName = MapIter.Key();
		const FAutomationTestExecutionInfo& CurExecutionInfo = MapIter.Value();

		UE_LOG(LogAutomationTest, Log, TEXT("%s: %s"), *CurTestName, CurExecutionInfo.bSuccessful ? *SuccessMessage : *FailMessage);

		for ( const FAutomationEvent& Event : CurExecutionInfo.GetEvents() )
		{
			switch ( Event.Type )
			{
				case EAutomationEventType::Info:
					UE_LOG(LogAutomationTest, Display, TEXT("%s"), *Event.Message);
					break;
				case EAutomationEventType::Warning:
					UE_LOG(LogAutomationTest, Warning, TEXT("%s"), *Event.Message);
					break;
				case EAutomationEventType::Error:
					UE_LOG(LogAutomationTest, Error, TEXT("%s"), *Event.Message);
					break;
			}
		}
	}
}

void FAutomationTestFramework::InternalStartTest( const FString& InTestToRun )
{
	FString TestName;
	if (!InTestToRun.Split(TEXT(" "), &TestName, &Parameters, ESearchCase::CaseSensitive))
	{
		TestName = InTestToRun;
	}

	if ( ContainsTest( TestName ) )
	{
		CurrentTest = *( AutomationTestClassNameToInstanceMap.Find( TestName ) );
		check( CurrentTest );

		// Clear any execution info from the test in case it has been run before
		CurrentTest->ClearExecutionInfo();

		// Associate the test that is about to be run with the special unit test feedback context
		AutomationTestFeedbackContext.SetCurrentAutomationTest( CurrentTest );

		StartTime = FPlatformTime::Seconds();

		//if non-
		uint32 NonSmokeTestFlags = (EAutomationTestFlags::FilterMask & (~EAutomationTestFlags::SmokeFilter));
		if (RequestedTestFilter & NonSmokeTestFlags)
		{
			UE_LOG(LogAutomationTest, Log, TEXT("%s %s is starting at %f"), *CurrentTest->GetBeautifiedTestName(), *Parameters, StartTime);
		}

		// Run the test!
		bTestSuccessful = CurrentTest->RunTest(Parameters);
	}
}

bool FAutomationTestFramework::InternalStopTest(FAutomationTestExecutionInfo& OutExecutionInfo)
{
	check(GIsAutomationTesting);
	check(LatentCommands.IsEmpty());

	double EndTime = FPlatformTime::Seconds();
	double TimeForTest = static_cast<float>(EndTime - StartTime);
	uint32 NonSmokeTestFlags = (EAutomationTestFlags::FilterMask & (~EAutomationTestFlags::SmokeFilter));
	if (RequestedTestFilter & NonSmokeTestFlags)
	{
		UE_LOG(LogAutomationTest, Log, TEXT("%s %s ran in %f"), *CurrentTest->GetBeautifiedTestName(), *Parameters, TimeForTest);
	}

	// Disassociate the test from the feedback context
	AutomationTestFeedbackContext.SetCurrentAutomationTest( NULL );

	// Determine if the test was successful based on three criteria:
	// 1) Did the test itself report success?
	// 2) Did any errors occur and were logged by the feedback context during execution?++----
	// 3) Did we meet any errors that were expected with this test
	bTestSuccessful = bTestSuccessful && !CurrentTest->HasAnyErrors() && CurrentTest->HasMetExpectedErrors();

	// Set the success state of the test based on the above criteria
	CurrentTest->SetSuccessState( bTestSuccessful );

	// Fill out the provided execution info with the info from the test
	CurrentTest->GetExecutionInfo( OutExecutionInfo );

	// Save off timing for the test
	OutExecutionInfo.Duration = TimeForTest;

	// Re-enable log parsing if it was disabled and empty the expected errors list
	if (CurrentTest->ExpectedErrors.Num())
	{
		GLog->Logf(ELogVerbosity::Display, TEXT("<-- Resume Log Parsing -->"));
	}
	CurrentTest->ExpectedErrors.Empty();

	// Release pointers to now-invalid data
	CurrentTest = NULL;

	return bTestSuccessful;
}

void FAutomationTestFramework::AddAnalyticsItemToCurrentTest( const FString& AnalyticsItem )
{
	if( CurrentTest != nullptr )
	{
		CurrentTest->AddAnalyticsItem( AnalyticsItem );
	}
	else
	{
		UE_LOG( LogAutomationTest, Warning, TEXT( "AddAnalyticsItemToCurrentTest() called when no automation test was actively running!" ) );
	}
}

bool FAutomationTestFramework::GetTreatWarningsAsErrors() const
{
	return AutomationTestFeedbackContext.TreatWarningsAsErrors;
}

void FAutomationTestFramework::SetTreatWarningsAsErrors(TOptional<bool> bTreatWarningsAsErrors)
{
	AutomationTestFeedbackContext.TreatWarningsAsErrors = bTreatWarningsAsErrors.IsSet() ? bTreatWarningsAsErrors.GetValue() : GWarn->TreatWarningsAsErrors;
}

void FAutomationTestFramework::NotifyScreenshotComparisonComplete(bool bWasNew, bool bWasSimilar, double MaxLocalDifference, double GlobalDifference, FString ErrorMessage)
{
	OnScreenshotCompared.Broadcast(bWasNew, bWasSimilar, MaxLocalDifference, GlobalDifference, ErrorMessage);
}

void FAutomationTestFramework::NotifyTestDataRetrieved(bool bWasNew, const FString& JsonData)
{
	OnTestDataRetrieved.Broadcast(bWasNew, JsonData);
}

void FAutomationTestFramework::NotifyPerformanceDataRetrieved(bool bSuccess, const FString& ErrorMessage)
{
	OnPerformanceDataRetrieved.Broadcast(bSuccess, ErrorMessage);
}

void FAutomationTestFramework::NotifyScreenshotTakenAndCompared()
{
	OnScreenshotTakenAndCompared.Broadcast();
}

FAutomationTestFramework::FAutomationTestFramework()
	: RequestedTestFilter(EAutomationTestFlags::SmokeFilter)
	, StartTime(0.0f)
	, bTestSuccessful(false)
	, CurrentTest(nullptr)
	, bDeveloperDirectoryIncluded(false)
	, NetworkRoleIndex(0)
	, bForceSmokeTests(false)
	, bCaptureStack(true)
{
}

FAutomationTestFramework::~FAutomationTestFramework()
{
	AutomationTestClassNameToInstanceMap.Empty();
}

FString FAutomationEvent::ToString() const
{
	FString ComplexString;

	if ( !Filename.IsEmpty() && LineNumber > 0 )
	{
		ComplexString += Filename;
		ComplexString += TEXT("(");
		ComplexString += FString::FromInt(LineNumber);
		ComplexString += TEXT("): ");
	}

	if ( !Context.IsEmpty() )
	{
		ComplexString += TEXT("[");
		ComplexString += Context;
		ComplexString += TEXT("] ");
	}

	ComplexString += Message;

	return ComplexString;
}

//------------------------------------------------------------------------------

void FAutomationTestExecutionInfo::Clear()
{
	ContextStack.Reset();

	Events.Empty();
	AnalyticsItems.Empty();

	Errors = 0;
	Warnings = 0;
}

int32 FAutomationTestExecutionInfo::RemoveAllEvents(EAutomationEventType EventType)
{
	return RemoveAllEvents([EventType] (const FAutomationEvent& Event) {
		return Event.Type == EventType;
	});
}

int32 FAutomationTestExecutionInfo::RemoveAllEvents(TFunctionRef<bool(FAutomationEvent&)> FilterPredicate)
{
	int32 TotalRemoved = Events.RemoveAll([&](FAutomationEvent& Event) {
		if ( FilterPredicate(Event) )
		{
			switch ( Event.Type )
			{
			case EAutomationEventType::Warning:
				Warnings--;
				break;
			case EAutomationEventType::Error:
				Errors--;
				break;
			}

			return true;
		}
		return false;
	});

	return TotalRemoved;
}

void FAutomationTestExecutionInfo::AddEvent(const FAutomationEvent& Event)
{
	switch ( Event.Type )
	{
	case EAutomationEventType::Warning:
		Warnings++;
		break;
	case EAutomationEventType::Error:
		Errors++;
		break;
	}

	const int32 Index = Events.Add(Event);
	FAutomationEvent& NewEvent = Events[Index];

	if ( NewEvent.Context.IsEmpty() )
	{
		NewEvent.Context = GetContext();
	}
}

void FAutomationTestExecutionInfo::AddWarning(const FString& WarningMessage)
{
	AddEvent(FAutomationEvent(EAutomationEventType::Warning, WarningMessage));
}

void FAutomationTestExecutionInfo::AddError(const FString& ErrorMessage)
{
	AddEvent(FAutomationEvent(EAutomationEventType::Error, ErrorMessage));
}

//------------------------------------------------------------------------------

void FAutomationTestBase::ClearExecutionInfo()
{
	ExecutionInfo.Clear();
}

void FAutomationTestBase::AddError(const FString& InError, int32 StackOffset)
{
	if( !bSuppressLogs && !IsExpectedError(InError))
	{
		if ( FAutomationTestFramework::Get().GetCaptureStack() )
		{
			TArray<FProgramCounterSymbolInfo> Stack = FPlatformStackWalk::GetStack(StackOffset + 1, 1);
			ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, InError, ExecutionInfo.GetContext(), Stack[0].Filename, Stack[0].LineNumber));
		}
		else
		{
			ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, InError, ExecutionInfo.GetContext()));
		}
	}
}

void FAutomationTestBase::AddError(const FString& InError, const FString& InFilename, int32 InLineNumber)
{
	if ( !bSuppressLogs && !IsExpectedError(InError))
	{
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error, InError, ExecutionInfo.GetContext(), InFilename, InLineNumber));
	}
}

void FAutomationTestBase::AddWarning(const FString& InWarning, const FString& InFilename, int32 InLineNumber)
{
	if ( !bSuppressLogs && !IsExpectedError(InWarning))
	{
		ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Warning, InWarning, ExecutionInfo.GetContext(), InFilename, InLineNumber));
	}
}

void FAutomationTestBase::AddWarning( const FString& InWarning, int32 StackOffset )
{
	if ( !bSuppressLogs && !IsExpectedError(InWarning))
	{
		if ( FAutomationTestFramework::Get().GetCaptureStack() )
		{
			TArray<FProgramCounterSymbolInfo> Stack = FPlatformStackWalk::GetStack(StackOffset + 1, 1);
			ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Warning, InWarning, ExecutionInfo.GetContext(), Stack[0].Filename, Stack[0].LineNumber));
		}
		else
		{
			ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Warning, InWarning, ExecutionInfo.GetContext()));
		}
	}
}

void FAutomationTestBase::AddInfo( const FString& InLogItem, int32 StackOffset )
{
	if ( !bSuppressLogs )
	{
		if ( FAutomationTestFramework::Get().GetCaptureStack() )
		{
			TArray<FProgramCounterSymbolInfo> Stack = FPlatformStackWalk::GetStack(StackOffset + 1, 1);
			ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Info, InLogItem, ExecutionInfo.GetContext(), Stack[0].Filename, Stack[0].LineNumber));
		}
		else
		{
			ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Info, InLogItem, ExecutionInfo.GetContext()));
		}
	}
}

void FAutomationTestBase::AddAnalyticsItem(const FString& InAnalyticsItem)
{
	ExecutionInfo.AnalyticsItems.Add(InAnalyticsItem);
}

bool FAutomationTestBase::HasAnyErrors() const
{
	return ExecutionInfo.GetErrorTotal() > 0;
}

bool FAutomationTestBase::HasMetExpectedErrors()
{
	bool HasMetAllExpectedErrors = true;

	for (auto& EError : ExpectedErrors)
	{
		if ((EError.ExpectedNumberOfOccurrences > 0) && (EError.ExpectedNumberOfOccurrences != EError.ActualNumberOfOccurrences))
		{
			HasMetAllExpectedErrors = false;

			ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error,
				FString::Printf(TEXT("Expected Error or Warning matching '%s' to occur %d times with %s match type, but it was found %d time(s).")
					, *EError.ErrorPatternString
					, EError.ExpectedNumberOfOccurrences
					, EAutomationExpectedErrorFlags::ToString(EError.CompareType)
					, EError.ActualNumberOfOccurrences)
				, ExecutionInfo.GetContext()));			
		}
		else if (EError.ExpectedNumberOfOccurrences == 0)
		{
			if (EError.ActualNumberOfOccurrences == 0)
			{
				HasMetAllExpectedErrors = false;

				ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Error,
					FString::Printf(TEXT("Expected suppressed Error or Warning matching '%s' did not occur."), *EError.ErrorPatternString),
					ExecutionInfo.GetContext()));
			}
			else
			{
				ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Info,
					FString::Printf(TEXT("Suppressed expected Error or Warning matching '%s' %d times.")
						, *EError.ErrorPatternString
						, EError.ActualNumberOfOccurrences)
					, ExecutionInfo.GetContext()));
			}
		}
	}

	return HasMetAllExpectedErrors;
}



void FAutomationTestBase::SetSuccessState( bool bSuccessful )
{
	ExecutionInfo.bSuccessful = bSuccessful;
}

void FAutomationTestBase::GetExecutionInfo( FAutomationTestExecutionInfo& OutInfo ) const
{
	OutInfo = ExecutionInfo;
}

void FAutomationTestBase::AddExpectedError(FString ExpectedErrorPattern, EAutomationExpectedErrorFlags::MatchType InCompareType, int32 Occurrences)
{
	if (Occurrences >= 0)
	{
		// If we already have an error matching string in our list, let's not add it again.
		FAutomationExpectedError* FoundEntry = ExpectedErrors.FindByPredicate(
			[ExpectedErrorPattern](const FAutomationExpectedError& InItem) 
				{
					return InItem.ErrorPatternString == ExpectedErrorPattern; 
				}
		);

		if (FoundEntry)
		{
			UE_LOG(LogAutomationTest, Warning, TEXT("Adding expected error matching '%s' failed: cannot add duplicate entries"), *ExpectedErrorPattern)
		}
		else
		{
			// Disable log pre-processor the first time we successfully add an expected error
			// so that successful tests don't trigger CIS failures
			if (!ExpectedErrors.Num())
			{
				GLog->Logf(ELogVerbosity::Display, TEXT("<-- Suspend Log Parsing -->"));
			}
			// ToDo: After UE-44340 is resolved, create FAutomationExpectedError and check that its ErrorPattern is valid before adding
			ExpectedErrors.Add(FAutomationExpectedError(ExpectedErrorPattern, InCompareType, Occurrences));
		}
	}
	else
	{
		UE_LOG(LogAutomationTest, Error, TEXT("Adding expected error matching '%s' failed: number of expected occurrences must be >= 0"), *ExpectedErrorPattern);
	}
}

void FAutomationTestBase::GetExpectedErrors(TArray<FAutomationExpectedError>& OutInfo) const
{
	OutInfo = ExpectedErrors;
}

void FAutomationTestBase::GenerateTestNames(TArray<FAutomationTestInfo>& TestInfo) const
{
	TArray<FString> BeautifiedNames;
	TArray<FString> ParameterNames;
	GetTests(BeautifiedNames, ParameterNames);

	FString BeautifiedTestName = GetBeautifiedTestName();

	for (int32 ParameterIndex = 0; ParameterIndex < ParameterNames.Num(); ++ParameterIndex)
	{
		FString CompleteBeautifiedNames = BeautifiedTestName;
		FString CompleteTestName = TestName;

		if (ParameterNames[ParameterIndex].Len())
		{
			CompleteBeautifiedNames = FString::Printf(TEXT("%s.%s"), *BeautifiedTestName, *BeautifiedNames[ParameterIndex]);;
			CompleteTestName = FString::Printf(TEXT("%s %s"), *TestName, *ParameterNames[ParameterIndex]);
		}

		// Add the test info to our collection
		FAutomationTestInfo NewTestInfo(
			CompleteBeautifiedNames,
			CompleteBeautifiedNames,
			CompleteTestName,
			GetTestFlags(),
			GetRequiredDeviceNum(),
			ParameterNames[ParameterIndex],
			GetTestSourceFileName(CompleteTestName),
			GetTestSourceFileLine(CompleteTestName),
			GetTestAssetPath(ParameterNames[ParameterIndex]),
			GetTestOpenCommand(ParameterNames[ParameterIndex])
		);
		
		TestInfo.Add( NewTestInfo );
	}
}

// --------------------------------------------------------------------------------------

void FAutomationTestBase::TestEqual(const FString& What, const int32 Actual, const int32 Expected)
{
	if ( Actual != Expected )
	{
		AddError(FString::Printf(TEXT("Expected '%s' to be %d, but it was %d."), *What, Expected, Actual), 1);
	}
	else
	{
		AddInfo(FString::Printf(TEXT("Expected '%s' to be %d."), *What, Expected), 1);
	}
}

void FAutomationTestBase::TestEqual(const FString& What, const float Actual, const float Expected, float Tolerance)
{
	if ( !FMath::IsNearlyEqual(Actual, Expected, Tolerance) )
	{
		AddError(FString::Printf(TEXT("Expected '%s' to be %f, but it was %f within tolerance %f."), *What, Expected, Actual, Tolerance), 1);
	}
	else
	{
		AddInfo(FString::Printf(TEXT("Expected '%s' to be %f within tolerance %f."), *What, Expected, Tolerance), 1);
	}
}

void FAutomationTestBase::TestEqual(const FString& What, const FVector Actual, const FVector Expected, float Tolerance)
{
	if ( !Expected.Equals(Actual, Tolerance) )
	{
		AddError(FString::Printf(TEXT("Expected '%s' to be %s, but it was %s within tolerance %f."), *What, *Expected.ToString(), *Actual.ToString(), Tolerance), 1);
	}
	else
	{
		AddInfo(FString::Printf(TEXT("Expected '%s' to be %s within tolerance %f."), *What, *Expected.ToString(), Tolerance), 1);
	}
}

void FAutomationTestBase::TestEqual(const FString& What, const FColor Actual, const FColor Expected)
{
	if ( Expected != Actual )
	{
		AddError(FString::Printf(TEXT("Expected '%s' to be %s, but it was %s."), *What, *Expected.ToString(), *Actual.ToString()), 1);
	}
	else
	{
		AddInfo(FString::Printf(TEXT("Expected '%s' to be %s."), *What, *Expected.ToString()), 1);
	}
}

void FAutomationTestBase::TestFalse(const FString& What, bool Value)
{
	if ( Value )
	{
		AddError(FString::Printf(TEXT("Expected '%s' to be false."), *What), 1);
	}
	else
	{
		AddInfo(FString::Printf(TEXT("Expected '%s' to be false."), *What), 1);
	}
}

void FAutomationTestBase::TestTrue(const FString& What, bool Value)
{
	if ( !Value )
	{
		AddError(FString::Printf(TEXT("Expected '%s' to be true."), *What), 1);
	}
	else
	{
		AddInfo(FString::Printf(TEXT("Expected '%s' to be true."), *What), 1);
	}
}

void FAutomationTestBase::TestNull(const FString& What, const void* Pointer)
{
	if ( Pointer != nullptr )
	{
		AddError(FString::Printf(TEXT("Expected '%s' to be null."), *What), 1);
	}
	else
	{
		AddInfo(FString::Printf(TEXT("Expected '%s' to be null."), *What), 1);
	}
}

bool FAutomationTestBase::IsExpectedError(const FString& Error)
{
	for (auto& EError : ExpectedErrors)
	{
		FRegexMatcher ErrorMatcher(EError.ErrorPattern, Error);

		if (ErrorMatcher.FindNext())
		{
			EError.ActualNumberOfOccurrences++;
			return true;
		}
	}

	return false;
}