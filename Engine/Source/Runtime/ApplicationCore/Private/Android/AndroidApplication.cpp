// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AndroidApplication.h"
#include "AndroidInputInterface.h"
#include "AndroidWindow.h"
#include "IInputDeviceModule.h"
#include "HAL/OutputDevices.h"
#include "Misc/AssertionMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogAndroidApplication, Log, All);

bool FAndroidApplication::bWindowSizeChanged = false;

FAndroidApplication* FAndroidApplication::_application = nullptr;

FAndroidApplication* FAndroidApplication::CreateAndroidApplication()
{
	return new FAndroidApplication();
}

FAndroidApplication::FAndroidApplication()
	: GenericApplication( NULL )
	, InputInterface( FAndroidInputInterface::Create( MessageHandler ) )
	, bHasLoadedInputPlugins(false)
{
	_application = this;
}

void FAndroidApplication::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	GenericApplication::SetMessageHandler(InMessageHandler);
	InputInterface->SetMessageHandler( MessageHandler );
}

void FAndroidApplication::AddExternalInputDevice(TSharedPtr<IInputDevice> InputDevice)
{
	if (InputDevice.IsValid())
	{
		InputInterface->AddExternalInputDevice(InputDevice);
	}
}

void FAndroidApplication::PollGameDeviceState( const float TimeDelta )
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
	InputInterface->Tick( TimeDelta );
	InputInterface->SendControllerEvents();
	
	if (bWindowSizeChanged && 
		Windows.Num() > 0 && 
		FAndroidWindow::GetHardwareWindow() != nullptr)
	{
		int32 WindowX,WindowY, WindowWidth,WindowHeight;
		Windows[0]->GetFullScreenInfo(WindowX, WindowY, WindowWidth, WindowHeight);

		GenericApplication::GetMessageHandler()->OnSizeChanged(Windows[0],WindowWidth,WindowHeight, false);
		GenericApplication::GetMessageHandler()->OnResizingWindow(Windows[0]);
		
		FDisplayMetrics DisplayMetrics;
		FDisplayMetrics::GetDisplayMetrics(DisplayMetrics);
		BroadcastDisplayMetricsChanged(DisplayMetrics);
		bWindowSizeChanged = false;
	}
}

FPlatformRect FAndroidApplication::GetWorkArea( const FPlatformRect& CurrentWindow ) const
{
	return FAndroidWindow::GetScreenRect();
}

IInputInterface* FAndroidApplication::GetInputInterface()
{
	// NOTE: This does not increase the reference count, so don't cache the result
	return InputInterface.Get();
}

bool FAndroidApplication::IsGamepadAttached() const
{
	FAndroidInputInterface* AndroidInputInterface = (FAndroidInputInterface*)InputInterface.Get();

	if (AndroidInputInterface)
	{
		return AndroidInputInterface->IsGamepadAttached();
	}

	return false;
}

void FDisplayMetrics::GetDisplayMetrics( FDisplayMetrics& OutDisplayMetrics )
{
	// Get screen rect
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect = FAndroidWindow::GetScreenRect();
	OutDisplayMetrics.VirtualDisplayRect = OutDisplayMetrics.PrimaryDisplayWorkAreaRect;

	// Total screen size of the primary monitor
	OutDisplayMetrics.PrimaryDisplayWidth = OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Right - OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Left;
	OutDisplayMetrics.PrimaryDisplayHeight = OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom - OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Top;

	// Apply the debug safe zones
	OutDisplayMetrics.ApplyDefaultSafeZones();
}

TSharedRef< FGenericWindow > FAndroidApplication::MakeWindow()
{
	return FAndroidWindow::Make();
}

void FAndroidApplication::InitializeWindow( const TSharedRef< FGenericWindow >& InWindow, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FGenericWindow >& InParent, const bool bShowImmediately )
{
	const TSharedRef< FAndroidWindow > Window = StaticCastSharedRef< FAndroidWindow >( InWindow );
	const TSharedPtr< FAndroidWindow > ParentWindow = StaticCastSharedPtr< FAndroidWindow >( InParent );

	Windows.Add( Window );
	Window->Initialize( this, InDefinition, ParentWindow, bShowImmediately );
}

void FAndroidApplication::OnWindowSizeChanged()
{
	bWindowSizeChanged = true;
}

