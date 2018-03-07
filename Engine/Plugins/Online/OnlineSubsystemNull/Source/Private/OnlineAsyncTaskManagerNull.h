// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineAsyncTaskManager.h"

/**
 *	Null version of the async task manager to register the various Null callbacks with the engine
 */
class FOnlineAsyncTaskManagerNull : public FOnlineAsyncTaskManager
{
protected:

	/** Cached reference to the main online subsystem */
	class FOnlineSubsystemNull* NullSubsystem;

public:

	FOnlineAsyncTaskManagerNull(class FOnlineSubsystemNull* InOnlineSubsystem)
		: NullSubsystem(InOnlineSubsystem)
	{
	}

	~FOnlineAsyncTaskManagerNull() 
	{
	}

	// FOnlineAsyncTaskManager
	virtual void OnlineTick() override;
};
