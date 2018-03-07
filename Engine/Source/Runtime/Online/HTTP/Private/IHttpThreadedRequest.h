// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"

class IHttpThreadedRequest : public IHttpRequest
{
public:
	// Called on http thread
	virtual bool StartThreadedRequest() = 0;
	virtual bool IsThreadedRequestComplete() = 0;
	virtual void TickThreadedRequest(float DeltaSeconds) = 0;

	// Called on game thread
	virtual void FinishRequest() = 0;

protected:
};
