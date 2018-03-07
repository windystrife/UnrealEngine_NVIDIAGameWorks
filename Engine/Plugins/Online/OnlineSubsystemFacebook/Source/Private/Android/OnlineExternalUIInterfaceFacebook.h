// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemFacebook.h"
#include "OnlineExternalUIFacebookCommon.h"
#include "OnlineSubsystemFacebookPackage.h"

class FOnlineSubsystemFacebook;

/** 
 * Implementation for the Facebook external UIs
 */
class FOnlineExternalUIFacebook : public FOnlineExternalUIFacebookCommon
{
private:

PACKAGE_SCOPE:

	/** 
	 * Constructor
	 * @param InSubsystem The owner of this external UI interface.
	 */
	 FOnlineExternalUIFacebook(FOnlineSubsystemFacebook* InSubsystem);

public:

	/**
	 * Destructor.
	 */
	virtual ~FOnlineExternalUIFacebook();
};

typedef TSharedPtr<FOnlineExternalUIFacebook, ESPMode::ThreadSafe> FOnlineExternalUIFacebookPtr;

