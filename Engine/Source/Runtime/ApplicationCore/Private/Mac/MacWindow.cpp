// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MacWindow.h"
#include "MacApplication.h"
#include "MacCursor.h"
#include "CocoaTextView.h"
#include "CocoaThread.h"
#include "MacPlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"

FMacWindow::FMacWindow()
:	WindowHandle(nullptr)
,	DisplayID(kCGNullDirectDisplay)
,	bIsVisible(false)
,	bIsClosed(false)
,	bIsFirstTimeVisible(true)
{
}

FMacWindow::~FMacWindow()
{
}

TSharedRef<FMacWindow> FMacWindow::Make()
{
	// First, allocate the new native window object.  This doesn't actually create a native window or anything,
	// we're simply instantiating the object so that we can keep shared references to it.
	return MakeShareable( new FMacWindow() );
}

void FMacWindow::Initialize( FMacApplication* const Application, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FMacWindow >& InParent, const bool bShowImmediately )
{
	SCOPED_AUTORELEASE_POOL;

	OwningApplication = Application;
	Definition = InDefinition;

	// Finally, let's initialize the new native window object.  Calling this function will often cause OS
	// window messages to be sent! (such as activation messages)

	TSharedRef<FMacScreen> TargetScreen = FMacApplication::FindScreenBySlatePosition(Definition->XDesiredPositionOnScreen, Definition->YDesiredPositionOnScreen);

	const int32 SizeX = FMath::Max(FMath::CeilToInt( Definition->WidthDesiredOnScreen ), 1);
	const int32 SizeY = FMath::Max(FMath::CeilToInt( Definition->HeightDesiredOnScreen ), 1);

	PositionX = Definition->XDesiredPositionOnScreen;
	PositionY = Definition->YDesiredPositionOnScreen >= TargetScreen->VisibleFramePixels.origin.y ? Definition->YDesiredPositionOnScreen : TargetScreen->VisibleFramePixels.origin.y;

	const float ScreenDPIScaleFactor = MacApplication->IsHighDPIModeEnabled() ? TargetScreen->Screen.backingScaleFactor : 1.0f;
	const FVector2D CocoaPosition = FMacApplication::ConvertSlatePositionToCocoa(PositionX, PositionY);
	const NSRect ViewRect = NSMakeRect(CocoaPosition.X, CocoaPosition.Y - (SizeY / ScreenDPIScaleFactor) + 1, SizeX / ScreenDPIScaleFactor, SizeY / ScreenDPIScaleFactor);

	uint32 WindowStyle = 0;
	if( Definition->IsRegularWindow )
	{
		if( Definition->HasCloseButton )
		{
			WindowStyle = NSClosableWindowMask;
		}

		// In order to support rounded, shadowed windows set the window to be titled - we'll set the content view to cover the whole window
		WindowStyle |= NSTitledWindowMask | (FPlatformMisc::IsRunningOnMavericks() ? NSTexturedBackgroundWindowMask : NSFullSizeContentViewWindowMask);
		
		if( Definition->SupportsMinimize )
		{
			WindowStyle |= NSMiniaturizableWindowMask;
		}
		if( Definition->SupportsMaximize || Definition->HasSizingFrame )
		{
			WindowStyle |= NSResizableWindowMask;
		}
	}
	else
	{
		WindowStyle = NSBorderlessWindowMask;
	}

	if( Definition->HasOSWindowBorder )
	{
		WindowStyle |= NSTitledWindowMask;
		WindowStyle &= FPlatformMisc::IsRunningOnMavericks() ? ~NSTexturedBackgroundWindowMask : ~NSFullSizeContentViewWindowMask;
	}

	MainThreadCall(^{
		SCOPED_AUTORELEASE_POOL;
		WindowHandle = [[FCocoaWindow alloc] initWithContentRect: ViewRect styleMask: WindowStyle backing: NSBackingStoreBuffered defer: NO];
		
		if( WindowHandle != nullptr )
		{
			[WindowHandle setReleasedWhenClosed:NO];
			[WindowHandle setWindowMode: EWindowMode::Windowed];
			[WindowHandle setAcceptsInput: Definition->AcceptsInput];
			[WindowHandle setDisplayReconfiguring: false];
			[WindowHandle setAcceptsMouseMovedEvents: YES];
			[WindowHandle setDelegate: WindowHandle];

			int32 WindowLevel = NSNormalWindowLevel;

			if (Definition->IsModalWindow)
			{
				WindowLevel = NSFloatingWindowLevel;
			}
			else
			{
				switch (Definition->Type)
				{
					case EWindowType::Normal:
						WindowLevel = NSNormalWindowLevel;
						break;

					case EWindowType::Menu:
						WindowLevel = NSStatusWindowLevel;
						break;

					case EWindowType::ToolTip:
						WindowLevel = NSPopUpMenuWindowLevel;
						break;

					case EWindowType::Notification:
						WindowLevel = NSModalPanelWindowLevel;
						break;

					case EWindowType::CursorDecorator:
						WindowLevel = NSMainMenuWindowLevel;
						break;
				}
			}

			[WindowHandle setLevel:WindowLevel];

			WindowedModeSavedState.WindowLevel = WindowLevel;

			if( !Definition->HasOSWindowBorder )
			{
				[WindowHandle setBackgroundColor: [NSColor clearColor]];
				[WindowHandle setHasShadow: YES];
			}

			[WindowHandle setOpaque: NO];

			[WindowHandle setMinSize:NSMakeSize(Definition->SizeLimits.GetMinWidth().Get(10.0f), Definition->SizeLimits.GetMinHeight().Get(10.0f))];
			[WindowHandle setMaxSize:NSMakeSize(Definition->SizeLimits.GetMaxWidth().Get(10000.0f), Definition->SizeLimits.GetMaxHeight().Get(10000.0f))];

			ReshapeWindow(PositionX, PositionY, SizeX, SizeY);

			if (Definition->ShouldPreserveAspectRatio)
			{
				[WindowHandle setContentAspectRatio:NSMakeSize((float)SizeX / (float)SizeY, 1.0f)];
			}

			if (Definition->IsRegularWindow)
			{
				[NSApp addWindowsItem:WindowHandle title:Definition->Title.GetNSString() filename:NO];

				// Tell Cocoa that we are opting into drag and drop.
				// Only makes sense for regular windows (windows that last a while.)
				[WindowHandle registerForDraggedTypes:@[NSFilenamesPboardType, NSPasteboardTypeString]];

				if( Definition->HasOSWindowBorder )
				{
					[WindowHandle setCollectionBehavior: NSWindowCollectionBehaviorFullScreenPrimary|NSWindowCollectionBehaviorDefault|NSWindowCollectionBehaviorManaged|NSWindowCollectionBehaviorParticipatesInCycle];
				}
				else
				{
					[WindowHandle setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary|NSWindowCollectionBehaviorDefault|NSWindowCollectionBehaviorManaged|NSWindowCollectionBehaviorParticipatesInCycle];

					if (!FPlatformMisc::IsRunningOnMavericks())
					{
						WindowHandle.titlebarAppearsTransparent = YES;
						WindowHandle.titleVisibility = NSWindowTitleHidden;
					}
				}

				SetText(*Definition->Title);
			}
			else if(Definition->AppearsInTaskbar)
			{
				if (!Definition->Title.IsEmpty())
				{
					[NSApp addWindowsItem:WindowHandle title:Definition->Title.GetNSString() filename:NO];
				}

				[WindowHandle setCollectionBehavior:NSWindowCollectionBehaviorFullScreenAuxiliary|NSWindowCollectionBehaviorDefault|NSWindowCollectionBehaviorManaged|NSWindowCollectionBehaviorParticipatesInCycle];
			}
			else
			{
				[WindowHandle setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces|NSWindowCollectionBehaviorTransient|NSWindowCollectionBehaviorIgnoresCycle];
			}

			if (Definition->TransparencySupport == EWindowTransparency::PerWindow)
			{
				SetOpacity(Definition->Opacity);
			}
			else
			{
				SetOpacity(1.0f);
			}

			OnWindowDidChangeScreen();
		}
		else
		{
			// @todo Error message should be localized!
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			NSRunInformationalAlertPanel( @"Error", @"Window creation failed!", @"Yes", NULL, NULL );
PRAGMA_ENABLE_DEPRECATION_WARNINGS
			check(0);
		}
	}, UE4ShowEventMode, true);
}

FCocoaWindow* FMacWindow::GetWindowHandle() const
{
	return WindowHandle;
}

void FMacWindow::ReshapeWindow(int32 X, int32 Y, int32 Width, int32 Height)
{
	if (WindowHandle)
	{
		ApplySizeAndModeChanges(X, Y, Width, Height, WindowHandle.TargetWindowMode);
	}
}

bool FMacWindow::GetFullScreenInfo( int32& X, int32& Y, int32& Width, int32& Height ) const
{
	SCOPED_AUTORELEASE_POOL;
	bool const bIsFullscreen = (GetWindowMode() == EWindowMode::Fullscreen);
	const NSRect Frame = WindowHandle.screen.frame;
	const FVector2D SlatePosition = FMacApplication::ConvertCocoaPositionToSlate(Frame.origin.x, Frame.origin.y - Frame.size.height + 1.0f);
	X = SlatePosition.X;
	Y = SlatePosition.Y;
	const float DPIScaleFactor = MacApplication->IsHighDPIModeEnabled() ? WindowHandle.screen.backingScaleFactor : 1.0f;
	Width = Frame.size.width * DPIScaleFactor;
	Height = Frame.size.height * DPIScaleFactor;
	return true;
}

void FMacWindow::MoveWindowTo( int32 X, int32 Y )
{
	MainThreadCall(^{
		SCOPED_AUTORELEASE_POOL;
		const FVector2D Point = FMacApplication::ConvertSlatePositionToCocoa(X, Y);
		[WindowHandle setFrameOrigin:NSMakePoint(Point.X, Point.Y - [WindowHandle openGLFrame].size.height + 1)];
	}, UE4ResizeEventMode, true);
}

void FMacWindow::BringToFront( bool bForce )
{
	if (!bIsClosed && bIsVisible)
	{
		MainThreadCall(^{
			SCOPED_AUTORELEASE_POOL;
			[WindowHandle orderFrontAndMakeMain:IsRegularWindow() andKey:IsRegularWindow()];
		}, UE4ShowEventMode, true);

		MacApplication->OnWindowOrderedFront(SharedThis(this));
	}
}

void FMacWindow::Destroy()
{
	if (WindowHandle)
	{
		SCOPED_AUTORELEASE_POOL;
		bIsClosed = true;
		[WindowHandle setAlphaValue:0.0f];
		[WindowHandle setBackgroundColor:[NSColor clearColor]];
		MacApplication->OnWindowDestroyed(SharedThis(this));
		WindowHandle = nullptr;
	}
}

void FMacWindow::Minimize()
{
	MainThreadCall(^{
		SCOPED_AUTORELEASE_POOL;
		[WindowHandle miniaturize:nil];
	}, UE4ResizeEventMode, true);
}

void FMacWindow::Maximize()
{
	MainThreadCall(^{
		SCOPED_AUTORELEASE_POOL;
		if (!WindowHandle.zoomed)
		{
			WindowHandle->bZoomed = true;
			[WindowHandle zoom:nil];
		}
	}, UE4ResizeEventMode, true);
}

void FMacWindow::Restore()
{
	MainThreadCall(^{
		SCOPED_AUTORELEASE_POOL;
		if (WindowHandle.miniaturized)
		{
			[WindowHandle deminiaturize:nil];
		}
		else if (WindowHandle.zoomed)
		{
			[WindowHandle zoom:nil];
		}
	}, UE4ResizeEventMode, true);

	WindowHandle->bZoomed = WindowHandle.zoomed;
}

void FMacWindow::Show()
{
	if (!bIsClosed && !bIsVisible)
	{
		// Should the show command include activation?
		// Do not activate windows that do not take input; e.g. tool-tips and cursor decorators
		bool bShouldActivate = false;
		if (Definition->AcceptsInput)
		{
			bShouldActivate = Definition->ActivationPolicy == EWindowActivationPolicy::Always;
			if (bIsFirstTimeVisible && Definition->ActivationPolicy == EWindowActivationPolicy::FirstShown)
			{
				bShouldActivate = true;
			}
		}

		bIsFirstTimeVisible = false;

		MainThreadCall(^{
			SCOPED_AUTORELEASE_POOL;
			[WindowHandle orderFrontAndMakeMain:bShouldActivate andKey:bShouldActivate];
		}, UE4ShowEventMode, true);

		if (bShouldActivate)
		{
			// Tell MacApplication to send window deactivate and activate messages to Slate without waiting for Cocoa events.
			MacApplication->OnWindowActivated(SharedThis(this));
		}
		else
		{
			MacApplication->OnWindowOrderedFront(SharedThis(this));
		}

		bIsVisible = true;
	}
}

void FMacWindow::Hide()
{
	if (bIsVisible)
	{
		bIsVisible = false;

		MainThreadCall(^{
			SCOPED_AUTORELEASE_POOL;
			[WindowHandle orderOut:nil];
		}, UE4CloseEventMode, true);
	}
}

void FMacWindow::SetWindowMode(EWindowMode::Type NewWindowMode)
{
	ApplySizeAndModeChanges(PositionX, PositionY, WindowHandle.contentView.frame.size.width, WindowHandle.contentView.frame.size.height, NewWindowMode);
}

EWindowMode::Type FMacWindow::GetWindowMode() const
{
	return [WindowHandle windowMode];
}

bool FMacWindow::IsMaximized() const
{
	return WindowHandle && WindowHandle->bZoomed;
}

bool FMacWindow::IsMinimized() const
{
	SCOPED_AUTORELEASE_POOL;
	return WindowHandle.miniaturized;
}

bool FMacWindow::IsVisible() const
{
	SCOPED_AUTORELEASE_POOL;
	return bIsVisible && [NSApp isHidden] == false;
}

bool FMacWindow::GetRestoredDimensions(int32& X, int32& Y, int32& Width, int32& Height)
{
	if (WindowHandle)
	{
		SCOPED_AUTORELEASE_POOL;

		NSRect Frame = WindowHandle.frame;

		const FVector2D SlatePosition = FMacApplication::ConvertCocoaPositionToSlate(Frame.origin.x, Frame.origin.y);

		const float DPIScaleFactor = MacApplication->IsHighDPIModeEnabled() ? WindowHandle.backingScaleFactor : 1.0f;
		Width = Frame.size.width * DPIScaleFactor;
		Height = Frame.size.height * DPIScaleFactor;

		X = SlatePosition.X;
		Y = SlatePosition.Y - Height + 1;

		return true;
	}
	else
	{
		return false;
	}
}

void FMacWindow::SetWindowFocus()
{
	MainThreadCall(^{
		SCOPED_AUTORELEASE_POOL;
		[WindowHandle orderFrontAndMakeMain:true andKey:true];
	}, UE4ShowEventMode, true);

	MacApplication->OnWindowOrderedFront(SharedThis(this));
}

void FMacWindow::SetOpacity( const float InOpacity )
{
	MainThreadCall(^{
		SCOPED_AUTORELEASE_POOL;
		[WindowHandle setAlphaValue:InOpacity];
	}, UE4NilEventMode, true);
}

bool FMacWindow::IsPointInWindow( int32 X, int32 Y ) const
{
	SCOPED_AUTORELEASE_POOL;

	bool PointInWindow = false;
	if (!WindowHandle.miniaturized)
	{
		NSRect WindowFrame = [WindowHandle frame];
		WindowFrame.size = [WindowHandle openGLFrame].size;
		NSRect VisibleFrame = WindowFrame;
		VisibleFrame.origin = NSMakePoint(0, 0);

		// Only the editor needs to handle the space-per-display logic introduced in Mavericks.
#if WITH_EDITOR
		// Only fetch the spans-displays once - it requires a log-out to change.
		// Note that we have no way to tell if the current setting is actually in effect,
		// so this won't work if the user schedules a change but doesn't logout to confirm it.
		static bool bScreensHaveSeparateSpaces = false;
		static bool bSettingFetched = false;
		if (!bSettingFetched)
		{
			bSettingFetched = true;
			bScreensHaveSeparateSpaces = [NSScreen screensHaveSeparateSpaces];
		}
		if (bScreensHaveSeparateSpaces)
		{
			NSRect ScreenFrame = [[WindowHandle screen] frame];
			NSRect Intersection = NSIntersectionRect(ScreenFrame, WindowFrame);
			VisibleFrame.size = Intersection.size;
			VisibleFrame.origin.x = Intersection.origin.x - WindowFrame.origin.x;
			VisibleFrame.origin.y = Intersection.origin.y - WindowFrame.origin.y;
		}
#endif

		if (WindowHandle->bIsOnActiveSpace)
		{
			const float DPIScaleFactor = GetDPIScaleFactor();
			NSPoint CursorPoint = NSMakePoint(X / DPIScaleFactor, WindowFrame.size.height - (Y / DPIScaleFactor + 1));
			PointInWindow = (NSPointInRect(CursorPoint, VisibleFrame) == YES);
		}
	}
	return PointInWindow;
}

int32 FMacWindow::GetWindowBorderSize() const
{
	return 0;
}

bool FMacWindow::IsForegroundWindow() const
{
	return WindowHandle.keyWindow;
}

void FMacWindow::SetText(const TCHAR* const Text)
{
	SCOPED_AUTORELEASE_POOL;
	if (FString(WindowHandle.title) != FString(Text))
	{
		CFStringRef CFName = FPlatformString::TCHARToCFString( Text );
		MainThreadCall(^{
			SCOPED_AUTORELEASE_POOL;
			[WindowHandle setTitle: (NSString*)CFName];
			if (IsRegularWindow())
			{
				[NSApp changeWindowsItem: WindowHandle title: (NSString*)CFName filename: NO];
			}
			CFRelease( CFName );
		}, UE4NilEventMode, true);
	}
}

bool FMacWindow::IsRegularWindow() const
{
	return Definition->IsRegularWindow;
}

float FMacWindow::GetDPIScaleFactor() const
{
	return MacApplication->IsHighDPIModeEnabled() ? WindowHandle.backingScaleFactor : 1.0f;
}

void FMacWindow::OnDisplayReconfiguration(CGDirectDisplayID Display, CGDisplayChangeSummaryFlags Flags)
{
	if(WindowHandle)
	{
		MainThreadCall(^{
			SCOPED_AUTORELEASE_POOL;
			if(Flags & kCGDisplayBeginConfigurationFlag)
			{
				[WindowHandle setMovable: YES];
				[WindowHandle setMovableByWindowBackground: NO];
				
				[WindowHandle setDisplayReconfiguring: true];
			}
			else if(Flags & kCGDisplayDesktopShapeChangedFlag)
			{
				[WindowHandle setDisplayReconfiguring: false];
			}
		});
	}
}

void FMacWindow::OnWindowDidChangeScreen()
{
	SCOPED_AUTORELEASE_POOL;
	DisplayID = [[WindowHandle.screen.deviceDescription objectForKey:@"NSScreenNumber"] unsignedIntegerValue];
}

void FMacWindow::ApplySizeAndModeChanges(int32 X, int32 Y, int32 Width, int32 Height, EWindowMode::Type WindowMode)
{
	SCOPED_AUTORELEASE_POOL;

	bool bIsFullScreen = [WindowHandle windowMode] == EWindowMode::WindowedFullscreen || [WindowHandle windowMode] == EWindowMode::Fullscreen;
	const bool bWantsFullScreen = WindowMode == EWindowMode::WindowedFullscreen || WindowMode == EWindowMode::Fullscreen;

	__block CGDisplayFadeReservationToken FadeReservationToken = 0;

	if ([WindowHandle windowMode] == EWindowMode::Fullscreen || WindowMode == EWindowMode::Fullscreen)
	{
		MainThreadCall(^{
			CGError Error = CGAcquireDisplayFadeReservation(kCGMaxDisplayReservationInterval, &FadeReservationToken);
			if (Error == kCGErrorSuccess)
			{
				CGDisplayFade(FadeReservationToken, 0.3, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0, 0, 0, true);
			}
		}, UE4ResizeEventMode, true);
	}

	if (WindowMode == EWindowMode::Windowed || WindowMode == EWindowMode::WindowedFullscreen)
	{
		if (WindowedModeSavedState.CapturedDisplayID != kCGNullDirectDisplay)
		{
			MainThreadCall(^{
				CGDisplaySetDisplayMode(WindowedModeSavedState.CapturedDisplayID, WindowedModeSavedState.DesktopDisplayMode, nullptr);
			}, UE4ResizeEventMode, true);

			CGDisplayModeRelease(WindowedModeSavedState.DesktopDisplayMode);
			WindowedModeSavedState.DesktopDisplayMode = nullptr;

			CGDisplayRelease(WindowedModeSavedState.CapturedDisplayID);
			WindowedModeSavedState.CapturedDisplayID = kCGNullDirectDisplay;

			WindowHandle.TargetWindowMode = EWindowMode::Windowed;
			UpdateFullScreenState(true);
			bIsFullScreen = false;
		}

		WindowHandle.TargetWindowMode = WindowMode;

		const float DPIScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(X, Y);
		Width /= DPIScaleFactor;
		Height /= DPIScaleFactor;

		const FVector2D CocoaPosition = FMacApplication::ConvertSlatePositionToCocoa(X, Y);
		NSRect Rect = NSMakeRect(CocoaPosition.X, CocoaPosition.Y - Height + 1, FMath::Max(Width, 1), FMath::Max(Height, 1));
		if (Definition->HasOSWindowBorder)
		{
			Rect = [WindowHandle frameRectForContentRect:Rect];
		}

		UpdateFullScreenState(bWantsFullScreen != bIsFullScreen);

		if (WindowMode == EWindowMode::Windowed && !NSEqualRects([WindowHandle frame], Rect))
		{
			MainThreadCall(^{
				SCOPED_AUTORELEASE_POOL;
				[WindowHandle setFrame:Rect display:YES];

				if (Definition->ShouldPreserveAspectRatio)
				{
					[WindowHandle setContentAspectRatio:NSMakeSize((float)Width / (float)Height, 1.0f)];
				}
			}, UE4ResizeEventMode, true);
		}
	}
	else
	{
		WindowHandle.TargetWindowMode = WindowMode;

		if (WindowedModeSavedState.CapturedDisplayID == kCGNullDirectDisplay)
		{
			const CGError Result = CGDisplayCapture(DisplayID);
			if (Result == kCGErrorSuccess)
			{
				WindowedModeSavedState.DesktopDisplayMode = CGDisplayCopyDisplayMode(DisplayID);
				WindowedModeSavedState.CapturedDisplayID = DisplayID;
			}
		}

		if (WindowedModeSavedState.CapturedDisplayID != kCGNullDirectDisplay)
		{
			CGDisplayModeRef DisplayMode = FPlatformApplicationMisc::GetSupportedDisplayMode(WindowedModeSavedState.CapturedDisplayID, Width, Height);
			MainThreadCall(^{
				CGDisplaySetDisplayMode(WindowedModeSavedState.CapturedDisplayID, DisplayMode, nullptr);
			}, UE4ResizeEventMode, true);

			UpdateFullScreenState(bIsFullScreen != bWantsFullScreen);

			MacApplication->DeferEvent([NSNotification notificationWithName:NSWindowDidResizeNotification object:WindowHandle]);
		}
	}

	WindowHandle->bZoomed = WindowHandle.zoomed;

	if (FadeReservationToken != 0)
	{
		MainThreadCall(^{
			CGDisplayFade(FadeReservationToken, 0.5, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0, 0, 0, false);
			CGReleaseDisplayFadeReservation(FadeReservationToken);
		}, UE4ResizeEventMode, false);
	}
}

void FMacWindow::UpdateFullScreenState(bool bToggleFullScreen)
{
	MainThreadCall(^{
		SCOPED_AUTORELEASE_POOL;
		if (bToggleFullScreen)
		{
			[WindowHandle toggleFullScreen:nil];
		}
		else
		{
			[WindowHandle setWindowMode:WindowHandle.TargetWindowMode];
		}

		if (WindowHandle.TargetWindowMode == EWindowMode::Fullscreen)
		{
			if (WindowHandle.level < CGShieldingWindowLevel())
			{
				WindowedModeSavedState.WindowLevel = WindowHandle.level;
				[WindowHandle setLevel:CGShieldingWindowLevel() + 1];
			}
			[NSApp setPresentationOptions:NSApplicationPresentationHideDock | NSApplicationPresentationHideMenuBar];
		}
		else if (WindowHandle.level != WindowedModeSavedState.WindowLevel)
		{
			[WindowHandle setLevel:WindowedModeSavedState.WindowLevel];
			[NSApp setPresentationOptions:NSApplicationPresentationDefault];
		}
	}, UE4FullscreenEventMode, true);

	// If we toggle fullscreen, ensure that the window has transitioned BEFORE leaving this function.
	// This prevents problems with failure to correctly update mouse locks and rendering contexts due to bad event ordering.
	bool bModeChanged = false;
	do
	{
		FPlatformProcess::Sleep(0.0f);
		FPlatformApplicationMisc::PumpMessages(true);
		bModeChanged = [WindowHandle windowMode] == WindowHandle.TargetWindowMode;
	} while (!bModeChanged);
}
