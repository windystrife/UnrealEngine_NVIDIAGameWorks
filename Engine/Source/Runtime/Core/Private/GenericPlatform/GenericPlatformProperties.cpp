// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GenericPlatform/GenericPlatformProperties.h"
#include "HAL/PlatformProperties.h"


// This has to be in a .cpp since it needs to come after the FPlatformProperties class has been typedef'd
const char* FGenericPlatformProperties::IniPlatformName()
{
	return FPlatformProperties::PlatformName();
}
