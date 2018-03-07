// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MacPlatformApplicationMisc.h"
#include "MacPlatformMisc.h"
#include "MacApplication.h"
#include "Math/Color.h"
#include "Mac/MacMallocZone.h"
#include "Misc/App.h"
#include "ModuleManager.h"
#include "MacConsoleOutputDevice.h"
#include "MacErrorOutputDevice.h"
#include "MacFeedbackContext.h"
#include "Misc/OutputDeviceHelper.h"
#include "Misc/OutputDeviceRedirector.h"
#include "HAL/ThreadHeartBeat.h"
#include "HAL/FeedbackContextAnsi.h"
#include "Mac/CocoaMenu.h"
#include "Mac/CocoaThread.h"

#include <dlfcn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/kext/KextManager.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IOEthernetController.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <mach-o/dyld.h>
#include <libproc.h>
#include <notify.h>
#include <uuid/uuid.h>

extern FMacMallocCrashHandler* GCrashMalloc;

UpdateCachedMacMenuStateProc FPlatformApplicationMisc::UpdateCachedMacMenuState = nullptr;
bool FPlatformApplicationMisc::bChachedMacMenuStateNeedsUpdate = true;
bool FPlatformApplicationMisc::bMacApplicationModalMode = false;
id<NSObject> FPlatformApplicationMisc::CommandletActivity = nil;

extern CORE_API TFunction<EAppReturnType::Type(EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption)> MessageBoxExtCallback;

EAppReturnType::Type MessageBoxExtImpl(EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption)
{
	FSlowHeartBeatScope SuspendHeartBeat;

	SCOPED_AUTORELEASE_POOL;

	EAppReturnType::Type ReturnValue = MainThreadReturn(^{
		EAppReturnType::Type RetValue = EAppReturnType::Cancel;
		NSInteger Result;

		NSAlert* AlertPanel = [NSAlert new];
		[AlertPanel setInformativeText:FString(Text).GetNSString()];
		[AlertPanel setMessageText:FString(Caption).GetNSString()];

		switch (MsgType)
		{
			case EAppMsgType::Ok:
				[AlertPanel addButtonWithTitle:@"OK"];
				[AlertPanel runModal];
				RetValue = EAppReturnType::Ok;
				break;

			case EAppMsgType::YesNo:
				[AlertPanel addButtonWithTitle:@"Yes"];
				[AlertPanel addButtonWithTitle:@"No"];
				Result = [AlertPanel runModal];
				if (Result == NSAlertFirstButtonReturn)
				{
					RetValue = EAppReturnType::Yes;
				}
				else if (Result == NSAlertSecondButtonReturn)
				{
					RetValue = EAppReturnType::No;
				}
				break;

			case EAppMsgType::OkCancel:
				[AlertPanel addButtonWithTitle:@"OK"];
				[AlertPanel addButtonWithTitle:@"Cancel"];
				Result = [AlertPanel runModal];
				if (Result == NSAlertFirstButtonReturn)
				{
					RetValue = EAppReturnType::Ok;
				}
				else if (Result == NSAlertSecondButtonReturn)
				{
					RetValue = EAppReturnType::Cancel;
				}
				break;

			case EAppMsgType::YesNoCancel:
				[AlertPanel addButtonWithTitle:@"Yes"];
				[AlertPanel addButtonWithTitle:@"No"];
				[AlertPanel addButtonWithTitle:@"Cancel"];
				Result = [AlertPanel runModal];
				if (Result == NSAlertFirstButtonReturn)
				{
					RetValue = EAppReturnType::Yes;
				}
				else if (Result == NSAlertSecondButtonReturn)
				{
					RetValue = EAppReturnType::No;
				}
				else
				{
					RetValue = EAppReturnType::Cancel;
				}
				break;

			case EAppMsgType::CancelRetryContinue:
				[AlertPanel addButtonWithTitle:@"Continue"];
				[AlertPanel addButtonWithTitle:@"Retry"];
				[AlertPanel addButtonWithTitle:@"Cancel"];
				Result = [AlertPanel runModal];
				if (Result == NSAlertFirstButtonReturn)
				{
					RetValue = EAppReturnType::Continue;
				}
				else if (Result == NSAlertSecondButtonReturn)
				{
					RetValue = EAppReturnType::Retry;
				}
				else
				{
					RetValue = EAppReturnType::Cancel;
				}
				break;

			case EAppMsgType::YesNoYesAllNoAll:
				[AlertPanel addButtonWithTitle:@"Yes"];
				[AlertPanel addButtonWithTitle:@"No"];
				[AlertPanel addButtonWithTitle:@"Yes to all"];
				[AlertPanel addButtonWithTitle:@"No to all"];
				Result = [AlertPanel runModal];
				if (Result == NSAlertFirstButtonReturn)
				{
					RetValue = EAppReturnType::Yes;
				}
				else if (Result == NSAlertSecondButtonReturn)
				{
					RetValue = EAppReturnType::No;
				}
				else if (Result == NSAlertThirdButtonReturn)
				{
					RetValue = EAppReturnType::YesAll;
				}
				else
				{
					RetValue = EAppReturnType::NoAll;
				}
				break;

			case EAppMsgType::YesNoYesAllNoAllCancel:
				[AlertPanel addButtonWithTitle:@"Yes"];
				[AlertPanel addButtonWithTitle:@"No"];
				[AlertPanel addButtonWithTitle:@"Yes to all"];
				[AlertPanel addButtonWithTitle:@"No to all"];
				[AlertPanel addButtonWithTitle:@"Cancel"];
				Result = [AlertPanel runModal];
				if (Result == NSAlertFirstButtonReturn)
				{
					RetValue = EAppReturnType::Yes;
				}
				else if (Result == NSAlertSecondButtonReturn)
				{
					RetValue = EAppReturnType::No;
				}
				else if (Result == NSAlertThirdButtonReturn)
				{
					RetValue = EAppReturnType::YesAll;
				}
				else if (Result == NSAlertThirdButtonReturn + 1)
				{
					RetValue = EAppReturnType::NoAll;
				}
				else
				{
					RetValue = EAppReturnType::Cancel;
				}
				break;

			case EAppMsgType::YesNoYesAll:
				[AlertPanel addButtonWithTitle:@"Yes"];
				[AlertPanel addButtonWithTitle:@"No"];
				[AlertPanel addButtonWithTitle:@"Yes to all"];
				Result = [AlertPanel runModal];
				if (Result == NSAlertFirstButtonReturn)
				{
					RetValue = EAppReturnType::Yes;
				}
				else if (Result == NSAlertSecondButtonReturn)
				{
					RetValue = EAppReturnType::No;
				}
				else
				{
					RetValue = EAppReturnType::YesAll;
				}
				break;

			default:
				break;
		}

		[AlertPanel release];

		return RetValue;
	});

	return ReturnValue;
}

void FMacPlatformApplicationMisc::PreInit()
{
	SCOPED_AUTORELEASE_POOL;

	// We don't support running from case-sensitive file systems on Mac yet
	NSURL* CurrentWorkingDirURL = [NSURL fileURLWithPath:[[NSFileManager defaultManager] currentDirectoryPath] isDirectory:YES];
	if (CurrentWorkingDirURL)
	{
		NSNumber* VolumeSupportsCaseSensitive;
		if ([CurrentWorkingDirURL getResourceValue:&VolumeSupportsCaseSensitive forKey:NSURLVolumeSupportsCaseSensitiveNamesKey error:nil])
		{
			if ([VolumeSupportsCaseSensitive boolValue])
			{
				MainThreadCall(^{
					NSAlert* AlertPanel = [NSAlert new];
					[AlertPanel setAlertStyle:NSAlertStyleCritical];
					[AlertPanel setInformativeText:[NSString stringWithFormat:@"Please install the application on a drive formatted as case-insensitive."]];
					[AlertPanel setMessageText:@"Unreal Engine does not support running from case-sensitive file systems."];
					[AlertPanel addButtonWithTitle:@"Quit"];
					[AlertPanel runModal];
					[AlertPanel release];
					exit(1);
				}, NSDefaultRunLoopMode, true);
			}
		}
	}

	FMacApplication::UpdateScreensArray();
	MessageBoxExtCallback = MessageBoxExtImpl;
}

void FMacPlatformApplicationMisc::PostInit()
{
	FMacPlatformMisc::PostInitMacAppInfoUpdate();

	if (MacApplication)
	{
		// Now that the engine is initialized we need to recalculate display work areas etc. that depend on DPI settings
		FMacApplication::OnDisplayReconfiguration(kCGNullDirectDisplay, kCGDisplayDesktopShapeChangedFlag, MacApplication);
	}

	// Setup the app menu in menu bar
	const bool bIsBundledApp = [[[NSBundle mainBundle] bundlePath] hasSuffix:@".app"];
	if (bIsBundledApp)
	{
        FCocoaMenu* MenuBar = [[FCocoaMenu new] autorelease];
        FCocoaMenu* AppMenu = [[FCocoaMenu new] autorelease];
        NSMenuItem* AppMenuItem = [[NSMenuItem new] autorelease];
        [AppMenuItem setTitle:@"AppMenuItem"];
        [MenuBar addItem:AppMenuItem];
        [AppMenuItem setSubmenu:AppMenu];
        [NSApp setMainMenu:MenuBar];
        
        NSString* AppName = GIsEditor ? @"Unreal Editor" : FString(FApp::GetProjectName()).GetNSString();
        NSMenu* MainMenu = [NSApp mainMenu];
        SEL ShowAboutSelector = [[NSApp delegate] respondsToSelector:@selector(showAboutWindow:)] ? @selector(showAboutWindow:) : @selector(orderFrontStandardAboutPanel:);
        NSMenuItem* AboutItem = [[[NSMenuItem alloc] initWithTitle:[NSString stringWithFormat:@"About %@", AppName] action:ShowAboutSelector keyEquivalent:@""] autorelease];
        NSMenuItem* PreferencesItem = GIsEditor ? [[[NSMenuItem alloc] initWithTitle:@"Preferences..." action:@selector(showPreferencesWindow:) keyEquivalent:@","] autorelease] : nil;
        NSMenuItem* HideItem = [[[NSMenuItem alloc] initWithTitle:[NSString stringWithFormat:@"Hide %@", AppName] action:@selector(hide:) keyEquivalent:@"h"] autorelease];
        NSMenuItem* HideOthersItem = [[[NSMenuItem alloc] initWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"] autorelease];
        [HideOthersItem setKeyEquivalentModifierMask:NSCommandKeyMask | NSAlternateKeyMask];
        NSMenuItem* ShowAllItem = [[[NSMenuItem alloc] initWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""] autorelease];
        
        SEL RequestQuitSelector = [[NSApp delegate] respondsToSelector:@selector(requestQuit:)] ? @selector(requestQuit:) : @selector(terminate:);
        NSMenuItem* QuitItem = [[[NSMenuItem alloc] initWithTitle:[NSString stringWithFormat:@"Quit %@", AppName] action:RequestQuitSelector keyEquivalent:@"q"] autorelease];
        
        NSMenuItem* ServicesItem = [[NSMenuItem new] autorelease];
        FCocoaMenu* ServicesMenu = [[FCocoaMenu new] autorelease];
        [ServicesItem setTitle:@"Services"];
        [ServicesItem setSubmenu:ServicesMenu];
        [NSApp setServicesMenu:ServicesMenu];
        [AppMenu addItem:AboutItem];
        [AppMenu addItem:[NSMenuItem separatorItem]];
        if (PreferencesItem)
        {
            [AppMenu addItem:PreferencesItem];
            [AppMenu addItem:[NSMenuItem separatorItem]];
        }
        [AppMenu addItem:ServicesItem];
        [AppMenu addItem:[NSMenuItem separatorItem]];
        [AppMenu addItem:HideItem];
        [AppMenu addItem:HideOthersItem];
        [AppMenu addItem:ShowAllItem];
        [AppMenu addItem:[NSMenuItem separatorItem]];
        [AppMenu addItem:QuitItem];

		if (FApp::IsGame())
		{
			NSMenu* ViewMenu = [[FCocoaMenu new] autorelease];
			[ViewMenu setTitle:@"View"];
			NSMenuItem* ViewMenuItem = [[NSMenuItem new] autorelease];
			[ViewMenuItem setSubmenu:ViewMenu];
			[[NSApp mainMenu] addItem:ViewMenuItem];

			NSMenuItem* ToggleFullscreenItem = [[[NSMenuItem alloc] initWithTitle:@"Enter Full Screen" action:@selector(toggleFullScreen:) keyEquivalent:@"f"] autorelease];
			[ToggleFullscreenItem setKeyEquivalentModifierMask:NSCommandKeyMask | NSControlKeyMask];
			[ViewMenu addItem:ToggleFullscreenItem];
		}
		
        NSMenu* WindowMenu = [NSApp windowsMenu];
        if (!WindowMenu)
        {
            WindowMenu = [[FCocoaMenu new] autorelease];
            [WindowMenu setTitle:@"Window"];
            NSMenuItem* WindowMenuItem = [[NSMenuItem new] autorelease];
            [WindowMenuItem setSubmenu:WindowMenu];
            [[NSApp mainMenu] addItem:WindowMenuItem];
            [NSApp setWindowsMenu:WindowMenu];
        }
        
        NSMenuItem* MinimizeItem = [[[NSMenuItem alloc] initWithTitle:@"Minimize" action:@selector(miniaturize:) keyEquivalent:@"m"] autorelease];
        NSMenuItem* ZoomItem = [[[NSMenuItem alloc] initWithTitle:@"Zoom" action:@selector(zoom:) keyEquivalent:@""] autorelease];
        NSMenuItem* CloseItem = [[[NSMenuItem alloc] initWithTitle:@"Close" action:@selector(performClose:) keyEquivalent:@"w"] autorelease];
        NSMenuItem* BringAllToFrontItem = [[[NSMenuItem alloc] initWithTitle:@"Bring All to Front" action:@selector(arrangeInFront:) keyEquivalent:@""] autorelease];
        [WindowMenu addItem:MinimizeItem];
        [WindowMenu addItem:ZoomItem];
        [WindowMenu addItem:CloseItem];
        [WindowMenu addItem:[NSMenuItem separatorItem]];
        [WindowMenu addItem:BringAllToFrontItem];
        [WindowMenu addItem:[NSMenuItem separatorItem]];
	}

	if (!MacApplication)
	{
		// No MacApplication means that app is a dedicated server, commandline tool or the editor running a commandlet. In these cases we don't want macOS to put our app into App Nap mode.
		CommandletActivity = [[NSProcessInfo processInfo] beginActivityWithOptions:NSActivityUserInitiated reason:IsRunningCommandlet() ? @"Running commandlet" : @"Running dedicated server"];
		[CommandletActivity retain];
	}
}

void FMacPlatformApplicationMisc::TearDown()
{
	if (CommandletActivity)
	{
		MainThreadCall(^{
			[[NSProcessInfo processInfo] endActivity:CommandletActivity];
			[CommandletActivity release];
		}, NSDefaultRunLoopMode, false);
		CommandletActivity = nil;
	}
}

void FMacPlatformApplicationMisc::LoadPreInitModules()
{
	FModuleManager::Get().LoadModule(TEXT("CoreAudio"));
	FModuleManager::Get().LoadModule(TEXT("AudioMixerAudioUnit"));
}

class FOutputDeviceConsole* FMacPlatformApplicationMisc::CreateConsoleOutputDevice()
{
	// this is a slightly different kind of singleton that gives ownership to the caller and should not be called more than once
	// this is a slightly different kind of singleton that gives ownership to the caller and should not be called more than once
	return new FMacConsoleOutputDevice();
}

class FOutputDeviceError* FMacPlatformApplicationMisc::GetErrorOutputDevice()
{
	static FMacErrorOutputDevice Singleton;
	return &Singleton;
}

class FFeedbackContext* FMacPlatformApplicationMisc::GetFeedbackContext()
{
#if WITH_EDITOR
	static FMacFeedbackContext Singleton;
	return &Singleton;
#else
	static FFeedbackContextAnsi Singleton;
	return &Singleton;
#endif
}

GenericApplication* FMacPlatformApplicationMisc::CreateApplication()
{
	return FMacApplication::CreateMacApplication();
}

void FMacPlatformApplicationMisc::RequestMinimize()
{
	[NSApp hide : nil];
}

bool FMacPlatformApplicationMisc::IsThisApplicationForeground()
{
	SCOPED_AUTORELEASE_POOL;
	return [NSApp isActive] && MacApplication && MacApplication->IsWorkspaceSessionActive();
}

bool FMacPlatformApplicationMisc::ControlScreensaver(EScreenSaverAction Action)
{
	static uint32 IOPMNoSleepAssertion = 0;
	static bool bDisplaySleepEnabled = true;
	
	switch(Action)
	{
		case EScreenSaverAction::Disable:
		{
			// Prevent display sleep.
			if(bDisplaySleepEnabled)
			{
				SCOPED_AUTORELEASE_POOL;
				
				//  NOTE: IOPMAssertionCreateWithName limits the string to 128 characters.
				FString ReasonForActivity = FString::Printf(TEXT("Running %s"), FApp::GetProjectName());
				
				CFStringRef ReasonForActivityCF = (CFStringRef)ReasonForActivity.GetNSString();
				
				IOReturn Success = IOPMAssertionCreateWithName(kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn, ReasonForActivityCF, &IOPMNoSleepAssertion);
				bDisplaySleepEnabled = !(Success == kIOReturnSuccess);
				ensure(!bDisplaySleepEnabled);
			}
			break;
		}
		case EScreenSaverAction::Enable:
		{
			// Stop preventing display sleep now that we are done.
			if(!bDisplaySleepEnabled)
			{
				IOReturn Success = IOPMAssertionRelease(IOPMNoSleepAssertion);
				bDisplaySleepEnabled = (Success == kIOReturnSuccess);
				ensure(bDisplaySleepEnabled);
			}
			break;
		}
	}
	
	return true;
}

FLinearColor FMacPlatformApplicationMisc::GetScreenPixelColor(const FVector2D& InScreenPos, float /*InGamma*/)
{
	SCOPED_AUTORELEASE_POOL;

	CGImageRef ScreenImage = CGWindowListCreateImage(CGRectMake(InScreenPos.X, InScreenPos.Y, 1, 1), kCGWindowListOptionOnScreenBelowWindow, kCGNullWindowID, kCGWindowImageDefault);
	
	CGDataProviderRef provider = CGImageGetDataProvider(ScreenImage);
	NSData* data = (id)CGDataProviderCopyData(provider);
	[data autorelease];
	const uint8* bytes = (const uint8*)[data bytes];
	
	// Mac colors are gamma corrected in Pow(2.2) space, so do the conversion using the 2.2 to linear conversion.
	FColor ScreenColor(bytes[2], bytes[1], bytes[0]);
	FLinearColor ScreenLinearColor = FLinearColor::FromPow22Color(ScreenColor);
	CGImageRelease(ScreenImage);

	return ScreenLinearColor;
}

float FMacPlatformApplicationMisc::GetDPIScaleFactorAtPoint(float X, float Y)
{
	if (MacApplication && MacApplication->IsHighDPIModeEnabled())
	{
		TSharedRef<FMacScreen> Screen = FMacApplication::FindScreenBySlatePosition(X, Y);
		return Screen->Screen.backingScaleFactor;
	}
	return 1.0f;
}

CGDisplayModeRef FMacPlatformApplicationMisc::GetSupportedDisplayMode(CGDirectDisplayID DisplayID, uint32 Width, uint32 Height)
{
	CGDisplayModeRef BestMatchingMode = nullptr;
	uint32 BestWidth = 0;
	uint32 BestHeight = 0;

	CFArrayRef AllModes = CGDisplayCopyAllDisplayModes(DisplayID, nullptr);
	if (AllModes)
	{
		const int32 NumModes = CFArrayGetCount(AllModes);
		for (int32 Index = 0; Index < NumModes; Index++)
		{
			CGDisplayModeRef Mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(AllModes, Index);
			const int32 ModeWidth = (int32)CGDisplayModeGetWidth(Mode);
			const int32 ModeHeight = (int32)CGDisplayModeGetHeight(Mode);

			const bool bIsEqualOrBetterWidth = FMath::Abs((int32)ModeWidth - (int32)Width) <= FMath::Abs((int32)BestWidth - (int32)Width);
			const bool bIsEqualOrBetterHeight = FMath::Abs((int32)ModeHeight - (int32)Height) <= FMath::Abs((int32)BestHeight - (int32)Height);
			if (!BestMatchingMode || (bIsEqualOrBetterWidth && bIsEqualOrBetterHeight))
			{
				BestWidth = ModeWidth;
				BestHeight = ModeHeight;
				BestMatchingMode = Mode;
			}
		}
		BestMatchingMode = CGDisplayModeRetain(BestMatchingMode);
		CFRelease(AllModes);
	}

	return BestMatchingMode;
}

void FMacPlatformApplicationMisc::PumpMessages(bool bFromMainLoop)
{
	if( bFromMainLoop )
	{
		ProcessGameThreadEvents();

		if (MacApplication && !MacApplication->IsProcessingDeferredEvents() && IsInGameThread())
		{
			if (UpdateCachedMacMenuState && bChachedMacMenuStateNeedsUpdate)
			{
				MainThreadCall(^{
					UpdateApplicationMenu();
					UpdateWindowMenu();
					UpdateCocoaButtons();
				}, NSDefaultRunLoopMode, false);

				UpdateCachedMacMenuState();
                bChachedMacMenuStateNeedsUpdate = false;
			}
		}
	}
}

void FMacPlatformApplicationMisc::ClipboardCopy(const TCHAR* Str)
{
	// Don't attempt to copy the text to the clipboard if we've crashed or we'll crash again & become unkillable.
	// The MallocZone used for crash reporting will be enabled before this call if we've crashed so that will do for testing.
	if ( GMalloc != GCrashMalloc )
	{
		SCOPED_AUTORELEASE_POOL;

		CFStringRef CocoaString = FPlatformString::TCHARToCFString(Str);
		NSPasteboard *Pasteboard = [NSPasteboard generalPasteboard];
		[Pasteboard clearContents];
		NSPasteboardItem *Item = [[[NSPasteboardItem alloc] init] autorelease];
		[Item setString: (NSString *)CocoaString forType: NSPasteboardTypeString];
		[Pasteboard writeObjects:[NSArray arrayWithObject:Item]];
		CFRelease(CocoaString);
	}
}

void FMacPlatformApplicationMisc::ClipboardPaste(class FString& Result)
{
	SCOPED_AUTORELEASE_POOL;

	NSPasteboard *Pasteboard = [NSPasteboard generalPasteboard];
	NSString *CocoaString = [Pasteboard stringForType: NSPasteboardTypeString];
	if (CocoaString)
	{
		TArray<TCHAR> Ch;
		Ch.AddUninitialized([CocoaString length] + 1);
		FPlatformString::CFStringToTCHAR((CFStringRef)CocoaString, Ch.GetData());
		Result = Ch.GetData();
	}
	else
	{
		Result = TEXT("");
	}
}

void FMacPlatformApplicationMisc::ActivateApplication()
{
	MainThreadCall(^{
		[NSApp activateIgnoringOtherApps:YES];
	}, NSDefaultRunLoopMode, false);
}

void FMacPlatformApplicationMisc::UpdateApplicationMenu()
{
    NSMenu* MainMenu = [NSApp mainMenu];
    NSMenuItem* AppMenuItem = [MainMenu itemWithTitle:@"AppMenuItem"];
    NSMenu* AppMenu = [AppMenuItem submenu];

    NSString* AppName = GIsEditor ? @"Unreal Editor" : FString(FApp::GetProjectName()).GetNSString();
    SEL ShowAboutSelector = [[NSApp delegate] respondsToSelector:@selector(showAboutWindow:)] ? @selector(showAboutWindow:) : @selector(orderFrontStandardAboutPanel:);
    NSMenuItem* AboutItem = [AppMenu itemWithTitle:[NSString stringWithFormat:@"About %@", AppName]];
    NSMenuItem* PreferencesItem = GIsEditor ? [AppMenu itemWithTitle:@"Preferences..."] : nil;
    NSMenuItem* HideItem = [AppMenu itemWithTitle:[NSString stringWithFormat:@"Hide %@", AppName]];
    NSMenuItem* HideOthersItem = [AppMenu itemWithTitle:@"Hide Others"];
    NSMenuItem* ShowAllItem = [AppMenu itemWithTitle:@"Show All"];
    NSMenuItem* QuitItem = [AppMenu itemWithTitle:[NSString stringWithFormat:@"Quit %@", AppName]];

    if(!bMacApplicationModalMode)
    {
        [AboutItem setAction:ShowAboutSelector];
        [PreferencesItem setAction:@selector(showPreferencesWindow:)];
        [HideItem setAction:@selector(hide:)];
        [HideOthersItem setAction:@selector(hideOtherApplications:)];
        [ShowAllItem setAction:@selector(unhideAllApplications:)];
        SEL RequestQuitSelector = [[NSApp delegate] respondsToSelector:@selector(requestQuit:)] ? @selector(requestQuit:) : @selector(terminate:);
        [QuitItem setAction:RequestQuitSelector];
    }
    else
    {
        for (NSMenuItem* Item in [AppMenu itemArray])
        {
            if(![Item hasSubmenu])
            {
            [Item setAction:nil];
            }
        }
    }
    
    [AppMenu update];
    [MainMenu update];
    
}

void FMacPlatformApplicationMisc::UpdateWindowMenu()
{
	NSMenu* WindowMenu = [NSApp windowsMenu];

	NSMenuItem* MinimizeItem = [WindowMenu itemWithTitle:@"Minimize"];
	NSMenuItem* ZoomItem = [WindowMenu itemWithTitle:@"Zoom"];
	NSMenuItem* CloseItem = [WindowMenu itemWithTitle:@"Close"];
	NSMenuItem* BringAllToFrontItem = [WindowMenu itemWithTitle:@"Bring All to Front"];
    
    if(!bMacApplicationModalMode)
    {
        [MinimizeItem setAction:@selector(miniaturize:)];
        [ZoomItem setAction:@selector(zoom:)];
        [CloseItem setAction:@selector(performClose:)];
        [BringAllToFrontItem setAction:@selector(arrangeInFront:)];

    }
    else
    {
        for (NSMenuItem* Item in [WindowMenu itemArray])
        {
            if(![Item hasSubmenu])
            {
                [Item setAction:nil];
            }
        }
    }
    
    [WindowMenu update];
    [[NSApp mainMenu] update];
}


void FMacPlatformApplicationMisc::UpdateCocoaButtons()
{
	MacApplication->GetWindowsArrayMutex().Lock();

    const TArray<TSharedRef<FMacWindow>>&AllWindows = MacApplication->GetAllWindows();
    for (auto Window : AllWindows)
    {
        NSWindow* WindowHandle = Window->GetWindowHandle();
        {
            NSButton* CloseButton = [WindowHandle standardWindowButton:NSWindowCloseButton];
            NSButton* MinimizeButton = [WindowHandle standardWindowButton:NSWindowMiniaturizeButton];
            NSButton* MaximizeButton = [WindowHandle standardWindowButton:NSWindowZoomButton];
            if(bMacApplicationModalMode && WindowHandle != [NSApp mainWindow])
            {
                CloseButton.enabled = false;
                MinimizeButton.enabled = false;
                MaximizeButton.enabled = false;
            }
            else if(!bMacApplicationModalMode)
            {
                CloseButton.enabled = Window->GetDefinition().HasCloseButton;
                MinimizeButton.enabled = Window->GetDefinition().SupportsMinimize;
                MaximizeButton.enabled = Window->GetDefinition().SupportsMaximize;
            }
        }
    }

	MacApplication->GetWindowsArrayMutex().Unlock();
}
