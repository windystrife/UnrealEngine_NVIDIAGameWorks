// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_LIBCURL

#include "HttpThread.h"

#if PLATFORM_WINDOWS
#include "WindowsHWrapper.h"
#include "AllowWindowsPlatformTypes.h"
#endif
	#include "curl/curl.h"
#if PLATFORM_WINDOWS
#include "HideWindowsPlatformTypes.h"
#endif

#endif //WITH_LIBCURL

class IHttpThreadedRequest;

#if WITH_LIBCURL

class FCurlHttpThread
	: public FHttpThread
{
public:
	
	FCurlHttpThread();

protected:
	//~ Begin FHttpThread Interface
	virtual void HttpThreadTick(float DeltaSeconds) override;
	virtual bool StartThreadedRequest(IHttpThreadedRequest* Request) override;
	virtual void CompleteThreadedRequest(IHttpThreadedRequest* Request) override;
	//~ End FHttpThread Interface
protected:

	/** Mapping of libcurl easy handles to HTTP requests */
	TMap<CURL*, IHttpThreadedRequest*> HandlesToRequests;
};


#endif //WITH_LIBCURL
