// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WindowsPlatformApplicationMisc.h"
#include "WindowsApplication.h"
#include "WindowsErrorOutputDevice.h"
#include "WindowsConsoleOutputDevice.h"
#include "WindowsFeedbackContext.h"
#include "HAL/FeedbackContextAnsi.h"
#include "Misc/App.h"
#include "Math/Color.h"
#include "WindowsHWrapper.h"
#include "Modules/ModuleManager.h"

// Resource includes.
#include "Runtime/Launch/Resources/Windows/Resource.h"

typedef HRESULT(STDAPICALLTYPE *GetDpiForMonitorProc)(HMONITOR Monitor, int32 DPIType, uint32 *DPIX, uint32 *DPIY);
extern CORE_API GetDpiForMonitorProc GetDpiForMonitor;

void FWindowsPlatformApplicationMisc::LoadPreInitModules()
{
	// D3D11 is not supported on WinXP, so in this case we use the OpenGL RHI
	if(FWindowsPlatformMisc::VerifyWindowsVersion(6, 0))
	{
		//#todo-rco: Only try on Win10
		const bool bForceD3D12 = FParse::Param(FCommandLine::Get(), TEXT("d3d12")) || FParse::Param(FCommandLine::Get(), TEXT("dx12"));
		if (bForceD3D12)
		{
			FModuleManager::Get().LoadModule(TEXT("D3D12RHI"));
		}
		FModuleManager::Get().LoadModule(TEXT("D3D11RHI"));
		// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
		FWindowsPlatformMisc::LoadVxgiModule();
#endif
		// NVCHANGE_END: Add VXGI
	}
	FModuleManager::Get().LoadModule(TEXT("OpenGLDrv"));
}

void FWindowsPlatformApplicationMisc::LoadStartupModules()
{
#if !UE_SERVER
	FModuleManager::Get().LoadModule(TEXT("XAudio2"));
	FModuleManager::Get().LoadModule(TEXT("HeadMountedDisplay"));
#endif // !UE_SERVER

#if WITH_EDITOR
	FModuleManager::Get().LoadModule(TEXT("SourceCodeAccess"));
#endif	//WITH_EDITOR
}

class FOutputDeviceConsole* FWindowsPlatformApplicationMisc::CreateConsoleOutputDevice()
{
	// this is a slightly different kind of singleton that gives ownership to the caller and should not be called more than once
	return new FWindowsConsoleOutputDevice();
}

class FOutputDeviceError* FWindowsPlatformApplicationMisc::GetErrorOutputDevice()
{
	static FWindowsErrorOutputDevice Singleton;
	return &Singleton;
}

class FFeedbackContext* FWindowsPlatformApplicationMisc::GetFeedbackContext()
{
#if WITH_EDITOR
	static FWindowsFeedbackContext Singleton;
	return &Singleton;
#else
	static FFeedbackContextAnsi Singleton;
	return &Singleton;
#endif
}

GenericApplication* FWindowsPlatformApplicationMisc::CreateApplication()
{
	HICON AppIconHandle = LoadIcon( hInstance, MAKEINTRESOURCE( GetAppIcon() ) );
	if( AppIconHandle == NULL )
	{
		AppIconHandle = LoadIcon( (HINSTANCE)NULL, IDI_APPLICATION ); 
	}

	return FWindowsApplication::CreateWindowsApplication( hInstance, AppIconHandle );
}

void FWindowsPlatformApplicationMisc::RequestMinimize()
{
	::ShowWindow(::GetActiveWindow(), SW_MINIMIZE);
}

bool FWindowsPlatformApplicationMisc::IsThisApplicationForeground()
{
	uint32 ForegroundProcess;
	::GetWindowThreadProcessId(GetForegroundWindow(), (::DWORD *)&ForegroundProcess);
	return (ForegroundProcess == GetCurrentProcessId());
}

int32 FWindowsPlatformApplicationMisc::GetAppIcon()
{
	return IDICON_UE4Game;
}

static void WinPumpMessages()
{
	{
		MSG Msg;
		while( PeekMessage(&Msg,NULL,0,0,PM_REMOVE) )
		{
			TranslateMessage( &Msg );
			DispatchMessage( &Msg );
		}
	}
}

static void WinPumpSentMessages()
{
	MSG Msg;
	PeekMessage(&Msg,NULL,0,0,PM_NOREMOVE | PM_QS_SENDMESSAGE);
}

void FWindowsPlatformApplicationMisc::PumpMessages(bool bFromMainLoop)
{
	if (!bFromMainLoop)
	{
		TGuardValue<bool> PumpMessageGuard( GPumpingMessagesOutsideOfMainLoop, true );
		// Process pending windows messages, which is necessary to the rendering thread in some rare cases where D3D
		// sends window messages (from IDXGISwapChain::Present) to the main thread owned viewport window.
		WinPumpSentMessages();
		return;
	}

	GPumpingMessagesOutsideOfMainLoop = false;
	WinPumpMessages();

	// Determine if application has focus
	bool HasFocus = FApp::UseVRFocus() ? FApp::HasVRFocus() : FWindowsPlatformApplicationMisc::IsThisApplicationForeground();

	// If editor thread doesn't have the focus, don't suck up too much CPU time.
	if( GIsEditor )
	{
		static bool HadFocus=1;
		if( HadFocus && !HasFocus )
		{
			// Drop our priority to speed up whatever is in the foreground.
			SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL );
		}
		else if( HasFocus && !HadFocus )
		{
			// Boost our priority back to normal.
			SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_NORMAL );
		}
		if( !HasFocus )
		{
			// Sleep for a bit to not eat up all CPU time.
			FPlatformProcess::Sleep(0.005f);
		}
		HadFocus = HasFocus;
	}

	// if its our window, allow sound, otherwise apply multiplier
	FApp::SetVolumeMultiplier( HasFocus ? 1.0f : FApp::GetUnfocusedVolumeMultiplier() );
}

void FWindowsPlatformApplicationMisc::PreventScreenSaver()
{
	INPUT Input = { 0 };
	Input.type = INPUT_MOUSE;
	Input.mi.dx = 0;
	Input.mi.dy = 0;	
	Input.mi.mouseData = 0;
	Input.mi.dwFlags = MOUSEEVENTF_MOVE;
	Input.mi.time = 0;
	Input.mi.dwExtraInfo = 0; 	
	SendInput(1,&Input,sizeof(INPUT));
}

FLinearColor FWindowsPlatformApplicationMisc::GetScreenPixelColor(const FVector2D& InScreenPos, float /*InGamma*/)
{
	COLORREF PixelColorRef = GetPixel(GetDC(HWND_DESKTOP), InScreenPos.X, InScreenPos.Y);

	FColor sRGBScreenColor(
		(PixelColorRef & 0xFF),
		((PixelColorRef & 0xFF00) >> 8),
		((PixelColorRef & 0xFF0000) >> 16),
		255);

	// Assume the screen color is coming in as sRGB space
	return FLinearColor(sRGBScreenColor);
}

bool FWindowsPlatformApplicationMisc::GetWindowTitleMatchingText(const TCHAR* TitleStartsWith, FString& OutTitle)
{
	bool bWasFound = false;
	WCHAR Buffer[8192];
	// Get the first window so we can start walking the window chain
	HWND hWnd = FindWindowW(NULL,NULL);
	if (hWnd != NULL)
	{
		size_t TitleStartsWithLen = _tcslen(TitleStartsWith);
		do
		{
			GetWindowText(hWnd,Buffer,8192);
			// If this matches, then grab the full text
			if (_tcsnccmp(TitleStartsWith, Buffer, TitleStartsWithLen) == 0)
			{
				OutTitle = Buffer;
				hWnd = NULL;
				bWasFound = true;
			}
			else
			{
				// Get the next window to interrogate
				hWnd = GetWindow(hWnd, GW_HWNDNEXT);
			}
		}
		while (hWnd != NULL);
	}
	return bWasFound;
}

float FWindowsPlatformApplicationMisc::GetDPIScaleFactorAtPoint(float X, float Y)
{
	if (GIsEditor && !FParse::Param(FCommandLine::Get(), TEXT("nohighdpi")))
	{
		if (GetDpiForMonitor)
		{
			POINT Position = { X, Y };
			HMONITOR Monitor = MonitorFromPoint(Position, MONITOR_DEFAULTTONEAREST);
			if (Monitor)
			{
				uint32 DPIX = 0;
				uint32 DPIY = 0;
				return SUCCEEDED(GetDpiForMonitor(Monitor, 0/*MDT_EFFECTIVE_DPI_VALUE*/, &DPIX, &DPIY)) ? DPIX / 96.0f : 1.0f;
			}
		}
		else
		{
			HDC Context = GetDC(nullptr);
			const float DPIScaleFactor = GetDeviceCaps(Context, LOGPIXELSX) / 96.0f;
			ReleaseDC(nullptr, Context);
			return DPIScaleFactor;
		}
	}
	return 1.0f;
}

// Disabling optimizations helps to reduce the frequency of OpenClipboard failing with error code 0. It still happens
// though only with really large text buffers and we worked around this by changing the editor to use an intermediate
// text buffer for internal operations.
PRAGMA_DISABLE_OPTIMIZATION 

void FWindowsPlatformApplicationMisc::ClipboardCopy(const TCHAR* Str)
{
	if( OpenClipboard(GetActiveWindow()) )
	{
		verify(EmptyClipboard());
		HGLOBAL GlobalMem;
		int32 StrLen = FCString::Strlen(Str);
		GlobalMem = GlobalAlloc( GMEM_MOVEABLE, sizeof(TCHAR)*(StrLen+1) );
		check(GlobalMem);
		TCHAR* Data = (TCHAR*) GlobalLock( GlobalMem );
		FCString::Strcpy( Data, (StrLen+1), Str );
		GlobalUnlock( GlobalMem );
		if( SetClipboardData( CF_UNICODETEXT, GlobalMem ) == NULL )
			UE_LOG(LogWindows, Fatal,TEXT("SetClipboardData failed with error code %i"), (uint32)GetLastError() );
		verify(CloseClipboard());
	}
}

void FWindowsPlatformApplicationMisc::ClipboardPaste(class FString& Result)
{
	if( OpenClipboard(GetActiveWindow()) )
	{
		HGLOBAL GlobalMem = NULL;
		bool Unicode = 0;
		GlobalMem = GetClipboardData( CF_UNICODETEXT );
		Unicode = 1;
		if( !GlobalMem )
		{
			GlobalMem = GetClipboardData( CF_TEXT );
			Unicode = 0;
		}
		if( !GlobalMem )
		{
			Result = TEXT("");
		}
		else
		{
			void* Data = GlobalLock( GlobalMem );
			check( Data );	
			if( Unicode )
				Result = (TCHAR*) Data;
			else
			{
				ANSICHAR* ACh = (ANSICHAR*) Data;
				int32 i;
				for( i=0; ACh[i]; i++ );
				TArray<TCHAR> Ch;
				Ch.AddUninitialized(i+1);
				for( i=0; i<Ch.Num(); i++ )
					Ch[i]=CharCast<TCHAR>(ACh[i]);
				Result.GetCharArray() = MoveTemp(Ch);
			}
			GlobalUnlock( GlobalMem );
		}
		verify(CloseClipboard());
	}
	else 
	{
		Result=TEXT("");
	}
}

PRAGMA_ENABLE_OPTIMIZATION 
