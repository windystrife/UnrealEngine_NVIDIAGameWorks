// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Templates/SharedPointer.h"

class FMessageEndpoint;
class IMessageContext;

struct FIOSLaunchDaemonLaunchApp;
struct FIOSLaunchDaemonPing;


class FGameLaunchDaemonMessageHandler
{
public:

	void Init();
	void Shutdown();

private:

	void HandlePingMessage(const FIOSLaunchDaemonPing& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);
	void HandleLaunchRequest(const FIOSLaunchDaemonLaunchApp& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

private:

	TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> MessageEndpoint;
	FString AppId;
};
