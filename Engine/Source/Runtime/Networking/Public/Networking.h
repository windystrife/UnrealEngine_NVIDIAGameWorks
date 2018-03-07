// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once


/* Boilerplate
 *****************************************************************************/

#include "Misc/MonolithicHeaderBoilerplate.h"
MONOLITHIC_HEADER_BOILERPLATE()


/* Public Dependencies
 *****************************************************************************/

#include "Core.h"
#include "Modules/ModuleManager.h"
#include "Sockets.h"
#include "SocketSubsystem.h"


/* Public Includes
 *****************************************************************************/

#include "Interfaces/IPv4/IPv4SubnetMask.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Interfaces/IPv4/IPv4Subnet.h"
#include "Interfaces/Steam/SteamEndpoint.h"
#include "Interfaces/INetworkingModule.h"


/* Common
 *****************************************************************************/

#include "Common/TcpSocketBuilder.h"
#include "Common/TcpListener.h"
#include "Common/UdpSocketReceiver.h"
#include "Common/UdpSocketSender.h"
#include "Common/UdpSocketBuilder.h"
