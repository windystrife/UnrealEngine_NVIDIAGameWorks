// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/App.h"
#include "UnitTestEnvironment.h"

enum class EUnitTestFlags : uint32;

/**
 * Implements unit test environment settings, for ShooterGame
 */
class FShooterGameEnvironment : public FUnitTestEnvironment
{
public:
	FShooterGameEnvironment()
	{
	}

	static void Register()
	{
		AddUnitTestEnvironment(TEXT("ShooterGame"), new FShooterGameEnvironment());
	}


	virtual FString GetDefaultMap(EUnitTestFlags UnitTestFlags) override
	{
		FString ReturnVal = FUnitTestEnvironment::GetDefaultMap(UnitTestFlags);
		FString CurrentGame = FApp::GetProjectName();

		if (CurrentGame == TEXT("ShooterGame"))
		{
			ReturnVal = TEXT("Sanctuary");
		}

		return ReturnVal;
	}
};

/**
 * Implements unit test environment settings, for QAGame
 */
class FQAGameEnvironment : public FUnitTestEnvironment
{
public:
	FQAGameEnvironment()
	{
	}

	static void Register()
	{
		AddUnitTestEnvironment(TEXT("QAGame"), new FQAGameEnvironment());
	}


	virtual FString GetDefaultMap(EUnitTestFlags UnitTestFlags) override
	{
		FString ReturnVal = FUnitTestEnvironment::GetDefaultMap(UnitTestFlags);
		FString CurrentGame = FApp::GetProjectName();

		if (CurrentGame == TEXT("QAGame"))
		{
			ReturnVal = TEXT("QAEntry");
		}

		return ReturnVal;
	}
};

// @todo JohnB: Once UT-specific exploits are added, move this to its own module, like NUTFortnite

/**
 * Implements unit test environment settings, for UnrealTournament
 */
class FUTEnvironment : public FUnitTestEnvironment
{
public:
	FUTEnvironment()
	{
	}

	static void Register()
	{
		AddUnitTestEnvironment(TEXT("UnrealTournament"), new FUTEnvironment());
	}


	virtual FString GetDefaultMap(EUnitTestFlags UnitTestFlags) override
	{
		FString ReturnVal = FUnitTestEnvironment::GetDefaultMap(UnitTestFlags);
		FString CurrentGame = FApp::GetProjectName();

		if (CurrentGame == TEXT("UnrealTournament"))
		{
			ReturnVal = TEXT("DM-DeckTest");
		}

		return ReturnVal;
	}

	virtual FString GetDefaultClientConnectURL() override
	{
		FString ReturnVal = FUnitTestEnvironment::GetDefaultClientConnectURL();
		FString CurrentGame = FApp::GetProjectName();

		if (CurrentGame == TEXT("UnrealTournament"))
		{
			ReturnVal += TEXT("?VersionCheck=1");
		}

		return ReturnVal;
	}
};

