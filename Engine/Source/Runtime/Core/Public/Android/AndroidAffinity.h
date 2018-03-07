// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
AndroidAffinity.h: Android affinity profile masks definitions.
==============================================================================================*/

#pragma once

#include "GenericPlatform/GenericPlatformAffinity.h"

class FAndroidAffinity : public FGenericPlatformAffinity
{
public:
	static const CORE_API uint64 GetMainGameMask()
	{
		return GameThreadMask;
	}

	static const CORE_API uint64 GetRenderingThreadMask()
	{
		return RenderingThreadMask;
	}

public:
	static int64 GameThreadMask;
	static int64 RenderingThreadMask;
};

typedef FAndroidAffinity FPlatformAffinity;
