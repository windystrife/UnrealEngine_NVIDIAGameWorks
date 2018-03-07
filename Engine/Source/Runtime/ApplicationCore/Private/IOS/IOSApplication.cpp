// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IOSApplication.h"
#include "IOSInputInterface.h"
#include "IOSWindow.h"
#include "Misc/CoreDelegates.h"
#include "IOSAppDelegate.h"
#include "IInputDeviceModule.h"
#include "IInputInterface.h"
#include "IInputDevice.h"
#include "Misc/ScopeLock.h"
#include "HAL/IConsoleManager.h"

FCriticalSection FIOSApplication::CriticalSection;
bool FIOSApplication::bOrientationChanged = false;

FIOSApplication* FIOSApplication::CreateIOSApplication()
{
	return new FIOSApplication();
}

FIOSApplication::FIOSApplication()
	: GenericApplication( NULL )
	, InputInterface( FIOSInputInterface::Create( MessageHandler ) )
	, bHasLoadedInputPlugins(false)
{
	[IOSAppDelegate GetDelegate].IOSApplication = this;
}

void FIOSApplication::InitializeWindow( const TSharedRef< FGenericWindow >& InWindow, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FGenericWindow >& InParent, const bool bShowImmediately )
{
	const TSharedRef< FIOSWindow > Window = StaticCastSharedRef< FIOSWindow >( InWindow );
	const TSharedPtr< FIOSWindow > ParentWindow = StaticCastSharedPtr< FIOSWindow >( InParent );

	Windows.Add( Window );
	Window->Initialize( this, InDefinition, ParentWindow, bShowImmediately );
}

void FIOSApplication::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	GenericApplication::SetMessageHandler(InMessageHandler);
	InputInterface->SetMessageHandler(InMessageHandler);

	TArray<IInputDeviceModule*> PluginImplementations = IModularFeatures::Get().GetModularFeatureImplementations<IInputDeviceModule>(IInputDeviceModule::GetModularFeatureName());
	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->SetMessageHandler(InMessageHandler);
	}
}

void FIOSApplication::AddExternalInputDevice(TSharedPtr<IInputDevice> InputDevice)
{
	if (InputDevice.IsValid())
	{
		ExternalInputDevices.Add(InputDevice);
	}
}

void FIOSApplication::PollGameDeviceState( const float TimeDelta )
{
	// initialize any externally-implemented input devices (we delay load initialize the array so any plugins have had time to load)
	if (!bHasLoadedInputPlugins)
	{
		TArray<IInputDeviceModule*> PluginImplementations = IModularFeatures::Get().GetModularFeatureImplementations<IInputDeviceModule>(IInputDeviceModule::GetModularFeatureName());
		for (auto InputPluginIt = PluginImplementations.CreateIterator(); InputPluginIt; ++InputPluginIt)
		{
			TSharedPtr<IInputDevice> Device = (*InputPluginIt)->CreateInputDevice(MessageHandler);
			AddExternalInputDevice(Device);
		}

		bHasLoadedInputPlugins = true;
	}

	// Poll game device state and send new events
	InputInterface->Tick(TimeDelta);
	InputInterface->SendControllerEvents();

	// Poll externally-implemented devices
	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->Tick(TimeDelta);
		(*DeviceIt)->SendControllerEvents();
	}

	FScopeLock Lock(&CriticalSection);
	if(bOrientationChanged && Windows.Num() > 0)
	{
		int32 WindowX,WindowY, WindowWidth,WindowHeight;
		Windows[0]->GetFullScreenInfo(WindowX, WindowY, WindowWidth, WindowHeight);

		GenericApplication::GetMessageHandler()->OnSizeChanged(Windows[0],WindowWidth,WindowHeight, false);
		GenericApplication::GetMessageHandler()->OnResizingWindow(Windows[0]);
		FDisplayMetrics DisplayMetrics;
		FDisplayMetrics::GetDisplayMetrics(DisplayMetrics);
		BroadcastDisplayMetricsChanged(DisplayMetrics);
		FCoreDelegates::OnSafeFrameChangedEvent.Broadcast();
		bOrientationChanged = false;
	}
}

FPlatformRect FIOSApplication::GetWorkArea( const FPlatformRect& CurrentWindow ) const
{
	return FIOSWindow::GetScreenRect();
}

void FDisplayMetrics::GetDisplayMetrics(FDisplayMetrics& OutDisplayMetrics)
{
	// Get screen rect
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect = FIOSWindow::GetScreenRect();
	OutDisplayMetrics.VirtualDisplayRect = OutDisplayMetrics.PrimaryDisplayWorkAreaRect;

	// Total screen size of the primary monitor
	OutDisplayMetrics.PrimaryDisplayWidth = OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Right - OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Left;
	OutDisplayMetrics.PrimaryDisplayHeight = OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom - OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Top;

#if !PLATFORM_TVOS
	if (@available(iOS 11, *))
	{
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MobileContentScaleFactor"));
		float RequestedContentScaleFactor = CVar->GetFloat();

		UIEdgeInsets insets = [[[[UIApplication sharedApplication] delegate] window] safeAreaInsets];
		// Hack: Temporary solution: we store the safe area offsets in TitleSafePaddingSize and ActionSafePaddingSize, because we cannot change them to FVector4 for hotfix.
		OutDisplayMetrics.TitleSafePaddingSize = FVector2D(insets.left * RequestedContentScaleFactor, insets.right * RequestedContentScaleFactor);
		OutDisplayMetrics.ActionSafePaddingSize = FVector2D(insets.top * RequestedContentScaleFactor, insets.bottom * RequestedContentScaleFactor);
	}
	else
#endif
	{
		OutDisplayMetrics.ApplyDefaultSafeZones();
	}
}

TSharedRef< FGenericWindow > FIOSApplication::MakeWindow()
{
	return FIOSWindow::Make();
}

#if !PLATFORM_TVOS
void FIOSApplication::OrientationChanged(UIDeviceOrientation orientation)
{
	FScopeLock Lock(&CriticalSection);
	bOrientationChanged = true;
}
#endif

