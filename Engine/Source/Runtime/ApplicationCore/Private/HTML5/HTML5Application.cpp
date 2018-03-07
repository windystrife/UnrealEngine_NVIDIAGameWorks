// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HTML5Application.h"
#include "HTML5Cursor.h"
#include "HTML5InputInterface.h"
#include "HAL/OutputDevices.h"

#include "SDL_opengl.h"

DEFINE_LOG_CATEGORY_STATIC(LogHTML5Application, Log, All);

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include "HTML5JavaScriptFx.h"

EM_BOOL mouse_callback(int eventType, const EmscriptenMouseEvent *e, void *userData)
{
	return 1;
}

EM_BOOL pointerlockchange_callback(int eventType, const EmscriptenPointerlockChangeEvent *Event, void *userData)
{
	UE_LOG(LogHTML5Application, Verbose, TEXT("PointerLockChangedEvent: Active:%d"), Event->isActive );

	static uint Prev = 0;
	// Generate a fake WindowsEnter event when the pointerlock goes from inactive to active.
	if (Event->isActive && Prev == 0)
	{
		SDL_Event event;
		SDL_zero(event);
		event.type = SDL_WINDOWEVENT;
		event.window.event = SDL_WINDOWEVENT_ENTER;
		SDL_PushEvent(&event);
	}
	Prev = Event->isActive;

	return 1;
}

static const uint32 MaxWarmUpTicks = 10;

FHTML5Application* FHTML5Application::CreateHTML5Application()
{
	return new FHTML5Application();
}

// In HTML5 builds, we do not directly listen to browser window resize events in UE4 engine side,
// because we want the web page author to be able to fully control how the canvas size should react to
// when window size changes. All canvas resize operations occur by logic on the .html page, and the web
// developer should call the JavaScript function UE_JSlib.UE_CanvasSizeChanged() to report when they
// resized the canvas. This way developers can customize the logic of how the canvas should scale with
// the page based on the needs of their web site layout.
// When UE_JSlib.UE_CanvasSizeChanged() is called, it is flagged true here, and the next iteration
// of the engine renderer will apply those changes and resize the GL viewport to match.
// Note that this size change refers to a change in the WebGL render target resolution, and not a
// change in the visible CSS pixel size of the canvas DOM element (those two can be separately set and
// do not need to match). If the CSS size of the <canvas> element changes, UE4 engine does not really
// care to know, but the engine only follows size changes on the WebGL render target itself.
static bool canvas_size_changed = false;

static void on_canvas_size_changed()
{
	canvas_size_changed = true;
}
static EM_BOOL canvas_resized_on_fullscreen_change(int eventType, const void *reserved, void *userData)
{
	on_canvas_size_changed();
	return 0;
}

// callback from javascript.
EM_BOOL request_fullscreen_callback(int eventType, const EmscriptenMouseEvent* evt, void* user)
{
	EmscriptenFullscreenStrategy FSStrat;
	FMemory::Memzero(FSStrat);

	// Ask user HTML page to resize the canvas when entering fullscreen. (Generally users do not need
	// to do anything specific here, but one of the premade resizing scenarios below should be good enough)
	bool abortFullscreen = EM_ASM_INT_V({
			if (Module['UE4_resizeCanvas'])
				return Module['UE4_resizeCanvas'](/*aboutToEnterFullscreen=*/true);
			return false;
		});
	if (abortFullscreen)
	{
		// If caller returns true above, abort the initiated attempt to go to move to fullscreen mode.
		return 0;
	}

	FSStrat.scaleMode = EM_ASM_INT_V({ return Module['UE4_fullscreenScaleMode']; });
	FSStrat.canvasResolutionScaleMode = EM_ASM_INT_V({ return Module['UE4_fullscreenCanvasResizeMode']; });
	FSStrat.filteringMode = EM_ASM_INT_V({ return Module['UE4_fullscreenFilteringMode']; });

	// If the WebGL render target size is going to change when entering & exiting fullscreen mode, track those changes
	// to be able to resize the viewport accordingly.
	if (FSStrat.canvasResolutionScaleMode != EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_NONE)
	{
		FSStrat.canvasResizedCallback = canvas_resized_on_fullscreen_change;
	}

	// TODO: UE4_useSoftFullscreenMode does not quite work right now because the "mainarea" div on the main page
	// has margins, which cause it to not align up, so this parameter is not exposed to the main html page
	// at the moment. Also page will need to manually hook e.g. esc button to exit the soft fullscreen mode,
	// which is not added. However this could be an useful feature to add at some point in the future.
	bool softFullscreen = EM_ASM_INT_V({ return Module['UE4_useSoftFullscreenMode']; });
	EMSCRIPTEN_RESULT result;
	if (softFullscreen)
	{
		result = emscripten_enter_soft_fullscreen("canvas", &FSStrat);
	}
	else
	{
		result = emscripten_request_fullscreen_strategy("canvas", true, &FSStrat);
	}

	if (result == EMSCRIPTEN_RESULT_SUCCESS)
	{
		on_canvas_size_changed();
	}
	return 0;
}

FHTML5Application::FHTML5Application()
	: GenericApplication( MakeShareable( new FHTML5Cursor() ) )
	, ApplicationWindow(FHTML5Window::Make())
	, InputInterface( FHTML5InputInterface::Create(MessageHandler, Cursor ) )
	, WarmUpTicks(-1)
{
	// full screen will only be requested after the first click after the window gains focus.
	// the problem is that because of security/UX reasons browsers don't allow pointer lock in main loop
	// but only through callbacks generated by browser.

	emscripten_set_click_callback("fullscreen_request", nullptr, true, request_fullscreen_callback);

	// work around emscripten bug where deffered browser requests are not called if there are no callbacks.
	emscripten_set_mousedown_callback("#canvas",0,1,mouse_callback);
	emscripten_set_pointerlockchange_callback(0,0,true,pointerlockchange_callback);

	// Register to listen to when web developer decides to change the size of the WebGL canvas.
	UE_EngineRegisterCanvasResizeListener(on_canvas_size_changed);
}


void FHTML5Application::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	GenericApplication::SetMessageHandler(InMessageHandler);
	InputInterface->SetMessageHandler( MessageHandler );
}

void FHTML5Application::PollGameDeviceState( const float TimeDelta )
{
	SDL_Event Event;
	while (SDL_PollEvent(&Event))
	{
		// Tick Input Interface.
		switch (Event.type)
		{
				case SDL_WINDOWEVENT:
				{
					SDL_WindowEvent windowEvent = Event.window;

					switch (windowEvent.event)
					{
					case SDL_WINDOWEVENT_ENTER:
						{
							UE_LOG(LogHTML5Application, Verbose, TEXT("WindowEnter"));
						//	MessageHandler->OnWindowActivationChanged(ApplicationWindow, EWindowActivation::Activate);
							WarmUpTicks = 0;
						}
						break;
					case SDL_WINDOWEVENT_LEAVE:
						{
							UE_LOG(LogHTML5Application, Verbose, TEXT("WindowLeave"));
						 //	MessageHandler->OnWindowActivationChanged(ApplicationWindow, EWindowActivation::Deactivate);
						}
						break;
					case SDL_WINDOWEVENT_FOCUS_GAINED:
						{
							UE_LOG(LogHTML5Application, Verbose, TEXT("WindowFocusGained"));
							MessageHandler->OnCursorSet();
							MessageHandler->OnWindowActivationChanged(ApplicationWindow, EWindowActivation::Activate);
									WarmUpTicks = 0;
						}
						break;
					case SDL_WINDOWEVENT_FOCUS_LOST:
						{
							UE_LOG(LogHTML5Application, Verbose, TEXT("WindowFocusLost"));
							MessageHandler->OnWindowActivationChanged(ApplicationWindow, EWindowActivation::Deactivate);
						}
						break;
					default:
						break;
					}
				}
			default:
			{
				InputInterface->Tick( TimeDelta,Event, ApplicationWindow);
			}
		}
	}
	InputInterface->SendControllerEvents();


	if ( WarmUpTicks >= 0)
		WarmUpTicks ++;


	if ( WarmUpTicks == MaxWarmUpTicks  )
	{
		// browsers don't allow locking and hiding to work independently. use warmup ticks after the application has settled
		// on its mouse lock/visibility status.  This is necessary even in cases where the game doesn't want to locking because
		// the lock status oscillates for few ticks before settling down. This causes a Browser UI pop even when we don't intend to lock.
		// see http://www.w3.org/TR/pointerlock more for information.
		if (((FHTML5Cursor*)Cursor.Get())->LockStatus && !((FHTML5Cursor*)Cursor.Get())->CursorStatus)
		{
			UE_LOG(LogHTML5Application, Verbose, TEXT("Request pointer lock"));
			emscripten_request_pointerlock ( "#canvas" , true);
		}
		else
		{
			UE_LOG(LogHTML5Application, Verbose, TEXT("Exit pointer lock"));
			//emscripten_exit_pointerlock();
		}
		WarmUpTicks = -1;
	}

	// If the WebGL canvas has changed its size, pick up the changes and propagate the viewport
	// resize throughout the engine.
	if (canvas_size_changed)
	{
		canvas_size_changed = false;

		int canvas_w, canvas_h, canvas_fs;
		emscripten_get_canvas_size(&canvas_w, &canvas_h, &canvas_fs);

		FDisplayMetrics DisplayMetrics;
		FDisplayMetrics::GetDisplayMetrics(DisplayMetrics);

		MessageHandler->OnSizeChanged(ApplicationWindow,canvas_w,canvas_h, false);
		MessageHandler->OnResizingWindow(ApplicationWindow);
		BroadcastDisplayMetricsChanged(DisplayMetrics);
	}
}

FPlatformRect FHTML5Application::GetWorkArea( const FPlatformRect& CurrentWindow ) const
{
	return FHTML5Window::GetScreenRect();
}

void FDisplayMetrics::GetDisplayMetrics(FDisplayMetrics& OutDisplayMetrics)
{
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect = FHTML5Window::GetScreenRect();
	OutDisplayMetrics.VirtualDisplayRect    =	OutDisplayMetrics.PrimaryDisplayWorkAreaRect;
	OutDisplayMetrics.PrimaryDisplayWidth   =	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Right;
	OutDisplayMetrics.PrimaryDisplayHeight  =	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom;
	UE_LOG(LogHTML5Application, Verbose, TEXT("GetDisplayMetrics Width:%d, Height:%d"), OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Right, OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom);

	// Apply the debug safe zones
	OutDisplayMetrics.ApplyDefaultSafeZones();
}

TSharedRef< FGenericWindow > FHTML5Application::MakeWindow()
{
	return ApplicationWindow;
}
