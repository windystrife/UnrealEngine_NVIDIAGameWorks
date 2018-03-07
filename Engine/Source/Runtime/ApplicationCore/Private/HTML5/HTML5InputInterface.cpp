// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HTML5InputInterface.h"
#include "HTML5Cursor.h"
#include "HAL/OutputDevices.h"
#include "HAL/PlatformTime.h"
THIRD_PARTY_INCLUDES_START
#include <SDL.h>
THIRD_PARTY_INCLUDES_END

DECLARE_LOG_CATEGORY_EXTERN(LogHTML5, Log, All);
DEFINE_LOG_CATEGORY(LogHTML5);
DEFINE_LOG_CATEGORY_STATIC(LogHTML5Input, Log, All);

THIRD_PARTY_INCLUDES_START
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
THIRD_PARTY_INCLUDES_END

static FGamepadKeyNames::Type AxisMapping[4] =
{
	FGamepadKeyNames::LeftAnalogX,
	FGamepadKeyNames::LeftAnalogY,
	FGamepadKeyNames::RightAnalogX,
	FGamepadKeyNames::RightAnalogY
};

// Axis Mapping, reversed or not.
static int Reversed[4] =
{
	1,
	-1,
	1,
	-1
};

// all are digital except Left and Right Trigger Analog.
static FGamepadKeyNames::Type ButtonMapping[16] =
{
	FGamepadKeyNames::FaceButtonBottom,
	FGamepadKeyNames::FaceButtonRight,
	FGamepadKeyNames::FaceButtonLeft,
	FGamepadKeyNames::FaceButtonTop,
	FGamepadKeyNames::LeftShoulder,
	FGamepadKeyNames::RightShoulder,
	FGamepadKeyNames::LeftTriggerThreshold,
	FGamepadKeyNames::RightTriggerThreshold,
	FGamepadKeyNames::SpecialLeft,
	FGamepadKeyNames::SpecialRight,
	FGamepadKeyNames::LeftStickDown,
	FGamepadKeyNames::RightStickDown,
	FGamepadKeyNames::DPadUp,
	FGamepadKeyNames::DPadDown,
	FGamepadKeyNames::DPadLeft,
	FGamepadKeyNames::DPadRight
};


TSharedRef< FHTML5InputInterface > FHTML5InputInterface::Create(  const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler, const TSharedPtr< ICursor >& InCursor )
{
	return MakeShareable( new FHTML5InputInterface( InMessageHandler, InCursor ) );
}

EM_BOOL mouse_move_callback(int evType, const EmscriptenMouseEvent* evt, void* userData) {
	/* rescale (in case canvas is being scaled)*/
	double client_w, client_h, xscale, yscale;
	int canvas_w, canvas_h, canvas_fs;
	emscripten_get_canvas_size(&canvas_w, &canvas_h, &canvas_fs);
	emscripten_get_element_css_size(NULL, &client_w, &client_h);
	xscale = canvas_w/client_w;
	yscale = canvas_h/client_h;
	int calc_x = (int)(evt->canvasX*xscale + .5);
	int calc_y = (int)(evt->canvasY*yscale + .5);
	UE_LOG(LogHTML5Input, Verbose, TEXT("MouseMoveCB Pos(%d or %d, %d or %d) XRel:%d YRel:%d"), evt->canvasX, calc_x, evt->canvasY, calc_y, evt->movementX, evt->movementY);
	(*((const TSharedPtr< ICursor >*)userData))->SetPosition(calc_x, calc_y);
	return 0;
}

FHTML5InputInterface::FHTML5InputInterface( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler, const TSharedPtr< ICursor >& InCursor )
	: MessageHandler( InMessageHandler )
	, Cursor( InCursor )
{
	emscripten_set_mousemove_callback("canvas", (void*)&Cursor, 1, mouse_move_callback);
}

void FHTML5InputInterface::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	MessageHandler = InMessageHandler;
}

void FHTML5InputInterface::Tick(float DeltaTime, const SDL_Event& Event,TSharedRef < FGenericWindow>& ApplicationWindow )
{

	switch (Event.type)
	{
		case SDL_KEYDOWN:
			{
				SDL_KeyboardEvent KeyEvent = Event.key;
				const SDL_Keycode KeyCode = KeyEvent.keysym.scancode;
				const bool bIsRepeated = KeyEvent.repeat != 0;

				// First KeyDown, then KeyChar. This is important, as in-game console ignores first character otherwise
				MessageHandler->OnKeyDown(KeyCode, KeyEvent.keysym.sym, bIsRepeated);

				// Backspace/Return input caught here.
				// Note that TextInput still seems to get characters messages too but slate seems to not process them.
				if (KeyCode == SDL_SCANCODE_BACKSPACE || KeyCode == SDL_SCANCODE_RETURN)
				{
					const TCHAR Character = SDL_GetKeyFromScancode(Event.key.keysym.scancode);
					UE_LOG(LogHTML5Input, Verbose, TEXT("TextInput: Text:%c bIsRepeated:%s"), Character, bIsRepeated ? TEXT("TRUE") : TEXT("FALSE"));
					MessageHandler->OnKeyChar(Character, bIsRepeated);
				}
				UE_LOG(LogHTML5Input, Verbose, TEXT("KeyDown: Code:%d bIsRepeated:%s"), KeyCode, bIsRepeated ? TEXT("TRUE") : TEXT("FALSE"));
			}
			break;
		case SDL_KEYUP:
			{
				SDL_KeyboardEvent keyEvent = Event.key;
				const SDL_Keycode KeyCode = keyEvent.keysym.scancode;
				const bool IsRepeat = keyEvent.repeat != 0;

				MessageHandler->OnKeyUp( KeyCode, keyEvent.keysym.sym, IsRepeat );
				UE_LOG(LogHTML5Input, Verbose, TEXT("KeyUp Code:%d"), KeyCode);
			}
			break;
		case SDL_TEXTINPUT:
			{
				const bool bIsRepeated = Event.key.repeat != 0;
				const TCHAR Character = *ANSI_TO_TCHAR(Event.text.text);

				MessageHandler->OnKeyChar(Character, bIsRepeated);
				UE_LOG(LogHTML5Input, Verbose, TEXT("TextInput: Text:%c bIsRepeated:%s"), Character, bIsRepeated ? TEXT("TRUE") : TEXT("FALSE"));
			}
			break;
		case SDL_MOUSEBUTTONDOWN:
			{
				EMouseButtons::Type MouseButton = Event.button.button == 1 ? EMouseButtons::Left : Event.button.button == 2 ? EMouseButtons::Middle : EMouseButtons::Right;
				MessageHandler->OnMouseDown(ApplicationWindow, MouseButton );
				UE_LOG(LogHTML5Input, Verbose, TEXT("MouseButtonDown ID:%d"), Event.button.button);
			}
			break;
		case SDL_MOUSEBUTTONUP:
			{
				EMouseButtons::Type MouseButton = Event.button.button == 1 ? EMouseButtons::Left : Event.button.button == 2 ? EMouseButtons::Middle : EMouseButtons::Right;
				MessageHandler->OnMouseUp(MouseButton );
				UE_LOG(LogHTML5Input, Verbose, TEXT("MouseButtonUp ID:%d"), Event.button.button);
			}
			break;
		case SDL_MOUSEMOTION:
			{
				//Cursor->SetPosition(Event.motion.x, Event.motion.y);
				MessageHandler->OnRawMouseMove(Event.motion.xrel, Event.motion.yrel);
				MessageHandler->OnMouseMove();
				UE_LOG(LogHTML5Input, Verbose, TEXT("MouseMotion Pos(%d, %d) XRel:%d YRel:%d"), Event.motion.x, Event.motion.y, Event.motion.xrel, Event.motion.yrel);
			}
			break;
		case SDL_MOUSEWHEEL:
			{
				SDL_MouseWheelEvent* w = (SDL_MouseWheelEvent*)&Event;
				const float SpinFactor = 1 / 120.0f;
				MessageHandler->OnMouseWheel(w->y * SpinFactor);
				UE_LOG(LogHTML5Input, Verbose, TEXT("MouseWheel %f"), SpinFactor);
			}
			break;
		default:
			// unhandled.
			break;
	}
}

void FHTML5InputInterface::SendControllerEvents()
{
	// game pads can only be polled.
	static int PrevNumGamepads = 0;
	static bool GamepadSupported = true;

	const double CurrentTime = FPlatformTime::Seconds();

	if (GamepadSupported)
	{
		int NumGamepads = emscripten_get_num_gamepads();
		if (NumGamepads != PrevNumGamepads)
		{
			if (NumGamepads == EMSCRIPTEN_RESULT_NOT_SUPPORTED)
			{
				GamepadSupported = false;
				return;
			}
		}

		for(int CurrentGamePad = 0; CurrentGamePad < NumGamepads && CurrentGamePad < 5; ++CurrentGamePad) // max 5 game pads.
		{
			EmscriptenGamepadEvent GamePadEvent;
			int Failed = emscripten_get_gamepad_status(CurrentGamePad, &GamePadEvent);
			if (!Failed)
			{
				check(CurrentGamePad == GamePadEvent.index);
				for(int CurrentAxis = 0; CurrentAxis < GamePadEvent.numAxes; ++CurrentAxis)
				{
					if (GamePadEvent.axis[CurrentAxis] != PrevGamePadState[CurrentGamePad].axis[CurrentAxis])
					{

						MessageHandler->OnControllerAnalog(AxisMapping[CurrentAxis],CurrentGamePad, Reversed[CurrentAxis]*GamePadEvent.axis[CurrentAxis]);
					}
				}
				// edge trigger.
				for(int CurrentButton = 0; CurrentButton < GamePadEvent.numButtons; ++CurrentButton)
				{
					// trigger for digital buttons.
					if ( GamePadEvent.digitalButton[CurrentButton]   != PrevGamePadState[CurrentGamePad].digitalButton[CurrentButton] )
					{
						bool Triggered = GamePadEvent.digitalButton[CurrentButton] != 0;
						if ( Triggered )
						{
							MessageHandler->OnControllerButtonPressed(ButtonMapping[CurrentButton],CurrentGamePad, false);
							LastPressedTime[CurrentGamePad][CurrentButton] = CurrentTime;
						}
						else
						{
							MessageHandler->OnControllerButtonReleased(ButtonMapping[CurrentButton],CurrentGamePad, false);
						}
					}
				}
				// repeat trigger.
				const float RepeatDelta = 0.2f;
				for(int CurrentButton = 0; CurrentButton < GamePadEvent.numButtons; ++CurrentButton)
				{
					if ( GamePadEvent.digitalButton[CurrentButton]  )
					{
						if (CurrentTime - LastPressedTime[CurrentGamePad][CurrentButton] > RepeatDelta)
						{
							MessageHandler->OnControllerButtonPressed(ButtonMapping[CurrentButton],CurrentGamePad, true);
							LastPressedTime[CurrentGamePad][CurrentButton] = CurrentTime;
						}
					}
				}
				PrevGamePadState[CurrentGamePad] = GamePadEvent;
			}
		}
	}
}
