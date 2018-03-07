// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformApplicationMisc.h"

struct APPLICATIONCORE_API FHTML5PlatformApplicationMisc : public FGenericPlatformApplicationMisc
{
	static void PostInit();
	static class GenericApplication* CreateApplication();
};

typedef FHTML5PlatformApplicationMisc FPlatformApplicationMisc;
