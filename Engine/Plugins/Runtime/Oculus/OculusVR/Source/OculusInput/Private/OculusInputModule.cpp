// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OculusInputModule.h"

#if OCULUS_INPUT_SUPPORTED_PLATFORMS
#include "OculusInput.h"
#include "OculusHMDModule.h"

#define LOCTEXT_NAMESPACE "OculusInput"


//-------------------------------------------------------------------------------------------------
// FOculusInputModule
//-------------------------------------------------------------------------------------------------

void FOculusInputModule::StartupModule()
{
	IInputDeviceModule::StartupModule();
	OculusInput::FOculusInput::PreInit();
}


TSharedPtr< class IInputDevice > FOculusInputModule::CreateInputDevice( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	if (IOculusHMDModule::IsAvailable())
	{
		if (FOculusHMDModule::Get().PreInit())
		{
			return TSharedPtr< class IInputDevice >(new OculusInput::FOculusInput(InMessageHandler));
		}
		// else, they may just not have a oculus headset plugged in (which we have to account for - no need for a warning)
	}
	else
	{
		UE_LOG(LogOcInput, Warning, TEXT("OculusInput plugin enabled, but OculusHMD plugin is not available."));
	}
	return nullptr;		
}


#endif	// OCULUS_INPUT_SUPPORTED_PLATFORMS

IMPLEMENT_MODULE( FOculusInputModule, OculusInput )

#undef LOCTEXT_NAMESPACE
