// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineAsyncTaskManager.h"
#include "OnlineSubsystemGooglePlayPackage.h"

/**
 *	Google Play version of the async task manager
 */
class FOnlineAsyncTaskManagerGooglePlay : public FOnlineAsyncTaskManager
{
public:

	FOnlineAsyncTaskManagerGooglePlay() {}

	// FOnlineAsyncTaskManager
	virtual void OnlineTick() override {}
};


