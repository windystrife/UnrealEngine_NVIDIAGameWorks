// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Shared/UdpMessagingSettings.h"


UUdpMessagingSettings::UUdpMessagingSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, EnableTransport(true)
	, MulticastTimeToLive(1)
	, EnableTunnel(false)
{ }
