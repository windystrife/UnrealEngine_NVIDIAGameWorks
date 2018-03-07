// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Logging/LogMacros.h"


/** Declares a log category for this module. */
DECLARE_LOG_CATEGORY_EXTERN(LogUdpMessaging, Log, All);


/** Defines the default IP endpoint for multicast traffic. */
#define UDP_MESSAGING_DEFAULT_MULTICAST_ENDPOINT FIPv4Endpoint(FIPv4Address(230, 0, 0, 1), 6666)

/** Defines the maximum number of annotations a message can have. */
#define UDP_MESSAGING_MAX_ANNOTATIONS 128

/** Defines the maximum number of recipients a message can have. */
#define UDP_MESSAGING_MAX_RECIPIENTS 1024

/** Defines the desired size of socket receive buffers (in bytes). */
#define UDP_MESSAGING_RECEIVE_BUFFER_SIZE 2 * 1024 * 1024

/** Defines the protocol version of the UDP message transport. */
#define UDP_MESSAGING_TRANSPORT_PROTOCOL_VERSION 10
