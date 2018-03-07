// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Logging/LogMacros.h"
#include "CoreGlobals.h"
#include "Misc/ScopeLock.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/FeedbackContext.h"
#include "Misc/VarargsHelper.h"

void StaticFailDebug( const TCHAR* Error, const ANSICHAR* File, int32 Line, const TCHAR* Description, bool bIsEnsure );

/** Statics to prevent FMsg::Logf from allocating too much stack memory. */
static FCriticalSection					MsgLogfStaticBufferGuard;
/** Increased from 4096 to fix crashes in the renderthread without autoreporter. */
static TCHAR							MsgLogfStaticBuffer[8192];

VARARG_BODY(void, FMsg::Logf, const TCHAR*, VARARG_EXTRA(const ANSICHAR* File) VARARG_EXTRA(int32 Line) VARARG_EXTRA(const class FName& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity))
{
#if !NO_LOGGING
	if (Verbosity != ELogVerbosity::Fatal)
	{
		// SetColour is routed to GWarn just like the other verbosities and handled in the 
		// device that does the actual printing.
		FOutputDevice* LogDevice = NULL;
		switch (Verbosity)
		{
		case ELogVerbosity::Error:
		case ELogVerbosity::Warning:
		case ELogVerbosity::Display:
		case ELogVerbosity::SetColor:
			if (GWarn)
			{
				LogDevice = GWarn;
				break;
			}
		default:
		{
			LogDevice = GLog;
		}
		break;
		}
		GROWABLE_LOGF(LogDevice->Log(Category, Verbosity, Buffer))
	}
	else
	{
		// Keep Message buffer small, in some cases, this code is executed with 16KB stack.
		TCHAR Message[4096];
		{
			// Simulate Sprintf_s
			// @todo: implement platform independent sprintf_S
			// We're using one big shared static buffer here, so guard against re-entry
			FScopeLock MsgLock(&MsgLogfStaticBufferGuard);
			// Print to a large static buffer so we can keep the stack allocation below 16K
			GET_VARARGS(MsgLogfStaticBuffer, ARRAY_COUNT(MsgLogfStaticBuffer), ARRAY_COUNT(MsgLogfStaticBuffer) - 1, Fmt, Fmt);
			// Copy the message to the stack-allocated buffer)
			FCString::Strncpy(Message, MsgLogfStaticBuffer, ARRAY_COUNT(Message) - 1);
			Message[ARRAY_COUNT(Message) - 1] = '\0';
		}

		StaticFailDebug(TEXT("Fatal error:"), File, Line, Message, false);
		FDebug::AssertFailed("", File, Line, Message);
	}
#endif
}

VARARG_BODY(void, FMsg::Logf_Internal, const TCHAR*, VARARG_EXTRA(const ANSICHAR* File) VARARG_EXTRA(int32 Line) VARARG_EXTRA(const class FName& Category) VARARG_EXTRA(ELogVerbosity::Type Verbosity))
{
#if !NO_LOGGING
	if (Verbosity != ELogVerbosity::Fatal)
	{
		// SetColour is routed to GWarn just like the other verbosities and handled in the 
		// device that does the actual printing.
		FOutputDevice* LogDevice = NULL;
		switch (Verbosity)
		{
		case ELogVerbosity::Error:
		case ELogVerbosity::Warning:
		case ELogVerbosity::Display:
		case ELogVerbosity::SetColor:
			if (GWarn)
			{
				LogDevice = GWarn;
				break;
			}
		default:
		{
			LogDevice = GLog;
		}
		break;
		}
		GROWABLE_LOGF(LogDevice->Log(Category, Verbosity, Buffer))
	}
	else
	{
		// Keep Message buffer small, in some cases, this code is executed with 16KB stack.
		TCHAR Message[4096];
		{
			// Simulate Sprintf_s
			// @todo: implement platform independent sprintf_S
			// We're using one big shared static buffer here, so guard against re-entry
			FScopeLock MsgLock(&MsgLogfStaticBufferGuard);
			// Print to a large static buffer so we can keep the stack allocation below 16K
			GET_VARARGS(MsgLogfStaticBuffer, ARRAY_COUNT(MsgLogfStaticBuffer), ARRAY_COUNT(MsgLogfStaticBuffer) - 1, Fmt, Fmt);
			// Copy the message to the stack-allocated buffer)
			FCString::Strncpy(Message, MsgLogfStaticBuffer, ARRAY_COUNT(Message) - 1);
			Message[ARRAY_COUNT(Message) - 1] = '\0';
		}

		StaticFailDebug(TEXT("Fatal error:"), File, Line, Message, false);
	}
#endif
}

/** Sends a formatted message to a remote tool. */
void VARARGS FMsg::SendNotificationStringf( const TCHAR *Fmt, ... )
{
	GROWABLE_LOGF(SendNotificationString(Buffer));
}

void FMsg::SendNotificationString( const TCHAR* Message )
{
	FPlatformMisc::LowLevelOutputDebugString(Message);
}
