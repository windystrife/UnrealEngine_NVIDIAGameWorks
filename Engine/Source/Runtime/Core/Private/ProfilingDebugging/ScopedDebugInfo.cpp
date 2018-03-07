// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScopedDebugInfo.cpp: Scoped debug info implementation.
=============================================================================*/

#include "ProfilingDebugging/ScopedDebugInfo.h"
#include "HAL/PlatformTLS.h"

/** The TLS index for the debug info stack. */
static uint32 GThreadDebugInfoTLSID = FPlatformTLS::AllocTlsSlot();

FScopedDebugInfo::FScopedDebugInfo(int32 InNumReplacedOuterCalls):
	NumReplacedOuterCalls(InNumReplacedOuterCalls),
	NextOuterInfo((FScopedDebugInfo*)FPlatformTLS::GetTlsValue(GThreadDebugInfoTLSID))
{
	// Set the this debug info as the current innermost debug info.
	FPlatformTLS::SetTlsValue(GThreadDebugInfoTLSID,this);
}

FScopedDebugInfo::~FScopedDebugInfo()
{
	FScopedDebugInfo* CurrentInnermostDebugInfo = (FScopedDebugInfo*)FPlatformTLS::GetTlsValue(GThreadDebugInfoTLSID);
	check(CurrentInnermostDebugInfo == this);

	// Set the next outermost link to the current innermost debug info.
	FPlatformTLS::SetTlsValue(GThreadDebugInfoTLSID,NextOuterInfo);
}

FScopedDebugInfo* FScopedDebugInfo::GetDebugInfoStack()
{
	return (FScopedDebugInfo*)FPlatformTLS::GetTlsValue(GThreadDebugInfoTLSID);
}
