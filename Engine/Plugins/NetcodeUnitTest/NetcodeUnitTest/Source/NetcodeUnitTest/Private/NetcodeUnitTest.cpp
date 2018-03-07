// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NetcodeUnitTest.h"
#include "Modules/ModuleManager.h"
#include "Engine/World.h"

#include "UI/LogWidgetCommands.h"

#include "INetcodeUnitTest.h"
#include "NUTUtil.h"
#include "NUTUtilDebug.h"
#include "UnitTestEnvironment.h"


/**
 * Globals
 */

UNetConnection* GActiveReceiveUnitConnection = nullptr;
bool GIsInitializingActorChan = false;

ELogType GActiveLogTypeFlags = ELogType::None;


/**
 * Definitions/implementations
 */

DEFINE_LOG_CATEGORY(LogUnitTest);
DEFINE_LOG_CATEGORY(NetCodeTestNone);


/**
 * Module implementation
 */
class FNetcodeUnitTest : public INetcodeUnitTest
{
private:
	static FWorldDelegates::FWorldInitializationEvent::FDelegate OnWorldCreatedDelegate;

#if TARGET_UE4_CL >= CL_DEPRECATEDEL
	static FDelegateHandle OnWorldCreatedDelegateHandle;
#endif

public:
	/**
	 * Called upon loading of the NetcodeUnitTest library
	 */
	virtual void StartupModule() override
	{
		static bool bSetDelegate = false;

		if (!bSetDelegate)
		{
			OnWorldCreatedDelegate = FWorldDelegates::FWorldInitializationEvent::FDelegate::CreateStatic(
										&FNetcodeUnitTest::OnWorldCreated);

#if TARGET_UE4_CL >= CL_DEPRECATEDEL
			OnWorldCreatedDelegateHandle = FWorldDelegates::OnPreWorldInitialization.Add(OnWorldCreatedDelegate);
#else
			FWorldDelegates::OnPreWorldInitialization.Add(OnWorldCreatedDelegate);
#endif

			bSetDelegate = true;
		}

		FLogWidgetCommands::Register();
		FUnitTestEnvironment::Register();

		// Hack-override the log category name
#if !NO_LOGGING
		class FLogOverride : public FLogCategoryBase
		{
		public:
			// Used to access protected CategoryFName
			void OverrideName(FName InName)
			{
				CategoryFName = InName;
			}
		};

		((FLogOverride&)NetCodeTestNone).OverrideName(TEXT("None"));
#endif
	}

	/**
	 * Called immediately prior to unloading of the NetcodeUnitTest library
	 */
	virtual void ShutdownModule() override
	{
		// Eliminate active global variables
		GUnitTestManager = nullptr;

		if (GTraceManager != nullptr)
		{
			delete GTraceManager;

			GTraceManager = nullptr;
		}

		if (GLogTraceManager != nullptr)
		{
			delete GLogTraceManager;

			GLogTraceManager = nullptr;
		}
		

		FLogWidgetCommands::Unregister();
		FUnitTestEnvironment::Unregister();
	}


	/**
	 * Delegate implementation, for adding NUTActor to ServerActors, early on in engine startup
	 */
	static void OnWorldCreated(UWorld* UnrealWorld, const UWorld::InitializationValues IVS)
	{
		// If NUTActor isn't already in RuntimeServerActors, add it now
		if (GEngine != nullptr)
		{
			bool bNUTActorPresent = GEngine->RuntimeServerActors.ContainsByPredicate(
				[](const FString& CurServerActor)
				{
					return CurServerActor == TEXT("/Script/NetcodeUnitTest.NUTActor");
				});

			if (!bNUTActorPresent)
			{
				UE_LOG(LogUnitTest, Log, TEXT("NUTActor not present in RuntimeServerActors - adding this"));

				GEngine->RuntimeServerActors.Add(TEXT("/Script/NetcodeUnitTest.NUTActor"));
			}
		}

		// Now remove it, so it's only called once
#if TARGET_UE4_CL >= CL_DEPRECATEDEL
		FWorldDelegates::OnPreWorldInitialization.Remove(OnWorldCreatedDelegateHandle);
#else
		FWorldDelegates::OnPreWorldInitialization.Remove(OnWorldCreatedDelegate);
#endif
	}
};

FWorldDelegates::FWorldInitializationEvent::FDelegate FNetcodeUnitTest::OnWorldCreatedDelegate = nullptr;

#if TARGET_UE4_CL >= CL_DEPRECATEDEL
FDelegateHandle FNetcodeUnitTest::OnWorldCreatedDelegateHandle;
#endif


// Essential for getting the .dll to compile, and for the package to be loadable
IMPLEMENT_MODULE(FNetcodeUnitTest, NetcodeUnitTest);
