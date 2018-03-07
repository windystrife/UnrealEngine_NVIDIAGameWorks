// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FSlackIncomingWebhook;
struct FSlackMessage;

/**
 * Public interface for accessing Slack incoming webhook methods
 */
class ISlackIncomingWebhookInterface
{
public:
	virtual bool SendMessage(const FSlackIncomingWebhook& InWebhook, const FSlackMessage& InMessage) const = 0;
};
