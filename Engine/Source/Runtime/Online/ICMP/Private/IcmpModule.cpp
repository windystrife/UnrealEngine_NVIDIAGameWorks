// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IcmpModule.h"

IMPLEMENT_MODULE(FIcmpModule, Icmp);

DEFINE_LOG_CATEGORY(LogIcmp);

void FIcmpModule::StartupModule()
{
}

void FIcmpModule::ShutdownModule()
{
}

bool FIcmpModule::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	// Ignore any execs that don't start with Icmp
	if (FParse::Command(&Cmd, TEXT("Icmp")))
	{
		return false;
	}

	return false;
}


