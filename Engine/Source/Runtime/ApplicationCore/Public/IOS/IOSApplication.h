// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericApplication.h"

class FIOSWindow;

class FIOSApplication : public GenericApplication
{
public:

	static FIOSApplication* CreateIOSApplication();


public:

	virtual ~FIOSApplication() {}

	void SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler );

	virtual void PollGameDeviceState( const float TimeDelta ) override;

	virtual FPlatformRect GetWorkArea( const FPlatformRect& CurrentWindow ) const override;

	virtual TSharedRef< FGenericWindow > MakeWindow() override;

	virtual void AddExternalInputDevice(TSharedPtr<class IInputDevice> InputDevice);

#if !PLATFORM_TVOS
	static void OrientationChanged(UIDeviceOrientation orientation);
#endif

	virtual IInputInterface* GetInputInterface() override { return (IInputInterface*)InputInterface.Get(); }

protected:
	virtual void InitializeWindow( const TSharedRef< FGenericWindow >& Window, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FGenericWindow >& InParent, const bool bShowImmediately ) override;

private:

	FIOSApplication();


private:

	TSharedPtr< class FIOSInputInterface > InputInterface;

	/** List of input devices implemented in external modules. */
	TArray< TSharedPtr<class IInputDevice> > ExternalInputDevices;
	bool bHasLoadedInputPlugins;

	TArray< TSharedRef< FIOSWindow > > Windows;

	static FCriticalSection CriticalSection;
	static bool bOrientationChanged;
};
