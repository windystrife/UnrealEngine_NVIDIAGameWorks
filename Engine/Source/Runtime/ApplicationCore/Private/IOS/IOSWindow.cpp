// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IOSWindow.h"
#include "IOSAppDelegate.h"
#include "IOSView.h"

FIOSWindow::~FIOSWindow()
{
	// NOTE: The Window is invalid here!
	//       Use NativeWindow_Destroy() instead.
}

TSharedRef<FIOSWindow> FIOSWindow::Make()
{
	return MakeShareable( new FIOSWindow() );
}

FIOSWindow::FIOSWindow()
{
}

void FIOSWindow::Initialize( class FIOSApplication* const Application, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FIOSWindow >& InParent, const bool bShowImmediately )
{
	OwningApplication = Application;
	Definition = InDefinition;

	Window = [[UIApplication sharedApplication] keyWindow];

#if !PLATFORM_TVOS
	if(InParent.Get() != NULL)
	{
		dispatch_async(dispatch_get_main_queue(),^ {
#ifdef __IPHONE_8_0
			if ([UIAlertController class])
			{
				UIAlertController* AlertController = [UIAlertController alertControllerWithTitle:@""
														message:@"Error: Only one UIWindow may be created on iOS."
														preferredStyle:UIAlertControllerStyleAlert];
				UIAlertAction* okAction = [UIAlertAction
											actionWithTitle:NSLocalizedString(@"OK", nil)
											style:UIAlertActionStyleDefault
											handler:^(UIAlertAction* action)
											{
												[AlertController dismissViewControllerAnimated : YES completion : nil];
											}
				];

				[AlertController addAction : okAction];
				[[IOSAppDelegate GetDelegate].IOSController presentViewController : AlertController animated : YES completion : nil];
			}
			else
#endif
			{
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_9_0
				UIAlertView* AlertView = [[UIAlertView alloc] initWithTitle:@""
											message:@"Error: Only one UIWindow may be created on iOS."
											delegate:nil
											cancelButtonTitle:NSLocalizedString(@"Ok", nil)
											otherButtonTitles:nil];

				[AlertView show];
				[AlertView release];
#endif
			}
		} );
	}
#endif
}

FPlatformRect FIOSWindow::GetScreenRect()
{
	// get the main view's frame
	IOSAppDelegate* AppDelegate = (IOSAppDelegate*)[[UIApplication sharedApplication] delegate];
	UIView* View = AppDelegate.IOSView;
	CGRect Frame = [View frame];
	CGFloat Scale = View.contentScaleFactor;

	FPlatformRect ScreenRect;
	ScreenRect.Top = Frame.origin.y * Scale;
	ScreenRect.Bottom = (Frame.origin.y + Frame.size.height) * Scale;
	ScreenRect.Left = Frame.origin.x * Scale;
	ScreenRect.Right = (Frame.origin.x + Frame.size.width) * Scale;

	return ScreenRect;
}


bool FIOSWindow::GetFullScreenInfo( int32& X, int32& Y, int32& Width, int32& Height ) const
{
	FPlatformRect ScreenRect = GetScreenRect();

	X = ScreenRect.Left;
	Y = ScreenRect.Top;
	Width = ScreenRect.Right - ScreenRect.Left;
	Height = ScreenRect.Bottom - ScreenRect.Top;

	return true;
}
