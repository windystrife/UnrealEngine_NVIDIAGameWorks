// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Icmp.h"
#include "IcmpModule.h"
#include "IcmpPrivate.h"
#include <netinet/in.h>

uint16 NtoHS(uint16 val)
{
	return ntohs(val);
}

uint16 HtoNS(uint16 val)
{
	return htons(val);
}

uint32 NtoHL(uint32 val)
{
	return ntohl(val);
}

uint32 HtoNL(uint32 val)
{
	return htonl(val);
}
