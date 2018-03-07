// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	MacPlatformMemory.h: Mac platform memory functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformMemory.h"
#include "Apple/ApplePlatformMemory.h"

/**
* Mac implementation of the memory OS functions
**/
struct CORE_API FMacPlatformMemory : public FApplePlatformMemory
{
	//~ Begin FGenericPlatformMemory Interface
	static FPlatformMemoryStats GetStats();
	static const FPlatformMemoryConstants& GetConstants();
	static FMalloc* BaseAllocator();
	//~ End FGenericPlatformMemory Interface
};

typedef FMacPlatformMemory FPlatformMemory;



