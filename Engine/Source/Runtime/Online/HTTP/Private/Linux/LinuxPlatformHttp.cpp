// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Linux/LinuxPlatformHttp.h"
#include "Curl/CurlHttp.h"
#include "Curl/CurlHttpManager.h"

void FLinuxPlatformHttp::Init()
{
	FCurlHttpManager::InitCurl();
}

class FHttpManager * FLinuxPlatformHttp::CreatePlatformHttpManager()
{
	return new FCurlHttpManager();
}

void FLinuxPlatformHttp::Shutdown()
{
	FCurlHttpManager::ShutdownCurl();
}

IHttpRequest* FLinuxPlatformHttp::ConstructRequest()
{
	return new FCurlHttpRequest();
}

