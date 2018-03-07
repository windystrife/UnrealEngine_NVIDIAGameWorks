// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UnitTests/FTextCrash.h"


#include "MinimalClient.h"
#include "NUTActor.h"

#include "UnitTestEnvironment.h"
#include "Net/NUTUtilNet.h"


/**
 * UFTextCrash
 */

UFTextCrash::UFTextCrash(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UnitTestName = TEXT("FTextCrash");
	UnitTestType = TEXT("Exploit");

	UnitTestDate = FDateTime(2014, 07, 11);

	UnitTestBugTrackIDs.Add(TEXT("JIRA UE-5691"));

	UnitTestCLs.Add(TEXT("2367048 (//depot/UE4/)"));


	ExpectedResult.Add(TEXT("ShooterGame"),			EUnitTestVerification::VerifiedFixed);
	ExpectedResult.Add(TEXT("QAGame"),				EUnitTestVerification::VerifiedFixed);
	ExpectedResult.Add(TEXT("UnrealTournament"),	EUnitTestVerification::VerifiedFixed);
	ExpectedResult.Add(TEXT("FortniteGame"),		EUnitTestVerification::VerifiedFixed);
	ExpectedResult.Add(TEXT("OrionGame"),			EUnitTestVerification::VerifiedFixed);

	UnitTestTimeout = 60;


	SetFlags<EUnitTestFlags::LaunchServer | EUnitTestFlags::AcceptPlayerController | EUnitTestFlags::RequireNUTActor |
				EUnitTestFlags::ExpectServerCrash | EUnitTestFlags::ExpectDisconnect,
				EMinClientFlags::AcceptActors | EMinClientFlags::SendRPCs | EMinClientFlags::NotifyNetActors>();
}

void UFTextCrash::InitializeEnvironmentSettings()
{
	BaseServerURL = UnitEnv->GetDefaultMap(UnitTestFlags);
	BaseServerParameters = UnitEnv->GetDefaultServerParameters();
}

void UFTextCrash::ExecuteClientUnitTest()
{
	// Wait until we receive the NUTActor from the server
	if (UnitNUTActor.IsValid())
	{
		// Create a blank FText, then send it to the server - should trigger an assert on the server
		FText BlankText;

		UnitNUTActor->ServerReceiveText(BlankText);


		// If the exploit was a failure, the next log message will IMMEDIATELY be the 'ExploitFailLog' message,
		// as that message is triggered within the same code chain as the RPC above
		// (and should be blocked, if the above succeeds).
		SendGenericExploitFailLog();
	}
}


void UFTextCrash::NotifyProcessLog(TWeakPtr<FUnitTestProcess> InProcess, const TArray<FString>& InLogLines)
{
	Super::NotifyProcessLog(InProcess, InLogLines);

	if (InProcess.HasSameObject(ServerHandle.Pin().Get()))
	{
		const TCHAR* AssertLog = TEXT("Unhandled Exception: EXCEPTION_ACCESS_VIOLATION reading address");

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

