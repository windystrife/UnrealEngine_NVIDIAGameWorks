// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/App.h"
#include "Misc/OutputDeviceError.h"
#include "Templates/ScopedPointer.h"
#include "LaunchEngineLoop.h"
THIRD_PARTY_INCLUDES_START
	#include <SDL.h>
	#include <emscripten/emscripten.h>
	#include <emscripten/trace.h>
THIRD_PARTY_INCLUDES_END

DEFINE_LOG_CATEGORY_STATIC(LogHTML5Launch, Log, All);

#include <string.h>

FEngineLoop	GEngineLoop;
TCHAR GCmdLine[2048];

void HTML5_Tick()
{
	static uint32 FrameCount = 1;
	char Buf[128];
	sprintf(Buf, "Frame %u", FrameCount);
	emscripten_trace_record_frame_start();
	emscripten_trace_enter_context(Buf);
	GEngineLoop.Tick();
	emscripten_trace_exit_context();
	emscripten_trace_record_frame_end();

	// Assuming 60fps, log out the memory report. This isn't important to be
	// exactly every second, it just needs done periodically
	if (FrameCount % 60 == 0)
	{
		emscripten_trace_report_memory_layout();
	}

	++FrameCount;
}

void HTML5_Init ()
{
	emscripten_trace_record_frame_start();

	// initialize the engine
	UE_LOG(LogTemp,Display,TEXT("PreInit Start"));
	emscripten_trace_enter_context("PreInit");
	GEngineLoop.PreInit(0, NULL, GCmdLine);
	emscripten_trace_exit_context();
	UE_LOG(LogHTML5Launch,Display,TEXT("PreInit Complete"));
	UE_LOG(LogHTML5Launch,Display,TEXT("Init Start"));
	emscripten_trace_enter_context("Init");
	GEngineLoop.Init();
	emscripten_trace_exit_context();
	UE_LOG(LogHTML5Launch,Display,TEXT("Init Complete"));

	emscripten_trace_record_frame_end();
	emscripten_set_main_loop(HTML5_Tick, 0, true);
}

class FHTML5Exec : private FSelfRegisteringExec
{
public:

	FHTML5Exec()
		{}

	/** Console commands **/
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override
	{
		if (FParse::Command(&Cmd, TEXT("em_trace_close")))
		{
			emscripten_trace_exit_context();//"main");
			emscripten_trace_close();
			return true;
		}
		return false;
	}
};
static TUniquePtr<FHTML5Exec> GHTML5Exec;

int main(int argc, char* argv[])
{
	UE_LOG(LogHTML5Launch,Display,TEXT("Starting UE4 ... %s\n"), GCmdLine);

	// TODO: configure this via the command line?
	emscripten_trace_configure("http://127.0.0.1:5000/", "UE4Game");
	GHTML5Exec = MakeUnique<FHTML5Exec>();

	emscripten_trace_enter_context("main");

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_NOPARACHUTE);
	GCmdLine[0] = 0;
	// to-do use Platform Str functions.
	FCString::Strcpy(GCmdLine, TEXT(" "));

	for (int Option = 1; Option < argc; Option++)
	{
		FCString::Strcat(GCmdLine, TEXT("  "));
		FCString::Strcat(GCmdLine, ANSI_TO_TCHAR(argv[Option]));
	}

	UE_LOG(LogHTML5Launch,Display,TEXT("Command line: %s\n"), GCmdLine);

	HTML5_Init();

	return 0;
}


void EmptyLinkFunctionForStaticInitializationHTML5Win32(void) {}
