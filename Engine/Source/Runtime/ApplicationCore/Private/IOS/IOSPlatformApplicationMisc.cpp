// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IOSPlatformApplicationMisc.h"

#include "HAL/PlatformProcess.h"
#include "Modules/ModuleManager.h"

#include "IOSApplication.h"
#include "IOSAppDelegate.h"
#include "IOSView.h"
#include "IOSInputInterface.h"
#include "IOSErrorOutputDevice.h"

FIOSApplication* FIOSPlatformApplicationMisc::CachedApplication = nullptr;

EAppReturnType::Type MessageBoxExtImpl( EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption )
{
#if UE_BUILD_SHIPPING || PLATFORM_TVOS
	return FGenericPlatformMisc::MessageBoxExt(MsgType, Text, Caption);
#else
	NSString* CocoaText = (NSString*)FPlatformString::TCHARToCFString(Text);
	NSString* CocoaCaption = (NSString*)FPlatformString::TCHARToCFString(Caption);

	NSMutableArray* StringArray = [NSMutableArray arrayWithCapacity:7];

	[StringArray addObject:CocoaCaption];
	[StringArray addObject:CocoaText];

	// Figured that the order of all of these should be the same as their enum name.
	switch (MsgType)
	{
		case EAppMsgType::YesNo:
			[StringArray addObject:@"Yes"];
			[StringArray addObject:@"No"];
			break;
		case EAppMsgType::OkCancel:
			[StringArray addObject:@"Ok"];
			[StringArray addObject:@"Cancel"];
			break;
		case EAppMsgType::YesNoCancel:
			[StringArray addObject:@"Yes"];
			[StringArray addObject:@"No"];
			[StringArray addObject:@"Cancel"];
			break;
		case EAppMsgType::CancelRetryContinue:
			[StringArray addObject:@"Cancel"];
			[StringArray addObject:@"Retry"];
			[StringArray addObject:@"Continue"];
			break;
		case EAppMsgType::YesNoYesAllNoAll:
			[StringArray addObject:@"Yes"];
			[StringArray addObject:@"No"];
			[StringArray addObject:@"Yes To All"];
			[StringArray addObject:@"No To All"];
			break;
		case EAppMsgType::YesNoYesAllNoAllCancel:
			[StringArray addObject:@"Yes"];
			[StringArray addObject:@"No"];
			[StringArray addObject:@"Yes To All"];
			[StringArray addObject:@"No To All"];
			[StringArray addObject:@"Cancel"];
			break;
		case EAppMsgType::YesNoYesAll:
			[StringArray addObject : @"Yes"];
			[StringArray addObject : @"No"];
			[StringArray addObject : @"Yes To All"];
			break;
		default:
			[StringArray addObject:@"Ok"];
			break;
	}

	IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];
	
	// reset our response to unset
	AppDelegate.AlertResponse = -1;

	[AppDelegate performSelectorOnMainThread:@selector(ShowAlert:) withObject:StringArray waitUntilDone:NO];

	while (AppDelegate.AlertResponse == -1)
	{
		FPlatformProcess::Sleep(.1);
	}

	EAppReturnType::Type Result = (EAppReturnType::Type)AppDelegate.AlertResponse;

	// Need to remap the return type to the correct one, since AlertResponse actually returns a button index.
	switch (MsgType)
	{
	case EAppMsgType::YesNo:
		Result = (Result == EAppReturnType::No ? EAppReturnType::Yes : EAppReturnType::No);
		break;
	case EAppMsgType::OkCancel:
		// return 1 for Ok, 0 for Cancel
		Result = (Result == EAppReturnType::No ? EAppReturnType::Ok : EAppReturnType::Cancel);
		break;
	case EAppMsgType::YesNoCancel:
		// return 0 for Yes, 1 for No, 2 for Cancel
		if(Result == EAppReturnType::No)
		{
			Result = EAppReturnType::Yes;
		}
		else if(Result == EAppReturnType::Yes)
		{
			Result = EAppReturnType::No;
		}
		else
		{
			Result = EAppReturnType::Cancel;
		}
		break;
	case EAppMsgType::CancelRetryContinue:
		// return 0 for Cancel, 1 for Retry, 2 for Continue
		if(Result == EAppReturnType::No)
		{
			Result = EAppReturnType::Cancel;
		}
		else if(Result == EAppReturnType::Yes)
		{
			Result = EAppReturnType::Retry;
		}
		else
		{
			Result = EAppReturnType::Continue;
		}
		break;
	case EAppMsgType::YesNoYesAllNoAll:
		// return 0 for No, 1 for Yes, 2 for YesToAll, 3 for NoToAll
		break;
	case EAppMsgType::YesNoYesAllNoAllCancel:
		// return 0 for No, 1 for Yes, 2 for YesToAll, 3 for NoToAll, 4 for Cancel
		break;
	case EAppMsgType::YesNoYesAll:
		// return 0 for No, 1 for Yes, 2 for YesToAll
		break;
	default:
		Result = EAppReturnType::Ok;
		break;
	}

	CFRelease((CFStringRef)CocoaCaption);
	CFRelease((CFStringRef)CocoaText);

	return Result;
#endif
}

void FIOSPlatformApplicationMisc::LoadPreInitModules()
{
	FModuleManager::Get().LoadModule(TEXT("OpenGLDrv"));
	FModuleManager::Get().LoadModule(TEXT("IOSAudio"));
	FModuleManager::Get().LoadModule(TEXT("AudioMixerAudioUnit"));
}

class FOutputDeviceError* FIOSPlatformApplicationMisc::GetErrorOutputDevice()
{
	static FIOSErrorOutputDevice Singleton;
	return &Singleton;
}

GenericApplication* FIOSPlatformApplicationMisc::CreateApplication()
{
	CachedApplication = FIOSApplication::CreateIOSApplication();
	return CachedApplication;
}

bool FIOSPlatformApplicationMisc::ControlScreensaver(EScreenSaverAction Action)
{
	IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];
	[AppDelegate EnableIdleTimer : (Action == FGenericPlatformApplicationMisc::Enable)];
	return true;
}

void FIOSPlatformApplicationMisc::ResetGamepadAssignments()
{
	UE_LOG(LogIOS, Warning, TEXT("Restting gamepad assignments is not allowed in IOS"))
}

void FIOSPlatformApplicationMisc::ResetGamepadAssignmentToController(int32 ControllerId)
{
	
}

bool FIOSPlatformApplicationMisc::IsControllerAssignedToGamepad(int32 ControllerId)
{
	FIOSInputInterface* InputInterface = (FIOSInputInterface*)CachedApplication->GetInputInterface();
	return InputInterface->IsControllerAssignedToGamepad(ControllerId);
}

void FIOSPlatformApplicationMisc::ClipboardCopy(const TCHAR* Str)
{
#if !PLATFORM_TVOS
	CFStringRef CocoaString = FPlatformString::TCHARToCFString(Str);
	UIPasteboard* Pasteboard = [UIPasteboard generalPasteboard];
	[Pasteboard setString:(NSString*)CocoaString];
#endif
}

void FIOSPlatformApplicationMisc::ClipboardPaste(class FString& Result)
{
#if !PLATFORM_TVOS
	UIPasteboard* Pasteboard = [UIPasteboard generalPasteboard];
	NSString* CocoaString = [Pasteboard string];
	if(CocoaString)
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
#endif
}

EScreenPhysicalAccuracy FIOSPlatformApplicationMisc::ComputePhysicalScreenDensity(int32& ScreenDensity)
{
	FPlatformMisc::EIOSDevice Device = FPlatformMisc::GetIOSDeviceType();
	static_assert( FPlatformMisc::EIOSDevice::IOS_Unknown == 32, "Every device needs to be handled here." );

	ScreenDensity = 0;
	EScreenPhysicalAccuracy Accuracy = EScreenPhysicalAccuracy::Unknown;

	// look up what the device can support
	const float NativeScale =[[UIScreen mainScreen] scale];

	switch ( Device )
	{
	case FPlatformMisc::IOS_IPhoneSE:
	case FPlatformMisc::IOS_IPhone4:
	case FPlatformMisc::IOS_IPhone4S:
	case FPlatformMisc::IOS_IPhone5:
	case FPlatformMisc::IOS_IPhone5S:
	case FPlatformMisc::IOS_IPodTouch5:
	case FPlatformMisc::IOS_IPodTouch6:
	case FPlatformMisc::IOS_IPhone6:
	case FPlatformMisc::IOS_IPhone6S:
	case FPlatformMisc::IOS_IPhone7:
	case FPlatformMisc::IOS_IPhone8:
		ScreenDensity = 326;
		Accuracy = EScreenPhysicalAccuracy::Truth;
		break;
	case FPlatformMisc::IOS_IPhone6Plus:
	case FPlatformMisc::IOS_IPhone6SPlus:
	case FPlatformMisc::IOS_IPhone7Plus:
	case FPlatformMisc::IOS_IPhone8Plus:
	case FPlatformMisc::IOS_IPhoneX:
		ScreenDensity = 401;
		Accuracy = EScreenPhysicalAccuracy::Truth;
		break;
	case FPlatformMisc::IOS_IPadMini:
	case FPlatformMisc::IOS_IPadMini2: // also the iPadMini3
	case FPlatformMisc::IOS_IPadMini4:
		ScreenDensity = 401;
		Accuracy = EScreenPhysicalAccuracy::Truth;
		break;
	case FPlatformMisc::IOS_IPad2:
	case FPlatformMisc::IOS_IPad3:
	case FPlatformMisc::IOS_IPad4:
	case FPlatformMisc::IOS_IPad5:
	case FPlatformMisc::IOS_IPadAir:
	case FPlatformMisc::IOS_IPadAir2:
	case FPlatformMisc::IOS_IPadPro_97:
		ScreenDensity = 264;
		Accuracy = EScreenPhysicalAccuracy::Truth;
		break;
	case FPlatformMisc::IOS_IPadPro:
	case FPlatformMisc::IOS_IPadPro_129:
	case FPlatformMisc::IOS_IPadPro_105:
	case FPlatformMisc::IOS_IPadPro2_129:
		ScreenDensity = 264;
		Accuracy = EScreenPhysicalAccuracy::Truth;
		break;
	case FPlatformMisc::IOS_AppleTV:
	case FPlatformMisc::IOS_AppleTV4K:
		Accuracy = EScreenPhysicalAccuracy::Unknown;
		break;
	default:
		// If we don't know, assume that the density is a multiple of the 
		// native Content Scaling Factor.  Won't be exact, but should be close enough.
		ScreenDensity = 163 * NativeScale;
		Accuracy = EScreenPhysicalAccuracy::Approximation;
		break;
	}

	// look up the current scale factor
	UIView* View = [IOSAppDelegate GetDelegate].IOSView;
	const float ContentScaleFactor = View.contentScaleFactor;

	if ( ContentScaleFactor != 0 )
	{
		ScreenDensity = ScreenDensity * ( ContentScaleFactor / NativeScale );
	}

	return Accuracy;
}

