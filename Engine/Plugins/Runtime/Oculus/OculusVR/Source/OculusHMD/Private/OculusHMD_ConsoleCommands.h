// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OculusHMDPrivate.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "HAL/IConsoleManager.h"

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FConsoleCommands
//-------------------------------------------------------------------------------------------------

class FConsoleCommands : private FSelfRegisteringExec
{
public:
	FConsoleCommands(class FOculusHMD* InHMDPtr);

	// FSelfRegisteringExec interface
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
private:
	FAutoConsoleCommand UpdateOnRenderThreadCommand;
	FAutoConsoleCommand PixelDensityCommand;
	FAutoConsoleCommand PixelDensityMinCommand;
	FAutoConsoleCommand PixelDensityMaxCommand;
	FAutoConsoleCommand PixelDensityAdaptiveCommand;
	FAutoConsoleCommand HQBufferCommand;
	FAutoConsoleCommand HQDistortionCommand;
	FAutoConsoleCommand ShowGlobalMenuCommand;
	FAutoConsoleCommand ShowQuitMenuCommand;

#if !UE_BUILD_SHIPPING
	// Debug console commands
	FAutoConsoleCommand EnforceHeadTrackingCommand;
	FAutoConsoleCommand StatsCommand;
	FAutoConsoleCommand CubemapCommand;
	FAutoConsoleCommand ShowSettingsCommand;
	FAutoConsoleCommand ResetSettingsCommand;
	FAutoConsoleCommand IPDCommand;
	FAutoConsoleCommand FCPCommand;
	FAutoConsoleCommand NCPCommand;
#endif // !UE_BUILD_SHIPPING
};


} // namespace OculusHMD

#endif //OCULUS_HMD_SUPPORTED_PLATFORMS
