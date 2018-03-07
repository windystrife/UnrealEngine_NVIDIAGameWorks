// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UnitTests/PacketLimitTest_Oodle.h"


// @todo #JohnB: Restore in a game-level package, eventually
#if 0
/**
 * UPacketLimitTest_Oodle
 */
UPacketLimitTest_Oodle::UPacketLimitTest_Oodle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UnitTestName = TEXT("PacketLimitTest_Oodle");

	UnitTestDate = FDateTime(2016, 1, 10);

	bWorkInProgress = true;

	// @todo #JohnBExploitCL: Bugtracking/changelist notes

	ExpectedResult.Add(TEXT("ShooterGame"), EUnitTestVerification::VerifiedNotFixed);


	bUseOodle = true;
}
#endif
