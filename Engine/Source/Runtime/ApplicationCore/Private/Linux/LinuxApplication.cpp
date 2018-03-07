// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Linux/LinuxApplication.h"
#include "HAL/PlatformTime.h"
#include "Misc/StringUtility.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/App.h"
#include "Features/IModularFeatures.h"
#include "Linux/LinuxPlatformApplicationMisc.h"
#include "IInputDeviceModule.h"

//
// GameController thresholds
//
#define GAMECONTROLLER_LEFT_THUMB_DEADZONE  7849
#define GAMECONTROLLER_RIGHT_THUMB_DEADZONE 8689
#define GAMECONTROLLER_TRIGGER_THRESHOLD    30

float ShortToNormalFloat(short AxisVal)
{
	// normalize [-32768..32767] -> [-1..1]
	const float Norm = (AxisVal <= 0 ? 32768.f : 32767.f);
	return float(AxisVal) / Norm;
}

FLinuxApplication* LinuxApplication = NULL;

FLinuxApplication* FLinuxApplication::CreateLinuxApplication()
{
	if (!FApp::CanEverRender())	// this assumes that we're running in "headless" mode, and we don't need any kind of multimedia
	{
		return new FLinuxApplication();
	}

	if (!FLinuxPlatformApplicationMisc::InitSDL()) //	will not initialize more than once
	{
		UE_LOG(LogInit, Error, TEXT("FLinuxApplication::CreateLinuxApplication() : InitSDL() failed, cannot create application instance."));
		FLinuxPlatformMisc::RequestExitWithStatus(true, 1);
		// unreachable
		return nullptr;
	}

#if DO_CHECK
	uint32 InitializedSubsystems = SDL_WasInit(SDL_INIT_EVERYTHING);
	check(InitializedSubsystems & SDL_INIT_EVENTS);
	check(InitializedSubsystems & SDL_INIT_JOYSTICK);
	check(InitializedSubsystems & SDL_INIT_GAMECONTROLLER);
#endif // DO_CHECK

	LinuxApplication = new FLinuxApplication();

	int32 ControllerIndex = 0;

	for (int i = 0; i < SDL_NumJoysticks(); ++i)
	{
		if (SDL_IsGameController(i))
		{
			auto Controller = SDL_GameControllerOpen(i);
			if (Controller == nullptr)
			{
				UE_LOG(LogLoad, Warning, TEXT("Could not open gamecontroller %i: %s\n"), i, UTF8_TO_TCHAR(SDL_GetError()) );
			}
			else
			{
				SDL_JoystickID Id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(Controller));
				LinuxApplication->ControllerStates.Add(Id);
				LinuxApplication->ControllerStates[Id].Controller = Controller;
				LinuxApplication->ControllerStates[Id].ControllerIndex = ControllerIndex++;
			}
		}
	}
	return LinuxApplication;
}


FLinuxApplication::FLinuxApplication() 
	:	GenericApplication( MakeShareable( new FLinuxCursor() ) )
	,	bIsMouseCursorLocked(false)
	,	bIsMouseCaptureEnabled(false)
	,	bHasLoadedInputPlugins(false)
	,	bInsideOwnWindow(false)
	,	bIsDragWindowButtonPressed(false)
	,	bActivateApp(false)
	,	bLockToCurrentMouseType(false)
	,	LastTimeCachedDisplays(-1.0)
{
	bUsingHighPrecisionMouseInput = false;
	bAllowedToDeferMessageProcessing = true;
	MouseCaptureWindow = NULL;

	fMouseWheelScrollAccel = 1.0f;
	if (GConfig)
	{
		GConfig->GetFloat(TEXT("X11.Tweaks"), TEXT( "MouseWheelScrollAcceleration" ), fMouseWheelScrollAccel, GEngineIni);
	}
}

FLinuxApplication::~FLinuxApplication()
{
	if ( GConfig )
	{
		GConfig->GetFloat(TEXT("X11.Tweaks"), TEXT("MouseWheelScrollAcceleration"), fMouseWheelScrollAccel, GEngineIni);
		GConfig->Flush(false, GEngineIni);
	}
}

void FLinuxApplication::DestroyApplication()
{
	for(auto ControllerIt = ControllerStates.CreateConstIterator(); ControllerIt; ++ControllerIt)
	{
		if(ControllerIt.Value().Controller != nullptr)
		{
			SDL_GameControllerClose(ControllerIt.Value().Controller);
		}
	}
	ControllerStates.Empty();
}

TSharedRef< FGenericWindow > FLinuxApplication::MakeWindow()
{
	return FLinuxWindow::Make();
}

void FLinuxApplication::InitializeWindow(	const TSharedRef< FGenericWindow >& InWindow,
											const TSharedRef< FGenericWindowDefinition >& InDefinition,
											const TSharedPtr< FGenericWindow >& InParent,
											const bool bShowImmediately )
{
	const TSharedRef< FLinuxWindow > Window = StaticCastSharedRef< FLinuxWindow >( InWindow );
	const TSharedPtr< FLinuxWindow > ParentWindow = StaticCastSharedPtr< FLinuxWindow >( InParent );

	Window->Initialize( this, InDefinition, ParentWindow, bShowImmediately );
	Windows.Add(Window);

	// Add the windows into the focus stack.
	if (Window->IsFocusWhenFirstShown())
	{
		RevertFocusStack.Add(Window);
	}

	// Add the window into the notification list if it is a notification window.
	if(Window->IsNotificationWindow())
	{
		NotificationWindows.Add(Window);
	}
}

void FLinuxApplication::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	GenericApplication::SetMessageHandler(InMessageHandler);
}

namespace
{
	TSharedPtr< FLinuxWindow > FindWindowBySDLWindow(const TArray< TSharedRef< FLinuxWindow > >& WindowsToSearch, SDL_HWindow const WindowHandle)
	{
		for (int32 WindowIndex=0; WindowIndex < WindowsToSearch.Num(); ++WindowIndex)
		{
			TSharedRef< FLinuxWindow > Window = WindowsToSearch[WindowIndex];
			if (Window->GetHWnd() == WindowHandle)
			{
				return Window;
			}
		}

		return TSharedPtr< FLinuxWindow >(nullptr);
	}
}

TSharedPtr< FLinuxWindow > FLinuxApplication::FindWindowBySDLWindow(SDL_Window *win)
{
	return ::FindWindowBySDLWindow(Windows, win);
}

void FLinuxApplication::PumpMessages( const float TimeDelta )
{
	FPlatformApplicationMisc::PumpMessages( true );
}

bool FLinuxApplication::IsCursorDirectlyOverSlateWindow() const
{
	return bInsideOwnWindow;
}

void FLinuxApplication::AddPendingEvent( SDL_Event SDLEvent )
{
	if( GPumpingMessagesOutsideOfMainLoop && bAllowedToDeferMessageProcessing )
	{
		PendingEvents.Add( SDLEvent );
	}
	else
	{
		// When not deferring messages, process them immediately
		ProcessDeferredMessage( SDLEvent );
	}
}

bool FLinuxApplication::GeneratesKeyCharMessage(const SDL_KeyboardEvent & KeyDownEvent)
{
	bool bCmdKeyPressed = (KeyDownEvent.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL)) != 0;
	const SDL_Keycode Sym = KeyDownEvent.keysym.sym;

	// filter out command keys, non-ASCI and arrow keycodes that don't generate WM_CHAR under Windows (TODO: find a table?)
	return !bCmdKeyPressed && Sym < 128 &&
		(Sym != SDLK_DOWN && Sym != SDLK_LEFT && Sym != SDLK_RIGHT && Sym != SDLK_UP && Sym != SDLK_DELETE);
}

static inline uint32 CharCodeFromSDLKeySym(const SDL_Keycode KeySym)
{
	if ((KeySym & SDLK_SCANCODE_MASK) != 0)
	{
		return 0;
	}
	else if (KeySym == SDLK_DELETE)  // this doesn't use the scancode mask for some reason.
	{
		return 0;
	}
	return (uint32) KeySym;
}

void FLinuxApplication::ProcessDeferredMessage( SDL_Event Event )
{
	// This function can be reentered when entering a modal tick loop.
	// We need to make a copy of the events that need to be processed or we may end up processing the same messages twice
	SDL_HWindow NativeWindow = NULL;

	// get pointer to window that received this event
	bool bWindowlessEvent = false;
	TSharedPtr< FLinuxWindow > CurrentEventWindow = FindEventWindow(&Event, bWindowlessEvent);

	if (CurrentEventWindow.IsValid())
	{
		NativeWindow = CurrentEventWindow->GetHWnd();
	}
	if (!NativeWindow && !bWindowlessEvent)
	{
		return;
	}

	switch(Event.type)
	{
	case SDL_KEYDOWN:
		{
			const SDL_KeyboardEvent &KeyEvent = Event.key;
			const SDL_Keycode KeySym = KeyEvent.keysym.sym;
			const uint32 CharCode = CharCodeFromSDLKeySym(KeySym);
			const bool bIsRepeated = KeyEvent.repeat != 0;

			// Text input is now handled in SDL_TEXTINPUT: see below
			MessageHandler->OnKeyDown(KeySym, CharCode, bIsRepeated);

			// Backspace input in only caught here.
			if (KeySym == SDLK_BACKSPACE)
			{
				MessageHandler->OnKeyChar('\b', bIsRepeated);
			}
			else if (KeySym == SDLK_RETURN)
			{
				MessageHandler->OnKeyChar('\r', bIsRepeated);
			}
		}
		break;
	case SDL_KEYUP:
		{
			const SDL_KeyboardEvent &KeyEvent = Event.key;
			const SDL_Keycode KeySym = KeyEvent.keysym.sym;
			const uint32 CharCode = CharCodeFromSDLKeySym(KeySym);
			const bool IsRepeat = KeyEvent.repeat != 0;

			MessageHandler->OnKeyUp( KeySym, CharCode, IsRepeat );
		}
		break;
	case SDL_TEXTINPUT:
		{
			// Slate now gets all its text from here, I hope.
			const bool bIsRepeated = false;  //Event.key.repeat != 0;
			const FString TextStr(UTF8_TO_TCHAR(Event.text.text));
			for (auto TextIter = TextStr.CreateConstIterator(); TextIter; ++TextIter)
			{
				MessageHandler->OnKeyChar(*TextIter, bIsRepeated);
			}
		}
		break;
	case SDL_MOUSEMOTION:
		{
			SDL_MouseMotionEvent motionEvent = Event.motion;
			FLinuxCursor *LinuxCursor = (FLinuxCursor*)Cursor.Get();
			LinuxCursor->InvalidateCaches();

			if (LinuxCursor->IsHidden())
			{
				// Check if the mouse got locked for dragging in viewport.
				if (bLockToCurrentMouseType == false)
				{
					int width, height;
					if(bIsMouseCursorLocked && CurrentClipWindow.IsValid())
					{
						NativeWindow =  CurrentClipWindow->GetHWnd();
					}
					
					SDL_GetWindowSize(NativeWindow, &width, &height);
					if (motionEvent.x != (width / 2) || motionEvent.y != (height / 2))
					{
						int xOffset, yOffset;
						GetWindowPositionInEventLoop(NativeWindow, &xOffset, &yOffset);
						LinuxCursor->SetPosition(width / 2 + xOffset, height / 2 + yOffset);
					}
					else
					{
						break;
					}
				}
			}
			else
			{
				int xOffset, yOffset;
				GetWindowPositionInEventLoop(NativeWindow, &xOffset, &yOffset);

				int32 BorderSizeX, BorderSizeY;
				CurrentEventWindow->GetNativeBordersSize(BorderSizeX, BorderSizeY);

				LinuxCursor->SetCachedPosition(motionEvent.x + xOffset + BorderSizeX, motionEvent.y + yOffset + BorderSizeY);

				FVector2D CurrentPosition = LinuxCursor->GetPosition();
				if( LinuxCursor->UpdateCursorClipping( CurrentPosition ) )
				{
					LinuxCursor->SetPosition( CurrentPosition.X, CurrentPosition.Y );
				}
				if( !CurrentEventWindow->GetDefinition().HasOSWindowBorder )
				{
					if ( CurrentEventWindow->IsRegularWindow() )
					{
						MessageHandler->GetWindowZoneForPoint( CurrentEventWindow.ToSharedRef(), CurrentPosition.X - xOffset, CurrentPosition.Y - yOffset );
						MessageHandler->OnCursorSet();
					}
				}
			}

			if(bUsingHighPrecisionMouseInput)
			{
				// hack to work around jumps (only matters when we have more than one window)
				const int kTooFarAway = 250;
				const int kTooFarAwaySquare = kTooFarAway * kTooFarAway;
				if (Windows.Num() > 1 && motionEvent.xrel * motionEvent.xrel + motionEvent.yrel * motionEvent.yrel > kTooFarAwaySquare)
				{
					UE_LOG(LogLinuxWindowEvent, Warning, TEXT("Suppressing too large relative mouse movement due to an apparent bug (%d, %d is larger than threshold %d)"),
						motionEvent.xrel, motionEvent.yrel,
						kTooFarAway
						);
				}
				else
				{
					MessageHandler->OnRawMouseMove(motionEvent.xrel, motionEvent.yrel);
				}
			}
			else
			{
				MessageHandler->OnMouseMove();
			}
		}
		break;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		{
			SDL_MouseButtonEvent buttonEvent = Event.button;

			EMouseButtons::Type button;
			switch(buttonEvent.button)
			{
			case SDL_BUTTON_LEFT:
				button = EMouseButtons::Left;
				break;
			case SDL_BUTTON_MIDDLE:
				button = EMouseButtons::Middle;
				break;
			case SDL_BUTTON_RIGHT:
				button = EMouseButtons::Right;
				break;
			case SDL_BUTTON_X1:
				button = EMouseButtons::Thumb01;
				break;
			case SDL_BUTTON_X2:
				button = EMouseButtons::Thumb02;
				break;
			default:
				button = EMouseButtons::Invalid;
				break;
			}
			
			if (buttonEvent.type == SDL_MOUSEBUTTONUP)
			{
				MessageHandler->OnMouseUp(button);
				
				if (buttonEvent.button == SDL_BUTTON_LEFT)
				{
					// Unlock the mouse dragging type.
					bLockToCurrentMouseType = false;

					bIsDragWindowButtonPressed = false;
				}
			}
			else
			{
				// User clicked any button. Is the application active? If not activate it.
				if (!bActivateApp)
				{
					ActivateApplication();
				}

				if (buttonEvent.button == SDL_BUTTON_LEFT)
				{
					// The user clicked an object and wants to drag maybe. We can use that to disable 
					// the resetting of the cursor. Before the user can drag objects, the pointer will change.
					// Usually it will be EMouseCursor::CardinalCross (Default added after IRC discussion how to fix selection in Front/Top/Side views). 
					// If that happends and the user clicks the left mouse button, we know they want to move something.
					// TODO Is this always true? Need more checks.
					if (((FLinuxCursor*)Cursor.Get())->GetType() != EMouseCursor::None)
					{
						bLockToCurrentMouseType = true;
					}

					bIsDragWindowButtonPressed = true;
				}

				if (buttonEvent.clicks == 2)
				{
					MessageHandler->OnMouseDoubleClick(CurrentEventWindow, button);
				}
				else
				{
					// Check if we have to activate the window.
					if (CurrentlyActiveWindow != CurrentEventWindow)
					{
						ActivateWindow(CurrentEventWindow);
						
						if(NotificationWindows.Num() > 0)
						{
							RaiseNotificationWindows(CurrentEventWindow);
						}
					}

					// Check if we have to set the focus.
					if(CurrentFocusWindow != CurrentEventWindow)
					{
						SDL_RaiseWindow(CurrentEventWindow->GetHWnd());
						if(CurrentEventWindow->IsPopupMenuWindow())
						{
							SDL_SetKeyboardGrab(CurrentEventWindow->GetHWnd(), SDL_TRUE);
						}
						else
						{
							SDL_SetWindowInputFocus(CurrentEventWindow->GetHWnd());
						}
					}

					MessageHandler->OnMouseDown(CurrentEventWindow, button);
				}
			}
		}
		break;
	case SDL_MOUSEWHEEL:
		{
			SDL_MouseWheelEvent *WheelEvent = &Event.wheel;
			float Amount = WheelEvent->y * fMouseWheelScrollAccel;

			MessageHandler->OnMouseWheel(Amount);
		}
		break;
	case SDL_CONTROLLERAXISMOTION:
		{
			SDL_ControllerAxisEvent caxisEvent = Event.caxis;
			FGamepadKeyNames::Type Axis = FGamepadKeyNames::Invalid;
			float AxisValue = ShortToNormalFloat(caxisEvent.value);

			if (!ControllerStates.Contains(caxisEvent.which))
			{
				break;
			}

			SDLControllerState &ControllerState = ControllerStates[caxisEvent.which];

			switch (caxisEvent.axis)
			{
			case SDL_CONTROLLER_AXIS_LEFTX:
				Axis = FGamepadKeyNames::LeftAnalogX;
				if(caxisEvent.value > GAMECONTROLLER_LEFT_THUMB_DEADZONE)
				{
					if(!ControllerState.AnalogOverThreshold[0])
					{
						MessageHandler->OnControllerButtonPressed(FGamepadKeyNames::LeftStickRight, ControllerState.ControllerIndex, false);
						ControllerState.AnalogOverThreshold[0] = true;
					}
				}
				else if(ControllerState.AnalogOverThreshold[0])
				{
					MessageHandler->OnControllerButtonReleased(FGamepadKeyNames::LeftStickRight, ControllerState.ControllerIndex, false);
					ControllerState.AnalogOverThreshold[0] = false;
				}
				if(caxisEvent.value < -GAMECONTROLLER_LEFT_THUMB_DEADZONE)
				{
					if(!ControllerState.AnalogOverThreshold[1])
					{
						MessageHandler->OnControllerButtonPressed(FGamepadKeyNames::LeftStickLeft, ControllerState.ControllerIndex, false);
						ControllerState.AnalogOverThreshold[1] = true;
					}
				}
				else if(ControllerState.AnalogOverThreshold[1])
				{
					MessageHandler->OnControllerButtonReleased(FGamepadKeyNames::LeftStickLeft, ControllerState.ControllerIndex, false);
					ControllerState.AnalogOverThreshold[1] = false;
				}
				break;
			case SDL_CONTROLLER_AXIS_LEFTY:
				Axis = FGamepadKeyNames::LeftAnalogY;
				AxisValue *= -1;
				if(caxisEvent.value > GAMECONTROLLER_LEFT_THUMB_DEADZONE)
				{
					if(!ControllerState.AnalogOverThreshold[2])
					{
						MessageHandler->OnControllerButtonPressed(FGamepadKeyNames::LeftStickDown, ControllerState.ControllerIndex, false);
						ControllerState.AnalogOverThreshold[2] = true;
					}
				}
				else if(ControllerState.AnalogOverThreshold[2])
				{
					MessageHandler->OnControllerButtonReleased(FGamepadKeyNames::LeftStickDown, ControllerState.ControllerIndex, false);
					ControllerState.AnalogOverThreshold[2] = false;
				}
				if(caxisEvent.value < -GAMECONTROLLER_LEFT_THUMB_DEADZONE)
				{
					if(!ControllerState.AnalogOverThreshold[3])
					{
						MessageHandler->OnControllerButtonPressed(FGamepadKeyNames::LeftStickUp, ControllerState.ControllerIndex, false);
						ControllerState.AnalogOverThreshold[3] = true;
					}
				}
				else if(ControllerState.AnalogOverThreshold[3])
				{
					MessageHandler->OnControllerButtonReleased(FGamepadKeyNames::LeftStickUp, ControllerState.ControllerIndex, false);
					ControllerState.AnalogOverThreshold[3] = false;
				}
				break;
			case SDL_CONTROLLER_AXIS_RIGHTX:
				Axis = FGamepadKeyNames::RightAnalogX;
				if(caxisEvent.value > GAMECONTROLLER_RIGHT_THUMB_DEADZONE)
				{
					if(!ControllerState.AnalogOverThreshold[4])
					{
						MessageHandler->OnControllerButtonPressed(FGamepadKeyNames::RightStickRight, ControllerState.ControllerIndex, false);
						ControllerState.AnalogOverThreshold[4] = true;
					}
				}
				else if(ControllerState.AnalogOverThreshold[4])
				{
					MessageHandler->OnControllerButtonReleased(FGamepadKeyNames::RightStickRight, ControllerState.ControllerIndex, false);
					ControllerState.AnalogOverThreshold[4] = false;
				}
				if(caxisEvent.value < -GAMECONTROLLER_RIGHT_THUMB_DEADZONE)
				{
					if(!ControllerState.AnalogOverThreshold[5])
					{
						MessageHandler->OnControllerButtonPressed(FGamepadKeyNames::RightStickLeft, ControllerState.ControllerIndex, false);
						ControllerState.AnalogOverThreshold[5] = true;
					}
				}
				else if(ControllerState.AnalogOverThreshold[5])
				{
					MessageHandler->OnControllerButtonReleased(FGamepadKeyNames::RightStickLeft, ControllerState.ControllerIndex, false);
					ControllerState.AnalogOverThreshold[5] = false;
				}
				break;
			case SDL_CONTROLLER_AXIS_RIGHTY:
				Axis = FGamepadKeyNames::RightAnalogY;
				AxisValue *= -1;
				if(caxisEvent.value > GAMECONTROLLER_RIGHT_THUMB_DEADZONE)
				{
					if(!ControllerState.AnalogOverThreshold[6])
					{
						MessageHandler->OnControllerButtonPressed(FGamepadKeyNames::RightStickDown, ControllerState.ControllerIndex, false);
						ControllerState.AnalogOverThreshold[6] = true;
					}
				}
				else if(ControllerState.AnalogOverThreshold[6])
				{
					MessageHandler->OnControllerButtonReleased(FGamepadKeyNames::RightStickDown, ControllerState.ControllerIndex, false);
					ControllerState.AnalogOverThreshold[6] = false;
				}
				if(caxisEvent.value < -GAMECONTROLLER_RIGHT_THUMB_DEADZONE)
				{
					if(!ControllerState.AnalogOverThreshold[7])
					{
						MessageHandler->OnControllerButtonPressed(FGamepadKeyNames::RightStickUp, ControllerState.ControllerIndex, false);
						ControllerState.AnalogOverThreshold[7] = true;
					}
				}
				else if(ControllerState.AnalogOverThreshold[7])
				{
					MessageHandler->OnControllerButtonReleased(FGamepadKeyNames::RightStickUp, ControllerState.ControllerIndex, false);
					ControllerState.AnalogOverThreshold[7] = false;
				}
				break;
			case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
				Axis = FGamepadKeyNames::LeftTriggerAnalog;
				if(caxisEvent.value > GAMECONTROLLER_TRIGGER_THRESHOLD)
				{
					if(!ControllerState.AnalogOverThreshold[8])
					{
						MessageHandler->OnControllerButtonPressed(FGamepadKeyNames::LeftTriggerThreshold, ControllerState.ControllerIndex, false);
						ControllerState.AnalogOverThreshold[8] = true;
					}
				}
				else if(ControllerState.AnalogOverThreshold[8])
				{
					MessageHandler->OnControllerButtonReleased(FGamepadKeyNames::LeftTriggerThreshold, ControllerState.ControllerIndex, false);
					ControllerState.AnalogOverThreshold[8] = false;
				}
				break;
			case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
				Axis = FGamepadKeyNames::RightTriggerAnalog;
				if(caxisEvent.value > GAMECONTROLLER_TRIGGER_THRESHOLD)
				{
					if(!ControllerState.AnalogOverThreshold[9])
					{
						MessageHandler->OnControllerButtonPressed(FGamepadKeyNames::RightTriggerThreshold, ControllerState.ControllerIndex, false);
						ControllerState.AnalogOverThreshold[9] = true;
					}
				}
				else if(ControllerState.AnalogOverThreshold[9])
				{
					MessageHandler->OnControllerButtonReleased(FGamepadKeyNames::RightTriggerThreshold, ControllerState.ControllerIndex, false);
					ControllerState.AnalogOverThreshold[9] = false;
				}
				break;
			default:
				break;
			}

			if (Axis != FGamepadKeyNames::Invalid)
			{
				float & ExistingAxisEventValue = ControllerState.AxisEvents.FindOrAdd(Axis);
				ExistingAxisEventValue = AxisValue;
			}
		}
		break;
	case SDL_CONTROLLERBUTTONDOWN:
	case SDL_CONTROLLERBUTTONUP:
		{
			SDL_ControllerButtonEvent cbuttonEvent = Event.cbutton;
			FGamepadKeyNames::Type Button = FGamepadKeyNames::Invalid;

			if (!ControllerStates.Contains(cbuttonEvent.which))
			{
				break;
			}

			switch (cbuttonEvent.button)
			{
			case SDL_CONTROLLER_BUTTON_A:
				Button = FGamepadKeyNames::FaceButtonBottom;
				break;
			case SDL_CONTROLLER_BUTTON_B:
				Button = FGamepadKeyNames::FaceButtonRight;
				break;
			case SDL_CONTROLLER_BUTTON_X:
				Button = FGamepadKeyNames::FaceButtonLeft;
				break;
			case SDL_CONTROLLER_BUTTON_Y:
				Button = FGamepadKeyNames::FaceButtonTop;
				break;
			case SDL_CONTROLLER_BUTTON_BACK:
				Button = FGamepadKeyNames::SpecialLeft;
				break;
			case SDL_CONTROLLER_BUTTON_START:
				Button = FGamepadKeyNames::SpecialRight;
				break;
			case SDL_CONTROLLER_BUTTON_LEFTSTICK:
				Button = FGamepadKeyNames::LeftStickDown;
				break;
			case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
				Button = FGamepadKeyNames::RightStickDown;
				break;
			case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
				Button = FGamepadKeyNames::LeftShoulder;
				break;
			case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
				Button = FGamepadKeyNames::RightShoulder;
				break;
			case SDL_CONTROLLER_BUTTON_DPAD_UP:
				Button = FGamepadKeyNames::DPadUp;
				break;
			case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
				Button = FGamepadKeyNames::DPadDown;
				break;
			case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
				Button = FGamepadKeyNames::DPadLeft;
				break;
			case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
				Button = FGamepadKeyNames::DPadRight;
				break;
			default:
				break;
			}

			if (Button != FGamepadKeyNames::Invalid)
			{
				if(cbuttonEvent.type == SDL_CONTROLLERBUTTONDOWN)
				{
					MessageHandler->OnControllerButtonPressed(Button, ControllerStates[cbuttonEvent.which].ControllerIndex, false);
				}
				else
				{
					MessageHandler->OnControllerButtonReleased(Button, ControllerStates[cbuttonEvent.which].ControllerIndex, false);
				}
			}
		}
		break;

	case SDL_WINDOWEVENT:
		{
			SDL_WindowEvent windowEvent = Event.window;

			switch (windowEvent.event)
			{
				case SDL_WINDOWEVENT_SIZE_CHANGED:
					{
						int NewWidth  = windowEvent.data1;
						int NewHeight = windowEvent.data2;

						MessageHandler->OnSizeChanged(
							CurrentEventWindow.ToSharedRef(),
							NewWidth,
							NewHeight,
							//	bWasMinimized
							false
						);
					}
					break;

				case SDL_WINDOWEVENT_RESIZED:
					{
						MessageHandler->OnResizingWindow( CurrentEventWindow.ToSharedRef() );
					}
					break;

				case SDL_WINDOWEVENT_CLOSE:
					{
						MessageHandler->OnWindowClose( CurrentEventWindow.ToSharedRef() );
					}
					break;

				case SDL_WINDOWEVENT_SHOWN:
					{
						// (re)cache native properties
						CurrentEventWindow->CacheNativeProperties();

						// A window did show up. Is the whole Application active? If not first activate it (ignore tooltips).
						if (!bActivateApp && !CurrentEventWindow->IsTooltipWindow())
						{
							ActivateApplication();
						}

						// Check if this window is different then the currently active one. If it is another one
						// activate that window and if necessary deactivate the one which was active.
						if (CurrentlyActiveWindow != CurrentEventWindow && CurrentEventWindow->GetActivationPolicy() != EWindowActivationPolicy::Never)
						{
							ActivateWindow(CurrentEventWindow);
						}

						// Set focus if the window wants to have a focus when first shown.
						if (CurrentEventWindow->IsFocusWhenFirstShown())
						{
							if (CurrentEventWindow->IsPopupMenuWindow())
							{
								// We use grab here because this seems to be a proper way to set focus to an override-redirect window.
								// This prevents additional window changed highlighting in some WMs.
								SDL_SetKeyboardGrab(CurrentEventWindow->GetHWnd(), SDL_TRUE);
							}
							else
							{
								SDL_SetWindowInputFocus(CurrentEventWindow->GetHWnd());
							}
						}
					}
					break;

				case SDL_WINDOWEVENT_MOVED:
					{
						int32 ClientScreenX = windowEvent.data1;
						int32 ClientScreenY = windowEvent.data2;

						int32 BorderSizeX, BorderSizeY;
						CurrentEventWindow->GetNativeBordersSize(BorderSizeX, BorderSizeY);
						ClientScreenX += BorderSizeX;
						ClientScreenY += BorderSizeY;

						MessageHandler->OnMovedWindow(CurrentEventWindow.ToSharedRef(), ClientScreenX, ClientScreenY);
					}
					break;

				case SDL_WINDOWEVENT_MAXIMIZED:
					{
						MessageHandler->OnWindowAction(CurrentEventWindow.ToSharedRef(), EWindowAction::Maximize);
						UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("Window: '%d' got maximized"), CurrentEventWindow->GetID());
					}
					break;

				case SDL_WINDOWEVENT_RESTORED:
					{
						MessageHandler->OnWindowAction(CurrentEventWindow.ToSharedRef(), EWindowAction::Restore);
						UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("Window: '%d' got restored"), CurrentEventWindow->GetID());
					}
					break;

				case SDL_WINDOWEVENT_ENTER:
					{
						if (CurrentEventWindow.IsValid())
						{
							MessageHandler->OnCursorSet();

							bInsideOwnWindow = true;
							UE_LOG(LogLinuxWindow, Verbose, TEXT("Entered one of application windows"));
						}
					}
					break;

				case SDL_WINDOWEVENT_LEAVE:
					{
						if (CurrentEventWindow.IsValid())
						{
							if (GetCapture() != nullptr)
							{
								UpdateMouseCaptureWindow((SDL_HWindow)GetCapture());
							}

							bInsideOwnWindow = false;
							UE_LOG(LogLinuxWindow, Verbose, TEXT("Left an application window we were hovering above."));
						}
					}
					break;

				case SDL_WINDOWEVENT_HIT_TEST:
					{
						// The user clicked into the hit test area (Titlebar for example). Is the whole Application active?
						// If not, first activate (ignore tooltips).
						if (!bActivateApp && !CurrentEventWindow->IsTooltipWindow())
						{
							ActivateApplication();
						}

						// Check if this window is different then the currently active one. If it is another one activate this 
						// window and deactivate the other one.
						if (CurrentlyActiveWindow != CurrentEventWindow)
						{
							ActivateWindow(CurrentEventWindow);
						}

						// Set the input focus.
						if (CurrentEventWindow.IsValid())
						{
							SDL_SetWindowInputFocus(CurrentEventWindow->GetHWnd());
						}

						if (NotificationWindows.Num() > 0)
						{
							RaiseNotificationWindows(CurrentEventWindow);
						}
					}
					break;

				case SDL_WINDOWEVENT_TAKE_FOCUS:
					{
						if (!bActivateApp)
						{
							ActivateApplication();
						}

						// Some windows like notification windows may popup without needing the focus. That is handled in the SDL_WINDOWEVENT_SHOWN case.
						// The problem would be that the WM will send the Take Focus event and wants to set the focus. We don't want it to set it
						// for notifications because they are already handled in the above mentioned event.
						if ((CurrentFocusWindow != CurrentEventWindow) && !CurrentEventWindow->IsNotificationWindow())
						{
							SDL_SetWindowInputFocus(CurrentEventWindow->GetHWnd());
						}
					}
					break;

				case SDL_WINDOWEVENT_FOCUS_GAINED:
					{
						UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("WM_SETFOCUS                                 : %d"), CurrentEventWindow->GetID());

						CurrentFocusWindow = CurrentEventWindow;
					}
					break;

				case SDL_WINDOWEVENT_FOCUS_LOST:
					{
						UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("WM_KILLFOCUS                                : %d"), CurrentEventWindow->GetID());

						// OK, the active window lost focus. This could mean the app went completely out of
						// focus. That means the app must be deactivated. To make sure that the user did
						// not click to another window we delay the deactivation.
						// TODO Figure out if the delay time may cause problems.
						if(CurrentFocusWindow == CurrentEventWindow)
						{
							// Only do if the application is active.
							if(bActivateApp)
							{
								SDL_Event event;
								event.type = SDL_USEREVENT;
								event.user.code = CheckForDeactivation;
								SDL_PushEvent(&event);
							}
						}
						CurrentFocusWindow = nullptr;
					}
					break;
				case SDL_WINDOWEVENT_HIDDEN:	// intended fall-through
				case SDL_WINDOWEVENT_EXPOSED:	// intended fall-through
				case SDL_WINDOWEVENT_MINIMIZED:	// intended fall-through
				default:
					break;
			}
		}
		break;

	case SDL_DROPBEGIN:
		{
			check(DragAndDropQueue.Num() == 0);  // did we get confused?
			check(DragAndDropTextQueue.Num() == 0);  // did we get confused?
		}
		break;

	case SDL_DROPFILE:
		{
			FString tmp = StringUtility::UnescapeURI(UTF8_TO_TCHAR(Event.drop.file));
			DragAndDropQueue.Add(tmp);
			SDL_free(Event.drop.file);
			UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("File dropped: %s"), *tmp);
		}
		break;

	case SDL_DROPTEXT:
		{
			FString tmp = UTF8_TO_TCHAR(Event.drop.file);
			DragAndDropTextQueue.Add(tmp);
			SDL_free(Event.drop.file);
			UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("Text dropped: %s"), *tmp);
		}
		break;

	case SDL_DROPCOMPLETE:
		{
			if (DragAndDropQueue.Num() > 0)
			{
				MessageHandler->OnDragEnterFiles(CurrentEventWindow.ToSharedRef(), DragAndDropQueue);
				MessageHandler->OnDragDrop(CurrentEventWindow.ToSharedRef());
				DragAndDropQueue.Empty();
			}

			if (DragAndDropTextQueue.Num() > 0)
			{
				for (const auto & Text : DragAndDropTextQueue)
				{
					MessageHandler->OnDragEnterText(CurrentEventWindow.ToSharedRef(), Text);
					MessageHandler->OnDragDrop(CurrentEventWindow.ToSharedRef());
				}
				DragAndDropTextQueue.Empty();
			}
			UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("DragAndDrop finished for Window              : %d"), CurrentEventWindow->GetID());
		}
		break;

	case SDL_USEREVENT:
		{
			if(Event.user.code == CheckForDeactivation)
			{
				// If we don't use bIsDragWindowButtonPressed the draged window will be destroyed because we
				// deactivate the whole appliacton. TODO Is that a bug? Do we have to do something?
				if (!CurrentFocusWindow.IsValid() && !bIsDragWindowButtonPressed)
				{
					DeactivateApplication();
				}
			}
		}
		break;

	case SDL_FINGERDOWN:
		{
			// touch events can have no window associated with them, in that case ignore (with a warning)
			if (LIKELY(!bWindowlessEvent))
			{
				int xOffset, yOffset;
				GetWindowPositionInEventLoop(NativeWindow, &xOffset, &yOffset);
				FVector2D Offset(static_cast<float>(xOffset), static_cast<float>(yOffset));

				// remove touch context even if it existed
				uint64 FingerId = static_cast<uint64>(Event.tfinger.fingerId);
				if (UNLIKELY(Touches.Find(FingerId) != nullptr))
				{
					Touches.Remove(FingerId);
					UE_LOG(LogLinuxWindow, Warning, TEXT("Received another SDL_FINGERDOWN for finger %llu which was already down."), FingerId);
				}

				FTouchContext NewTouch;
				NewTouch.TouchIndex = Touches.Num();
				NewTouch.Location = GetTouchEventLocation(Event) + Offset;
				NewTouch.DeviceId = Event.tfinger.touchId;
				Touches.Add(FingerId, NewTouch);

				UE_LOG(LogLinuxWindow, Verbose, TEXT("OnTouchStarted at (%f, %f), finger %d (system touch id %llu)"), NewTouch.Location.X, NewTouch.Location.Y, NewTouch.TouchIndex, FingerId);
				MessageHandler->OnTouchStarted(CurrentEventWindow, NewTouch.Location, NewTouch.TouchIndex, 0);// NewTouch.DeviceId);
			}
			else
			{
				UE_LOG(LogLinuxWindow, Warning, TEXT("Ignoring touch event SDL_FINGERDOWN (finger: %llu, x=%f, y=%f) that doesn't have a window associated with it"),
					Event.tfinger.fingerId, Event.tfinger.x, Event.tfinger.y);
			}
		}
		break;
	case SDL_FINGERUP:
		{
			UE_LOG(LogLinuxWindow, Verbose, TEXT("Finger %llu is up at (%f, %f)"), Event.tfinger.fingerId, Event.tfinger.x, Event.tfinger.y);

			// touch events can have no window associated with them, in that case ignore (with a warning)
			if (LIKELY(!bWindowlessEvent))
			{
				int xOffset, yOffset;
				GetWindowPositionInEventLoop(NativeWindow, &xOffset, &yOffset);
				FVector2D Offset(static_cast<float>(xOffset), static_cast<float>(yOffset));

				uint64 FingerId = static_cast<uint64>(Event.tfinger.fingerId);
				FTouchContext* TouchContext = Touches.Find(FingerId);
				if (UNLIKELY(TouchContext == nullptr))
				{
					UE_LOG(LogLinuxWindow, Warning, TEXT("Received SDL_FINGERUP for finger %llu which was already up."), FingerId);
					// do not send a duplicate up
				}
				else
				{
					TouchContext->Location = GetTouchEventLocation(Event) + Offset;
					// check touch device?

					UE_LOG(LogLinuxWindow, Verbose, TEXT("OnTouchEnded at (%f, %f), finger %d (system touch id %llu)"), TouchContext->Location.X, TouchContext->Location.Y, TouchContext->TouchIndex, FingerId);
					MessageHandler->OnTouchEnded(TouchContext->Location, TouchContext->TouchIndex, 0);// TouchContext->DeviceId);

					// remove the touch
					Touches.Remove(FingerId);
				}
			}
			else
			{
				UE_LOG(LogLinuxWindow, Warning, TEXT("Ignoring touch event SDL_FINGERUP (finger: %llu, x=%f, y=%f) that doesn't have a window associated with it"),
					Event.tfinger.fingerId, Event.tfinger.x, Event.tfinger.y);
			}
		}
		break;
	case SDL_FINGERMOTION:
		{
			// touch events can have no window associated with them, in that case ignore (with a warning)
			if (LIKELY(!bWindowlessEvent))
			{
				int xOffset, yOffset;
				GetWindowPositionInEventLoop(NativeWindow, &xOffset, &yOffset);
				FVector2D Offset(static_cast<float>(xOffset), static_cast<float>(yOffset));

				uint64 FingerId = static_cast<uint64>(Event.tfinger.fingerId);
				FTouchContext* TouchContext = Touches.Find(FingerId);
				if (UNLIKELY(TouchContext == nullptr))
				{
					UE_LOG(LogLinuxWindow, Warning, TEXT("Received SDL_FINGERMOTION for finger %llu which was not down."), FingerId);
					// ignore the event
				}
				else
				{
					// do not send moved event if position has not changed
					FVector2D Location = GetTouchEventLocation(Event) + Offset;
					if (LIKELY((Location - TouchContext->Location).IsNearlyZero() == false))
					{
						TouchContext->Location = Location;
						UE_LOG(LogLinuxWindow, Verbose, TEXT("OnTouchMoved at (%f, %f), finger %d (system touch id %llu)"), TouchContext->Location.X, TouchContext->Location.Y, TouchContext->TouchIndex, FingerId);
						MessageHandler->OnTouchMoved(TouchContext->Location, TouchContext->TouchIndex, 0);// TouchContext->DeviceId);
					}
				}
			}
			else
			{
				UE_LOG(LogLinuxWindow, Warning, TEXT("Ignoring touch event SDL_FINGERMOTION (finger: %llu, x=%f, y=%f) that doesn't have a window associated with it"),
					Event.tfinger.fingerId, Event.tfinger.x, Event.tfinger.y);
			}
		}
		break;

	default:
		UE_LOG(LogLinuxWindow, Verbose, TEXT("Received unknown SDL event type=%d"), Event.type);
		break;
	}
}

FVector2D FLinuxApplication::GetTouchEventLocation(SDL_Event TouchEvent)
{
	checkf(TouchEvent.type == SDL_FINGERDOWN || TouchEvent.type == SDL_FINGERUP || TouchEvent.type == SDL_FINGERMOTION, TEXT("Wrong touch event."));
	// contrary to SDL2 documentation, the coordinates received from touchscreen monitors are screen space (window space to be more correct)
	return FVector2D(TouchEvent.tfinger.x, TouchEvent.tfinger.y);
}


EWindowZone::Type FLinuxApplication::WindowHitTest(const TSharedPtr< FLinuxWindow > &Window, int x, int y)
{
	return MessageHandler->GetWindowZoneForPoint(Window.ToSharedRef(), x, y);
}

void FLinuxApplication::ProcessDeferredEvents( const float TimeDelta )
{
	// delete pending destroy windows before, and not after processing events, to prolong their lifetime
	DestroyPendingWindows();

	// This function can be reentered when entering a modal tick loop.
	// We need to make a copy of the events that need to be processed or we may end up processing the same messages twice
	SDL_HWindow NativeWindow = NULL;

	TArray< SDL_Event > Events( PendingEvents );
	PendingEvents.Empty();

	for( int32 Index = 0; Index < Events.Num(); ++Index )
	{
		ProcessDeferredMessage( Events[Index] );
	}
}

void FLinuxApplication::DestroyPendingWindows()
{
	if (UNLIKELY(PendingDestroyWindows.Num()))
	{
		// destroy native windows that we deferred
		const double Now = FPlatformTime::Seconds();
		for(TMap<SDL_HWindow, double>::TIterator It(PendingDestroyWindows); It; ++It)
		{
			if (Now > It.Value())
			{
				SDL_HWindow Window = It.Key();
				UE_LOG(LogLinuxWindow, Verbose, TEXT("Destroying SDL window %p"), Window);
				SDL_DestroyWindow(Window);
				It.RemoveCurrent();
			}
		}
	}
}

void FLinuxApplication::PollGameDeviceState( const float TimeDelta )
{
	for(auto ControllerIt = ControllerStates.CreateIterator(); ControllerIt; ++ControllerIt)
	{
		for(auto Event = ControllerIt.Value().AxisEvents.CreateConstIterator(); Event; ++Event)
		{
			MessageHandler->OnControllerAnalog(Event.Key(), ControllerIt.Value().ControllerIndex, Event.Value());
		}
		ControllerIt.Value().AxisEvents.Empty();
	}

	// initialize any externally-implemented input devices (we delay load initialize the array so any plugins have had time to load)
	if (!bHasLoadedInputPlugins)
	{
		TArray<IInputDeviceModule*> PluginImplementations = IModularFeatures::Get().GetModularFeatureImplementations<IInputDeviceModule>(IInputDeviceModule::GetModularFeatureName());
		for (auto InputPluginIt = PluginImplementations.CreateIterator(); InputPluginIt; ++InputPluginIt)
		{
			TSharedPtr<IInputDevice> Device = (*InputPluginIt)->CreateInputDevice(MessageHandler);
			if (Device.IsValid())
			{
				UE_LOG(LogInit, Log, TEXT("Adding external input plugin."));
				ExternalInputDevices.Add(Device);
			}
		}

		bHasLoadedInputPlugins = true;
	}
	
	// Poll externally-implemented devices
	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->Tick(TimeDelta);
		(*DeviceIt)->SendControllerEvents();
	}
}

TCHAR FLinuxApplication::ConvertChar( SDL_Keysym Keysym )
{
	if (SDL_GetKeyFromScancode(Keysym.scancode) >= 128)
	{
		return 0;
	}

	TCHAR Char = SDL_GetKeyFromScancode(Keysym.scancode);

    if (Keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT))
    {
        // Convert to uppercase (FIXME: what about CAPS?)
		if( SDL_GetKeyFromScancode(Keysym.scancode)  >= 97 && SDL_GetKeyFromScancode(Keysym.scancode)  <= 122)
        {
            return Keysym.sym - 32;
        }
		else if( SDL_GetKeyFromScancode(Keysym.scancode) >= 91 && SDL_GetKeyFromScancode(Keysym.scancode)  <= 93)
        {
			return SDL_GetKeyFromScancode(Keysym.scancode) + 32; // [ \ ] -> { | }
        }
        else
        {
			switch(SDL_GetKeyFromScancode(Keysym.scancode) )
            {
                case '`': // ` -> ~
                    Char = TEXT('`');
                    break;

                case '-': // - -> _
                    Char = TEXT('_');
                    break;

                case '=': // - -> _
                    Char = TEXT('+');
                    break;

                case ',':
                    Char = TEXT('<');
                    break;

                case '.':
                    Char = TEXT('>');
                    break;

                case ';':
                    Char = TEXT(':');
                    break;

                case '\'':
                    Char = TEXT('\"');
                    break;

                case '/':
                    Char = TEXT('?');
                    break;

                case '0':
                    Char = TEXT(')');
                    break;

                case '9':
                    Char = TEXT('(');
                    break;

                case '8':
                    Char = TEXT('*');
                    break;

                case '7':
                    Char = TEXT('&');
                    break;

                case '6':
                    Char = TEXT('^');
                    break;

                case '5':
                    Char = TEXT('%');
                    break;

                case '4':
                    Char = TEXT('$');
                    break;

                case '3':
                    Char = TEXT('#');
                    break;

                case '2':
                    Char = TEXT('@');
                    break;

                case '1':
                    Char = TEXT('!');
                    break;

                default:
                    break;
            }
        }
    }

    return Char;
}

TSharedPtr< FLinuxWindow > FLinuxApplication::FindEventWindow(SDL_Event* Event, bool& bOutWindowlessEvent)
{
	uint32 WindowID = 0;
	bOutWindowlessEvent = false;

	switch (Event->type)
	{
		case SDL_TEXTINPUT:
			WindowID = Event->text.windowID;
			break;
		case SDL_TEXTEDITING:
			WindowID = Event->edit.windowID;
			break;
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			WindowID = Event->key.windowID;
			break;
		case SDL_MOUSEMOTION:
			WindowID = Event->motion.windowID;
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			WindowID = Event->button.windowID;
			break;
		case SDL_MOUSEWHEEL:
			WindowID = Event->wheel.windowID;
			break;
		case SDL_WINDOWEVENT:
			WindowID = Event->window.windowID;
			break;
		case SDL_DROPBEGIN:
		case SDL_DROPFILE:
		case SDL_DROPTEXT:
		case SDL_DROPCOMPLETE:
			WindowID = Event->drop.windowID;
			break;
		case SDL_FINGERDOWN:
			// SDL touch events are windowless, but Slate needs to associate touch down with a particular window.
			// Assume that the current focus window is the one relevant for the touch and if there's none, assume the event windowless
			if (CurrentFocusWindow.IsValid())
			{
				return CurrentFocusWindow;
			}
			else
			{
				bOutWindowlessEvent = true;
				return TSharedPtr<FLinuxWindow>(nullptr);
			}
			break;
		default:
			bOutWindowlessEvent = true;
			return TSharedPtr< FLinuxWindow >(nullptr);
	}

	for (int32 WindowIndex=0; WindowIndex < Windows.Num(); ++WindowIndex)
	{
		TSharedRef< FLinuxWindow > Window = Windows[WindowIndex];
		
		if (SDL_GetWindowID(Window->GetHWnd()) == WindowID)
		{
			return Window;
		}
	}

	return TSharedPtr< FLinuxWindow >(nullptr);
}

void FLinuxApplication::RemoveEventWindow(SDL_HWindow HWnd)
{
	for (int32 WindowIndex=0; WindowIndex < Windows.Num(); ++WindowIndex)
	{
		TSharedRef< FLinuxWindow > Window = Windows[ WindowIndex ];
		
		if ( Window->GetHWnd() == HWnd )
		{
			Windows.RemoveAt(WindowIndex);
			return;
		}
	}
}

FModifierKeysState FLinuxApplication::GetModifierKeys() const
{
	SDL_Keymod modifiers = SDL_GetModState();

	const bool bIsLeftShiftDown		= (modifiers & KMOD_LSHIFT) != 0;
	const bool bIsRightShiftDown	= (modifiers & KMOD_RSHIFT) != 0;
	const bool bIsLeftControlDown	= (modifiers & KMOD_LCTRL) != 0;
	const bool bIsRightControlDown	= (modifiers & KMOD_RCTRL) != 0;
	const bool bIsLeftAltDown		= (modifiers & KMOD_LALT) != 0;
	const bool bIsRightAltDown		= (modifiers & KMOD_RALT) != 0;
	const bool bAreCapsLocked		= (modifiers & KMOD_CAPS) != 0;

	return FModifierKeysState( bIsLeftShiftDown, bIsRightShiftDown, bIsLeftControlDown, bIsRightControlDown, bIsLeftAltDown, bIsRightAltDown, false, false, bAreCapsLocked );
}


void FLinuxApplication::SetCapture( const TSharedPtr< FGenericWindow >& InWindow )
{
	bIsMouseCaptureEnabled = InWindow.IsValid();
	UpdateMouseCaptureWindow( bIsMouseCaptureEnabled ? ((FLinuxWindow*)InWindow.Get())->GetHWnd() : NULL );
}


void* FLinuxApplication::GetCapture( void ) const
{
	return ( bIsMouseCaptureEnabled && MouseCaptureWindow ) ? MouseCaptureWindow : NULL;
}

void FLinuxApplication::UpdateMouseCaptureWindow(SDL_HWindow TargetWindow)
{
	const bool bEnable = bIsMouseCaptureEnabled || bIsMouseCursorLocked;
	FLinuxCursor *LinuxCursor = static_cast<FLinuxCursor*>(Cursor.Get());

	// this is a hacky heuristic which makes QA-ClickHUD work while not ruining SlateViewer...
	bool bShouldGrab = (IS_PROGRAM != 0 || GIsEditor) && !LinuxCursor->IsHidden();

	if (bEnable)
	{
		if (TargetWindow)
		{
			MouseCaptureWindow = TargetWindow;
		}
		if (bShouldGrab && MouseCaptureWindow)
		{
			SDL_CaptureMouse(SDL_TRUE);
		}
	}
	else
	{
		if (MouseCaptureWindow)
		{
			if (bShouldGrab)
			{
				SDL_CaptureMouse(SDL_FALSE);
			}
			MouseCaptureWindow = nullptr;
		}
	}
}

void FLinuxApplication::SetHighPrecisionMouseMode( const bool Enable, const TSharedPtr< FGenericWindow >& InWindow )
{
	MessageHandler->OnCursorSet();
	bUsingHighPrecisionMouseInput = Enable;
}

void FLinuxApplication::RefreshDisplayCache()
{
	const double kCacheLifetime = 5.0;	// ask once in 5 seconds
	
	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastTimeCachedDisplays > kCacheLifetime)
	{
		CachedDisplays.Empty();

		int NumDisplays = SDL_GetNumVideoDisplays();

		for (int DisplayIdx = 0; DisplayIdx < NumDisplays; ++DisplayIdx)
		{
			SDL_Rect DisplayBounds;
			SDL_GetDisplayBounds(DisplayIdx, &DisplayBounds);
			
			CachedDisplays.Add(DisplayBounds);
		}

		LastTimeCachedDisplays = CurrentTime;
	}
}

FPlatformRect FLinuxApplication::GetWorkArea( const FPlatformRect& CurrentWindow ) const
{
	(const_cast<FLinuxApplication *>(this))->RefreshDisplayCache();

	// loop over all monitors to determine which one is the best
	int NumDisplays = CachedDisplays.Num();
	if (NumDisplays <= 0)
	{
		// fake something
		return CurrentWindow;
	}

	SDL_Rect BestDisplayBounds = CachedDisplays[0];

	// see if any other are better (i.e. cover top left)
	for (int DisplayIdx = 1; DisplayIdx < NumDisplays; ++DisplayIdx)
	{
		const SDL_Rect & DisplayBounds = CachedDisplays[DisplayIdx];

		// only check top left corner for "bestness"
		if (DisplayBounds.x <= CurrentWindow.Left && DisplayBounds.x + DisplayBounds.w > CurrentWindow.Left &&
			DisplayBounds.y <= CurrentWindow.Top && DisplayBounds.y + DisplayBounds.h > CurrentWindow.Bottom)
		{
			BestDisplayBounds = DisplayBounds;
			// there can be only one, as we don't expect overlapping displays
			break;
		}
	}

	FPlatformRect WorkArea;
	WorkArea.Left	= BestDisplayBounds.x;
	WorkArea.Top	= BestDisplayBounds.y;
	WorkArea.Right	= BestDisplayBounds.x + BestDisplayBounds.w;
	WorkArea.Bottom	= BestDisplayBounds.y + BestDisplayBounds.h;

	return WorkArea;
}

void FLinuxApplication::OnMouseCursorLock( bool bLockEnabled )
{
	if (UNLIKELY(!FApp::CanEverRender()))
	{
		return;
	}

	bIsMouseCursorLocked = bLockEnabled;
	UpdateMouseCaptureWindow( NULL );
	if(bLockEnabled)
	{
		CurrentClipWindow = CurrentlyActiveWindow;
	}
	else
	{
		CurrentClipWindow = nullptr;
	}
}

void FDisplayMetrics::GetDisplayMetrics(FDisplayMetrics& OutDisplayMetrics)
{
	int NumDisplays = 0;

	if (LIKELY(FApp::CanEverRender()))
	{
		if (FLinuxPlatformApplicationMisc::InitSDL()) //	will not initialize more than once
		{
			NumDisplays = SDL_GetNumVideoDisplays();
		}
		else
		{
			UE_LOG(LogInit, Warning, TEXT("FDisplayMetrics::GetDisplayMetrics: InitSDL() failed, cannot get display metrics"));
		}
	}

	OutDisplayMetrics.MonitorInfo.Empty();

	// exit early if no displays connected
	if (NumDisplays <= 0)
	{
		OutDisplayMetrics.PrimaryDisplayWorkAreaRect = FPlatformRect(0, 0, 0, 0);
		OutDisplayMetrics.VirtualDisplayRect = OutDisplayMetrics.PrimaryDisplayWorkAreaRect;
		OutDisplayMetrics.PrimaryDisplayWidth = 0;
		OutDisplayMetrics.PrimaryDisplayHeight = 0;

		return;
	}

	for (int32 DisplayIdx = 0; DisplayIdx < NumDisplays; ++DisplayIdx)
	{
		SDL_Rect DisplayBounds, UsableBounds;
		FMonitorInfo Display;
		SDL_GetDisplayBounds(DisplayIdx, &DisplayBounds);
		SDL_GetDisplayUsableBounds(DisplayIdx, &UsableBounds);

		Display.Name = UTF8_TO_TCHAR(SDL_GetDisplayName(DisplayIdx));
		Display.ID = FString::Printf(TEXT("display%d"), DisplayIdx);
		Display.NativeWidth = DisplayBounds.w;
		Display.NativeHeight = DisplayBounds.h;
		Display.DisplayRect = FPlatformRect(DisplayBounds.x, DisplayBounds.y, DisplayBounds.x + DisplayBounds.w, DisplayBounds.y + DisplayBounds.h);
		Display.WorkArea = FPlatformRect(UsableBounds.x, UsableBounds.y, UsableBounds.x + UsableBounds.w, UsableBounds.y + UsableBounds.h);
		Display.bIsPrimary = DisplayIdx == 0;
		OutDisplayMetrics.MonitorInfo.Add(Display);

		if (Display.bIsPrimary)
		{
			OutDisplayMetrics.PrimaryDisplayWorkAreaRect = FPlatformRect(UsableBounds.x, UsableBounds.y, UsableBounds.x + UsableBounds.w, UsableBounds.y + UsableBounds.h);

			OutDisplayMetrics.PrimaryDisplayWidth = DisplayBounds.w;
			OutDisplayMetrics.PrimaryDisplayHeight = DisplayBounds.h;

			OutDisplayMetrics.VirtualDisplayRect = OutDisplayMetrics.PrimaryDisplayWorkAreaRect;
		}
		else
		{
			// accumulate the total bound rect
			OutDisplayMetrics.VirtualDisplayRect.Left = FMath::Min(DisplayBounds.x, OutDisplayMetrics.VirtualDisplayRect.Left);
			OutDisplayMetrics.VirtualDisplayRect.Right = FMath::Max(OutDisplayMetrics.VirtualDisplayRect.Right, DisplayBounds.x + DisplayBounds.w);
			OutDisplayMetrics.VirtualDisplayRect.Top = FMath::Min(DisplayBounds.y, OutDisplayMetrics.VirtualDisplayRect.Top);
			OutDisplayMetrics.VirtualDisplayRect.Bottom = FMath::Max(OutDisplayMetrics.VirtualDisplayRect.Bottom, DisplayBounds.y + DisplayBounds.h);
		}
	}

	// Apply the debug safe zones
	OutDisplayMetrics.ApplyDefaultSafeZones();
}

void FLinuxApplication::RemoveNotificationWindow(SDL_HWindow HWnd)
{
	for (int32 WindowIndex=0; WindowIndex < NotificationWindows.Num(); ++WindowIndex)
	{
		TSharedRef< FLinuxWindow > Window = NotificationWindows[ WindowIndex ];
		
		if ( Window->GetHWnd() == HWnd )
		{
			NotificationWindows.RemoveAt(WindowIndex);
			return;
		}
	}
}

void FLinuxApplication::RaiseNotificationWindows(const TSharedPtr< FLinuxWindow >& ParentWindow)
{
	// Raise notification window only for the correct parent window.
	// TODO Do we have to make this restriction?
	for (int32 WindowIndex=0; WindowIndex < NotificationWindows.Num(); ++WindowIndex)
	{
		TSharedRef< FLinuxWindow > NotificationWindow = NotificationWindows[WindowIndex];
		if(ParentWindow == NotificationWindow->GetParent())
		{
			SDL_RaiseWindow(NotificationWindow->GetHWnd());
		}
	}
}

void FLinuxApplication::RemoveRevertFocusWindow(SDL_HWindow HWnd)
{
	for (int32 WindowIndex=0; WindowIndex < RevertFocusStack.Num(); ++WindowIndex)
	{
		TSharedRef< FLinuxWindow > Window = RevertFocusStack[ WindowIndex ];

		if (Window->GetHWnd() == HWnd)
		{
			UE_LOG(LogLinuxWindow, Verbose, TEXT("Found Window that is going to be destroyed. Going to revert focus ..."), Window->GetID());
			RevertFocusStack.RemoveAt(WindowIndex);

			if(Window->IsUtilityWindow() || Window->IsDialogWindow())
			{
				ActivateWindow(Window->GetParent());

				SDL_RaiseWindow(Window->GetParent()->GetHWnd() );
				SDL_SetWindowInputFocus(Window->GetParent()->GetHWnd() );
			}
			// Was the deleted window a Blueprint, Cascade, Matinee etc. window?
			else if (Window->IsNotificationWindow())
			{
				// Do not revert focus if the root window of the destroyed window is another one.
				TSharedPtr<FLinuxWindow> RevertFocusToWindow = Window->GetParent(); 
				TSharedPtr<FLinuxWindow> RootWindow = GetRootWindow(Window);
				UE_LOG(LogLinuxWindow, Verbose, TEXT("CurrentlyActiveWindow: %d, RootParentWindow: %d "), 	CurrentlyActiveWindow.IsValid() ? CurrentlyActiveWindow->GetID() : -1,
																											RootWindow.IsValid() ? RootWindow->GetID() : -1);

				// Only do this if the destroyed window had a root and the currently active is neither itself nor the root window.
				// If the currently active window is not the root another window got active before we could destroy it. So we give the focus to the
				// currently active one and the currently active window shouldn't be the destructed one, if yes that means that no other window got active
				// so we can process normally.
				if(CurrentlyActiveWindow.IsValid() && RootWindow.IsValid() && (CurrentlyActiveWindow != RootWindow) && (CurrentlyActiveWindow != Window) )
				{
					UE_LOG(LogLinuxWindow, Verbose, TEXT("Root Parent is different, going to set focus to CurrentlyActiveWindow: %d"), CurrentlyActiveWindow.IsValid() ? CurrentlyActiveWindow->GetID() : -1);
					RevertFocusToWindow = CurrentlyActiveWindow;
				}

				ActivateWindow(RevertFocusToWindow);

				SDL_RaiseWindow(RevertFocusToWindow->GetHWnd());
				SDL_SetWindowInputFocus(RevertFocusToWindow->GetHWnd());
			}
			// Was the deleted window a top level window and we have still at least one other window in the stack?
			else if (Window->IsTopLevelWindow() && (RevertFocusStack.Num() > 0))
			{
				// OK, give focus to the one on top of the stack.
				TSharedPtr< FLinuxWindow > TopmostWindow = RevertFocusStack.Top();
				if (TopmostWindow.IsValid())
				{
					ActivateWindow(TopmostWindow);

					SDL_RaiseWindow(TopmostWindow->GetHWnd());
					SDL_SetWindowInputFocus(TopmostWindow->GetHWnd());
				}
			}
			// Was it a popup menu?
			else if (Window->IsPopupMenuWindow() && bActivateApp)
			{
				ActivateWindow(Window->GetParent());

				SDL_RaiseWindow(Window->GetParent()->GetHWnd());
				if(Window->GetParent()->IsPopupMenuWindow())
				{
					SDL_SetKeyboardGrab(Window->GetParent()->GetHWnd(), SDL_TRUE);
				}
				else
				{
					SDL_SetWindowInputFocus(Window->GetParent()->GetHWnd());
				}
				UE_LOG(LogLinuxWindowType, Verbose, TEXT("FLinuxWindow::Destroy: Going to revert focus to %d"), Window->GetParent()->GetID());
			}
			break;
		}
	}
}

void FLinuxApplication::ActivateApplication()
{
	MessageHandler->OnApplicationActivationChanged( true );
	bActivateApp = true;
	UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("WM_ACTIVATEAPP, wParam = 1"));
}

void FLinuxApplication::DeactivateApplication()
{
	MessageHandler->OnApplicationActivationChanged( false );
	CurrentlyActiveWindow = nullptr;
	CurrentFocusWindow = nullptr;
	bActivateApp = false;
	UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("WM_ACTIVATEAPP, wParam = 0"));
}

void FLinuxApplication::ActivateWindow(const TSharedPtr< FLinuxWindow >& Window) 
{
	PreviousActiveWindow = CurrentlyActiveWindow;
	CurrentlyActiveWindow = Window;
	if(PreviousActiveWindow.IsValid())
	{
		MessageHandler->OnWindowActivationChanged(PreviousActiveWindow.ToSharedRef(), EWindowActivation::Deactivate);
		UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("WM_ACTIVATE,    wParam = WA_INACTIVE     : %d"), PreviousActiveWindow->GetID());
	}
	MessageHandler->OnWindowActivationChanged(CurrentlyActiveWindow.ToSharedRef(), EWindowActivation::Activate);
	UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("WM_ACTIVATE,    wParam = WA_ACTIVE       : %d"), CurrentlyActiveWindow->GetID());
}

void FLinuxApplication::ActivateRootWindow(const TSharedPtr< FLinuxWindow >& Window)
{
	TSharedPtr< FLinuxWindow > ParentWindow = Window;
	while(ParentWindow.IsValid() && ParentWindow->GetParent().IsValid())
	{
		ParentWindow = ParentWindow->GetParent();
	}
	ActivateWindow(ParentWindow);
}

TSharedPtr< FLinuxWindow > FLinuxApplication::GetRootWindow(const TSharedPtr< FLinuxWindow >& Window)
{
	TSharedPtr< FLinuxWindow > ParentWindow = Window;
	while(ParentWindow->GetParent().IsValid())
	{
		ParentWindow = ParentWindow->GetParent();
	}
	return ParentWindow;
}

bool FLinuxApplication::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	// Ignore any execs that doesn't start with LinuxApp
	if (!FParse::Command(&Cmd, TEXT("LinuxApp")))
	{
		return false;
	}

	if (FParse::Command(&Cmd, TEXT("Cursor")))
	{
		return HandleCursorCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("Window")))
	{
		return HandleWindowCommand(Cmd, Ar);
	}

	return false;
}

bool FLinuxApplication::HandleCursorCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (FParse::Command(&Cmd, TEXT("Status")))
	{
		FLinuxCursor *LinuxCursor = static_cast<FLinuxCursor*>(Cursor.Get());
		FVector2D CurrentPosition = LinuxCursor->GetPosition();

		Ar.Logf(TEXT("Cursor status:"));
		Ar.Logf(TEXT("Position: (%f, %f)"), CurrentPosition.X, CurrentPosition.Y);
		Ar.Logf(TEXT("IsHidden: %s"), LinuxCursor->IsHidden() ? TEXT("true") : TEXT("false"));
		Ar.Logf(TEXT("bIsMouseCaptureEnabled: %s"), bIsMouseCaptureEnabled ? TEXT("true") : TEXT("false"));
		Ar.Logf(TEXT("bUsingHighPrecisionMouseInput: %s"), bUsingHighPrecisionMouseInput ? TEXT("true") : TEXT("false"));
		Ar.Logf(TEXT("bIsMouseCaptureEnabled: %s"), bIsMouseCaptureEnabled ? TEXT("true") : TEXT("false"));

		return true;
	}

	return false;
}

bool FLinuxApplication::HandleWindowCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (FParse::Command(&Cmd, TEXT("List")))
	{
		Ar.Logf(TEXT("Window list:"));
		for (int WindowIdx = 0, NumWindows = Windows.Num(); WindowIdx < NumWindows; ++WindowIdx)
		{
			Ar.Logf(TEXT("%d: native handle: %p, debugging ID: %d"), WindowIdx, Windows[WindowIdx]->GetHWnd(), Windows[WindowIdx]->GetID());
		}

		return true;
	}

	return false;
}

void FLinuxApplication::SaveWindowLocationsForEventLoop(void)
{
	for (int32 WindowIndex = 0; WindowIndex < Windows.Num(); ++WindowIndex)
	{
		TSharedRef< FLinuxWindow > Window = Windows[WindowIndex];
		int x = 0;
		int y = 0;
		SDL_HWindow NativeWindow = Window->GetHWnd();
		SDL_GetWindowPosition(NativeWindow, &x, &y);
		SavedWindowLocationsForEventLoop.FindOrAdd(NativeWindow) = FVector2D(x, y);
	}
}

void FLinuxApplication::ClearWindowLocationsAfterEventLoop(void)
{
	SavedWindowLocationsForEventLoop.Empty();
}

void FLinuxApplication::GetWindowPositionInEventLoop(SDL_HWindow NativeWindow, int *x, int *y)
{
	FVector2D *Position = SavedWindowLocationsForEventLoop.Find(NativeWindow);
	if(Position)
	{
		// Found saved location.
		*x = Position->X;
		*y = Position->Y;
	}
	else if(NativeWindow)
	{
		SDL_GetWindowPosition(NativeWindow, x, y);

		// If we've hit this case, then we're either not in the event
		// loop, or suddenly have a new window to keep track of.
		// Record the initial window position.
		SavedWindowLocationsForEventLoop.FindOrAdd(NativeWindow) = FVector2D(*x, *y);
	}
	else
	{
		UE_LOG(LogLinuxWindowEvent, Error, TEXT("Tried to get the location of a non-existing window\n"));
		*x = 0;
		*y = 0;
	}
}

void FLinuxApplication::DestroyNativeWindow(SDL_HWindow NativeWindow)
{
	UE_LOG(LogLinuxWindow, Verbose, TEXT("Asked to destroy SDL window %p"), NativeWindow);

	if (PendingDestroyWindows.Find(NativeWindow) != nullptr)
	{
		UE_LOG(LogLinuxWindow, Verbose, TEXT("  SDL window %p is already pending deletion!"), NativeWindow);
		return;	// use the original 'deadline', do not renew it.
	}

	// Set deadline to make sure the window survives at least one tick.
	PendingDestroyWindows.Add(NativeWindow, FPlatformTime::Seconds() + 0.1);

	UE_LOG(LogLinuxWindow, Verbose, TEXT("  Deferring destroying of SDL window %p"), NativeWindow);
}

bool FLinuxApplication::IsMouseAttached() const
{
	int rc;
	char Mouse[64] = "/sys/class/input/mouse0";
	int MouseIdx = strlen(Mouse) - 1;
	FCStringAnsi::Strncat(Mouse, "/device/name", sizeof(Mouse) - 1);

	for (int i=0; i<9; i++)
	{
		Mouse[MouseIdx] = '0' + i;
		if (access(Mouse, F_OK) == 0)
		{
			return true;
		}
	}

	return false;
}
