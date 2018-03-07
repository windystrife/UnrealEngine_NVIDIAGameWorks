// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformAffinity.h"

class CORE_API FApplePlatformAffinity : public FGenericPlatformAffinity
{
public:
	static EThreadPriority GetRenderingThreadPriority()
	{
		return TPri_TimeCritical;
	}
	
	static EThreadPriority GetRHIThreadPriority()
	{
		return TPri_Highest;
	}
};
