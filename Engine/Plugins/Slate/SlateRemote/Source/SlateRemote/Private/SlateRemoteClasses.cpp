// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Shared/SlateRemoteSettings.h"
#include "SlateRemotePrivate.h"


USlateRemoteSettings::USlateRemoteSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
	, EnableRemoteServer(true)
	, EditorServerEndpoint(SLATE_REMOTE_SERVER_DEFAULT_EDITOR_ENDPOINT.ToString())
	, GameServerEndpoint(SLATE_REMOTE_SERVER_DEFAULT_GAME_ENDPOINT.ToString())
{ }
