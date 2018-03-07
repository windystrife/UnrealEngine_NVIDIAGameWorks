// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "SlateRemoteSettings.generated.h"


/**
 * Implements the settings for the Slate Remote plug-in.
 */
UCLASS(config=Engine)
class USlateRemoteSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Whether the Slate Remote server is enabled. */
	UPROPERTY(config, EditAnywhere, Category=RemoteServer)
	bool EnableRemoteServer;

	/** The IP endpoint to listen to when the Remote Server runs in the Editor. */
	UPROPERTY(config, EditAnywhere, Category=RemoteServer)
	FString EditorServerEndpoint;

	/** The IP endpoint to listen to when the Remote Server runs in a game. */
	UPROPERTY(config, EditAnywhere, Category=RemoteServer)
	FString GameServerEndpoint;
};
