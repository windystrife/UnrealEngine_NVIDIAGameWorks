// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Linux/LinuxWindow.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Internationalization/Text.h"
#include "Math/Vector2D.h"
#include "GenericPlatform/GenericWindowDefinition.h"
#include "Misc/App.h"
#include "Linux/LinuxApplication.h"
#include "Linux/LinuxPlatformApplicationMisc.h"
#include "Internationalization.h" // LOCTEXT

#define LOCTEXT_NAMESPACE "LinuxWindow"

DEFINE_LOG_CATEGORY( LogLinuxWindow );
DEFINE_LOG_CATEGORY( LogLinuxWindowType );
DEFINE_LOG_CATEGORY( LogLinuxWindowEvent );

// SDL 2.0.4 as of 10374:dccf51aee79b will account for border width/height automatically (see SDL_x11window.c)
// might need to be a function in case SDL gets overriden at runtime
#define UE4_USING_BORDERS_AWARE_SDL					1

FLinuxWindow::~FLinuxWindow()
{
	// NOTE: The HWnd is invalid here!
	//       Doing stuff with HWnds will fail silently.
	//       Use Destroy() instead.
}

TSharedRef< FLinuxWindow > FLinuxWindow::Make()
{
	// First, allocate the new native window object.  This doesn't actually create a native window or anything,
	// we're simply instantiating the object so that we can keep shared references to it.
	return MakeShareable( new FLinuxWindow() );
}

FLinuxWindow::FLinuxWindow()
	: HWnd(NULL)
	, WindowMode( EWindowMode::Windowed )
	, bIsVisible( false )
	, bWasFullscreen( false )
	, bIsPopupWindow(false)
	, bIsTooltipWindow(false)
	, bIsConsoleWindow(false)
	, bIsDialogWindow(false)
	, bIsNotificationWindow(false)
	, bIsTopLevelWindow(false)
	, bIsDragAndDropWindow(false)
	, bIsUtilityWindow(false)
	, bIsPointerInsideWindow(false)
	, LeftBorderWidth(0)
	, TopBorderHeight(0)
	, bValidNativePropertiesCache(false)
	, DPIScaleFactor(1.0f)
{
	PreFullscreenWindowRect.left = PreFullscreenWindowRect.top = PreFullscreenWindowRect.right = PreFullscreenWindowRect.bottom = 0;
}

SDL_HWindow FLinuxWindow::GetHWnd() const
{
	return HWnd;
}

//	HINSTANCE InHInstance,
void FLinuxWindow::Initialize( FLinuxApplication* const Application, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FLinuxWindow >& InParent, const bool bShowImmediately )
{
	Definition = InDefinition;
	OwningApplication = Application;
	ParentWindow = InParent;

	if (!FPlatformApplicationMisc::InitSDL()) //	will not initialize more than once
	{
		UE_LOG(LogInit, Fatal, TEXT("FLinuxWindow::Initialize() : InitSDL() failed, cannot initialize window."));
		// unreachable
		return;
	}

#if DO_CHECK
	uint32 InitializedSubsystems = SDL_WasInit(SDL_INIT_EVERYTHING);
	check(InitializedSubsystems & SDL_INIT_VIDEO);
#endif // DO_CHECK

	// Finally, let's initialize the new native window object.  Calling this function will often cause OS
	// window messages to be sent! (such as activation messages)
	uint32 WindowExStyle = 0;
	uint32 WindowStyle = 0;

	RegionWidth = RegionHeight = INDEX_NONE;

	const float XInitialRect = Definition->XDesiredPositionOnScreen;
	const float YInitialRect = Definition->YDesiredPositionOnScreen;

	const float WidthInitial = Definition->WidthDesiredOnScreen;
	const float HeightInitial = Definition->HeightDesiredOnScreen;

	// calculate the DPI at the centerpoint
	float XCenter = XInitialRect + WidthInitial / 2.0f;
	float YCenter = YInitialRect + HeightInitial / 2.0f;
	DPIScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(XCenter, YCenter);

	int32 X = FMath::TruncToInt( XInitialRect + 0.5f );
	int32 Y = FMath::TruncToInt( YInitialRect + 0.5f );
	int32 ClientWidth = FMath::TruncToInt( WidthInitial + 0.5f );
	int32 ClientHeight = FMath::TruncToInt( HeightInitial + 0.5f );
	int32 WindowWidth = ClientWidth;
	int32 WindowHeight = ClientHeight;

	WindowStyle |= FLinuxPlatformApplicationMisc::WindowStyle() | SDL_WINDOW_SHOWN;

	if ( !Definition->HasOSWindowBorder )
	{
		WindowStyle |= SDL_WINDOW_BORDERLESS;

		if (Definition->IsTopmostWindow)
		{
			WindowStyle |= SDL_WINDOW_ALWAYS_ON_TOP;
		}

		if (!Definition->AppearsInTaskbar)
		{
			WindowStyle |= SDL_WINDOW_SKIP_TASKBAR;
		}

		if (Definition->IsRegularWindow && Definition->HasSizingFrame)
		{
			WindowStyle |= SDL_WINDOW_RESIZABLE;
		}
	}

	const bool bShouldActivate = Definition->ActivationPolicy != EWindowActivationPolicy::Never;

	// This is a tool tip window.
	if (!InParent.IsValid() && !Definition->HasOSWindowBorder &&
		!Definition->AcceptsInput && Definition->IsTopmostWindow && 
		!Definition->AppearsInTaskbar && !Definition->HasSizingFrame &&
		!Definition->IsModalWindow && !Definition->IsRegularWindow &&
		Definition->SizeWillChangeOften)
	{
		WindowStyle |= SDL_WINDOW_TOOLTIP;
		bIsTooltipWindow = true;
		UE_LOG(LogLinuxWindowType, Verbose, TEXT("*** New Window is a Tooltip Window ***"));
	}
	// This is a notification window.
	else if (InParent.IsValid() && !Definition->HasOSWindowBorder &&
		Definition->AcceptsInput && !Definition->IsTopmostWindow && 
		!Definition->AppearsInTaskbar && !Definition->HasSizingFrame &&
		!Definition->IsModalWindow && !Definition->IsRegularWindow &&
		!bShouldActivate && Definition->SizeWillChangeOften)
	{
		WindowStyle |= SDL_WINDOW_NOTIFICATION;
		bIsNotificationWindow = true;
		UE_LOG(LogLinuxWindowType, Verbose, TEXT("*** New Window is a Notification Window ***"));
	}
	// Is it another notification window?
	else if (InParent.IsValid() && !Definition->HasOSWindowBorder &&
		Definition->AcceptsInput && !Definition->IsTopmostWindow && 
		!Definition->AppearsInTaskbar && !Definition->HasSizingFrame &&
		Definition->IsModalWindow && !Definition->IsRegularWindow &&
		bShouldActivate && !Definition->SizeWillChangeOften)
	{
		WindowStyle |= SDL_WINDOW_NOTIFICATION;
		bIsNotificationWindow = true;
		UE_LOG(LogLinuxWindowType, Verbose, TEXT("*** New Window is another Notification Window ***"));
	}
	// This is a popup menu window?
	else if (InParent.IsValid() && !Definition->HasOSWindowBorder &&
		Definition->AcceptsInput && !Definition->IsTopmostWindow && 
		!Definition->AppearsInTaskbar && !Definition->HasSizingFrame &&
		!Definition->IsModalWindow && !Definition->IsRegularWindow &&
		bShouldActivate && !Definition->SizeWillChangeOften)
	{
		WindowStyle |= SDL_WINDOW_POPUP_MENU;
		bIsPopupWindow = true;
		UE_LOG(LogLinuxWindowType, Verbose, TEXT("*** New Window is a Popup Menu Window ***"));
	}
	// Is it a console window?
	else if( InParent.IsValid() && !Definition->HasOSWindowBorder &&
		Definition->AcceptsInput && !Definition->IsTopmostWindow && 
		!Definition->AppearsInTaskbar && !Definition->HasSizingFrame &&
		!Definition->IsModalWindow && !Definition->IsRegularWindow &&
		!bShouldActivate && !Definition->SizeWillChangeOften)
	{
		WindowStyle |= SDL_WINDOW_POPUP_MENU;
		bIsConsoleWindow = true;
		bIsPopupWindow = true;
		UE_LOG(LogLinuxWindowType, Verbose, TEXT("*** New Window is a Console Window ***"));
	}
	// Is it a drag and drop window?
	else if (!InParent.IsValid() && !Definition->HasOSWindowBorder &&
		!Definition->AcceptsInput && Definition->IsTopmostWindow && 
		!Definition->AppearsInTaskbar && !Definition->HasSizingFrame &&
		!Definition->IsModalWindow && !Definition->IsRegularWindow &&
		!bShouldActivate && !Definition->SizeWillChangeOften)
	{
		// TODO Experimental (The SDL_WINDOW_DND sets focus)
		WindowStyle |= SDL_WINDOW_DND;
		bIsDragAndDropWindow = true;
		UE_LOG(LogLinuxWindowType, Verbose, TEXT("*** New Window is a Drag and Drop Window ***"));
	}
	// Is modal dialog window?
	else if (InParent.IsValid() && !Definition->HasOSWindowBorder &&
		Definition->AcceptsInput && !Definition->IsTopmostWindow && 
		Definition->AppearsInTaskbar && Definition->IsModalWindow &&
		Definition->IsRegularWindow && bShouldActivate &&
		!Definition->SizeWillChangeOften)
	{
		WindowStyle |= SDL_WINDOW_DIALOG;
		bIsDialogWindow = true;
		UE_LOG(LogLinuxWindowType, Verbose, TEXT("*** New Window is a Modal Dialog Window ***"));
	}
	// Is a Blueprint, Cascade, etc. utility window.
	else if (InParent.IsValid() && !Definition->HasOSWindowBorder &&
		Definition->AcceptsInput && !Definition->IsTopmostWindow && 
		Definition->AppearsInTaskbar && Definition->HasSizingFrame &&
		!Definition->IsModalWindow && Definition->IsRegularWindow &&
		bShouldActivate && !Definition->SizeWillChangeOften)
	{
		WindowStyle |= SDL_WINDOW_DIALOG;
		bIsUtilityWindow = true;
		UE_LOG(LogLinuxWindowType, Verbose, TEXT("*** New Window is a BP, Cascade, etc. Window ***"));
	}
	else
	{
		UE_LOG(LogLinuxWindowType, Verbose, TEXT("*** New Window is TopLevel Window ***"));
		bIsTopLevelWindow = true;
	}

	//	The SDL window doesn't need to be reshaped.
	//	the size of the window you input is the sizeof the client.
	HWnd = SDL_CreateWindow( TCHAR_TO_ANSI( *Definition->Title ), X, Y, ClientWidth, ClientHeight, WindowStyle  );
	// produce a helpful message for common driver errors
	if (HWnd == nullptr)
	{
		FString ErrorMessage;

		if ((WindowStyle & SDL_WINDOW_VULKAN) != 0)
		{
			ErrorMessage = LOCTEXT("VulkanWindowCreationFailedLinux", "Unable to create a Vulkan window - make sure an up-to-date libvulkan.so.1 is installed.").ToString();
			FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *ErrorMessage,
										 *LOCTEXT("VulkanWindowCreationFailedLinuxTitle", "Unable to create a Vulkan window.").ToString());
		}
		else if ((WindowStyle & SDL_WINDOW_OPENGL) != 0)
		{
			ErrorMessage = LOCTEXT("OpenGLWindowCreationFailedLinux", "Unable to create an OpenGL window - make sure your drivers support at least OpenGL 4.3.").ToString();
			FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *ErrorMessage,
										 *LOCTEXT("OpenGLWindowCreationFailedLinuxTitle", "Unable to create an OpenGL window.").ToString());
		}
		else
		{
			ErrorMessage = FString::Printf(*LOCTEXT("SDLWindowCreationFailedLinux", "Window creation failed (SDL error: '%s'')").ToString(), UTF8_TO_TCHAR(SDL_GetError()));
			FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *ErrorMessage,
										 *LOCTEXT("SDLWindowCreationFailedLinuxTitle", "Unable to create an SDL window.").ToString());
		}

		checkf(false, TEXT("%s"), *ErrorMessage);
		// unreachable
		return;
	}

	if (Definition->AppearsInTaskbar)
	{
		// Try to find an icon for the window
		const FText GameName = FText::FromString(FApp::GetProjectName());
		FString Iconpath;
		SDL_Surface *IconImage = nullptr;

		if (!GameName.IsEmpty())
		{
			if (GIsEditor)
			{
				Iconpath = FPaths::ProjectContentDir() / TEXT("Splash/EdIcon.bmp");
			}
			else
			{
				Iconpath = FPaths::ProjectContentDir() / TEXT("Splash/Icon.bmp");
			}

			Iconpath = FPaths::FPaths::ConvertRelativePathToFull(Iconpath);
			if (IFileManager::Get().FileSize(*Iconpath) != -1)
			{
				IconImage = SDL_LoadBMP(TCHAR_TO_UTF8(*Iconpath));
			}
		}

		if (IconImage == nullptr)
		{
			// no game specified or there's no custom icons for the game. use default icons.
			if (GIsEditor)
			{
				Iconpath = FPaths::EngineContentDir() / TEXT("Splash/EdIconDefault.bmp");
			}
			else
			{
				Iconpath = FPaths::EngineContentDir() / TEXT("Splash/IconDefault.bmp");
			}

			Iconpath = FPaths::FPaths::ConvertRelativePathToFull(Iconpath);
			if (IFileManager::Get().FileSize(*Iconpath) != -1)
			{
				IconImage = SDL_LoadBMP(TCHAR_TO_UTF8(*Iconpath));
			}
		}

		if (IconImage != nullptr)
		{
			SDL_SetWindowIcon(HWnd, IconImage);

			SDL_FreeSurface(IconImage);
			IconImage = nullptr;
		}
	}

	SDL_SetWindowHitTest( HWnd, FLinuxWindow::HitTest, this );

	/* 
		Do not set for Notification Windows the transient flag because the WM's usually raise the the parent window
		if the Notificaton Window gets raised. That behaviour is to aggresive and disturbs users doing other things 
		while UE4 calculates lights and other things and pop ups notifications. Notifications will be handled so that 
		they are some sort of independend but will be raised if the TopLevel Window gets focused or activated.
	*/
	// Make the Window modal for it's parent.
	if (bIsUtilityWindow || bIsDialogWindow || bIsConsoleWindow || bIsDialogWindow)
	{
		SDL_SetWindowModalFor(HWnd, InParent->GetHWnd());
	}

	VirtualWidth  = ClientWidth;
	VirtualHeight = ClientHeight;

	// attempt to early cache native properties
	CacheNativeProperties();

	// We call reshape window here because we didn't take into account the non-client area
	// in the initial creation of the window. Slate should only pass client area dimensions.
	// Reshape window may resize the window if the non-client area is encroaching on our
	// desired client area space.
	ReshapeWindow( X, Y, ClientWidth, ClientHeight );

	if ( Definition->TransparencySupport == EWindowTransparency::PerWindow )
	{
		SetOpacity( Definition->Opacity );
	}

	// TODO This can be removed later - for debugging purposes.
	WindowSDLID = SDL_GetWindowID( HWnd );
}

SDL_HitTestResult FLinuxWindow::HitTest( SDL_Window *SDLwin, const SDL_Point *point, void *data )
{
	FLinuxWindow *pself = static_cast<FLinuxWindow *>( data );
	TSharedPtr< FLinuxWindow > self = pself->OwningApplication->FindWindowBySDLWindow( SDLwin );
	if ( !self.IsValid() ) 
	{
		UE_LOG(LogLinuxWindow, Warning, TEXT("BAD EVENT: SDL window = %p\n"), SDLwin);
		return SDL_HITTEST_NORMAL;
	}

	EWindowZone::Type eventZone = pself->OwningApplication->WindowHitTest( self, point->x, point->y );

	static const SDL_HitTestResult Results[] = 
	{
		SDL_HITTEST_NORMAL,
		SDL_HITTEST_RESIZE_TOPLEFT,
		SDL_HITTEST_RESIZE_TOP,
		SDL_HITTEST_RESIZE_TOPRIGHT,
		SDL_HITTEST_RESIZE_LEFT,
		SDL_HITTEST_NORMAL,
		SDL_HITTEST_RESIZE_RIGHT,
		SDL_HITTEST_RESIZE_BOTTOMLEFT,
		SDL_HITTEST_RESIZE_BOTTOM,
		SDL_HITTEST_RESIZE_BOTTOMRIGHT,
		SDL_HITTEST_DRAGGABLE,
		SDL_HITTEST_NORMAL,
		SDL_HITTEST_NORMAL,
		SDL_HITTEST_NORMAL,
		SDL_HITTEST_NORMAL
	};

	return Results[eventZone];
}

/** Native windows should implement MoveWindowTo by relocating the platform-specific window to (X,Y). */
void FLinuxWindow::MoveWindowTo( int32 X, int32 Y )
{
	if (UE4_USING_BORDERS_AWARE_SDL)
	{
		SDL_SetWindowPosition( HWnd, X, Y );
	}
	else
	{
		// we are passed coordinates of a client area, so account for decorations
		checkf(bValidNativePropertiesCache, TEXT("Attempted to use border sizes too early, native properties aren't yet cached. Review the flow"));

		SDL_SetWindowPosition( HWnd, X - LeftBorderWidth, Y - TopBorderHeight );
	}
}

/** Native windows should implement BringToFront by making this window the top-most window (i.e. focused).
 *
 * @param bForce	Forces the window to the top of the Z order, even if that means stealing focus from other windows
 *					In general do not pass true for this.  It is really only useful for some windows, like game windows where not forcing it to the front
 *					could cause mouse capture and mouse lock to happen but without the window visible
 */
void FLinuxWindow::BringToFront( bool bForce )
{
	// TODO Forces the the window to top of z order? Only that? SDL is using XMapRaised which changes the z order
	// so we do not steal focus here I guess.
	if(bForce)
	{
		SDL_RaiseWindow(HWnd);
	}
	else
	{
		SDL_ShowWindow(HWnd);
	}
}

/** Native windows should implement this function by asking the OS to destroy OS-specific resource associated with the window (e.g. Win32 window handle) */
void FLinuxWindow::Destroy()
{
	OwningApplication->RemoveRevertFocusWindow( HWnd );
	OwningApplication->RemoveEventWindow( HWnd );
	OwningApplication->RemoveNotificationWindow( HWnd );

	// We cannot destroy the window right now as it may be accessed by render thread, since Slate queued it for drawing earlier.
	// To make sure no window gets destroyed while we're blitting into it, defer destroying the window to the app.
	OwningApplication->DestroyNativeWindow(HWnd);
}

/** Native window should implement this function by performing the equivalent of the Win32 minimize-to-taskbar operation */
void FLinuxWindow::Minimize()
{
	SDL_MinimizeWindow( HWnd );
}

/** Native window should implement this function by performing the equivalent of the Win32 maximize operation */
void FLinuxWindow::Maximize()
{
	SDL_MaximizeWindow( HWnd );
}

/** Native window should implement this function by performing the equivalent of the Win32 maximize operation */
void FLinuxWindow::Restore()
{
	SDL_RestoreWindow( HWnd );
}

/** Native window should make itself visible */
void FLinuxWindow::Show()
{
	if ( !bIsVisible )
	{
		bIsVisible = true;
		SDL_ShowWindow( HWnd );
	}
}

/** Native window should hide itself */
void FLinuxWindow::Hide()
{
	if ( bIsVisible )
	{
		bIsVisible = false;
		SDL_HideWindow( HWnd );
	}
}

static void _GetBestFullscreenResolution( SDL_HWindow hWnd, int32 *pWidth, int32 *pHeight )
{
	uint32 InitializedMode = false;
	uint32 BestWidth = 0;
	uint32 BestHeight = 0;
	uint32 ModeIndex = 0;

	int32 dsp_idx = SDL_GetWindowDisplayIndex( hWnd );
	if ( dsp_idx < 0 )
	{	dsp_idx = 0;	}

	SDL_DisplayMode dsp_mode;
	FMemory::Memzero( &dsp_mode, sizeof(SDL_DisplayMode) );

	while ( !SDL_GetDisplayMode( dsp_idx, ModeIndex++, &dsp_mode ) )
	{
		bool IsEqualOrBetterWidth  = FMath::Abs((int32)dsp_mode.w - (int32)(*pWidth))  <= FMath::Abs((int32)BestWidth  - (int32)(*pWidth ));
		bool IsEqualOrBetterHeight = FMath::Abs((int32)dsp_mode.h - (int32)(*pHeight)) <= FMath::Abs((int32)BestHeight - (int32)(*pHeight));
		if	(!InitializedMode || (IsEqualOrBetterWidth && IsEqualOrBetterHeight))
		{
			BestWidth = dsp_mode.w;
			BestHeight = dsp_mode.h;
			InitializedMode = true;
		}
	}

	check(InitializedMode);

	*pWidth  = BestWidth;
	*pHeight = BestHeight;
}

void FLinuxWindow::ReshapeWindow( int32 NewX, int32 NewY, int32 NewWidth, int32 NewHeight )
{
	switch( WindowMode )
	{
		// Fullscreen and WindowedFullscreen both use SDL_WINDOW_FULLSCREEN_DESKTOP now
		//  and code elsewhere handles the backbufer blit properly. This solves several
		//  problems that actual mode switches cause, and a GPU scales better than LCD display.
		// If this is changed, change SetWindowMode() and FSystemResolution::RequestResolutionChange() as well.
		case EWindowMode::Fullscreen:
		case EWindowMode::WindowedFullscreen:
		{
			SDL_SetWindowFullscreen( HWnd, 0 );
			SDL_SetWindowSize( HWnd, NewWidth, NewHeight );
			SDL_SetWindowFullscreen( HWnd, SDL_WINDOW_FULLSCREEN_DESKTOP );
			bWasFullscreen = true;
		}
		break;

		case EWindowMode::Windowed:
		{
			if (UE4_USING_BORDERS_AWARE_SDL == 0 && Definition->HasOSWindowBorder)
			{
				// we are passed coordinates of a client area, so account for decorations
				checkf(bValidNativePropertiesCache, TEXT("Attempted to use border sizes too early, native properties aren't yet cached. Review the flow"));
				NewX -= LeftBorderWidth;
				NewY -= TopBorderHeight;
			}
			SDL_SetWindowPosition( HWnd, NewX, NewY );
			SDL_SetWindowSize( HWnd, NewWidth, NewHeight );

			bWasFullscreen = false;

		}
		break;
	}

	RegionWidth   = NewWidth;
	RegionHeight  = NewHeight;
	VirtualWidth  = NewWidth;
	VirtualHeight = NewHeight;
}

/** Toggle native window between fullscreen and normal mode */
void FLinuxWindow::SetWindowMode( EWindowMode::Type NewWindowMode )
{
	if( NewWindowMode != WindowMode )
	{
		switch( NewWindowMode )
		{
			// Fullscreen and WindowedFullscreen both use SDL_WINDOW_FULLSCREEN_DESKTOP now
			//  and code elsewhere handles the backbufer blit properly. This solves several
			//  problems that actual mode switches cause, and a GPU scales better than LCD display.
			// If this is changed, change ReshapeWindow() and FSystemResolution::RequestResolutionChange() as well.
			case EWindowMode::Fullscreen:
			case EWindowMode::WindowedFullscreen:
			{
				if ( bWasFullscreen != true )
				{
					SDL_SetWindowSize( HWnd, VirtualWidth, VirtualHeight );
					SDL_SetWindowFullscreen( HWnd, SDL_WINDOW_FULLSCREEN_DESKTOP );
					bWasFullscreen = true;
				}
			}
			break;

			case EWindowMode::Windowed:
			{
				// when going back to windowed from desktop, make window smaller (but not too small),
				// since some too smart window managers (Compiz) will maximize the window if it's set to desktop size.
				// @FIXME: [RCL] 2015-02-10: this is a hack.
				int SmallerWidth = FMath::Max(100, VirtualWidth - 100);
				int SmallerHeight = FMath::Max(100, VirtualHeight - 100);
				SDL_SetWindowSize(HWnd, SmallerWidth, SmallerHeight);

				SDL_SetWindowFullscreen( HWnd, 0 );
				SDL_SetWindowBordered( HWnd, SDL_TRUE );

				SDL_SetWindowGrab( HWnd, SDL_FALSE );

				bWasFullscreen = false;
			}
			break;
		}


		WindowMode = NewWindowMode;
	}
}


/** @return	Gives the native window a chance to adjust our stored window size before we cache it off */

void FLinuxWindow::AdjustCachedSize( FVector2D& Size ) const
{
	if	( Definition.IsValid() && Definition->SizeWillChangeOften )
	{
		Size = FVector2D( VirtualWidth, VirtualHeight );
	}
	else
	if	( HWnd )
	{
		int SizeW, SizeH;

		if ( WindowMode == EWindowMode::Windowed )
		{
			SDL_GetWindowSize( HWnd, &SizeW, &SizeH );
		}
		else // windowed fullscreen or fullscreen
		{
			SizeW = VirtualWidth ;
			SizeH = VirtualHeight;

			_GetBestFullscreenResolution( HWnd, &SizeW, &SizeH );
		}

		Size = FVector2D( SizeW, SizeH );
	}
}

bool FLinuxWindow::GetFullScreenInfo( int32& X, int32& Y, int32& Width, int32& Height ) const
{
	SDL_Rect DisplayRect;

	int DisplayIdx = SDL_GetWindowDisplayIndex(HWnd);
	if (DisplayIdx >= 0 && SDL_GetDisplayBounds(DisplayIdx, &DisplayRect) == 0)
	{
		X = DisplayRect.x;
		Y = DisplayRect.y;
		Width = DisplayRect.w;
		Height = DisplayRect.h;

		return true;
	}

	return false;
}

/** @return true if the native window is maximized, false otherwise */
bool FLinuxWindow::IsMaximized() const
{
	uint32 flag = SDL_GetWindowFlags( HWnd );

	if ( flag & SDL_WINDOW_MAXIMIZED )
	{
		return true;
	}
	else
	{
		return false;
	}
}

/** @return true if the native window is visible, false otherwise */
bool FLinuxWindow::IsVisible() const
{
	return bIsVisible;
}

/** Returns the size and location of the window when it is restored */
bool FLinuxWindow::GetRestoredDimensions(int32& X, int32& Y, int32& Width, int32& Height)
{
	SDL_GetWindowPosition(HWnd, &X, &Y);
	SDL_GetWindowSize(HWnd, &Width, &Height);

	return true;
}

/** Sets focus on the native window */
void FLinuxWindow::SetWindowFocus()
{
	// Setting focus here is troublesome at least when running on X11, since unlike other platforms
	// it is asynchronous and SetWindowFocus() may happen at an inappropriate time (e.g. on an unmapped window).
	// Instead of allowing this silently fail this is abstracted away from Slate, actual focus change will happen later (when handling window messages).
	// @todo: should we queue the focus change if this function gets called long after the window creation?
//	SDL_SetWindowInputFocus( HWnd );
}

/**
 * Sets the opacity of this window
 *
 * @param	InOpacity	The new window opacity represented as a floating point scalar
 */
void FLinuxWindow::SetOpacity( const float InOpacity )
{
	SDL_SetWindowOpacity(HWnd, InOpacity);
}

/**
 * Enables or disables this window.  When disabled, a window will receive no input.       
 *
 * @param bEnable	true to enable the window, false to disable it.
 */
void FLinuxWindow::Enable( bool bEnable )
{
	// Different WMs handle this in different way.
	// TODO: figure out if ignoring this causes problems for Slate
}

/** @return true if native window exists underneath the coordinates */
bool FLinuxWindow::IsPointInWindow( int32 X, int32 Y ) const
{
	int32 width = 0, height = 0;

	SDL_GetWindowSize( HWnd, &width, &height );
	
	return X > 0 && Y > 0 && X < width && Y < height;
}

int32 FLinuxWindow::GetWindowBorderSize() const
{
	return 0;
}

bool FLinuxWindow::IsForegroundWindow() const
{
	if (OwningApplication->GetCurrentActiveWindow().IsValid())
	{
		return OwningApplication->GetCurrentActiveWindow()->GetHWnd() == HWnd;
	}
	else
	{
		return false;
	}
}

void FLinuxWindow::SetText( const TCHAR* const Text )
{
	SDL_SetWindowTitle( HWnd, TCHAR_TO_ANSI(Text));
}

bool FLinuxWindow::IsRegularWindow() const
{
	return Definition->IsRegularWindow;
}

bool FLinuxWindow::IsPopupMenuWindow() const
{
	return bIsPopupWindow;
}

bool FLinuxWindow::IsTooltipWindow() const
{
	return bIsTooltipWindow;
}

bool FLinuxWindow::IsNotificationWindow() const
{
	return bIsNotificationWindow;
}

bool FLinuxWindow::IsTopLevelWindow() const
{
	return bIsTopLevelWindow;
}

bool FLinuxWindow::IsDialogWindow() const
{
	return bIsDialogWindow;
}

bool FLinuxWindow::IsDragAndDropWindow() const
{
	return bIsDragAndDropWindow;
}

bool FLinuxWindow::IsUtilityWindow() const
{
	return bIsUtilityWindow;
}

bool FLinuxWindow::IsActivateWhenFirstShown() const
{
	return GetActivationPolicy() != EWindowActivationPolicy::Never;
}

EWindowActivationPolicy FLinuxWindow::GetActivationPolicy() const
{
	return Definition->ActivationPolicy;
}

bool FLinuxWindow::IsFocusWhenFirstShown() const
{
	return Definition->FocusWhenFirstShown;
}

uint32 FLinuxWindow::GetID() const
{
	return WindowSDLID;
}

void FLinuxWindow::LogInfo() 
{
	UE_LOG(LogLinuxWindowType, Verbose, TEXT("---------- Windows ID: %d Properties -----------)"), GetID());
	UE_LOG(LogLinuxWindowType, Verbose, TEXT("InParent: %d Parent Window ID: %d"), ParentWindow.IsValid(), ParentWindow.IsValid() ? ParentWindow->GetID() : -1);
	UE_LOG(LogLinuxWindowType, Verbose, TEXT("HasOSWindowBorder: %d"), Definition->HasOSWindowBorder);
	UE_LOG(LogLinuxWindowType, Verbose, TEXT("IsTopmostWindow: %d"), Definition->IsTopmostWindow);
	UE_LOG(LogLinuxWindowType, Verbose, TEXT("HasSizingFrame: %d"), Definition->HasSizingFrame);
	UE_LOG(LogLinuxWindowType, Verbose, TEXT("AppearsInTaskbar: %d"), Definition->AppearsInTaskbar);
	UE_LOG(LogLinuxWindowType, Verbose, TEXT("AcceptsInput: %d"), Definition->AcceptsInput);
	UE_LOG(LogLinuxWindowType, Verbose, TEXT("IsModalWindow: %d"), Definition->IsModalWindow);
	UE_LOG(LogLinuxWindowType, Verbose, TEXT("IsRegularWindow: %d"), Definition->IsRegularWindow);
	UE_LOG(LogLinuxWindowType, Verbose, TEXT("ActivationPolicy: %d"), Definition->ActivationPolicy);
	UE_LOG(LogLinuxWindowType, Verbose, TEXT("FocusWhenFirstShown: %d"), Definition->FocusWhenFirstShown);
	UE_LOG(LogLinuxWindowType, Verbose, TEXT("SizeWillChangeOften: %d"), Definition->SizeWillChangeOften);
}

const TSharedPtr< FLinuxWindow >& FLinuxWindow::GetParent() const
{
	return ParentWindow;
}

void FLinuxWindow::GetNativeBordersSize(int32& OutLeftBorderWidth, int32& OutTopBorderHeight) const
{
	checkf(bValidNativePropertiesCache, TEXT("Attempted to get border sizes too early, native properties aren't yet cached. Review the flow"));
	OutLeftBorderWidth = LeftBorderWidth;
	OutTopBorderHeight = TopBorderHeight;
}

void FLinuxWindow::CacheNativeProperties()
{
	// cache border sizes
	int Top, Left;
	if (SDL_GetWindowBordersSize(HWnd, &Top, &Left, nullptr, nullptr) == 0)
	{
		LeftBorderWidth = Left;
		TopBorderHeight = Top;
	}

	bValidNativePropertiesCache = true;
}

#undef LOCTEXT_NAMESPACE
