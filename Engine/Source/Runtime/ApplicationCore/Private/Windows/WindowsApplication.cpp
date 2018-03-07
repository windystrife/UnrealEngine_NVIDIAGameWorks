// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Windows/WindowsApplication.h"
#include "Containers/StringConv.h"
#include "Templates/ScopedPointer.h"
#include "CoreGlobals.h"
#include "Internationalization/Text.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/App.h"
#include "Windows/WindowsWindow.h"
#include "Windows/WindowsCursor.h"
#include "XInputInterface.h"
#include "Features/IModularFeatures.h"
#include "IInputDeviceModule.h"
#include "IInputDevice.h"
#include "IHapticDevice.h"
#include "HAL/ThreadHeartBeat.h"
#include "UniquePtr.h"

#if WITH_EDITOR
#include "ModuleManager.h"
#include "Developer/SourceCodeAccess/Public/ISourceCodeAccessModule.h"
#endif

// Allow Windows Platform types in the entire file.
#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include <ShlObj.h>
#include <objbase.h>
#include <SetupAPI.h>
#include <devguid.h>
#include <dwmapi.h>
#include <cfgmgr32.h>
#include <windowsx.h>
#include <VersionHelpers.h>
THIRD_PARTY_INCLUDES_END

// Platform code uses IsMaximized which is defined to IsZoomed by windowsx.h
#pragma push_macro("IsMaximized")
#undef IsMaximized

// This might not be defined by Windows when maintaining backwards-compatibility to pre-Vista builds
#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL                  0x020E
#endif

#ifndef WM_DPICHANGED
#define WM_DPICHANGED                   0x02E0
#endif

DEFINE_LOG_CATEGORY(LogWindowsDesktop);

/**
 * Hack to get around multiple mouse events being triggered for touch events.
 * Enabling this will prevent pen tablets from working since until we switch to the windows 8 sdk (and can use WM_POINTER*) events we cannot detect the difference
 */
static int32 bPreventDuplicateMouseEventsForTouch = false;

FAutoConsoleVariableRef	CVarPreventDuplicateMouseEventsForTouch(
	TEXT("Slate.PreventDuplicateMouseEventsForTouchForWindows7"),
	bPreventDuplicateMouseEventsForTouch,
	TEXT("Hack to get around multiple mouse events being triggered for touch events on Windows 7 and lower.  Enabling this will prevent pen tablets from working on windows 7 since until we switch to the windows 8 sdk (and can use WM_POINTER* events) we cannot detect the difference")
);

const FIntPoint FWindowsApplication::MinimizedWindowPosition(-32000,-32000);

FWindowsApplication* WindowsApplication = nullptr;


FWindowsApplication* FWindowsApplication::CreateWindowsApplication( const HINSTANCE InstanceHandle, const HICON IconHandle )
{
	WindowsApplication = new FWindowsApplication( InstanceHandle, IconHandle );
	return WindowsApplication;
}


FWindowsApplication::FWindowsApplication( const HINSTANCE HInstance, const HICON IconHandle )
	: GenericApplication( MakeShareable( new FWindowsCursor() ) )
	, InstanceHandle( HInstance )
	, bUsingHighPrecisionMouseInput( false )
	, bIsMouseAttached( false )
	, bForceActivateByMouse( false )
	, XInput( XInputInterface::Create( MessageHandler ) )
	, bHasLoadedInputPlugins( false )
	, bAllowedToDeferMessageProcessing(true)
	, CVarDeferMessageProcessing( 
		TEXT( "Slate.DeferWindowsMessageProcessing" ),
		bAllowedToDeferMessageProcessing,
		TEXT( "Whether windows message processing is deferred until tick or if they are processed immediately" ) )
	, bInModalSizeLoop( false )

{
	FMemory::Memzero(ModifierKeyState, EModifierKey::Count);

	// Disable the process from being showing "ghosted" not responding messages during slow tasks
	// This is a hack.  A more permanent solution is to make our slow tasks not block the editor for so long
	// that message pumping doesn't occur (which causes these messages).
	::DisableProcessWindowsGhosting();

	if (GIsEditor)
	{
		FWindowsPlatformMisc::SetHighDPIMode();
	}

	// Register the Win32 class for Slate windows and assign the application instance and icon
	const bool bClassRegistered = RegisterClass( InstanceHandle, IconHandle );

	// Initialize OLE for Drag and Drop support.
	CA_SUPPRESS(6031);
	OleInitialize( NULL );

	TextInputMethodSystem = MakeShareable( new FWindowsTextInputMethodSystem );
	if(!TextInputMethodSystem->Initialize())
	{
		TextInputMethodSystem.Reset();
	}

	TaskbarList = FTaskbarList::Create();

	// Get initial display metrics. (display information for existing desktop, before we start changing resolutions)
	FDisplayMetrics::GetDisplayMetrics(InitialDisplayMetrics);

	// Save the current sticky/toggle/filter key settings so they can be restored them later
	// If there are .ini settings, use them instead of the current system settings.
	// NOTE: Whenever we exit and restore these settings gracefully, the .ini settings are removed.
	FMemory::Memzero(StartupStickyKeys);
	FMemory::Memzero(StartupToggleKeys);
	FMemory::Memzero(StartupFilterKeys);
	
	StartupStickyKeys.cbSize = sizeof(StartupStickyKeys);
	StartupToggleKeys.cbSize = sizeof(StartupToggleKeys);
	StartupFilterKeys.cbSize = sizeof(StartupFilterKeys);

	SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &StartupStickyKeys, 0);
	SystemParametersInfo(SPI_GETTOGGLEKEYS, sizeof(TOGGLEKEYS), &StartupToggleKeys, 0);
	SystemParametersInfo(SPI_GETFILTERKEYS, sizeof(FILTERKEYS), &StartupFilterKeys, 0);

	bool bSKHotkey = (StartupStickyKeys.dwFlags & SKF_HOTKEYACTIVE) ? true : false;
	bool bTKHotkey = (StartupToggleKeys.dwFlags & TKF_HOTKEYACTIVE) ? true : false;
	bool bFKHotkey = (StartupFilterKeys.dwFlags & FKF_HOTKEYACTIVE) ? true : false;
	bool bSKConfirmation = (StartupStickyKeys.dwFlags & SKF_CONFIRMHOTKEY) ? true : false;
	bool bTKConfirmation = (StartupToggleKeys.dwFlags & TKF_CONFIRMHOTKEY) ? true : false;
	bool bFKConfirmation = (StartupFilterKeys.dwFlags & FKF_CONFIRMHOTKEY) ? true : false;

	GConfig->GetBool(TEXT("WindowsApplication.Accessibility"), TEXT("StickyKeysHotkey"), bSKHotkey, GEngineIni);
	GConfig->GetBool(TEXT("WindowsApplication.Accessibility"), TEXT("ToggleKeysHotkey"), bTKHotkey, GEngineIni);
	GConfig->GetBool(TEXT("WindowsApplication.Accessibility"), TEXT("FilterKeysHotkey"), bFKHotkey, GEngineIni);
	GConfig->GetBool(TEXT("WindowsApplication.Accessibility"), TEXT("StickyKeysConfirmation"), bSKConfirmation, GEngineIni);
	GConfig->GetBool(TEXT("WindowsApplication.Accessibility"), TEXT("ToggleKeysConfirmation"), bTKConfirmation, GEngineIni);
	GConfig->GetBool(TEXT("WindowsApplication.Accessibility"), TEXT("FilterKeysConfirmation"), bFKConfirmation, GEngineIni);

	StartupStickyKeys.dwFlags = bSKHotkey ? (StartupStickyKeys.dwFlags | SKF_HOTKEYACTIVE) : (StartupStickyKeys.dwFlags & ~SKF_HOTKEYACTIVE);
	StartupToggleKeys.dwFlags = bTKHotkey ? (StartupToggleKeys.dwFlags | TKF_HOTKEYACTIVE) : (StartupToggleKeys.dwFlags & ~TKF_HOTKEYACTIVE);
	StartupFilterKeys.dwFlags = bFKHotkey ? (StartupFilterKeys.dwFlags | FKF_HOTKEYACTIVE) : (StartupFilterKeys.dwFlags & ~FKF_HOTKEYACTIVE);
	StartupStickyKeys.dwFlags = bSKConfirmation ? (StartupStickyKeys.dwFlags | SKF_CONFIRMHOTKEY) : (StartupStickyKeys.dwFlags & ~SKF_CONFIRMHOTKEY);
	StartupToggleKeys.dwFlags = bTKConfirmation ? (StartupToggleKeys.dwFlags | TKF_CONFIRMHOTKEY) : (StartupToggleKeys.dwFlags & ~TKF_CONFIRMHOTKEY);
	StartupFilterKeys.dwFlags = bFKConfirmation ? (StartupFilterKeys.dwFlags | FKF_CONFIRMHOTKEY) : (StartupFilterKeys.dwFlags & ~FKF_CONFIRMHOTKEY);

	GConfig->SetBool(TEXT("WindowsApplication.Accessibility"), TEXT("StickyKeysHotkey"), bSKHotkey, GEngineIni);
	GConfig->SetBool(TEXT("WindowsApplication.Accessibility"), TEXT("ToggleKeysHotkey"), bTKHotkey, GEngineIni);
	GConfig->SetBool(TEXT("WindowsApplication.Accessibility"), TEXT("FilterKeysHotkey"), bFKHotkey, GEngineIni);
	GConfig->SetBool(TEXT("WindowsApplication.Accessibility"), TEXT("StickyKeysConfirmation"), bSKConfirmation, GEngineIni);
	GConfig->SetBool(TEXT("WindowsApplication.Accessibility"), TEXT("ToggleKeysConfirmation"), bTKConfirmation, GEngineIni);
	GConfig->SetBool(TEXT("WindowsApplication.Accessibility"), TEXT("FilterKeysConfirmation"), bFKConfirmation, GEngineIni);

	GConfig->Flush(false, GEngineIni);

	FCoreDelegates::OnShutdownAfterError.AddRaw(this, &FWindowsApplication::ShutDownAfterError);

	// Disable accessibility shortcuts
	AllowAccessibilityShortcutKeys(false);

	QueryConnectedMice();
}

void FWindowsApplication::AllowAccessibilityShortcutKeys(const bool bAllowKeys)
{
	if (bAllowKeys)
	{
		// Restore StickyKeys/etc to original state and enable Windows key      
		SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &StartupStickyKeys, 0);
		SystemParametersInfo(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &StartupToggleKeys, 0);
		SystemParametersInfo(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &StartupFilterKeys, 0);
	}
	else
	{
		// Disable StickyKeys/etc shortcuts but if the accessibility feature is on, 
		// then leave the settings alone as its probably being usefully used

		STICKYKEYS skOff = StartupStickyKeys;
		if ((skOff.dwFlags & SKF_STICKYKEYSON) == 0)
		{
			// Disable the hotkey and the confirmation
			skOff.dwFlags &= ~SKF_HOTKEYACTIVE;
			skOff.dwFlags &= ~SKF_CONFIRMHOTKEY;

			SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &skOff, 0);
		}

		TOGGLEKEYS tkOff = StartupToggleKeys;
		if ((tkOff.dwFlags & TKF_TOGGLEKEYSON) == 0)
		{
			// Disable the hotkey and the confirmation
			tkOff.dwFlags &= ~TKF_HOTKEYACTIVE;
			tkOff.dwFlags &= ~TKF_CONFIRMHOTKEY;

			SystemParametersInfo(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &tkOff, 0);
		}

		FILTERKEYS fkOff = StartupFilterKeys;
		if ((fkOff.dwFlags & FKF_FILTERKEYSON) == 0)
		{
			// Disable the hotkey and the confirmation
			fkOff.dwFlags &= ~FKF_HOTKEYACTIVE;
			fkOff.dwFlags &= ~FKF_CONFIRMHOTKEY;

			SystemParametersInfo(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &fkOff, 0);
		}
	}
}

void FWindowsApplication::DestroyApplication()
{
	// Restore accessibility shortcuts and remove the saved state from the .ini
	AllowAccessibilityShortcutKeys(true);
	GConfig->EmptySection(TEXT("WindowsApplication.Accessibility"), GEngineIni);

	TaskbarList = nullptr;
}

void FWindowsApplication::ShutDownAfterError()
{
	// Restore accessibility shortcuts and remove the saved state from the .ini
	AllowAccessibilityShortcutKeys(true);
	GConfig->EmptySection(TEXT("WindowsApplication.Accessibility"), GEngineIni);

	TaskbarList = nullptr;
}

bool FWindowsApplication::RegisterClass( const HINSTANCE HInstance, const HICON HIcon )
{
	WNDCLASS wc;
	FMemory::Memzero( &wc, sizeof(wc) );
	wc.style = CS_DBLCLKS; // We want to receive double clicks
	wc.lpfnWndProc = AppWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = HInstance;
	wc.hIcon = HIcon;
	wc.hCursor = NULL;	// We manage the cursor ourselves
	wc.hbrBackground = NULL; // Transparent
	wc.lpszMenuName = NULL;
	wc.lpszClassName = FWindowsWindow::AppWindowClass;

	if( !::RegisterClass( &wc ) )
	{
		//ShowLastError();

		// @todo Slate: Error message should be localized!
		FSlowHeartBeatScope SuspendHeartBeat;
		MessageBox(NULL, TEXT("Window Registration Failed!"), TEXT("Error!"), MB_ICONEXCLAMATION | MB_OK);

		return false;
	}

	return true;
}

FWindowsApplication::~FWindowsApplication()
{
	if (TextInputMethodSystem.IsValid())
	{
		TextInputMethodSystem->Terminate();
	}

	::CoUninitialize();
	OleUninitialize();
}

TSharedRef< FGenericWindow > FWindowsApplication::MakeWindow() 
{ 
	return FWindowsWindow::Make(); 
}

void FWindowsApplication::InitializeWindow( const TSharedRef< FGenericWindow >& InWindow, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FGenericWindow >& InParent, const bool bShowImmediately )
{
	const TSharedRef< FWindowsWindow > Window = StaticCastSharedRef< FWindowsWindow >( InWindow );
	const TSharedPtr< FWindowsWindow > ParentWindow = StaticCastSharedPtr< FWindowsWindow >( InParent );

	Windows.Add( Window );
	Window->Initialize( this, InDefinition, InstanceHandle, ParentWindow, bShowImmediately );
}

void FWindowsApplication::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	GenericApplication::SetMessageHandler(InMessageHandler);
	XInput->SetMessageHandler( InMessageHandler );

	TArray<IInputDeviceModule*> PluginImplementations = IModularFeatures::Get().GetModularFeatureImplementations<IInputDeviceModule>( IInputDeviceModule::GetModularFeatureName() );
	for( auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt )
	{
		(*DeviceIt)->SetMessageHandler(InMessageHandler);
	}

}

bool FWindowsApplication::IsGamepadAttached() const
{
	if (XInput->IsGamepadAttached())
	{
		return true;
	}

	for( auto DeviceIt = ExternalInputDevices.CreateConstIterator(); DeviceIt; ++DeviceIt )
	{
		if ((*DeviceIt)->IsGamepadAttached())
		{
			return true;
		}
	}

	return false;
}

FModifierKeysState FWindowsApplication::GetModifierKeys() const
{
	return FModifierKeysState(
		ModifierKeyState[EModifierKey::LeftShift], ModifierKeyState[EModifierKey::RightShift], 
		ModifierKeyState[EModifierKey::LeftControl], ModifierKeyState[EModifierKey::RightControl], 
		ModifierKeyState[EModifierKey::LeftAlt], ModifierKeyState[EModifierKey::RightAlt], 
		false, false, 
		ModifierKeyState[EModifierKey::CapsLock]
		); // Win key is ignored
}

void FWindowsApplication::UpdateAllModifierKeyStates()
{
	ModifierKeyState[EModifierKey::LeftShift]		= (::GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0;
	ModifierKeyState[EModifierKey::RightShift]		= (::GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;
	ModifierKeyState[EModifierKey::LeftControl]		= (::GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0;
	ModifierKeyState[EModifierKey::RightControl]	= (::GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0;
	ModifierKeyState[EModifierKey::LeftAlt]			= (::GetAsyncKeyState(VK_LMENU) & 0x8000) != 0;
	ModifierKeyState[EModifierKey::RightAlt]		= (::GetAsyncKeyState(VK_RMENU) & 0x8000) != 0;
	ModifierKeyState[EModifierKey::CapsLock]		= (::GetKeyState(VK_CAPITAL) & 0x0001) != 0;
}

static TSharedPtr< FWindowsWindow > FindWindowByHWND(const TArray< TSharedRef< FWindowsWindow > >& WindowsToSearch, HWND HandleToFind)
{
	for (int32 WindowIndex = 0; WindowIndex < WindowsToSearch.Num(); ++WindowIndex)
	{
		TSharedRef< FWindowsWindow > Window = WindowsToSearch[WindowIndex];
		if (Window->GetHWnd() == HandleToFind)
		{
			return Window;
		}
	}

	return TSharedPtr< FWindowsWindow >(nullptr);
}


bool FWindowsApplication::IsCursorDirectlyOverSlateWindow() const
{
	POINT CursorPos;
	BOOL bGotPoint = ::GetCursorPos(&CursorPos);
	if (bGotPoint)
	{
		HWND hWnd = ::WindowFromPoint(CursorPos);

		TSharedPtr< FWindowsWindow > SlatWindowUnderCursor = FindWindowByHWND(Windows, hWnd);
		return SlatWindowUnderCursor.IsValid();
	}
	return false;
}


void FWindowsApplication::SetCapture( const TSharedPtr< FGenericWindow >& InWindow )
{
	if ( InWindow.IsValid() )
	{
		::SetCapture( (HWND)InWindow->GetOSWindowHandle() );
	}
	else
	{
		::ReleaseCapture();
	}
}

void* FWindowsApplication::GetCapture( void ) const
{
	return ::GetCapture();
}

void FWindowsApplication::SetHighPrecisionMouseMode( const bool Enable, const TSharedPtr< FGenericWindow >& InWindow )
{
	HWND hwnd = NULL;
	DWORD flags = RIDEV_REMOVE;
	bUsingHighPrecisionMouseInput = Enable;

	if ( Enable )
	{
		flags = 0;

		if ( InWindow.IsValid() )
		{
			hwnd = (HWND)InWindow->GetOSWindowHandle(); 
		}
	}

	// NOTE: Currently has to be created every time due to conflicts with Direct8 Input used by the wx unrealed
	RAWINPUTDEVICE RawInputDevice;

	//The HID standard for mouse
	const uint16 StandardMouse = 0x02;

	RawInputDevice.usUsagePage = 0x01; 
	RawInputDevice.usUsage = StandardMouse;
	RawInputDevice.dwFlags = flags;

	// Process input for just the window that requested it.  NOTE: If we pass NULL here events are routed to the window with keyboard focus
	// which is not always known at the HWND level with Slate
	RawInputDevice.hwndTarget = hwnd; 

	// Register the raw input device
	::RegisterRawInputDevices( &RawInputDevice, 1, sizeof( RAWINPUTDEVICE ) );
}

FPlatformRect FWindowsApplication::GetWorkArea( const FPlatformRect& CurrentWindow ) const
{
	RECT WindowsWindowDim;
	WindowsWindowDim.left = CurrentWindow.Left;
	WindowsWindowDim.top = CurrentWindow.Top;
	WindowsWindowDim.right = CurrentWindow.Right;
	WindowsWindowDim.bottom = CurrentWindow.Bottom;

	// ... figure out the best monitor for that window.
	HMONITOR hBestMonitor = MonitorFromRect( &WindowsWindowDim, MONITOR_DEFAULTTONEAREST );

	// Get information about that monitor...
	MONITORINFO MonitorInfo;
	MonitorInfo.cbSize = sizeof(MonitorInfo);
	GetMonitorInfo( hBestMonitor, &MonitorInfo);

	// ... so that we can figure out the work area (are not covered by taskbar)

	FPlatformRect WorkArea;
	WorkArea.Left = MonitorInfo.rcWork.left;
	WorkArea.Top = MonitorInfo.rcWork.top;
	WorkArea.Right = MonitorInfo.rcWork.right;
	WorkArea.Bottom = MonitorInfo.rcWork.bottom;

	return WorkArea;
}

/**
 * Extracts EDID data from the given registry key and reads out native display with and height
 * @param hDevRegKey - Registry key where EDID is stored
 * @param OutWidth - Reference to output variable for monitor native width
 * @param OutHeight - Reference to output variable for monitor native height
 * @returns 'true' if data was extracted successfully, 'false' otherwise
 **/
static bool GetMonitorSizeFromEDID(const HKEY hDevRegKey, int32& OutWidth, int32& OutHeight)
{	
	static const uint32 NameSize = 512;
	static TCHAR ValueName[NameSize];

	DWORD Type;
	DWORD ActualValueNameLength = NameSize;

	BYTE EDIDData[1024];
	DWORD EDIDSize = sizeof(EDIDData);

	for (LONG i = 0, RetValue = ERROR_SUCCESS; RetValue != ERROR_NO_MORE_ITEMS; ++i)
	{
		RetValue = RegEnumValue ( hDevRegKey, 
			i, 
			&ValueName[0],
			&ActualValueNameLength, NULL, &Type,
			EDIDData,
			&EDIDSize);

		if (RetValue != ERROR_SUCCESS || (FCString::Strcmp(ValueName, TEXT("EDID")) != 0))
		{
			continue;
		}

		// EDID data format documented here:
		// http://en.wikipedia.org/wiki/EDID

		int DetailTimingDescriptorStartIndex = 54;
		OutWidth = ((EDIDData[DetailTimingDescriptorStartIndex+4] >> 4) << 8) | EDIDData[DetailTimingDescriptorStartIndex+2];
		OutHeight = ((EDIDData[DetailTimingDescriptorStartIndex+7] >> 4) << 8) | EDIDData[DetailTimingDescriptorStartIndex+5];

		return true; // valid EDID found
	}

	return false; // EDID not found
}

/**
 * Locate registry record for the given display device ID and extract native size information
 * @param TargetDevID - Name of taret device
 * @praam OutWidth - Reference to output variable for monitor native width
 * @praam OutHeight - Reference to output variable for monitor native height
 * @returns TRUE if data was extracted successfully, FALSE otherwise
 **/
inline bool GetSizeForDevID(const FString& TargetDevID, int32& Width, int32& Height)
{
	static const GUID ClassMonitorGuid = {0x4d36e96e, 0xe325, 0x11ce, {0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18}};

	HDEVINFO DevInfo = SetupDiGetClassDevsEx(
		&ClassMonitorGuid, //class GUID
		NULL,
		NULL,
		DIGCF_PRESENT,
		NULL,
		NULL,
		NULL);

	if (NULL == DevInfo)
	{
		return false;
	}

	bool bRes = false;

	for (ULONG MonitorIndex = 0; ERROR_NO_MORE_ITEMS != GetLastError(); ++MonitorIndex)
	{ 
		SP_DEVINFO_DATA DevInfoData;
		ZeroMemory(&DevInfoData, sizeof(DevInfoData));
		DevInfoData.cbSize = sizeof(DevInfoData);

		if (SetupDiEnumDeviceInfo(DevInfo, MonitorIndex, &DevInfoData) == TRUE)
		{
			TCHAR Buffer[MAX_DEVICE_ID_LEN];
			if (CM_Get_Device_ID(DevInfoData.DevInst, Buffer, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS)
			{
				FString DevID(Buffer);
				DevID = DevID.Mid(8, DevID.Find(TEXT("\\"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 9) - 8);
				if (DevID == TargetDevID)
				{
					HKEY hDevRegKey = SetupDiOpenDevRegKey(DevInfo, &DevInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);

					if (hDevRegKey && hDevRegKey != INVALID_HANDLE_VALUE)
					{
						bRes = GetMonitorSizeFromEDID(hDevRegKey, Width, Height);
						RegCloseKey(hDevRegKey);
						break;
					}
				}
			}
		}
	}

	if (SetupDiDestroyDeviceInfoList(DevInfo) == FALSE)
	{
		bRes = false;
	}

	return bRes;
}

static BOOL CALLBACK MonitorEnumProc(HMONITOR Monitor, HDC MonitorDC, LPRECT Rect, LPARAM UserData)
{
	MONITORINFOEX MonitorInfoEx;
	MonitorInfoEx.cbSize = sizeof(MonitorInfoEx);
	GetMonitorInfo(Monitor, &MonitorInfoEx);

	FMonitorInfo* Info = (FMonitorInfo*)UserData;
	if (Info->Name == MonitorInfoEx.szDevice)
	{
		Info->DisplayRect.Bottom = MonitorInfoEx.rcMonitor.bottom;
		Info->DisplayRect.Left = MonitorInfoEx.rcMonitor.left;
		Info->DisplayRect.Right = MonitorInfoEx.rcMonitor.right;
		Info->DisplayRect.Top = MonitorInfoEx.rcMonitor.top;

		Info->WorkArea.Bottom = MonitorInfoEx.rcWork.bottom;
		Info->WorkArea.Left = MonitorInfoEx.rcWork.left;
		Info->WorkArea.Right = MonitorInfoEx.rcWork.right;
		Info->WorkArea.Top = MonitorInfoEx.rcWork.top;

		return FALSE;
	}

	return TRUE;
}

/**
 * Extract hardware information about connect monitors
 * @param OutMonitorInfo - Reference to an array for holding records about each detected monitor
 **/
static void GetMonitorsInfo(TArray<FMonitorInfo>& OutMonitorInfo)
{
	DISPLAY_DEVICE DisplayDevice;
	DisplayDevice.cb = sizeof(DisplayDevice);
	DWORD DeviceIndex = 0; // device index

	FMonitorInfo* PrimaryDevice = nullptr;
	OutMonitorInfo.Empty(2); // Reserve two slots, as that will be the most common maximum

	FString DeviceID;
	while (EnumDisplayDevices(0, DeviceIndex, &DisplayDevice, 0))
	{
		if ((DisplayDevice.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) > 0)
		{
			DISPLAY_DEVICE Monitor;
			ZeroMemory(&Monitor, sizeof(Monitor));
			Monitor.cb = sizeof(Monitor);
			DWORD MonitorIndex = 0;

			while (EnumDisplayDevices(DisplayDevice.DeviceName, MonitorIndex, &Monitor, 0))
			{
				if (Monitor.StateFlags & DISPLAY_DEVICE_ACTIVE &&
					!(Monitor.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER))
				{
					FMonitorInfo Info;

					Info.Name = DisplayDevice.DeviceName;
					EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, (LPARAM)&Info);

					Info.ID = FString::Printf(TEXT("%s"), Monitor.DeviceID);
					Info.Name = Info.ID.Mid (8, Info.ID.Find (TEXT("\\"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 9) - 8);

					if (GetSizeForDevID(Info.Name, Info.NativeWidth, Info.NativeHeight))
					{
						Info.ID = Monitor.DeviceID;
						Info.bIsPrimary = (DisplayDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) > 0;
						OutMonitorInfo.Add(Info);

						if (PrimaryDevice == nullptr && Info.bIsPrimary)
						{
							PrimaryDevice = &OutMonitorInfo.Last();
						}
					}
				}
				MonitorIndex++;

				ZeroMemory(&Monitor, sizeof(Monitor));
				Monitor.cb = sizeof(Monitor);
			}
		}

		ZeroMemory(&DisplayDevice, sizeof(DisplayDevice));
		DisplayDevice.cb = sizeof(DisplayDevice);
		DeviceIndex++;
	}
}

void FDisplayMetrics::GetDisplayMetrics(struct FDisplayMetrics& OutDisplayMetrics)
{
	// Total screen size of the primary monitor
	OutDisplayMetrics.PrimaryDisplayWidth = ::GetSystemMetrics( SM_CXSCREEN );
	OutDisplayMetrics.PrimaryDisplayHeight = ::GetSystemMetrics( SM_CYSCREEN );

	// Get the screen rect of the primary monitor, excluding taskbar etc.
	RECT WorkAreaRect;
	if(!SystemParametersInfo(SPI_GETWORKAREA, 0, &WorkAreaRect, 0))
	{
		WorkAreaRect.top = WorkAreaRect.bottom = WorkAreaRect.left = WorkAreaRect.right = 0;
	}

	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Left = WorkAreaRect.left;
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Top = WorkAreaRect.top;
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Right = WorkAreaRect.right;
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom = WorkAreaRect.bottom;
	
	// Virtual desktop area
	OutDisplayMetrics.VirtualDisplayRect.Left = ::GetSystemMetrics( SM_XVIRTUALSCREEN );
	OutDisplayMetrics.VirtualDisplayRect.Top = ::GetSystemMetrics( SM_YVIRTUALSCREEN );
	OutDisplayMetrics.VirtualDisplayRect.Right = OutDisplayMetrics.VirtualDisplayRect.Left + ::GetSystemMetrics( SM_CXVIRTUALSCREEN );
	OutDisplayMetrics.VirtualDisplayRect.Bottom = OutDisplayMetrics.VirtualDisplayRect.Top + ::GetSystemMetrics( SM_CYVIRTUALSCREEN );

	// Get connected monitor information
	GetMonitorsInfo(OutDisplayMetrics.MonitorInfo);

	// Apply the debug safe zones
	OutDisplayMetrics.ApplyDefaultSafeZones();
}

void FWindowsApplication::GetInitialDisplayMetrics( FDisplayMetrics& OutDisplayMetrics ) const
{
	OutDisplayMetrics = InitialDisplayMetrics;
}

EWindowTitleAlignment::Type FWindowsApplication::GetWindowTitleAlignment() const
{
	OSVERSIONINFOEX VersionInfo;
	FMemory::Memzero(VersionInfo);
	VersionInfo.dwMajorVersion = 6;
	VersionInfo.dwMinorVersion = 2;
	VersionInfo.dwOSVersionInfoSize = sizeof(VersionInfo);

	DWORDLONG LongConditionMask = 0;
	int ConditionMask = VER_GREATER_EQUAL;
	VER_SET_CONDITION(LongConditionMask, VER_MAJORVERSION, ConditionMask);
	VER_SET_CONDITION(LongConditionMask, VER_MINORVERSION, ConditionMask);

	if (::VerifyVersionInfo(&VersionInfo, VER_MAJORVERSION | VER_MINORVERSION, LongConditionMask) != 0)
	{
		return EWindowTitleAlignment::Center;
	}		

	return EWindowTitleAlignment::Left;
}
	
EWindowTransparency FWindowsApplication::GetWindowTransparencySupport() const
{
#if ALPHA_BLENDED_WINDOWS
	BOOL bIsCompositionEnabled = FALSE;
	::DwmIsCompositionEnabled(&bIsCompositionEnabled);

	return bIsCompositionEnabled ? EWindowTransparency::PerPixel : EWindowTransparency::PerWindow;
#else
	return EWindowTransparency::PerWindow;
#endif
}


LRESULT CALLBACK FWindowsApplication::AppWndProc(HWND hwnd, uint32 msg, WPARAM wParam, LPARAM lParam)
{
	ensure( IsInGameThread() );

	return WindowsApplication->ProcessMessage( hwnd, msg, wParam, lParam );
}

int32 FWindowsApplication::ProcessMessage( HWND hwnd, uint32 msg, WPARAM wParam, LPARAM lParam )
{
	TSharedPtr< FWindowsWindow > CurrentNativeEventWindowPtr = FindWindowByHWND( Windows, hwnd );

	if( Windows.Num() && CurrentNativeEventWindowPtr.IsValid() )
	{
		TSharedRef< FWindowsWindow > CurrentNativeEventWindow = CurrentNativeEventWindowPtr.ToSharedRef();

		static const TMap<uint32, FString> WindowsMessageStrings = []()
		{
			TMap<uint32, FString> Result;
#define ADD_WINDOWS_MESSAGE_STRING(WMCode) Result.Add(WMCode, TEXT(#WMCode))
			ADD_WINDOWS_MESSAGE_STRING(WM_INPUTLANGCHANGEREQUEST);
			ADD_WINDOWS_MESSAGE_STRING(WM_INPUTLANGCHANGE);
			ADD_WINDOWS_MESSAGE_STRING(WM_IME_SETCONTEXT);
			ADD_WINDOWS_MESSAGE_STRING(WM_IME_NOTIFY);
			ADD_WINDOWS_MESSAGE_STRING(WM_IME_REQUEST);
			ADD_WINDOWS_MESSAGE_STRING(WM_IME_STARTCOMPOSITION);
			ADD_WINDOWS_MESSAGE_STRING(WM_IME_COMPOSITION);
			ADD_WINDOWS_MESSAGE_STRING(WM_IME_ENDCOMPOSITION);
			ADD_WINDOWS_MESSAGE_STRING(WM_IME_CHAR);
#undef ADD_WINDOWS_MESSAGE_STRING
			return Result;
		}();

		static const TMap<uint32, FString> IMNStrings = []()
		{
			TMap<uint32, FString> Result;
#define ADD_IMN_STRING(IMNCode) Result.Add(IMNCode, TEXT(#IMNCode))
			ADD_IMN_STRING(IMN_CLOSESTATUSWINDOW);
			ADD_IMN_STRING(IMN_OPENSTATUSWINDOW);
			ADD_IMN_STRING(IMN_CHANGECANDIDATE);
			ADD_IMN_STRING(IMN_CLOSECANDIDATE);
			ADD_IMN_STRING(IMN_OPENCANDIDATE);
			ADD_IMN_STRING(IMN_SETCONVERSIONMODE);
			ADD_IMN_STRING(IMN_SETSENTENCEMODE);
			ADD_IMN_STRING(IMN_SETOPENSTATUS);
			ADD_IMN_STRING(IMN_SETCANDIDATEPOS);
			ADD_IMN_STRING(IMN_SETCOMPOSITIONFONT);
			ADD_IMN_STRING(IMN_SETCOMPOSITIONWINDOW);
			ADD_IMN_STRING(IMN_SETSTATUSWINDOWPOS);
			ADD_IMN_STRING(IMN_GUIDELINE);
			ADD_IMN_STRING(IMN_PRIVATE);
#undef ADD_IMN_STRING
			return Result;
		}();

		static const TMap<uint32, FString> IMRStrings = []()
		{
			TMap<uint32, FString> Result;
#define ADD_IMR_STRING(IMRCode) Result.Add(IMRCode, TEXT(#IMRCode))
	ADD_IMR_STRING(IMR_CANDIDATEWINDOW);
	ADD_IMR_STRING(IMR_COMPOSITIONFONT);
	ADD_IMR_STRING(IMR_COMPOSITIONWINDOW);
	ADD_IMR_STRING(IMR_CONFIRMRECONVERTSTRING);
	ADD_IMR_STRING(IMR_DOCUMENTFEED);
	ADD_IMR_STRING(IMR_QUERYCHARPOSITION);
	ADD_IMR_STRING(IMR_RECONVERTSTRING);
#undef ADD_IMR_STRING
			return Result;
		}();

		bool bMessageExternallyHandled = false;
		int32 ExternalMessageHandlerResult = 0;

		// give others a chance to handle messages
		for (IWindowsMessageHandler* Handler : MessageHandlers)
		{
			int32 HandlerResult = 0;
			if (Handler->ProcessMessage(hwnd, msg, wParam, lParam, HandlerResult))
			{
				if (!bMessageExternallyHandled)
				{
					bMessageExternallyHandled = true;
					ExternalMessageHandlerResult = HandlerResult;
				}
			}
		}

#if WINVER >= 0x0601
		// ORION - REMOVED FOR WACOM SUPPORT!
		/*
		if (IsFakeMouseInputMessage(msg))
		{
			return 0;
		}
		*/
#endif

		switch(msg)
		{
		case WM_INPUTLANGCHANGEREQUEST:
		case WM_INPUTLANGCHANGE:
		case WM_IME_SETCONTEXT:
		case WM_IME_STARTCOMPOSITION:
		case WM_IME_COMPOSITION:
		case WM_IME_ENDCOMPOSITION:
		case WM_IME_CHAR:
			UE_LOG(LogWindowsDesktop, Verbose, TEXT("%s"), *(WindowsMessageStrings[msg]));
			DeferMessage( CurrentNativeEventWindowPtr, hwnd, msg, wParam, lParam );
			return 0;
		case WM_IME_NOTIFY:
			UE_LOG(LogWindowsDesktop, Verbose, TEXT("WM_IME_NOTIFY - %s"), IMNStrings.Find(wParam) ? *(IMNStrings[wParam]) : nullptr);
			DeferMessage( CurrentNativeEventWindowPtr, hwnd, msg, wParam, lParam );
			return 0;
		case WM_IME_REQUEST:
			UE_LOG(LogWindowsDesktop, Verbose, TEXT("WM_IME_REQUEST - %s"), IMRStrings.Find(wParam) ? *(IMRStrings[wParam]) : nullptr);
			DeferMessage( CurrentNativeEventWindowPtr, hwnd, msg, wParam, lParam );
			return 0;
			// Character
		case WM_CHAR:
			DeferMessage( CurrentNativeEventWindowPtr, hwnd, msg, wParam, lParam );
			return 0;
		case WM_SYSCHAR:
			{
				if( ( HIWORD(lParam) & 0x2000 ) != 0 && wParam == VK_SPACE )
				{
					// Do not handle Alt+Space so that it passes through and opens the window system menu
					break;
				}
				else
				{
					return 0;
				}
			}
			
			break;

		case WM_SYSKEYDOWN:
			{
				// Alt-F4 or Alt+Space was pressed. 
				// Allow alt+f4 to close the window and alt+space to open the window menu
				if( wParam != VK_F4 && wParam != VK_SPACE)
				{
					DeferMessage( CurrentNativeEventWindowPtr, hwnd, msg, wParam, lParam );
				}
			}
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYUP:
		case WM_LBUTTONDBLCLK:
		case WM_LBUTTONDOWN:
		case WM_MBUTTONDBLCLK:
		case WM_MBUTTONDOWN:
		case WM_RBUTTONDBLCLK:
		case WM_RBUTTONDOWN:
		case WM_XBUTTONDBLCLK:
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
		case WM_RBUTTONUP:
		case WM_NCMOUSEMOVE:
		case WM_MOUSEMOVE:
		case WM_MOUSEWHEEL:
#if WINVER >= 0x0601
		case WM_TOUCH:
#endif
			{
				DeferMessage( CurrentNativeEventWindowPtr, hwnd, msg, wParam, lParam );
				// Handled
				return 0;
			}
			break;

		case WM_SETCURSOR:
		{
			DeferMessage(CurrentNativeEventWindowPtr, hwnd, msg, wParam, lParam);

			// If we're rendering our own window border, we'll "handle" this event so that Windows doesn't try to manage the cursor
			// appearance for us in the non-client area.  However, for OS window borders we need to fall through to DefWindowProc to
			// allow Windows to draw the resize cursor
			if (!CurrentNativeEventWindow->GetDefinition().HasOSWindowBorder)
			{
				// Handled
				return 0;
			}
		}
		break;

		// Mouse Movement
		case WM_INPUT:
			{
				uint32 Size = 0;
				::GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &Size, sizeof(RAWINPUTHEADER));

				TUniquePtr<uint8[]> RawData = MakeUnique<uint8[]>(Size);

				if (::GetRawInputData((HRAWINPUT)lParam, RID_INPUT, RawData.Get(), &Size, sizeof(RAWINPUTHEADER)) == Size )
				{
					const RAWINPUT* const Raw = (const RAWINPUT* const)RawData.Get();

					if (Raw->header.dwType == RIM_TYPEMOUSE) 
					{
						const bool IsAbsoluteInput = (Raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == MOUSE_MOVE_ABSOLUTE;
						if( IsAbsoluteInput )
						{
							// Since the raw input is coming in as absolute it is likely the user is using a tablet
							// or perhaps is interacting through a virtual desktop
							DeferMessage( CurrentNativeEventWindowPtr, hwnd, msg, wParam, lParam, 0, 0, MOUSE_MOVE_ABSOLUTE );
							return 1;
						}

						// Since raw input is coming in as relative it is likely a traditional mouse device
						const int xPosRelative = Raw->data.mouse.lLastX;
						const int yPosRelative = Raw->data.mouse.lLastY;

						DeferMessage( CurrentNativeEventWindowPtr, hwnd, msg, wParam, lParam, xPosRelative, yPosRelative, MOUSE_MOVE_RELATIVE );
						return 1;
					} 
				}
			}
			break;


		case WM_NCCALCSIZE:
			{
				// Let windows absorb this message if using the standard border
				if ( wParam && !CurrentNativeEventWindow->GetDefinition().HasOSWindowBorder )
				{
					// Borderless game windows are not actually borderless, they have a thick border that we simply draw game content over (client
					// rect contains the window border). When maximized Windows will bleed our border over the edges of the monitor. So that we
					// don't draw content we are going to later discard, we change a maximized window's size and position so that the entire
					// window rect (including the border) sits inside the monitor. The size adjustments here will be sent to WM_MOVE and
					// WM_SIZE and the window will still be considered maximized.
					if (CurrentNativeEventWindow->GetDefinition().Type == EWindowType::GameWindow && CurrentNativeEventWindow->IsMaximized())
					{
						// Ask the system for the window border size as this is the amount that Windows will bleed our window over the edge
						// of our desired space. The value returned by CurrentNativeEventWindow will be incorrect for our usage here as it
						// refers to the border of the window that Slate should consider.
						WINDOWINFO WindowInfo;
						FMemory::Memzero(WindowInfo);
						WindowInfo.cbSize = sizeof(WindowInfo);
						::GetWindowInfo(hwnd, &WindowInfo);

						// A pointer to the window size data that Windows will use is passed to us in lParam
						LPNCCALCSIZE_PARAMS ResizingRects = (LPNCCALCSIZE_PARAMS)lParam;
						// The first rectangle contains the client rectangle of the resized window. Decrease window size on all sides by
						// the border size.
						ResizingRects->rgrc[0].left += WindowInfo.cxWindowBorders;
						ResizingRects->rgrc[0].top += WindowInfo.cxWindowBorders;
						ResizingRects->rgrc[0].right -= WindowInfo.cxWindowBorders;
						ResizingRects->rgrc[0].bottom -= WindowInfo.cxWindowBorders;
						// The second rectangle contains the destination rectangle for the content currently displayed in the window's
						// client rect. Windows will blit the previous client content into this new location to simulate the move of
						// the window until the window can repaint itself. This should also be adjusted to our new window size.
						ResizingRects->rgrc[1].left = ResizingRects->rgrc[0].left;
						ResizingRects->rgrc[1].top = ResizingRects->rgrc[0].top;
						ResizingRects->rgrc[1].right = ResizingRects->rgrc[0].right;
						ResizingRects->rgrc[1].bottom = ResizingRects->rgrc[0].bottom;
						// A third rectangle is passed in that contains the source rectangle (client area from window pre-maximize).
						// It's value should not be changed.

						// The new window position. Pull in the window on all sides by the width of the window border so that the
						// window fits entirely on screen. We'll draw over these borders with game content.
						ResizingRects->lppos->x += WindowInfo.cxWindowBorders;
						ResizingRects->lppos->y += WindowInfo.cxWindowBorders;
						ResizingRects->lppos->cx -= 2 * WindowInfo.cxWindowBorders;
						ResizingRects->lppos->cy -= 2 * WindowInfo.cxWindowBorders;

						// Informs Windows to use the values as we altered them.
						return WVR_VALIDRECTS;
					}

					return 0;
				}
			}
			break;

		case WM_SHOWWINDOW:
			{
				DeferMessage( CurrentNativeEventWindowPtr, hwnd, msg, wParam, lParam );
			}
			break;

		case WM_SIZE:
			{
				DeferMessage( CurrentNativeEventWindowPtr, hwnd, msg, wParam, lParam );

				const bool bWasMaximized = (wParam == SIZE_MAXIMIZED);
				const bool bWasRestored = (wParam == SIZE_RESTORED);

				if (bWasMaximized || bWasRestored)
				{
					MessageHandler->OnWindowAction(CurrentNativeEventWindow, bWasMaximized ? EWindowAction::Maximize : EWindowAction::Restore);
				}

				return 0;
			}
			break;

		case WM_SIZING:
			{
				DeferMessage( CurrentNativeEventWindowPtr, hwnd, msg, wParam, lParam, 0, 0 );
		
				if (CurrentNativeEventWindowPtr->GetDefinition().ShouldPreserveAspectRatio)
				{
					// The rect we get in lParam is window rect, but we need to preserve client's aspect ratio,
					// so we need to find what the border and title bar sizes are, if window has them and adjust the rect.
					WINDOWINFO WindowInfo;
					FMemory::Memzero(WindowInfo);
					WindowInfo.cbSize = sizeof(WindowInfo);
					::GetWindowInfo(hwnd, &WindowInfo);

					RECT TestRect;
					TestRect.left = TestRect.right = TestRect.top = TestRect.bottom = 0;
					AdjustWindowRectEx(&TestRect, WindowInfo.dwStyle, false, WindowInfo.dwExStyle);

					RECT* Rect = (RECT*)lParam;
					Rect->left -= TestRect.left;
					Rect->right -= TestRect.right;
					Rect->top -= TestRect.top;
					Rect->bottom -= TestRect.bottom;

					const float AspectRatio = CurrentNativeEventWindowPtr->GetAspectRatio();
					int32 NewWidth = Rect->right - Rect->left;
					int32 NewHeight = Rect->bottom - Rect->top;

					switch (wParam)
					{
					case WMSZ_LEFT:
					case WMSZ_RIGHT:
						{
							int32 AdjustedHeight = NewWidth / AspectRatio;
							Rect->top -= (AdjustedHeight - NewHeight) / 2;
							Rect->bottom += (AdjustedHeight - NewHeight) / 2;
							break;
						}
					case WMSZ_TOP:
					case WMSZ_BOTTOM:
						{
							int32 AdjustedWidth = NewHeight * AspectRatio;
							Rect->left -= (AdjustedWidth - NewWidth) / 2;
							Rect->right += (AdjustedWidth - NewWidth) / 2;
							break;
						}
					case WMSZ_TOPLEFT:
						{
							int32 AdjustedHeight = NewWidth / AspectRatio;
							Rect->top -= AdjustedHeight - NewHeight;
							break;
						}
					case WMSZ_TOPRIGHT:
						{
							int32 AdjustedHeight = NewWidth / AspectRatio;
							Rect->top -= AdjustedHeight - NewHeight;
							break;
						}
					case WMSZ_BOTTOMLEFT:
						{
							int32 AdjustedHeight = NewWidth / AspectRatio;
							Rect->bottom += AdjustedHeight - NewHeight;
							break;
						}
					case WMSZ_BOTTOMRIGHT:
						{
							int32 AdjustedHeight = NewWidth / AspectRatio;
							Rect->bottom += AdjustedHeight - NewHeight;
							break;
						}
					}

					AdjustWindowRectEx(Rect, WindowInfo.dwStyle, false, WindowInfo.dwExStyle);

					return TRUE;
				}
		}
			break;
		case WM_ENTERSIZEMOVE:
			{
				bInModalSizeLoop = true;
				DeferMessage( CurrentNativeEventWindowPtr, hwnd, msg, wParam, lParam, 0, 0 );
			}
			break;
		case WM_EXITSIZEMOVE:
			{
				bInModalSizeLoop = false;
				DeferMessage( CurrentNativeEventWindowPtr, hwnd, msg, wParam, lParam, 0, 0 );
			}
			break;


		case WM_MOVE:
			{
				// client area position
				const int32 NewX = (int)(short)(LOWORD(lParam));
				const int32 NewY = (int)(short)(HIWORD(lParam));
				FIntPoint NewPosition(NewX,NewY);

				// Only cache the screen position if its not minimized
				if ( FWindowsApplication::MinimizedWindowPosition != NewPosition )
				{
					MessageHandler->OnMovedWindow( CurrentNativeEventWindow, NewX, NewY );

					return 0;
				}
			}
			break;

		case WM_NCHITTEST:
			{
				// Only needed if not using the os window border as this is determined automatically
				if( !CurrentNativeEventWindow->GetDefinition().HasOSWindowBorder )
				{
					RECT rcWindow;
					GetWindowRect(hwnd, &rcWindow);

					const int32 LocalMouseX = (int)(short)(LOWORD(lParam)) - rcWindow.left;
					const int32 LocalMouseY = (int)(short)(HIWORD(lParam)) - rcWindow.top;
					if ( CurrentNativeEventWindow->IsRegularWindow() )
					{
						EWindowZone::Type Zone;
					
						if( MessageHandler->ShouldProcessUserInputMessages( CurrentNativeEventWindowPtr ) )
						{
							// Assumes this is not allowed to leave Slate or touch rendering
							Zone = MessageHandler->GetWindowZoneForPoint( CurrentNativeEventWindow, LocalMouseX, LocalMouseY );
						}
						else
						{
							// Default to client area so that we are able to see the feedback effect when attempting to click on a non-modal window when a modal window is active
							// Any other window zones could have side effects and NotInWindow prevents the feedback effect.
							Zone = EWindowZone::ClientArea;
						}

						static const LRESULT Results[] = {HTNOWHERE, HTTOPLEFT, HTTOP, HTTOPRIGHT, HTLEFT, HTCLIENT,
							HTRIGHT, HTBOTTOMLEFT, HTBOTTOM, HTBOTTOMRIGHT,
							HTCAPTION, HTMINBUTTON, HTMAXBUTTON, HTCLOSE, HTSYSMENU};

						return Results[Zone];
					}
				}
			}
			break;
			
#if WINVER > 0x502
		case WM_DWMCOMPOSITIONCHANGED:
			{
				DeferMessage( CurrentNativeEventWindowPtr, hwnd, msg, wParam, lParam );
			}
			break;
#endif

			// Window focus and activation
		case WM_MOUSEACTIVATE:
			{
				DeferMessage(CurrentNativeEventWindowPtr, hwnd, msg, wParam, lParam);
			}
			break;

			// Window focus and activation
		case WM_ACTIVATE:
			{
				DeferMessage( CurrentNativeEventWindowPtr, hwnd, msg, wParam, lParam );
			}
			break;

		case WM_ACTIVATEAPP:
			{
				// When window activation changes we are not in a modal size loop
				bInModalSizeLoop = false;
				DeferMessage( CurrentNativeEventWindowPtr, hwnd, msg, wParam, lParam );
			}
			break;

		case WM_SETTINGCHANGE:
			{
				// Convertible mode change
				if ((lParam != NULL) && (wcscmp(TEXT("ConvertibleSlateMode"), (TCHAR *)lParam) == 0))
				{
					DeferMessage(CurrentNativeEventWindowPtr, hwnd, msg, wParam, lParam);
				}
			}
			break;

		case WM_PAINT:
			{
				if( bInModalSizeLoop && IsInGameThread() )
				{
					MessageHandler->OnOSPaint(CurrentNativeEventWindowPtr.ToSharedRef() );
				}
			}
			break;

		case WM_ERASEBKGND:
			{
				// Intercept background erasing to eliminate flicker.
				// Return non-zero to indicate that we'll handle the erasing ourselves
				return 1;
			}
			break;

		case WM_NCACTIVATE:
			{
				if( !CurrentNativeEventWindow->GetDefinition().HasOSWindowBorder )
				{
					// Unless using the OS window border, intercept calls to prevent non-client area drawing a border upon activation or deactivation
					// Return true to ensure standard activation happens
					return true;
				}
			}
			break;

		case WM_NCPAINT:
			{
				if( !CurrentNativeEventWindow->GetDefinition().HasOSWindowBorder )
				{
					// Unless using the OS window border, intercept calls to draw the non-client area - we do this ourselves
					return 0;
				}
			}
			break;

		case WM_DESTROY:
			{
				Windows.Remove( CurrentNativeEventWindow );
				return 0;
			}
			break;

		case WM_CLOSE:
			{
				DeferMessage( CurrentNativeEventWindowPtr, hwnd, msg, wParam, lParam );
				return 0;
			}
			break;

		case WM_SYSCOMMAND:
			{
				switch( wParam & 0xfff0 )
				{
				case SC_RESTORE:
					// Checks to see if the window is minimized.
					if( IsIconic(hwnd) )
					{
						// This is required for restoring a minimized fullscreen window
						::ShowWindow(hwnd,SW_RESTORE);
						return 0;
					}
					else
					{
						if(!MessageHandler->OnWindowAction( CurrentNativeEventWindow, EWindowAction::Restore))
						{
							return 1;
						}
					}
					break;
				case SC_MAXIMIZE:
					{
						if(!MessageHandler->OnWindowAction( CurrentNativeEventWindow, EWindowAction::Maximize))
						{
							return 1;
						}
					}
					break;
				case SC_CLOSE:
					{
						DeferMessage(CurrentNativeEventWindowPtr, hwnd, WM_CLOSE, 0, 0);
						return 1;
					}
					break;
				default:
					if( !( MessageHandler->ShouldProcessUserInputMessages( CurrentNativeEventWindow ) && IsInputMessage( msg ) ) )
					{
						return 0;
					}
					break;
				}
			}
			break;

		case WM_GETMINMAXINFO:
			{
				MINMAXINFO* MinMaxInfo = (MINMAXINFO*)lParam;
				FWindowSizeLimits SizeLimits = MessageHandler->GetSizeLimitsForWindow(CurrentNativeEventWindow);

				// We need to inflate the max values if using an OS window border
				int32 BorderWidth = 0;
				int32 BorderHeight = 0;
				if (CurrentNativeEventWindow->GetDefinition().HasOSWindowBorder)
				{
					const DWORD WindowStyle = ::GetWindowLong(hwnd, GWL_STYLE);
					const DWORD WindowExStyle = ::GetWindowLong(hwnd, GWL_EXSTYLE);

					// This adjusts a zero rect to give us the size of the border
					RECT BorderRect = { 0, 0, 0, 0 };
					::AdjustWindowRectEx(&BorderRect, WindowStyle, false, WindowExStyle);

					BorderWidth = BorderRect.right - BorderRect.left;
					BorderHeight = BorderRect.bottom - BorderRect.top;
				}

				// We always apply BorderWidth and BorderHeight since Slate always works with client area window sizes
				MinMaxInfo->ptMinTrackSize.x = FMath::RoundToInt( SizeLimits.GetMinWidth().Get(MinMaxInfo->ptMinTrackSize.x) );
				MinMaxInfo->ptMinTrackSize.y = FMath::RoundToInt( SizeLimits.GetMinHeight().Get(MinMaxInfo->ptMinTrackSize.y) );
				MinMaxInfo->ptMaxTrackSize.x = FMath::RoundToInt( SizeLimits.GetMaxWidth().Get(MinMaxInfo->ptMaxTrackSize.x) ) + BorderWidth;
				MinMaxInfo->ptMaxTrackSize.y = FMath::RoundToInt( SizeLimits.GetMaxHeight().Get(MinMaxInfo->ptMaxTrackSize.y) ) + BorderHeight;
				return 0;
			}
			break;
			
		case WM_NCLBUTTONDOWN:
		case WM_NCRBUTTONDOWN:
		case WM_NCMBUTTONDOWN:
			{
				switch( wParam )
				{
				case HTMINBUTTON:
					{
						if(!MessageHandler->OnWindowAction( CurrentNativeEventWindow, EWindowAction::ClickedNonClientArea))
						{
							return 1;
						}
					}
					break;
				case HTMAXBUTTON:
					{
						if(!MessageHandler->OnWindowAction( CurrentNativeEventWindow, EWindowAction::ClickedNonClientArea))
						{
							return 1;
						}
					}
					break;
				case HTCLOSE:
					{
						if(!MessageHandler->OnWindowAction( CurrentNativeEventWindow, EWindowAction::ClickedNonClientArea))
						{
							return 1;
						}
					}
					break;
				case HTCAPTION:
					{
						if(!MessageHandler->OnWindowAction( CurrentNativeEventWindow, EWindowAction::ClickedNonClientArea))
						{
							return 1;
						}
					}
					break;
				}
			}
			break;

		case WM_DISPLAYCHANGE:
			{
				// Slate needs to know when desktop size changes.
				FDisplayMetrics DisplayMetrics;
				FDisplayMetrics::GetDisplayMetrics(DisplayMetrics);
				BroadcastDisplayMetricsChanged(DisplayMetrics);
			}
			break;

		case WM_DPICHANGED:
			DeferMessage(CurrentNativeEventWindowPtr, hwnd, msg, wParam, lParam);
			break;

		case WM_GETDLGCODE:
			{
				// Slate wants all keys and messages.
				return DLGC_WANTALLKEYS;
			}
			break;
		
		case WM_CREATE:
			return 0;

		case WM_DEVICECHANGE:
			{
				XInput->SetNeedsControllerStateUpdate(); 
				QueryConnectedMice();
			}
			break;

		default:
			if (bMessageExternallyHandled)
			{
				return ExternalMessageHandlerResult;
			}
		}
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void FWindowsApplication::CheckForShiftUpEvents(const int32 KeyCode)
{
	check(KeyCode == VK_LSHIFT || KeyCode == VK_RSHIFT);

	// Since VK_SHIFT doesn't get an up message if the other shift key is held we need to poll for it
	const EModifierKey::Type ModifierKeyIndex = KeyCode == VK_LSHIFT ? EModifierKey::LeftShift : EModifierKey::RightShift;
	if (ModifierKeyState[ModifierKeyIndex] && ((::GetKeyState(KeyCode) & 0x8000) == 0) )
	{
		ModifierKeyState[ModifierKeyIndex] = false;
		MessageHandler->OnKeyUp( KeyCode, 0, false );
	}
}

int32 FWindowsApplication::ProcessDeferredMessage( const FDeferredWindowsMessage& DeferredMessage )
{
	if ( Windows.Num() && DeferredMessage.NativeWindow.IsValid() )
	{
		HWND hwnd = DeferredMessage.hWND;
		uint32 msg = DeferredMessage.Message;
		WPARAM wParam = DeferredMessage.wParam;
		LPARAM lParam = DeferredMessage.lParam;

		TSharedPtr< FWindowsWindow > CurrentNativeEventWindowPtr = DeferredMessage.NativeWindow.Pin();

		// This effectively disables a window without actually disabling it natively with the OS.
		// This allows us to continue receiving messages for it.
		if ( !MessageHandler->ShouldProcessUserInputMessages( CurrentNativeEventWindowPtr ) && IsInputMessage( msg ) )
		{
			if (IsKeyboardInputMessage(msg))
			{
				// Force an update since we may have just consumed a modifier key state change
				UpdateAllModifierKeyStates();
			}
			return 0;	// consume input messages
		}

		switch(msg)
		{
		case WM_INPUTLANGCHANGEREQUEST:
		case WM_INPUTLANGCHANGE:
		case WM_IME_SETCONTEXT:
		case WM_IME_NOTIFY:
		case WM_IME_REQUEST:
		case WM_IME_STARTCOMPOSITION:
		case WM_IME_COMPOSITION:
		case WM_IME_ENDCOMPOSITION:
		case WM_IME_CHAR:
			{
				if(TextInputMethodSystem.IsValid())
				{
					TextInputMethodSystem->ProcessMessage(hwnd, msg, wParam, lParam);
				}
				return 0;
			}
			break;
			// Character
		case WM_CHAR:
			{
				// Character code is stored in WPARAM
				const TCHAR Character = wParam;

				// LPARAM bit 30 will be ZERO for new presses, or ONE if this is a repeat
				const bool bIsRepeat = ( lParam & 0x40000000 ) != 0;

				MessageHandler->OnKeyChar( Character, bIsRepeat );

				// Note: always return 0 to handle the message.  Win32 beeps if WM_CHAR is not handled...
				return 0;
			}
			break;


			// Key down
		case WM_SYSKEYDOWN:
		case WM_KEYDOWN:
			{
				// Character code is stored in WPARAM
				const int32 Win32Key = wParam;

				// The actual key to use.  Some keys will be translated into other keys. 
				// I.E VK_CONTROL will be translated to either VK_LCONTROL or VK_RCONTROL as these
				// keys are never sent on their own
				int32 ActualKey = Win32Key;

				// LPARAM bit 30 will be ZERO for new presses, or ONE if this is a repeat
				bool bIsRepeat = ( lParam & 0x40000000 ) != 0;

				switch( Win32Key )
				{
				case VK_MENU:
					// Differentiate between left and right alt
					if( (lParam & 0x1000000) == 0 )
					{
						ActualKey = VK_LMENU;
						bIsRepeat = ModifierKeyState[EModifierKey::LeftAlt];
						ModifierKeyState[EModifierKey::LeftAlt] = true;
					}
					else
					{
						ActualKey = VK_RMENU;
						bIsRepeat = ModifierKeyState[EModifierKey::RightAlt];
						ModifierKeyState[EModifierKey::RightAlt] = true;
					}
					break;
				case VK_CONTROL:
					// Differentiate between left and right control
					if( (lParam & 0x1000000) == 0 )
					{
						ActualKey = VK_LCONTROL;
						bIsRepeat = ModifierKeyState[EModifierKey::LeftControl];
						ModifierKeyState[EModifierKey::LeftControl] = true;
					}
					else
					{
						ActualKey = VK_RCONTROL;
						bIsRepeat = ModifierKeyState[EModifierKey::RightControl];
						ModifierKeyState[EModifierKey::RightControl] = true;
					}
					break;
				case VK_SHIFT:
					// Differentiate between left and right shift
					ActualKey = MapVirtualKey( (lParam & 0x00ff0000) >> 16, MAPVK_VSC_TO_VK_EX);
					if (ActualKey == VK_LSHIFT)
					{
						bIsRepeat = ModifierKeyState[EModifierKey::LeftShift];
						ModifierKeyState[EModifierKey::LeftShift] = true;
					}
					else
					{
						bIsRepeat = ModifierKeyState[EModifierKey::RightShift];
						ModifierKeyState[EModifierKey::RightShift] = true;
					}
					break;
				case VK_CAPITAL:
					ModifierKeyState[EModifierKey::CapsLock] = (::GetKeyState(VK_CAPITAL) & 0x0001) != 0;
					break;
				default:
					// No translation needed
					break;
				}

				// Get the character code from the virtual key pressed.  If 0, no translation from virtual key to character exists
				uint32 CharCode = ::MapVirtualKey( Win32Key, MAPVK_VK_TO_CHAR );

				const bool Result = MessageHandler->OnKeyDown( ActualKey, CharCode, bIsRepeat );

				// Always return 0 to handle the message or else windows will beep
				if( Result || msg != WM_SYSKEYDOWN )
				{
					// Handled
					return 0;
				}
			}
			break;


			// Key up
		case WM_SYSKEYUP:
		case WM_KEYUP:
			{
				// Character code is stored in WPARAM
				int32 Win32Key = wParam;

				// The actual key to use.  Some keys will be translated into other keys. 
				// I.E VK_CONTROL will be translated to either VK_LCONTROL or VK_RCONTROL as these
				// keys are never sent on their own
				int32 ActualKey = Win32Key;

				bool bModifierKeyReleased = false;
				switch( Win32Key )
				{
				case VK_MENU:
					// Differentiate between left and right alt
					if( (lParam & 0x1000000) == 0 )
					{
						ActualKey = VK_LMENU;
						ModifierKeyState[EModifierKey::LeftAlt] = false;
					}
					else
					{
						ActualKey = VK_RMENU;
						ModifierKeyState[EModifierKey::RightAlt] = false;
					}
					break;
				case VK_CONTROL:
					// Differentiate between left and right control
					if( (lParam & 0x1000000) == 0 )
					{
						ActualKey = VK_LCONTROL;
						ModifierKeyState[EModifierKey::LeftControl] = false;
					}
					else
					{
						ActualKey = VK_RCONTROL;
						ModifierKeyState[EModifierKey::RightControl] = false;
					}
					break;
				case VK_SHIFT:
					// Differentiate between left and right shift
					ActualKey = MapVirtualKey( (lParam & 0x00ff0000) >> 16, MAPVK_VSC_TO_VK_EX);
					if (ActualKey == VK_LSHIFT)
					{
						ModifierKeyState[EModifierKey::LeftShift] = false;
					}
					else
					{
						ModifierKeyState[EModifierKey::RightShift] = false;
					}
					break;
				case VK_CAPITAL:
					ModifierKeyState[EModifierKey::CapsLock] = (::GetKeyState(VK_CAPITAL) & 0x0001) != 0;
					break;
				default:
					// No translation needed
					break;
				}

				// Get the character code from the virtual key pressed.  If 0, no translation from virtual key to character exists
				uint32 CharCode = ::MapVirtualKey( Win32Key, MAPVK_VK_TO_CHAR );

				// Key up events are never repeats
				const bool bIsRepeat = false;

				const bool Result = MessageHandler->OnKeyUp( ActualKey, CharCode, bIsRepeat );

				// Note that we allow system keys to pass through to DefWndProc here, so that core features
				// like Alt+F4 to close a window work.
				if( Result || msg != WM_SYSKEYUP )
				{
					// Handled
					return 0;
				}
			}
			break;

			// Mouse Button Down
		case WM_LBUTTONDBLCLK:
		case WM_LBUTTONDOWN:
		case WM_MBUTTONDBLCLK:
		case WM_MBUTTONDOWN:
		case WM_RBUTTONDBLCLK:
		case WM_RBUTTONDOWN:
		case WM_XBUTTONDBLCLK:
		case WM_XBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
		case WM_RBUTTONUP:
		case WM_XBUTTONUP:
			{
				POINT CursorPoint;
				CursorPoint.x = GET_X_LPARAM(lParam);
				CursorPoint.y = GET_Y_LPARAM(lParam); 

				ClientToScreen(hwnd, &CursorPoint);

				const FVector2D CursorPos(CursorPoint.x, CursorPoint.y);

				EMouseButtons::Type MouseButton = EMouseButtons::Invalid;
				bool bDoubleClick = false;
				bool bMouseUp = false;
				switch(msg)
				{
				case WM_LBUTTONDBLCLK:
					bDoubleClick = true;
					MouseButton = EMouseButtons::Left;
					break;
				case WM_LBUTTONUP:
					bMouseUp = true;
					MouseButton = EMouseButtons::Left;
					break;
				case WM_LBUTTONDOWN:
					MouseButton = EMouseButtons::Left;
					break;
				case WM_MBUTTONDBLCLK:
					bDoubleClick = true;
					MouseButton = EMouseButtons::Middle;
					break;
				case WM_MBUTTONUP:
					bMouseUp = true;
					MouseButton = EMouseButtons::Middle;
					break;
				case WM_MBUTTONDOWN:
					MouseButton = EMouseButtons::Middle;
					break;
				case WM_RBUTTONDBLCLK:
					bDoubleClick = true;
					MouseButton = EMouseButtons::Right;
					break;
				case WM_RBUTTONUP:
					bMouseUp = true;
					MouseButton = EMouseButtons::Right;
					break;
				case WM_RBUTTONDOWN:
					MouseButton = EMouseButtons::Right;
					break;
				case WM_XBUTTONDBLCLK:
					bDoubleClick = true;
					MouseButton = ( HIWORD(wParam) & XBUTTON1 ) ? EMouseButtons::Thumb01  : EMouseButtons::Thumb02;
					break;
				case WM_XBUTTONUP:
					bMouseUp = true;
					MouseButton = ( HIWORD(wParam) & XBUTTON1 ) ? EMouseButtons::Thumb01  : EMouseButtons::Thumb02;
					break;
				case WM_XBUTTONDOWN:
					MouseButton = ( HIWORD(wParam) & XBUTTON1 ) ? EMouseButtons::Thumb01  : EMouseButtons::Thumb02;
					break;
				default:
					check(0);
				}

				if (bMouseUp)
				{
					return MessageHandler->OnMouseUp( MouseButton, CursorPos ) ? 0 : 1;
				}
				else if (bDoubleClick)
				{
					MessageHandler->OnMouseDoubleClick( CurrentNativeEventWindowPtr, MouseButton, CursorPos );
				}
				else
				{
					MessageHandler->OnMouseDown( CurrentNativeEventWindowPtr, MouseButton, CursorPos );
				}
				return 0;
			}
			break;

		// Mouse Movement
		case WM_INPUT:
			{
				if( DeferredMessage.RawInputFlags == MOUSE_MOVE_RELATIVE )
				{
					MessageHandler->OnRawMouseMove( DeferredMessage.X, DeferredMessage.Y );
				}
				else
				{
					// Absolute coordinates given through raw input are simulated using MouseMove to get relative coordinates
					MessageHandler->OnMouseMove();
				}

				return 0;
			}
			break;

		// Mouse Movement
		case WM_NCMOUSEMOVE:
		case WM_MOUSEMOVE:
			{
				BOOL Result = false;
				if( !bUsingHighPrecisionMouseInput )
				{
					Result = MessageHandler->OnMouseMove();
				}

				return Result ? 0 : 1;
			}
			break;
			// Mouse Wheel
		case WM_MOUSEWHEEL:
			{
				const float SpinFactor = 1 / 120.0f;
				const SHORT WheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);

				POINT CursorPoint;
				CursorPoint.x = GET_X_LPARAM(lParam);
				CursorPoint.y = GET_Y_LPARAM(lParam); 

				const FVector2D CursorPos(CursorPoint.x, CursorPoint.y);

				const BOOL Result = MessageHandler->OnMouseWheel( static_cast<float>( WheelDelta ) * SpinFactor, CursorPos );
				return Result ? 0 : 1;
			}
			break;

			// Mouse Cursor
		case WM_SETCURSOR:
			{
				// WM_SETCURSOR - Sent to a window if the mouse causes the cursor to move within a window and mouse input is not captured.
				return MessageHandler->OnCursorSet() ? 0 : 1;
			}
			break;

#if WINVER >= 0x0601
		case WM_TOUCH:
			{
				UINT InputCount = LOWORD( wParam );
				if ( InputCount > 0 )
				{
					TUniquePtr<TOUCHINPUT[]> Inputs = MakeUnique<TOUCHINPUT[]>( InputCount );
					if ( GetTouchInputInfo( (HTOUCHINPUT)lParam, InputCount, Inputs.Get(), sizeof(TOUCHINPUT) ) )
					{
						for ( uint32 i = 0; i < InputCount; i++ )
						{
							TOUCHINPUT Input = Inputs[i];
							FVector2D Location( Input.x / 100.0f, Input.y / 100.0f );
							if ( Input.dwFlags & TOUCHEVENTF_DOWN )
							{
								int32 TouchIndex = GetTouchIndexForID( Input.dwID );
								if (TouchIndex == INDEX_NONE)
								{
									TouchIndex = GetFirstFreeTouchIndex();
									check(TouchIndex >= 0);
									
									TouchIDs[TouchIndex] = TOptional<int32>(Input.dwID);
									UE_LOG(LogWindowsDesktop, Verbose, TEXT("OnTouchStarted at (%f, %f), finger %d (system touch id %d)"), Location.X, Location.Y, TouchIndex, Input.dwID);
									MessageHandler->OnTouchStarted(CurrentNativeEventWindowPtr, Location, TouchIndex, 0);
								}
								else
								{
									// TODO: Error handling.
								}
							}
							else if ( Input.dwFlags & TOUCHEVENTF_MOVE )
							{
								int32 TouchIndex = GetTouchIndexForID( Input.dwID );
								if ( TouchIndex >= 0 )
								{
									UE_LOG(LogWindowsDesktop, Verbose, TEXT("OnTouchMoved at (%f, %f), finger %d (system touch id %d)"), Location.X, Location.Y, TouchIndex, Input.dwID);
									MessageHandler->OnTouchMoved(Location, TouchIndex, 0);
								}
							}
							else if ( Input.dwFlags & TOUCHEVENTF_UP )
							{
								int32 TouchIndex = GetTouchIndexForID( Input.dwID );
								if ( TouchIndex >= 0 )
								{
									TouchIDs[TouchIndex] = TOptional<int32>();
									UE_LOG(LogWindowsDesktop, Verbose, TEXT("OnTouchEnded at (%f, %f), finger %d (system touch id %d)"), Location.X, Location.Y, TouchIndex, Input.dwID);
									MessageHandler->OnTouchEnded(Location, TouchIndex, 0);
								}
								else
								{
									// TODO: Error handling.
								}
							}
						}
						CloseTouchInputHandle( (HTOUCHINPUT)lParam );
						return 0;
					}
				}
				break;
			}
#endif

			// Window focus and activation
		case WM_MOUSEACTIVATE:
			{
				// If the mouse activate isn't in the client area we'll force the WM_ACTIVATE to be EWindowActivation::ActivateByMouse
				// This ensures that clicking menu buttons on the header doesn't generate a WM_ACTIVATE with EWindowActivation::Activate
				// which may cause mouse capture to be taken because is not differentiable from Alt-Tabbing back to the application.
				bForceActivateByMouse = !(LOWORD(lParam) & HTCLIENT);
				return 0;
			}
			break;

			// Window focus and activation
		case WM_ACTIVATE:
			{
				EWindowActivation ActivationType;

				if (LOWORD(wParam) & WA_ACTIVE)
				{
					ActivationType = bForceActivateByMouse ? EWindowActivation::ActivateByMouse : EWindowActivation::Activate;
				}
				else if (LOWORD(wParam) & WA_CLICKACTIVE)
				{
					ActivationType = EWindowActivation::ActivateByMouse;
				}
				else
				{
					ActivationType = EWindowActivation::Deactivate;
				}
				bForceActivateByMouse = false;

				UpdateAllModifierKeyStates();

				if ( CurrentNativeEventWindowPtr.IsValid() )
				{
					BOOL Result = false;
					Result = MessageHandler->OnWindowActivationChanged( CurrentNativeEventWindowPtr.ToSharedRef(), ActivationType );
					return Result ? 0 : 1;
				}

				return 1;
			}
			break;

		case WM_ACTIVATEAPP:
			UpdateAllModifierKeyStates();
			MessageHandler->OnApplicationActivationChanged( !!wParam );
			break;

		case WM_SETTINGCHANGE:
			if ((lParam != 0) && (FCString::Strcmp((LPCTSTR)lParam, TEXT("ConvertibleSlateMode")) == 0))
			{
				MessageHandler->OnConvertibleLaptopModeChanged();
			}
			break;
	
		case WM_NCACTIVATE:
			{
				if( CurrentNativeEventWindowPtr.IsValid() && !CurrentNativeEventWindowPtr->GetDefinition().HasOSWindowBorder )
				{
					// Unless using the OS window border, intercept calls to prevent non-client area drawing a border upon activation or deactivation
					// Return true to ensure standard activation happens
					return true;
				}
			}
			break;

		case WM_NCPAINT:
			{
				if( CurrentNativeEventWindowPtr.IsValid() && !CurrentNativeEventWindowPtr->GetDefinition().HasOSWindowBorder )
				{
					// Unless using the OS window border, intercept calls to draw the non-client area - we do this ourselves
					return 0;
				}
			}
			break;

		case WM_CLOSE:
			{
				if ( CurrentNativeEventWindowPtr.IsValid() )
				{
					// Called when the OS close button is pressed
					MessageHandler->OnWindowClose( CurrentNativeEventWindowPtr.ToSharedRef() );
				}
				return 0;
			}
			break;

		case WM_SHOWWINDOW:
			{
				if( CurrentNativeEventWindowPtr.IsValid() )
				{
					switch(lParam)
					{
					case SW_PARENTCLOSING:
						CurrentNativeEventWindowPtr->OnParentWindowMinimized();
						break;
					case SW_PARENTOPENING:
						CurrentNativeEventWindowPtr->OnParentWindowRestored();
						break;
					default:
						break;
					}
				}
			}
			break;

		case WM_SIZE:
			{
				if( CurrentNativeEventWindowPtr.IsValid() )
				{
					// @todo Fullscreen - Perform deferred resize
					// Note WM_SIZE provides the client dimension which is not equal to the window dimension if there is a windows border 
					const int32 NewWidth = (int)(short)(LOWORD(lParam));
					const int32 NewHeight = (int)(short)(HIWORD(lParam));

					const FGenericWindowDefinition& Definition = CurrentNativeEventWindowPtr->GetDefinition();
					if ( Definition.IsRegularWindow && !Definition.HasOSWindowBorder )
					{
						CurrentNativeEventWindowPtr->AdjustWindowRegion(NewWidth, NewHeight);
					}

					const bool bWasMinimized = (wParam == SIZE_MINIMIZED);

					const bool bIsFullscreen = (CurrentNativeEventWindowPtr->GetWindowMode() == EWindowMode::Type::Fullscreen);

					// When in fullscreen Windows rendering size should be determined by the application. Do not adjust based on WM_SIZE messages.
 					if ( !bIsFullscreen )
					{
						const bool Result = MessageHandler->OnSizeChanged(CurrentNativeEventWindowPtr.ToSharedRef(), NewWidth, NewHeight, bWasMinimized);
					}
				}
			}
			break;
		case WM_SIZING:
			{
				if( CurrentNativeEventWindowPtr.IsValid() )
				{
					MessageHandler->OnResizingWindow( CurrentNativeEventWindowPtr.ToSharedRef() );
				}
			}
			break;
		case WM_ENTERSIZEMOVE:
			{
				if( CurrentNativeEventWindowPtr.IsValid() )
				{
					MessageHandler->BeginReshapingWindow( CurrentNativeEventWindowPtr.ToSharedRef() );
				}
			}
			break;
		case WM_EXITSIZEMOVE:
			{
				if( CurrentNativeEventWindowPtr.IsValid() )
				{
					MessageHandler->FinishedReshapingWindow( CurrentNativeEventWindowPtr.ToSharedRef() );
				}
			}
			break;

#if WINVER > 0x502
		case WM_DWMCOMPOSITIONCHANGED:
			{
				CurrentNativeEventWindowPtr->OnTransparencySupportChanged(GetWindowTransparencySupport());
			}
			break;
#endif

		case WM_DPICHANGED:
			{
				if( CurrentNativeEventWindowPtr.IsValid() )
				{
					CurrentNativeEventWindowPtr->SetDPIScaleFactor(LOWORD(wParam) / 96.0f);


					LPRECT NewRect = (LPRECT)lParam;
					SetWindowPos(hwnd, nullptr, NewRect->left, NewRect->top, NewRect->right - NewRect->left, NewRect->bottom - NewRect->top, SWP_NOZORDER | SWP_NOACTIVATE);

					MessageHandler->HandleDPIScaleChanged(CurrentNativeEventWindowPtr.ToSharedRef());
				}
			}
			break;
		}
	}

	return 0;
}

void FWindowsApplication::ProcessDeferredDragDropOperation(const FDeferredWindowsDragDropOperation& Op)
{
	// Since we deferred the drag/drop event, we could not specify the correct cursor effect in time. Now we will just throw away the value.
	DWORD DummyCursorEffect = 0;

	switch (Op.OperationType)
	{
		case EWindowsDragDropOperationType::DragEnter:
			OnOLEDragEnter(Op.HWnd, Op.OLEData, Op.KeyState, Op.CursorPosition, &DummyCursorEffect);
			break;
		case EWindowsDragDropOperationType::DragOver:
			OnOLEDragOver(Op.HWnd, Op.KeyState, Op.CursorPosition, &DummyCursorEffect);
			break;
		case EWindowsDragDropOperationType::DragLeave:
			OnOLEDragOut(Op.HWnd);
			break;
		case EWindowsDragDropOperationType::Drop:
			OnOLEDrop(Op.HWnd, Op.OLEData, Op.KeyState, Op.CursorPosition, &DummyCursorEffect);
			break;
		default:
			ensureMsgf(0, TEXT("Unhandled deferred drag/drop operation type: %d"), Op.OperationType);
			break;
	}
}

bool FWindowsApplication::IsKeyboardInputMessage( uint32 msg )
{
	switch(msg)
	{
	// Keyboard input notification messages...
	case WM_CHAR:
	case WM_SYSCHAR:
	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYUP:
	case WM_SYSCOMMAND:
		return true;
	}
	return false;
}

bool FWindowsApplication::IsMouseInputMessage( uint32 msg )
{
	switch(msg)
	{
	// Mouse input notification messages...
	case WM_MOUSEHWHEEL:
	case WM_MOUSEWHEEL:
	case WM_MOUSEHOVER:
	case WM_MOUSELEAVE:
	case WM_MOUSEMOVE:
	case WM_NCMOUSEHOVER:
	case WM_NCMOUSELEAVE:
	case WM_NCMOUSEMOVE:
	case WM_NCMBUTTONDBLCLK:
	case WM_NCMBUTTONDOWN:
	case WM_NCMBUTTONUP:
	case WM_NCRBUTTONDBLCLK:
	case WM_NCRBUTTONDOWN:
	case WM_NCRBUTTONUP:
	case WM_NCXBUTTONDBLCLK:
	case WM_NCXBUTTONDOWN:
	case WM_NCXBUTTONUP:
	case WM_LBUTTONDBLCLK:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_MBUTTONDBLCLK:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_RBUTTONDBLCLK:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_XBUTTONDBLCLK:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
		return true;
	}
	return false;
}

bool FWindowsApplication::IsInputMessage( uint32 msg )
{
	if (IsKeyboardInputMessage(msg) || IsMouseInputMessage(msg))
	{
		return true;
	}

	switch(msg)
	{
	// Raw input notification messages...
	case WM_INPUT:
	case WM_INPUT_DEVICE_CHANGE:
		return true;
	}
	return false;
}

#define MOUSEEVENTF_FROMTOUCH 0xFF515780
#define SIGNATURE_MASK 0xFFFFFF80
#define IsTouchEvent(dw) (((dw) & SIGNATURE_MASK) == MOUSEEVENTF_FROMTOUCH)

bool FWindowsApplication::IsFakeMouseInputMessage(uint32 msg)
{
	const bool bShouldPrevent = IsWindowsVistaOrGreater() || bPreventDuplicateMouseEventsForTouch;

	if (bShouldPrevent && IsMouseInputMessage(msg))
	{
		LPARAM ExtraInfo = GetMessageExtraInfo();
		// This is only legal to call when handling messages in the pump, and is not valid
		// to call in a deferred fashion.
		return IsTouchEvent(ExtraInfo);
	}
	
	// Click was generated by the mouse.
	return false;
}

#undef MOUSEEVENTF_FROMTOUCH

void FWindowsApplication::DeferMessage( TSharedPtr<FWindowsWindow>& NativeWindow, HWND InHWnd, uint32 InMessage, WPARAM InWParam, LPARAM InLParam, int32 MouseX, int32 MouseY, uint32 RawInputFlags )
{
	if( GPumpingMessagesOutsideOfMainLoop && bAllowedToDeferMessageProcessing )
	{
		DeferredMessages.Add( FDeferredWindowsMessage( NativeWindow, InHWnd, InMessage, InWParam, InLParam, MouseX, MouseY, RawInputFlags ) );
	}
	else
	{
		// When not deferring messages, process them immediately
		ProcessDeferredMessage( FDeferredWindowsMessage( NativeWindow, InHWnd, InMessage, InWParam, InLParam, MouseX, MouseY, RawInputFlags ) );
	}
}

void FWindowsApplication::PumpMessages( const float TimeDelta )
{
	MSG Message;

	// standard Windows message handling
	while(PeekMessage(&Message, NULL, 0, 0, PM_REMOVE))
	{ 
		TranslateMessage(&Message);
		DispatchMessage(&Message); 
	}
}

void FWindowsApplication::ProcessDeferredEvents( const float TimeDelta )
{
	// Process windows messages
	{
		// This function can be reentered when entering a modal tick loop.
		// We need to make a copy of the events that need to be processed or we may end up processing the same messages twice 
		TArray<FDeferredWindowsMessage> EventsToProcess( DeferredMessages );

		DeferredMessages.Empty();
		for( int32 MessageIndex = 0; MessageIndex < EventsToProcess.Num(); ++MessageIndex )
		{
			const FDeferredWindowsMessage& DeferredMessage = EventsToProcess[MessageIndex];
			ProcessDeferredMessage( DeferredMessage );
		}

		CheckForShiftUpEvents(VK_LSHIFT);
		CheckForShiftUpEvents(VK_RSHIFT);
	}

	// Process drag/drop operations
	{
		TArray<FDeferredWindowsDragDropOperation> DragDropOperationsToProcess(DeferredDragDropOperations);

		DeferredDragDropOperations.Empty();
		for (int32 OperationIndex = 0; OperationIndex < DragDropOperationsToProcess.Num(); ++OperationIndex)
		{
			const FDeferredWindowsDragDropOperation& DeferredDragDropOperation = DragDropOperationsToProcess[OperationIndex];
			ProcessDeferredDragDropOperation(DeferredDragDropOperation);
		}
	}
}

void FWindowsApplication::PollGameDeviceState( const float TimeDelta )
{
	// initialize any externally-implemented input devices (we delay load initialize the array so any plugins have had time to load)
	if (!bHasLoadedInputPlugins)
	{
		TArray<IInputDeviceModule*> PluginImplementations = IModularFeatures::Get().GetModularFeatureImplementations<IInputDeviceModule>( IInputDeviceModule::GetModularFeatureName() );
		for( auto InputPluginIt = PluginImplementations.CreateIterator(); InputPluginIt; ++InputPluginIt )
		{
			TSharedPtr<IInputDevice> Device = (*InputPluginIt)->CreateInputDevice(MessageHandler);
			AddExternalInputDevice(Device);			
		}

		bHasLoadedInputPlugins = true;
	}

	if (FApp::UseVRFocus() && !FApp::HasVRFocus())
	{
		return; // do not proceed if the app uses VR focus but doesn't have it
	}

	// Poll game device states and send new events
	XInput->SendControllerEvents();

	// Poll externally-implemented devices
	for( auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt )
	{
		(*DeviceIt)->Tick( TimeDelta );
		(*DeviceIt)->SendControllerEvents();
	}
}

void FWindowsApplication::SetForceFeedbackChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
{
	if (FApp::UseVRFocus() && !FApp::HasVRFocus())
	{
		return; // do not proceed if the app uses VR focus but doesn't have it
	}

	// send vibration to externally-implemented devices
	for( auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt )
	{
		(*DeviceIt)->SetChannelValue(ControllerId, ChannelType, Value);
	}
}

void FWindowsApplication::SetForceFeedbackChannelValues(int32 ControllerId, const FForceFeedbackValues &Values)
{
	if (FApp::UseVRFocus() && !FApp::HasVRFocus())
	{
		return; // do not proceed if the app uses VR focus but doesn't have it
	}

	const FForceFeedbackValues* InternalValues = &Values;
 
	XInput->SetChannelValues( ControllerId, *InternalValues );
 
	// send vibration to externally-implemented devices
	for( auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt )
	{
		// *N.B 06/20/2016*: Ideally, we would want to use GetHapticDevice instead
		// but they're not implemented for SteamController and SteamVRController
		if ((*DeviceIt)->IsGamepadAttached()) 
		{
			(*DeviceIt)->SetChannelValues(ControllerId, Values);
		}
	}
}

void FWindowsApplication::SetHapticFeedbackValues(int32 ControllerId, int32 Hand, const FHapticFeedbackValues& Values)
{
	if (FApp::UseVRFocus() && !FApp::HasVRFocus())
	{
		return; // do not proceed if the app uses VR focus but doesn't have it
	}

	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		IHapticDevice* HapticDevice = (*DeviceIt)->GetHapticDevice();
		if (HapticDevice)
		{
			HapticDevice->SetHapticFeedbackValues(ControllerId, Hand, Values);
		}
	}
}

void FWindowsApplication::AddExternalInputDevice(TSharedPtr<IInputDevice> InputDevice)
{
	if (InputDevice.IsValid())
	{
		ExternalInputDevices.Add(InputDevice);
	}
}

TSharedPtr<FTaskbarList> FWindowsApplication::GetTaskbarList()
{
	return TaskbarList;
}

void FWindowsApplication::DeferDragDropOperation(const FDeferredWindowsDragDropOperation& DeferredDragDropOperation)
{
	DeferredDragDropOperations.Add(DeferredDragDropOperation);
}

HRESULT FWindowsApplication::OnOLEDragEnter( const HWND HWnd, const FDragDropOLEData& OLEData, DWORD KeyState, POINTL CursorPosition, DWORD *CursorEffect)
{
	const TSharedPtr< FWindowsWindow > Window = FindWindowByHWND( Windows, HWnd );

	if ( !Window.IsValid() )
	{
		return 0;
	}

	if ( Window->IsEnabled() )
	{
		if ((OLEData.Type & FDragDropOLEData::Text) && (OLEData.Type & FDragDropOLEData::Files))
		{
			*CursorEffect = MessageHandler->OnDragEnterExternal(Window.ToSharedRef(), OLEData.OperationText, OLEData.OperationFilenames);
		}
		else if (OLEData.Type & FDragDropOLEData::Text)
		{
			*CursorEffect = MessageHandler->OnDragEnterText(Window.ToSharedRef(), OLEData.OperationText);
		}
		else if (OLEData.Type & FDragDropOLEData::Files)
		{
			*CursorEffect = MessageHandler->OnDragEnterFiles(Window.ToSharedRef(), OLEData.OperationFilenames);
		}
	}
	else
	{
		*CursorEffect = EDropEffect::None;
	}

	return 0;
}

HRESULT FWindowsApplication::OnOLEDragOver( const HWND HWnd, DWORD KeyState, POINTL CursorPosition, DWORD *CursorEffect)
{
	const TSharedPtr< FWindowsWindow > Window = FindWindowByHWND( Windows, HWnd );

	if ( Window.IsValid() )
	{
		if ( Window->IsEnabled() )
		{
			*CursorEffect = MessageHandler->OnDragOver( Window.ToSharedRef() );
		}
		else
		{
			*CursorEffect = EDropEffect::None;
		}
	}

	return 0;
}

HRESULT FWindowsApplication::OnOLEDragOut( const HWND HWnd )
{
	const TSharedPtr< FWindowsWindow > Window = FindWindowByHWND( Windows, HWnd );

	if ( Window.IsValid() && Window->IsEnabled() )
	{
		// User dragged out of a Slate window. We must tell Slate it is no longer in drag and drop mode.
		// Note that this also gets triggered when the user hits ESC to cancel a drag and drop.
		MessageHandler->OnDragLeave( Window.ToSharedRef() );
	}

	return 0;
}

HRESULT FWindowsApplication::OnOLEDrop( const HWND HWnd, const FDragDropOLEData& OLEData, DWORD KeyState, POINTL CursorPosition, DWORD *CursorEffect)
{
	const TSharedPtr< FWindowsWindow > Window = FindWindowByHWND( Windows, HWnd );

	if ( Window.IsValid() )
	{
		if ( Window->IsEnabled() )
		{
			*CursorEffect = MessageHandler->OnDragDrop(Window.ToSharedRef());
		}
		else
		{
			*CursorEffect = EDropEffect::None;
		}
	}

	return 0;
}


void FWindowsApplication::AddMessageHandler(IWindowsMessageHandler& InMessageHandler)
{
	WindowsApplication->MessageHandlers.AddUnique(&InMessageHandler);
}


void FWindowsApplication::RemoveMessageHandler(IWindowsMessageHandler& InMessageHandler)
{
	WindowsApplication->MessageHandlers.RemoveSwap(&InMessageHandler);
}


void FWindowsApplication::QueryConnectedMice()
{
	TArray<RAWINPUTDEVICELIST> DeviceList;
	UINT DeviceCount = 0;

	GetRawInputDeviceList(nullptr, &DeviceCount, sizeof(RAWINPUTDEVICELIST));
	if (DeviceCount == 0)
	{
		bIsMouseAttached = false;
		return;
	}

	DeviceList.AddUninitialized(DeviceCount);
	GetRawInputDeviceList(DeviceList.GetData(), &DeviceCount, sizeof(RAWINPUTDEVICELIST));
	
	int32 MouseCount = 0;
	for (const auto& Device : DeviceList)
	{
		UINT NameLen = 0;
		TUniquePtr<char[]> Name;
		if (Device.dwType != RIM_TYPEMOUSE)
			continue;

		//Force the use of ANSI versions of these calls
		if (GetRawInputDeviceInfoA(Device.hDevice, RIDI_DEVICENAME, nullptr, &NameLen) == static_cast<UINT>(-1))
			continue;

		Name = MakeUnique<char[]>(NameLen+1);
		if (GetRawInputDeviceInfoA(Device.hDevice, RIDI_DEVICENAME, Name.Get(), &NameLen) == static_cast<UINT>(-1))
			continue;

		Name[NameLen] = 0;
		FString WName = ANSI_TO_TCHAR(Name.Get());
		WName.ReplaceInline(TEXT("#"), TEXT("\\"), ESearchCase::CaseSensitive);

		/*
		 * Name XP starts with \??\, vista+ starts \\?\ 
		 * In the device list exists a fake Mouse device with the device name of RDP_MOU
		 * This is used for Remote Desktop so ignore it.
		 */
		if (WName.StartsWith(TEXT("\\??\\ROOT\\RDP_MOU\\")) || WName.StartsWith(TEXT("\\\\?\\ROOT\\RDP_MOU\\")))
		{
			continue;
		}

		++MouseCount;
	}

	// If the session is a remote desktop session - assume that a mouse is present, it seems that you can end up
	// in a situation where RDP mice don't have a valid name, so the code above fails to locate a valid mouse, 
	// even though one is present.
	if ( MouseCount == 0 )
	{
		if ( GetSystemMetrics(SM_REMOTESESSION) )
		{
			MouseCount++;
		}
	}

	bIsMouseAttached = MouseCount > 0;
}

#if WINVER >= 0x0601
uint32 FWindowsApplication::GetTouchIndexForID( int32 TouchID )
{
	for (int i = 0; i < TouchIDs.Num(); i++)
	{
		if ( TouchIDs[i].IsSet() && TouchIDs[i].GetValue() == TouchID )
		{
			return i;
		}
	}
	return INDEX_NONE;
}

uint32 FWindowsApplication::GetFirstFreeTouchIndex()
{
	for ( int i = 0; i < TouchIDs.Num(); i++ )
	{
		if ( TouchIDs[i].IsSet() == false )
		{
			return i;
		}
	}

	return TouchIDs.Add(TOptional<int32>());
}
#endif

void FTaskbarList::Initialize()
{
	if (LIKELY(FApp::CanEverRender()) && FWindowsPlatformMisc::CoInitialize())
	{
		if (CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, IID_ITaskbarList3, (void **)&TaskBarList3) != S_OK)
		{
			TaskBarList3 = nullptr;
		}
	}
}

FTaskbarList::FTaskbarList() 
	: TaskBarList3(NULL)
{

}

FTaskbarList::~FTaskbarList()
{
	if (TaskBarList3 && FWindowsPlatformMisc::CoInitialize())
	{
		TaskBarList3->Release();
	}

	TaskBarList3 = NULL;
}

void FTaskbarList::SetOverlayIcon(const TSharedRef<FGenericWindow>& NativeWindow, HICON Icon, FText Description)
{
	if (TaskBarList3)
	{
		const TSharedRef< FWindowsWindow > Window = StaticCastSharedRef< FWindowsWindow >(NativeWindow);
		TaskBarList3->SetOverlayIcon(Window->GetHWnd(), Icon, *Description.ToString());
	}
}

void FTaskbarList::SetProgressValue(const TSharedRef<FGenericWindow>& NativeWindow, uint64 Current, uint64 Total)
{
	if (TaskBarList3)
	{
		const TSharedRef< FWindowsWindow > Window = StaticCastSharedRef< FWindowsWindow >(NativeWindow);
		TaskBarList3->SetProgressValue(Window->GetHWnd(), (ULONGLONG)Current, (ULONGLONG)Total);
	}
}

void FTaskbarList::SetProgressState(const TSharedRef<FGenericWindow>& NativeWindow, ETaskbarProgressState::Type State)
{
	if (TaskBarList3)
	{
		const TSharedRef< FWindowsWindow > Window = StaticCastSharedRef< FWindowsWindow >(NativeWindow);
		TaskBarList3->SetProgressState(Window->GetHWnd(), (TBPFLAG)State);
	}
}

TSharedRef<FTaskbarList> FTaskbarList::Create()
{
	TSharedRef<FTaskbarList> TaskbarList = MakeShareable(new FTaskbarList());
	TaskbarList->Initialize();

	return TaskbarList;
}

// Restore the windowsx.h macro for IsMaximized
#pragma pop_macro("IsMaximized")

#include "Windows/HideWindowsPlatformTypes.h"
