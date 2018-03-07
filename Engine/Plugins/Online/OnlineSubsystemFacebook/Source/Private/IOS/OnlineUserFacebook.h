// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once
 
// Module includes
#include "OnlineUserFacebookCommon.h"
#include "OnlineSubsystemFacebookPackage.h"

class FOnlineSubsystemFacebook;

/**
 * Facebook implementation of the Online User Interface
 */
class FOnlineUserFacebook : public FOnlineUserFacebookCommon
{

public:
	
	// IOnlineUser
	virtual bool QueryUserInfo(int32 LocalUserNum, const TArray<TSharedRef<const FUniqueNetId> >& UserIds) override;

	// FOnlineUserFacebook

	/**
	 * Constructor used to indicate which OSS we are a part of
	 */
	FOnlineUserFacebook(FOnlineSubsystemFacebook* InSubsystem);
	
	/**
	 * Default destructor
	 */
	virtual ~FOnlineUserFacebook();

};


typedef TSharedPtr<FOnlineUserFacebook, ESPMode::ThreadSafe> FOnlineUserFacebookPtr;
