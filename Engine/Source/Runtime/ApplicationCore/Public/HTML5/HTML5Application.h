// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericApplication.h"
#include "HTML5Window.h"

/**
 * HTML5-specific application implementation.
 */
class FHTML5Application : public GenericApplication
{
public:

	static FHTML5Application* CreateHTML5Application();


public:	

	virtual ~FHTML5Application() {}

	void SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler );

	virtual void PollGameDeviceState( const float TimeDelta ) override;

	virtual FPlatformRect GetWorkArea( const FPlatformRect& CurrentWindow ) const override;

	TSharedRef< FGenericWindow > MakeWindow();
private:

	FHTML5Application();


private:

	TSharedPtr< class FHTML5InputInterface > InputInterface;
	TSharedRef< class FGenericWindow > ApplicationWindow;

	int32 WarmUpTicks; 

	int32 WindowWidth;
	int32 WindowHeight;
};