// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineFriendsFacebookCommon.h"
#include "OnlineSubsystemFacebookPackage.h"

class FOnlineSubsystemFacebook;

/**
 * Facebook service implementation of the online friends interface (Windows)
 */
class FOnlineFriendsFacebook :
	public FOnlineFriendsFacebookCommon
{	

public:

	/**
	 * Constructor
	 *
	 * @param InSubsystem Facebook subsystem being used
	 */
	FOnlineFriendsFacebook(FOnlineSubsystemFacebook* InSubsystem);
	
	/**
	 * Destructor
	 */
	virtual ~FOnlineFriendsFacebook();

private:

	/**
	 * Should use the initialization constructor instead
	 */
	FOnlineFriendsFacebook();

};

typedef TSharedPtr<FOnlineFriendsFacebook, ESPMode::ThreadSafe> FOnlineFriendsFacebookPtr;
