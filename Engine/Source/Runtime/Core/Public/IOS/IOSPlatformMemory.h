// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	IOSPlatformMemory.h: IOS platform memory functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformMemory.h"
#include "Apple/ApplePlatformMemory.h"

/**
* IOS implementation of the memory OS functions
**/
struct CORE_API FIOSPlatformMemory : public FApplePlatformMemory
{
};

typedef FIOSPlatformMemory FPlatformMemory;
