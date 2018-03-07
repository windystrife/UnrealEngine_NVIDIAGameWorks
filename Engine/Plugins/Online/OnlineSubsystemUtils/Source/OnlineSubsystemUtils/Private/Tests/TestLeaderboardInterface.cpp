// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Tests/TestLeaderboardInterface.h"
#include "OnlineSubsystemUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 *	Example of a leaderboard write object
 */
class TestLeaderboardWrite : public FOnlineLeaderboardWrite
{
public:
	TestLeaderboardWrite()
	{
		// Default properties
		new (LeaderboardNames) FName(TEXT("TestLeaderboard"));
		RatedStat = "TestIntStat1";
		DisplayFormat = ELeaderboardFormat::Number;
		SortMethod = ELeaderboardSort::Descending;
		UpdateMethod = ELeaderboardUpdateMethod::KeepBest;
	}
};

/**
 *	Example of a leaderboard read object
 */
class TestLeaderboardRead : public FOnlineLeaderboardRead
{
public:
	TestLeaderboardRead()
	{
		// Default properties
		LeaderboardName = FName(TEXT("TestLeaderboard"));
		SortedColumn = "TestIntStat1";

		// Define default columns
		new (ColumnMetadata) FColumnMetaData("TestIntStat1", EOnlineKeyValuePairDataType::Int32);
		new (ColumnMetadata) FColumnMetaData("TestFloatStat1", EOnlineKeyValuePairDataType::Float);
	}
};

void FTestLeaderboardInterface::Test(UWorld* InWorld)
{
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(InWorld, FName(*Subsystem));
	check(OnlineSub); 

	if (OnlineSub->GetIdentityInterface().IsValid())
	{
		UserId = OnlineSub->GetIdentityInterface()->GetUniquePlayerId(0);
	}

	// Cache interfaces
	Leaderboards = OnlineSub->GetLeaderboardsInterface();
	check(Leaderboards.IsValid());

	// Define delegates
	LeaderboardFlushDelegate = FOnLeaderboardFlushCompleteDelegate::CreateRaw(this, &FTestLeaderboardInterface::OnLeaderboardFlushComplete);
	LeaderboardReadCompleteDelegate = FOnLeaderboardReadCompleteDelegate::CreateRaw(this, &FTestLeaderboardInterface::OnLeaderboardReadComplete);
}

void FTestLeaderboardInterface::WriteLeaderboards()
{
	TestLeaderboardWrite WriteObject;
	
	// Set some data
	WriteObject.SetIntStat("TestIntStat1", 50);
	WriteObject.SetFloatStat("TestFloatStat1", 99.5f);

	// Write it to the buffers
	Leaderboards->WriteLeaderboards(TEXT("TEST"), *UserId, WriteObject);
	TestPhase++;
}

void FTestLeaderboardInterface::OnLeaderboardFlushComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogOnline, Verbose, TEXT("OnLeaderboardFlushComplete Session: %s bWasSuccessful: %d"), *SessionName.ToString(), bWasSuccessful);
	bOverallSuccess = bOverallSuccess && bWasSuccessful;

	Leaderboards->ClearOnLeaderboardFlushCompleteDelegate_Handle(LeaderboardFlushDelegateHandle);
	TestPhase++;
}

void FTestLeaderboardInterface::FlushLeaderboards()
{
	LeaderboardFlushDelegateHandle = Leaderboards->AddOnLeaderboardFlushCompleteDelegate_Handle(LeaderboardFlushDelegate);
	Leaderboards->FlushLeaderboards(TEXT("TEST"));
}

void FTestLeaderboardInterface::OnLeaderboardReadComplete(bool bWasSuccessful)
{
	UE_LOG(LogOnline, Verbose, TEXT("OnLeaderboardReadComplete bWasSuccessful: %d"), bWasSuccessful);
	bOverallSuccess = bOverallSuccess && bWasSuccessful;

	for (int32 RowIdx = 0; RowIdx < ReadObject->Rows.Num(); ++RowIdx)
	{
		const FOnlineStatsRow& StatsRow = ReadObject->Rows[RowIdx];
		UE_LOG(LogOnline, Log, TEXT("Leaderboard stats for: Nickname = %s, Rank = %d"), *StatsRow.NickName, StatsRow.Rank);

		for (FStatsColumnArray::TConstIterator It(StatsRow.Columns); It; ++It)
		{
			UE_LOG(LogOnline, Log, TEXT("  %s = %s"), *It.Key().ToString(), *It.Value().ToString());
		}
	}

	Leaderboards->ClearOnLeaderboardReadCompleteDelegate_Handle(LeaderboardReadCompleteDelegateHandle);
	TestPhase++;
}

void FTestLeaderboardInterface::ReadLeaderboards()
{
	ReadObject = MakeShareable(new TestLeaderboardRead());
	FOnlineLeaderboardReadRef ReadObjectRef = ReadObject.ToSharedRef();

	LeaderboardReadCompleteDelegateHandle = Leaderboards->AddOnLeaderboardReadCompleteDelegate_Handle(LeaderboardReadCompleteDelegate);
	Leaderboards->ReadLeaderboardsForFriends(0, ReadObjectRef);
}

bool FTestLeaderboardInterface::Tick( float DeltaTime )
{
	if (TestPhase != LastTestPhase)
	{
		LastTestPhase = TestPhase;
		if (!bOverallSuccess)
		{
			UE_LOG(LogOnline, Log, TEXT("Testing failed in phase %d"), LastTestPhase);
			TestPhase = 3;
		}

		switch(TestPhase)
		{
		case 0:
			WriteLeaderboards();
			break;
		case 1:
			FlushLeaderboards();
			break;
		case 2:
			ReadLeaderboards();
			break;
		case 3:
			UE_LOG(LogOnline, Log, TEXT("TESTING COMPLETE Success:%s!"), bOverallSuccess ? TEXT("true") : TEXT("false"));
			delete this;
			return false;
		}
	}
	return true;
}

#endif //WITH_DEV_AUTOMATION_TESTS
