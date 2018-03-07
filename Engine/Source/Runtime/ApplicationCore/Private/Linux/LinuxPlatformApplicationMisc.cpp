// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LinuxPlatformApplicationMisc.h"
#include "LinuxApplication.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "HAL/ThreadHeartBeat.h"
#include "Modules/ModuleManager.h"
#include "LinuxConsoleOutputDevice.h"
#include "LinuxErrorOutputDevice.h"
#include "LinuxFeedbackContext.h"

bool GInitializedSDL = false;

namespace
{
	uint32 GWindowStyleSDL = SDL_WINDOW_OPENGL;

	FString GetHeadlessMessageBoxMessage(EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption, EAppReturnType::Type& Answer)
	{
		FString MessageSuffix;
		switch (MsgType)
		{
		case EAppMsgType::YesNo:
		case EAppMsgType::YesNoYesAllNoAll:
		case EAppMsgType::YesNoYesAll:
			Answer = EAppReturnType::No;
			MessageSuffix = FString(TEXT("No is implied."));
			break;

		case EAppMsgType::OkCancel:
		case EAppMsgType::YesNoCancel:
		case EAppMsgType::CancelRetryContinue:
		case EAppMsgType::YesNoYesAllNoAllCancel:
			Answer = EAppReturnType::Cancel;
			MessageSuffix = FString(TEXT("Cancel is implied."));
			break;
		}

		FString Message = UTF8_TO_TCHAR(SDL_GetError());
		if (Message != FString(TEXT("No message system available")))
		{
			Message = FString::Printf(TEXT("MessageBox: %s: %s: %s: %s"), Caption, Text, *Message, *MessageSuffix);
		}
		else
		{
			Message = FString::Printf(TEXT("MessageBox: %s: %s: %s"), Caption, Text, *MessageSuffix);
		}
		return Message;
	}
}

extern CORE_API TFunction<void()> UngrabAllInputCallback;
extern CORE_API TFunction<EAppReturnType::Type(EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption)> MessageBoxExtCallback;

EAppReturnType::Type MessageBoxExtImpl(EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption)
{
	int NumberOfButtons = 0;

	// if multimedia cannot be initialized for messagebox, just fall back to default implementation
	if (!FPlatformApplicationMisc::InitSDL()) //	will not initialize more than once
	{
		EAppReturnType::Type Answer = EAppReturnType::Type::Cancel;
		FString Message = GetHeadlessMessageBoxMessage(MsgType, Caption, Text, Answer);
		UE_LOG(LogLinux, Warning, TEXT("%s"), *Message);
		return Answer;
	}

#if DO_CHECK
	uint32 InitializedSubsystems = SDL_WasInit(SDL_INIT_EVERYTHING);
	check(InitializedSubsystems & SDL_INIT_VIDEO);
#endif // DO_CHECK

	SDL_MessageBoxButtonData *Buttons = nullptr;

	switch (MsgType)
	{
	case EAppMsgType::Ok:
		NumberOfButtons = 1;
		Buttons = new SDL_MessageBoxButtonData[NumberOfButtons];
		Buttons[0].flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
		Buttons[0].text = "Ok";
		Buttons[0].buttonid = EAppReturnType::Ok;
		break;

	case EAppMsgType::YesNo:
		NumberOfButtons = 2;
		Buttons = new SDL_MessageBoxButtonData[NumberOfButtons];
		Buttons[0].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[0].text = "Yes";
		Buttons[0].buttonid = EAppReturnType::Yes;
		Buttons[1].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[1].text = "No";
		Buttons[1].buttonid = EAppReturnType::No;
		break;

	case EAppMsgType::OkCancel:
		NumberOfButtons = 2;
		Buttons = new SDL_MessageBoxButtonData[NumberOfButtons];
		Buttons[0].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[0].text = "Ok";
		Buttons[0].buttonid = EAppReturnType::Ok;
		Buttons[1].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[1].text = "Cancel";
		Buttons[1].buttonid = EAppReturnType::Cancel;
		break;

	case EAppMsgType::YesNoCancel:
		NumberOfButtons = 3;
		Buttons = new SDL_MessageBoxButtonData[NumberOfButtons];
		Buttons[0].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[0].text = "Yes";
		Buttons[0].buttonid = EAppReturnType::Yes;
		Buttons[1].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[1].text = "No";
		Buttons[1].buttonid = EAppReturnType::No;
		Buttons[2].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[2].text = "Cancel";
		Buttons[2].buttonid = EAppReturnType::Cancel;
		break;

	case EAppMsgType::CancelRetryContinue:
		NumberOfButtons = 3;
		Buttons = new SDL_MessageBoxButtonData[NumberOfButtons];
		Buttons[0].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[0].text = "Continue";
		Buttons[0].buttonid = EAppReturnType::Continue;
		Buttons[1].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[1].text = "Retry";
		Buttons[1].buttonid = EAppReturnType::Retry;
		Buttons[2].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[2].text = "Cancel";
		Buttons[2].buttonid = EAppReturnType::Cancel;
		break;

	case EAppMsgType::YesNoYesAllNoAll:
		NumberOfButtons = 4;
		Buttons = new SDL_MessageBoxButtonData[NumberOfButtons];
		Buttons[0].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[0].text = "Yes";
		Buttons[0].buttonid = EAppReturnType::Yes;
		Buttons[1].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[1].text = "No";
		Buttons[1].buttonid = EAppReturnType::No;
		Buttons[2].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[2].text = "Yes to all";
		Buttons[2].buttonid = EAppReturnType::YesAll;
		Buttons[3].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[3].text = "No to all";
		Buttons[3].buttonid = EAppReturnType::NoAll;
		break;

	case EAppMsgType::YesNoYesAllNoAllCancel:
		NumberOfButtons = 5;
		Buttons = new SDL_MessageBoxButtonData[NumberOfButtons];
		Buttons[0].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[0].text = "Yes";
		Buttons[0].buttonid = EAppReturnType::Yes;
		Buttons[1].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[1].text = "No";
		Buttons[1].buttonid = EAppReturnType::No;
		Buttons[2].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[2].text = "Yes to all";
		Buttons[2].buttonid = EAppReturnType::YesAll;
		Buttons[3].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[3].text = "No to all";
		Buttons[3].buttonid = EAppReturnType::NoAll;
		Buttons[4].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[4].text = "Cancel";
		Buttons[4].buttonid = EAppReturnType::Cancel;
		break;

	case EAppMsgType::YesNoYesAll:
		NumberOfButtons = 3;
		Buttons = new SDL_MessageBoxButtonData[NumberOfButtons];
		Buttons[0].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[0].text = "Yes";
		Buttons[0].buttonid = EAppReturnType::Yes;
		Buttons[1].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[1].text = "No";
		Buttons[1].buttonid = EAppReturnType::No;
		Buttons[2].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		Buttons[2].text = "Yes to all";
		Buttons[2].buttonid = EAppReturnType::YesAll;
		break;
	}

	FTCHARToUTF8 CaptionUTF8(Caption);
	FTCHARToUTF8 TextUTF8(Text);
	SDL_MessageBoxData MessageBoxData =
	{
		SDL_MESSAGEBOX_INFORMATION,
		NULL, // No parent window
		CaptionUTF8.Get(),
		TextUTF8.Get(),
		NumberOfButtons,
		Buttons,
		NULL // Default color scheme
	};

	int ButtonPressed = -1;
	EAppReturnType::Type Answer = EAppReturnType::Type::Cancel;

	FSlowHeartBeatScope SuspendHeartBeat;
	if (SDL_ShowMessageBox(&MessageBoxData, &ButtonPressed) == -1)
	{
		FString Message = GetHeadlessMessageBoxMessage(MsgType, Caption, Text, Answer);
		UE_LOG(LogLinux, Warning, TEXT("%s"), *Message);
	}
	else
	{
		Answer = ButtonPressed == -1 ? EAppReturnType::Cancel : static_cast<EAppReturnType::Type>(ButtonPressed);
	}

	delete[] Buttons;

	return Answer;
}

#if !UE_BUILD_SHIPPING
void UngrabAllInputImpl()
{
	if (GInitializedSDL)
	{
		SDL_Window * GrabbedWindow = SDL_GetGrabbedWindow();
		if (GrabbedWindow)
		{
			SDL_SetWindowGrab(GrabbedWindow, SDL_FALSE);
			SDL_SetKeyboardGrab(GrabbedWindow, SDL_FALSE);
		}

		SDL_CaptureMouse(SDL_FALSE);
	}
}
#endif // !UE_BUILD_SHIPPING

uint32 FLinuxPlatformApplicationMisc::WindowStyle()
{
	return GWindowStyleSDL;
}

void FLinuxPlatformApplicationMisc::PreInit()
{
	MessageBoxExtCallback = MessageBoxExtImpl;
}

void FLinuxPlatformApplicationMisc::Init()
{
	// skip for servers and programs, unless they request later
	bool bIsNullRHI = !FApp::CanEverRender();
	if (!IS_PROGRAM && !bIsNullRHI)
	{
		InitSDL();
	}

	FGenericPlatformApplicationMisc::Init();

#if !UE_BUILD_SHIPPING
	UngrabAllInputCallback = UngrabAllInputImpl;
#endif
}

bool FLinuxPlatformApplicationMisc::InitSDL()
{
	if (!GInitializedSDL)
	{
		UE_LOG(LogInit, Log, TEXT("Initializing SDL."));

		SDL_SetHint("SDL_VIDEO_X11_REQUIRE_XRANDR", "1");  // workaround for misbuilt SDL libraries on X11.
		// we don't use SDL for audio
		if (SDL_Init((SDL_INIT_EVERYTHING ^ SDL_INIT_AUDIO) | SDL_INIT_NOPARACHUTE) != 0)
		{
			FString ErrorMessage = UTF8_TO_TCHAR(SDL_GetError());
			if(ErrorMessage != FString(TEXT("No message system available")))
			{
				// do not fail at this point, allow caller handle failure
				UE_LOG(LogInit, Warning, TEXT("Could not initialize SDL: %s"), *ErrorMessage);
			}
			return false;
		}

		if (FParse::Param(FCommandLine::Get(), TEXT("vulkan")))
		{
			GWindowStyleSDL = SDL_WINDOW_VULKAN;
			UE_LOG(LogInit, Log, TEXT("Using SDL_WINDOW_VULKAN"));
		}
		else
		{
			GWindowStyleSDL = SDL_WINDOW_OPENGL;
			UE_LOG(LogInit, Log, TEXT("Using SDL_WINDOW_OPENGL"));
		}

		// print out version information
		SDL_version CompileTimeSDLVersion;
		SDL_version RunTimeSDLVersion;
		SDL_VERSION(&CompileTimeSDLVersion);
		SDL_GetVersion(&RunTimeSDLVersion);
		int SdlRevisionNum = SDL_GetRevisionNumber();
		FString SdlRevision = UTF8_TO_TCHAR(SDL_GetRevision());
		UE_LOG(LogInit, Log, TEXT("Initialized SDL %d.%d.%d revision: %d (%s) (compiled against %d.%d.%d)"),
			RunTimeSDLVersion.major, RunTimeSDLVersion.minor, RunTimeSDLVersion.patch,
			SdlRevisionNum, *SdlRevision,
			CompileTimeSDLVersion.major, CompileTimeSDLVersion.minor, CompileTimeSDLVersion.patch
			);

		// Used to make SDL push SDL_TEXTINPUT events.
		SDL_StartTextInput();

		GInitializedSDL = true;

		// needs to come after GInitializedSDL, otherwise it will recurse here
		if (!UE_BUILD_SHIPPING)
		{
			// dump information about screens for debug
			FDisplayMetrics DisplayMetrics;
			FDisplayMetrics::GetDisplayMetrics(DisplayMetrics);
			DisplayMetrics.PrintToLog();
		}
	}

	return true;
}

void FLinuxPlatformApplicationMisc::TearDown()
{
	FGenericPlatformApplicationMisc::TearDown();

	if (GInitializedSDL)
	{
		UE_LOG(LogInit, Log, TEXT("Tearing down SDL."));
		SDL_Quit();
		GInitializedSDL = false;

		MessageBoxExtCallback = nullptr;
#if !UE_BUILD_SHIPPING
		UngrabAllInputCallback = nullptr;
#endif
	}
}


void FLinuxPlatformApplicationMisc::LoadPreInitModules()
{
#if WITH_EDITOR
	FModuleManager::Get().LoadModule(TEXT("OpenGLDrv"));
#endif // WITH_EDITOR
}

void FLinuxPlatformApplicationMisc::LoadStartupModules()
{
#if !IS_PROGRAM && !UE_SERVER
	FModuleManager::Get().LoadModule(TEXT("ALAudio"));	// added in Launch.Build.cs for non-server targets
	FModuleManager::Get().LoadModule(TEXT("AudioMixerSDL"));	// added in Launch.Build.cs for non-server targets
	FModuleManager::Get().LoadModule(TEXT("HeadMountedDisplay"));
#endif // !IS_PROGRAM && !UE_SERVER

#if defined(WITH_STEAMCONTROLLER) && WITH_STEAMCONTROLLER
	FModuleManager::Get().LoadModule(TEXT("SteamController"));
#endif // WITH_STEAMCONTROLLER

#if WITH_EDITOR
	FModuleManager::Get().LoadModule(TEXT("SourceCodeAccess"));
#endif	//WITH_EDITOR
}

class FOutputDeviceConsole* FLinuxPlatformApplicationMisc::CreateConsoleOutputDevice()
{
	// this is a slightly different kind of singleton that gives ownership to the caller and should not be called more than once
	return new FLinuxConsoleOutputDevice();
}

class FOutputDeviceError* FLinuxPlatformApplicationMisc::GetErrorOutputDevice()
{
	static FLinuxErrorOutputDevice Singleton;
	return &Singleton;
}

class FFeedbackContext* FLinuxPlatformApplicationMisc::GetFeedbackContext()
{
	static FLinuxFeedbackContext Singleton;
	return &Singleton;
}

GenericApplication* FLinuxPlatformApplicationMisc::CreateApplication()
{
	return FLinuxApplication::CreateLinuxApplication();
}

bool FLinuxPlatformApplicationMisc::IsThisApplicationForeground()
{
	extern FLinuxApplication* LinuxApplication;
	return (LinuxApplication != nullptr) ? LinuxApplication->IsForeground() : true;
}

void FLinuxPlatformApplicationMisc::PumpMessages( bool bFromMainLoop )
{
	if (GInitializedSDL && bFromMainLoop)
	{
		if( LinuxApplication )
		{
			LinuxApplication->SaveWindowLocationsForEventLoop();

			SDL_Event event;

			while (SDL_PollEvent(&event))
			{
				LinuxApplication->AddPendingEvent( event );
			}

			LinuxApplication->ClearWindowLocationsAfterEventLoop();
		}
		else
		{
			// No application to send events to. Just flush out the
			// queue.
			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				// noop
			}
		}
	}
}

bool FLinuxPlatformApplicationMisc::ControlScreensaver(EScreenSaverAction Action)
{
	if (Action == FGenericPlatformApplicationMisc::EScreenSaverAction::Disable)
	{
		SDL_DisableScreenSaver();
	}
	else
	{
		SDL_EnableScreenSaver();
	}

	return true;
}

namespace LinuxPlatformApplicationMisc
{
	/**
	 * Round the scale to 0.5, 1, 1.5, etc (note - step coarser than 0.25 is needed because a lot of monitors are 107-108 DPI and not 96).
	 */
	float QuantizeScale(float Scale)
	{
		float NewScale = FMath::FloorToFloat((64.0f * Scale / 32.0f) + 0.5f) / 2.0f;
		return NewScale > 0.0f ? NewScale : 1.0f;
	}
}

float FLinuxPlatformApplicationMisc::GetDPIScaleFactorAtPoint(float X, float Y)
{
	if ((GIsEditor || IS_PROGRAM) && !FParse::Param(FCommandLine::Get(), TEXT("nohighdpi")))
	{
		FDisplayMetrics DisplayMetrics;
		FDisplayMetrics::GetDisplayMetrics(DisplayMetrics);
		// find the monitor
		int32 XInt = static_cast<int32>(X);
		int32 YInt = static_cast<int32>(Y);
		for(int Idx = 0, NumMonitors = DisplayMetrics.MonitorInfo.Num(); Idx < NumMonitors; ++Idx)
		{
			const FMonitorInfo & MonitorInfo = DisplayMetrics.MonitorInfo[Idx];

			if (MonitorInfo.DisplayRect.Left <= XInt && MonitorInfo.DisplayRect.Right > XInt &&
				MonitorInfo.DisplayRect.Top <= YInt && MonitorInfo.DisplayRect.Bottom > YInt)
			{
				float HorzDPI = 1.0f, VertDPI = 1.0f;
				if (SDL_GetDisplayDPI(Idx, nullptr, &HorzDPI, &VertDPI) == 0)
				{
					float Scale = LinuxPlatformApplicationMisc::QuantizeScale((HorzDPI + VertDPI) / 192.0f);	// average between two scales (divided by 96.0f)
					UE_LOG(LogLinux, Log, TEXT("Scale at X=%f, Y=%f: %f (monitor=#%d, HDPI=%f (horz scale: %f), VDPI=%f (vert scale: %f))"), X, Y, Scale, Idx, HorzDPI, HorzDPI / 96.0f, VertDPI, VertDPI / 96.0f);
					return Scale;
				}
				else
				{
					// this can also happen for headless, so don't use Warning here
					UE_LOG(LogLinux, Log, TEXT("Could not get DPI information for monitor #%d, assuming 1.0f"), Idx);
					break;	// should fall-through to 1.0f
				}
			}
		}
	}
	return 1.0f;
}

void FLinuxPlatformApplicationMisc::ClipboardCopy(const TCHAR* Str)
{
	if (SDL_SetClipboardText(TCHAR_TO_UTF8(Str)))
	{
		UE_LOG(LogInit, Fatal, TEXT("Error copying clipboard contents: %s\n"), UTF8_TO_TCHAR(SDL_GetError()));
	}
}

void FLinuxPlatformApplicationMisc::ClipboardPaste(class FString& Result)
{
	char* ClipContent;
	ClipContent = SDL_GetClipboardText();

	if (!ClipContent)
	{
		UE_LOG(LogInit, Fatal, TEXT("Error pasting clipboard contents: %s\n"), UTF8_TO_TCHAR(SDL_GetError()));
		// unreachable
		Result = TEXT("");
	}
	else
	{
		Result = FString(UTF8_TO_TCHAR(ClipContent));
	}
	SDL_free(ClipContent);
}
