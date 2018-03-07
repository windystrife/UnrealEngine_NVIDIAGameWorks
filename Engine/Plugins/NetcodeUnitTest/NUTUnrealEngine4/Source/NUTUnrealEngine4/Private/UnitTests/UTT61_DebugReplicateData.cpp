// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UnitTests/UTT61_DebugReplicateData.h"
#include "UObject/Package.h"


#include "NUTActor.h"
#include "MinimalClient.h"
#include "Net/NUTUtilNet.h"
#include "UnitTestEnvironment.h"


UClass* UUTT61_DebugReplicateData::RepClass = FindObject<UClass>(ANY_PACKAGE, TEXT("GameplayDebuggingReplicator"));


/**
 * UUTT61_DebugReplicateData
 */

UUTT61_DebugReplicateData::UUTT61_DebugReplicateData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Replicator(nullptr)
{
	UnitTestName = TEXT("ReplicateDataCheck");
	UnitTestType = TEXT("DevExploit");

	// Date reflects ReVuln doc, not the date I coded this up
	UnitTestDate = FDateTime(2014, 06, 20);


	UnitTestBugTrackIDs.Add(TEXT("TTP #335193"));
	UnitTestBugTrackIDs.Add(TEXT("TTP #335195"));

	UnitTestBugTrackIDs.Add(TEXT("JIRA UE-4225"));
	UnitTestBugTrackIDs.Add(TEXT("JIRA UE-4209"));


	ExpectedResult.Add(TEXT("ShooterGame"),			EUnitTestVerification::VerifiedFixed);
	ExpectedResult.Add(TEXT("QAGame"),				EUnitTestVerification::VerifiedFixed);
	ExpectedResult.Add(TEXT("UnrealTournament"),	EUnitTestVerification::VerifiedFixed);
	ExpectedResult.Add(TEXT("FortniteGame"),		EUnitTestVerification::VerifiedFixed);

	UnitTestTimeout = 60;


	SetFlags<EUnitTestFlags::LaunchServer | EUnitTestFlags::AcceptPlayerController | EUnitTestFlags::RequirePlayerController |
				EUnitTestFlags::ExpectServerCrash | EUnitTestFlags::ExpectDisconnect,
				EMinClientFlags::AcceptActors | EMinClientFlags::NotifyNetActors | EMinClientFlags::SendRPCs>();
}

void UUTT61_DebugReplicateData::InitializeEnvironmentSettings()
{
	BaseServerURL = UnitEnv->GetDefaultMap(UnitTestFlags);
	BaseServerParameters = UnitEnv->GetDefaultServerParameters();
}

void UUTT61_DebugReplicateData::NotifyAllowNetActor(UClass* ActorClass, bool bActorChannel, bool& bBlockActor)
{
	Super::NotifyAllowNetActor(ActorClass, bActorChannel, bBlockActor);

	if (bBlockActor)
	{
		if (RepClass != nullptr)
		{
			if (ActorClass->IsChildOf(RepClass))
			{
				bBlockActor = false;
			}
		}
		else
		{
			UNIT_LOG(ELogType::StatusFailure | ELogType::StatusWarning | ELogType::StyleBold,
						TEXT("WARNING: Unit test broken. Could not find class 'GameplayDebuggingReplicator'."));

			VerificationState = EUnitTestVerification::VerifiedNeedsUpdate;
		}
	}
}

void UUTT61_DebugReplicateData::NotifyNetActor(UActorChannel* ActorChannel, AActor* Actor)
{
	Super::NotifyNetActor(ActorChannel, Actor);

	if (Replicator == nullptr)
	{
		if (RepClass != nullptr)
		{
			if (Actor->IsA(RepClass))
			{
				Replicator = Actor;

				if (Replicator != nullptr)
				{
					// Once the replicator is found, pass back to the main exploit function
					ExecuteClientUnitTest();
				}
			}
		}
		else
		{
			UNIT_LOG(ELogType::StatusFailure | ELogType::StatusWarning | ELogType::StyleBold,
						TEXT("WARNING: Unit test broken. Could not find class 'GameplayDebuggingReplicator'."));

			VerificationState = EUnitTestVerification::VerifiedNeedsUpdate;
		}
	}
}

void UUTT61_DebugReplicateData::ExecuteClientUnitTest()
{
	// Send a command to spawn the GameplayDebuggingReplicator on the server
	if (!Replicator.IsValid())
	{
		FString LogMsg = TEXT("Sending GameplayDebuggingReplicator summon command");

		ResetTimeout(LogMsg);
		UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);


		FString Cmd = TEXT("GameplayDebugger.GameplayDebuggingReplicator -ForceBeginPlay -GameplayDebuggerHack");
		bool bSuccess = SendNUTControl(ENUTControlCommand::Summon, Cmd);

		if (!bSuccess)
		{
			UNIT_LOG(ELogType::StatusFailure, TEXT("Failed to send summon command - marking unit test as needing update."));

			VerificationState = EUnitTestVerification::VerifiedNeedsUpdate;
		}
	}
	// Now execute the exploit on the replicator
	else
	{
		FString LogMsg = TEXT("Found replicator - executing exploit");

		ResetTimeout(LogMsg);
		UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);

		struct ServerReplicateMessageParms
		{
			AActor* Actor;
			uint32 InMessage;
			uint32 DataView;
		};

		ServerReplicateMessageParms Parms;
		Parms.Actor = nullptr;
		Parms.InMessage = 4; //EDebugComponentMessage::ActivateDataView;
		Parms.DataView = -1;

		static const FName ServerRepMessageName = FName(TEXT("ServerReplicateMessage"));

		Replicator->ProcessEvent(Replicator->FindFunctionChecked(ServerRepMessageName), &Parms);


		// If the exploit was a failure, the next log message will IMMEDIATELY be the 'ExploitFailLog' message,
		// as that message is triggered within the same code chain as the RPC above
		// (and should be blocked, if the above succeeds).
		SendGenericExploitFailLog();
	}
}

void UUTT61_DebugReplicateData::NotifyProcessLog(TWeakPtr<FUnitTestProcess> InProcess, const TArray<FString>& InLogLines)
{
	Super::NotifyProcessLog(InProcess, InLogLines);

	if (InProcess.HasSameObject(ServerHandle.Pin().Get()))
	{
		const TCHAR* AssertLog = TEXT("appError called: Assertion failed: (Index >= 0) & (Index < ArrayNum)");

		for (auto CurLine : InLogLines)
		{
			if (CurLine.Contains(AssertLog))
			{
				VerificationState = EUnitTestVerification::VerifiedNotFixed;
				break;
			}
			else if (CurLine.Contains(GetGenericExploitFailLog()))
			{
				VerificationState = EUnitTestVerification::VerifiedFixed;
				break;
			}
		}
	}
}


