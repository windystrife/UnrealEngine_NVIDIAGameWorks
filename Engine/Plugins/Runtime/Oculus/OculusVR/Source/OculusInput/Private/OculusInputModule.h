// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IOculusInputModule.h"
#include "IInputDevice.h"

#define LOCTEXT_NAMESPACE "OculusInput"


//-------------------------------------------------------------------------------------------------
// FOculusInputModule
//-------------------------------------------------------------------------------------------------

#if OCULUS_INPUT_SUPPORTED_PLATFORMS

class FOculusInputModule : public IOculusInputModule
{
	// IInputDeviceModule overrides
	virtual void StartupModule() override;
	virtual TSharedPtr< class IInputDevice > CreateInputDevice( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler ) override;
};

#else	//	OCULUS_INPUT_SUPPORTED_PLATFORMS

class FOculusInputModule : public FDefaultModuleImpl
{
};

#endif	// OCULUS_INPUT_SUPPORTED_PLATFORMS


#undef LOCTEXT_NAMESPACE
