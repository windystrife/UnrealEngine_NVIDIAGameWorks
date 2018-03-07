// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemFacebookCommon.h"
#include "OnlineSubsystemFacebookPackage.h"

/** Forward declarations of all interface classes */
typedef TSharedPtr<class FOnlineIdentityFacebook, ESPMode::ThreadSafe> FOnlineIdentityFacebookPtr;
typedef TSharedPtr<class FOnlineFriendsFacebook, ESPMode::ThreadSafe> FOnlineFriendsFacebookPtr;
typedef TSharedPtr<class FOnlineSharingFacebook, ESPMode::ThreadSafe> FOnlineSharingFacebookPtr;
typedef TSharedPtr<class FOnlineUserFacebook, ESPMode::ThreadSafe> FOnlineUserFacebookPtr;
typedef TSharedPtr<class FOnlineExternalUIFacebook, ESPMode::ThreadSafe> FOnlineExternalUIFacebookPtr;

/**
 *	OnlineSubsystemFacebook - Implementation of the online subsystem for Facebook services
 */
class ONLINESUBSYSTEMFACEBOOK_API FOnlineSubsystemFacebook 
	: public FOnlineSubsystemFacebookCommon
{
public:

	// FOnlineSubsystemFacebookCommon Interface
	virtual bool Init() override;
	virtual bool Shutdown() override;
	virtual bool IsEnabled() const override;

	// FOnlineSubsystemFacebook

	/**
	 * Destructor
	 */
	virtual ~FOnlineSubsystemFacebook();

PACKAGE_SCOPE:

	/** Only the factory makes instances */
	FOnlineSubsystemFacebook();
	FOnlineSubsystemFacebook(FName InInstanceName);
};

typedef TSharedPtr<FOnlineSubsystemFacebook, ESPMode::ThreadSafe> FOnlineSubsystemFacebookPtr;
