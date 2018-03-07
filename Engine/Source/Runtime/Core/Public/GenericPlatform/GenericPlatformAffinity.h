// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#define		MAKEAFFINITYMASK1(x)				(1<<x)
#define		MAKEAFFINITYMASK2(x,y)				((1<<x)+(1<<y))
#define		MAKEAFFINITYMASK3(x,y,z)			((1<<x)+(1<<y)+(1<<z))
#define		MAKEAFFINITYMASK4(w,x,y,z)			((1<<w)+(1<<x)+(1<<y)+(1<<z))
#define		MAKEAFFINITYMASK5(v,w,x,y,z)		((1<<v)+(1<<w)+(1<<x)+(1<<y)+(1<<z))
#define		MAKEAFFINITYMASK6(u,v,w,x,y,z)		((1<<u)+(1<<v)+(1<<w)+(1<<x)+(1<<y)+(1<<z))
#define		MAKEAFFINITYMASK7(t,u,v,w,x,y,z)	((1<<t)+(1<<u)+(1<<v)+(1<<w)+(1<<x)+(1<<y)+(1<<z))

/**
* The list of enumerated thread priorities we support
*/
enum EThreadPriority
{
	TPri_Normal,
	TPri_AboveNormal,
	TPri_BelowNormal,
	TPri_Highest,
	TPri_Lowest,
	TPri_SlightlyBelowNormal,
	TPri_TimeCritical
};

class FGenericPlatformAffinity
{
public:
	static const CORE_API uint64 GetMainGameMask()
	{
		return 0xFFFFFFFFFFFFFFFF;
	}

	static const CORE_API uint64 GetRenderingThreadMask()
	{
		return 0xFFFFFFFFFFFFFFFF;
	}

	static const CORE_API uint64 GetRHIThreadMask()
	{
		return 0xFFFFFFFFFFFFFFFF;
	}

	static const CORE_API uint64 GetRTHeartBeatMask()
	{
		return 0xFFFFFFFFFFFFFFFF;
	}

	static const CORE_API uint64 GetPoolThreadMask()
	{
		return 0xFFFFFFFFFFFFFFFF;
	}

	static const CORE_API uint64 GetTaskGraphThreadMask()
	{
		return 0xFFFFFFFFFFFFFFFF;
	}

	static const CORE_API uint64 GetStatsThreadMask()
	{
		return 0xFFFFFFFFFFFFFFFF;
	}

	static const CORE_API uint64 GetAudioThreadMask()
	{
		return 0xFFFFFFFFFFFFFFFF;
	}

	static const CORE_API uint64 GetNoAffinityMask()
	{
		return 0xFFFFFFFFFFFFFFFF;
	}

	static const CORE_API uint64 GetTaskGraphBackgroundTaskMask()
	{
		return 0xFFFFFFFFFFFFFFFF;
	}

	// @todo what do we think about having this as a function in this class? Should be make a whole new one? 
	// scrap it and force the priority like before?
	static EThreadPriority GetRenderingThreadPriority()
	{
		return TPri_Normal;
	}
	
	static EThreadPriority GetRHIThreadPriority()
	{
		return TPri_SlightlyBelowNormal;
	}
};


