// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HTML5Misc.cpp: HTML5 implementations of misc functions
=============================================================================*/

#include "HTML5PlatformMisc.h"
#include "HTML5PlatformProcess.h"

#include "HTML5JavaScriptFx.h"
#include <emscripten/trace.h>

THIRD_PARTY_INCLUDES_START
	#include "unicode/locid.h"
THIRD_PARTY_INCLUDES_END
#include "GenericPlatformCrashContext.h"
THIRD_PARTY_INCLUDES_START
	#include <SDL.h>
	#include <ctime>
THIRD_PARTY_INCLUDES_END

#include "MapPakDownloaderModule.h"
#include "MapPakDownloader.h"


void FHTML5Misc::PlatformInit()
{
	// Identity.
	UE_LOG(LogInit, Log, TEXT("Computer: %s"), FPlatformProcess::ComputerName());
	UE_LOG(LogInit, Log, TEXT("User: %s"), FPlatformProcess::UserName());
	UE_LOG(LogInit, Log, TEXT("Current Culture: %s"), *FHTML5Misc::GetDefaultLocale());

	// Timer resolution.
	UE_LOG(LogInit, Log, TEXT("High frequency timer resolution =%f MHz"), 0.000001 / FPlatformTime::GetSecondsPerCycle());
}

// Defines the PlatformFeatures module name for HTML5, used by PlatformFeatures.h.
const TCHAR* FHTML5Misc::GetPlatformFeaturesModuleName()
{
	return TEXT("HTML5PlatformFeatures");
}

FString FHTML5Misc::GetDefaultLocale()
{
	char AsciiCultureName[512];
	if (UE_GetCurrentCultureName(AsciiCultureName,sizeof(AsciiCultureName)))
	{
		return FString(StringCast<TCHAR>(AsciiCultureName).Get());
	}
	else
	{
		icu::Locale ICUDefaultLocale = icu::Locale::getDefault();
		return FString(ICUDefaultLocale.getName());
	}
}

EAppReturnType::Type FHTML5Misc::MessageBoxExt(EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption)
{
	ANSICHAR* AText = TCHAR_TO_ANSI(Text);
	ANSICHAR* ACaption = TCHAR_TO_ANSI(Caption);
	return static_cast<EAppReturnType::Type>(UE_MessageBox(MsgType,AText,ACaption));
}

void (*GHTML5CrashHandler)(const FGenericCrashContext& Context) = nullptr;

extern "C"
{
	// callback from javascript.
	void on_fatal(const char* msg, const char* error)
	{
#ifdef __EMSCRIPTEN_TRACING__
		emscripten_log(EM_LOG_CONSOLE, "Fatal Error: Closing trace!");
		emscripten_trace_close();
#endif
		// !!JM todo: pass msg & error to a crash context? Must be copied?
		if (GHTML5CrashHandler)
		{
			FGenericCrashContext Ctx;
			GHTML5CrashHandler(Ctx);
		}
	}
}

void FHTML5Misc::SetCrashHandler(void(* CrashHandler)(const FGenericCrashContext& Context))
{
	GHTML5CrashHandler = CrashHandler;
}

