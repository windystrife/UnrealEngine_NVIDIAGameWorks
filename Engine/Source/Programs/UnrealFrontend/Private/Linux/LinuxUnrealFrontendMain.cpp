// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UnrealFrontendMain.h"
#include "RequiredProgramMainCPPInclude.h"
#include "LinuxCommonStartup.h"

int main(int argc, char *argv[])
{
	return CommonLinuxMain(argc, argv, &UnrealFrontendMain);
}
