// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Android/AndroidHttp.h"
#include "Curl/CurlHttp.h"
#include "Curl/CurlHttpManager.h"

void FAndroidPlatformHttp::Init()
{
	FCurlHttpManager::InitCurl();
}

class FHttpManager * FAndroidPlatformHttp::CreatePlatformHttpManager()
{
	return new FCurlHttpManager();
}

void FAndroidPlatformHttp::Shutdown()
{
	FCurlHttpManager::ShutdownCurl();
}

IHttpRequest* FAndroidPlatformHttp::ConstructRequest()
{
	return new FCurlHttpRequest();
}

