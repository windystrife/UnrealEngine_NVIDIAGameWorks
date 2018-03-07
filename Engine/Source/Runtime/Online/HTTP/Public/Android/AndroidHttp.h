// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "GenericPlatform/GenericPlatformHttp.h"

/**
 * Platform specific Http implementations
 */
class FAndroidPlatformHttp : public FGenericPlatformHttp
{
public:

	/**
	 * Platform initialization step
	 */
	static void Init();

	/**
	 * Creates a platform-specific HTTP manager.
	 *
	 * @return NULL if default implementation is to be used
	 */
	static FHttpManager* CreatePlatformHttpManager();

	/**
	 * Platform shutdown step
	 */
	static void Shutdown();

	/**
	 * Creates a new Http request instance for the current platform
	 *
	 * @return request object
	 */
	static IHttpRequest* ConstructRequest();
};


typedef FAndroidPlatformHttp FPlatformHttp;