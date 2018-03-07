// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HTML5PlatformHttp.h"
#include "HTML5HTTP.h"


void FHTML5PlatformHttp::Init()
{
}


void FHTML5PlatformHttp::Shutdown()
{
}


IHttpRequest* FHTML5PlatformHttp::ConstructRequest()
{
	return new FHTML5HttpRequest();
}
