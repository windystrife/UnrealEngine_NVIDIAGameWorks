// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// This isn't really third-party but is a convenient place

#include <android/log.h>
#include <dlfcn.h>
#include <signal.h>

__sighandler_t bsd_signal(int s, __sighandler_t f)
{
	return signal(s, f);
}
