// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "Engine/Engine.h"
#include "AssetData.h"
#include "FunctionalTestingModule.h"
#include "EngineGlobals.h"
#include "Tests/AutomationCommon.h"
#include "FunctionalTestingHelper.h"

#if WITH_DEV_AUTOMATION_TESTS

#define LOCTEXT_NAMESPACE "FunctionalTesting"

DEFINE_LOG_CATEGORY_STATIC(LogFunctionalTesting, Log, All);

class FClientFunctionalTestingMapsBase : public FAutomationTestBase
{
public:
	FClientFunctionalTestingMapsBase(const FString& InName, const bool bInComplexTask)
		: FAutomationTestBase(InName, bInComplexTask)
	{
	}

	// Project.Maps.Client Functional Testing
	// Project.Maps.Functional Tests

	static void ParseTestMapInfo(const FString& Parameters, FString& MapObjectPath, FString& MapPackageName, FString& MapTestName)
	{
		TArray<FString> ParamArray;
		Parameters.ParseIntoArray(ParamArray, TEXT(";"), true);

		MapObjectPath = ParamArray[0];
		MapPackageName = ParamArray[1];
		MapTestName = (ParamArray.Num() > 2) ? ParamArray[2] : TEXT("");
	}

	// @todo this is a temporary solution. Once we know how to get test's hands on a proper world
	// this function should be redone/removed
	static UWorld* GetAnyGameWorld()
	{
		UWorld* TestWorld = nullptr;
		const TIndirectArray<FWorldContext>& WorldContexts = GEngine->GetWorldContexts();
		for (const FWorldContext& Context : WorldContexts)
		{
			if (((Context.WorldType == EWorldType::PIE) || (Context.WorldType == EWorldType::Game)) && (Context.World() != NULL))
			{
				TestWorld = Context.World();
				break;
			}
		}

		return TestWorld;
	}

	virtual FString GetTestOpenCommand(const FString& Parameters) const override
	{
		FString MapObjectPath, MapPackageName, MapTestName;
		ParseTestMapInfo(Parameters, MapObjectPath, MapPackageName, MapTestName);

		return FString::Printf(TEXT("Automate.OpenMapAndFocusActor %s %s"), *MapObjectPath, *MapTestName);
	}

	virtual FString GetTestAssetPath(const FString& Parameters) const override
	{
		FString MapObjectPath, MapPackageName, MapTestName;
		ParseTestMapInfo(Parameters, MapObjectPath, MapPackageName, MapTestName);

		return MapObjectPath;
	}

	/** 
	 * Requests a enumeration of all maps to be loaded
	 */
	virtual void GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const override
	{
		bool bEditorOnlyTests = !(GetTestFlags() & EAutomationTestFlags::ClientContext);
		TArray<FString> MapAssets;
		IFunctionalTestingModule::Get().GetMapTests(bEditorOnlyTests, OutBeautifiedNames, OutTestCommands, MapAssets);
	}

	/**
	 * Execute the loading of each map and performance captures
	 *
	 * @param Parameters - Should specify which map name to load
	 * @return	TRUE if the test was successful, FALSE otherwise
	 */
	virtual bool RunTest(const FString& Parameters) override
	{
		FString MapObjectPath, MapPackageName, MapTestName;
		ParseTestMapInfo(Parameters, MapObjectPath, MapPackageName, MapTestName);

		bool bCanProceed = false;

		IFunctionalTestingModule::Get().MarkPendingActivation();

		UWorld* TestWorld = GetAnyGameWorld();
		if (TestWorld && TestWorld->GetMapName() == MapPackageName)
		{
			// Map is already loaded.
			bCanProceed = true;
		}
		else
		{
			bCanProceed = AutomationOpenMap(MapPackageName);
		}

		if (bCanProceed)
		{
			if (MapTestName.IsEmpty())
			{
				ADD_LATENT_AUTOMATION_COMMAND(FStartFTestsOnMap());
			}
			else
			{
				ADD_LATENT_AUTOMATION_COMMAND(FStartFTestOnMap(MapTestName));
			}

			return true;
		}

		/// FAutomationTestFramework::GetInstance().UnregisterAutomationTest

		//	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
		//  ADD_LATENT_AUTOMATION_COMMAND(FExitGameCommand);

		UE_LOG(LogFunctionalTest, Error, TEXT("Failed to start the %s map (possibly due to BP compilation issues)"), *MapPackageName);
		return false;
	}
};

// Runtime tests
IMPLEMENT_CUSTOM_COMPLEX_AUTOMATION_TEST(FClientFunctionalTestingMapsRuntime, FClientFunctionalTestingMapsBase, "Project.Functional Tests", (EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter))

void FClientFunctionalTestingMapsRuntime::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	FClientFunctionalTestingMapsBase::GetTests(OutBeautifiedNames, OutTestCommands);
}

bool FClientFunctionalTestingMapsRuntime::RunTest(const FString& Parameters)
{
	return FClientFunctionalTestingMapsBase::RunTest(Parameters);
}

// Editor only tests
IMPLEMENT_CUSTOM_COMPLEX_AUTOMATION_TEST(FClientFunctionalTestingMapsEditor, FClientFunctionalTestingMapsBase, "Project.Functional Tests", (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter))

void FClientFunctionalTestingMapsEditor::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	FClientFunctionalTestingMapsBase::GetTests(OutBeautifiedNames, OutTestCommands);
}

bool FClientFunctionalTestingMapsEditor::RunTest(const FString& Parameters)
{
	return FClientFunctionalTestingMapsBase::RunTest(Parameters);
}

#undef LOCTEXT_NAMESPACE

#endif //WITH_DEV_AUTOMATION_TESTS
