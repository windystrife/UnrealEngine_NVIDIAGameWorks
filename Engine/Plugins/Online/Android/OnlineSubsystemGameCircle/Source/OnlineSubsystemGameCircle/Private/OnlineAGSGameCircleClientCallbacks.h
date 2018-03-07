// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AGS/GameCircleClientInterface.h"


class FOnlineShowGameCircleCallback : public AmazonGames::IShowGameCircleCb
{
public:
	virtual void onShowGameCircleCb(AmazonGames::ErrorCode errorCode, int developerTag) final;
};

typedef TSharedPtr<FOnlineShowGameCircleCallback, ESPMode::ThreadSafe> FOnlineShowGameCircleCallbackPtr;


class FOnlineShowSignInPageCallback : public AmazonGames::IShowSignInPageCb
{
public:
	virtual void onShowSignInPageCb(AmazonGames::ErrorCode errorCode, int developerTag) final;
};

typedef TSharedPtr<FOnlineShowSignInPageCallback, ESPMode::ThreadSafe> FOnlineShowSignInPageCallbackPtr;
