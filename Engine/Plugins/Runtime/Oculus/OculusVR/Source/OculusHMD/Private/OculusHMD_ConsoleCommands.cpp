// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "OculusHMD_ConsoleCommands.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "OculusHMD.h"
#include "OculusSceneCaptureCubemap.h"

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FConsoleCommands
//-------------------------------------------------------------------------------------------------

/// @cond DOXYGEN_WARNINGS

FConsoleCommands::FConsoleCommands(class FOculusHMD* InHMDPtr)
	: UpdateOnRenderThreadCommand(TEXT("vr.oculus.bUpdateOnRenderThread"),
		*NSLOCTEXT("OculusRift", "CCommandText_UpdateRT",
			"Oculus Rift specific extension.\nEnables or disables updating on the render thread.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(InHMDPtr, &FOculusHMD::UpdateOnRenderThreadCommandHandler))
	, PixelDensityCommand(TEXT("vr.oculus.PixelDensity"),
		*NSLOCTEXT("OculusRift", "CCommandText_PixelDensity",
			"Oculus Rift specific extension.\nPixel density sets the render target texture size as a factor of recommended texture size.\nSince this may be slighly larger than the native resolution, setting PixelDensity to 1.0 is\nusually not the same as setting r.ScreenPercentage to 100").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(InHMDPtr, &FOculusHMD::PixelDensityCommandHandler))
	, PixelDensityMinCommand(TEXT("vr.oculus.PixelDensity.min"),
		*NSLOCTEXT("OculusRift", "CCommandText_PixelDensityMin",
			"Oculus Rift specific extension.\nMinimum pixel density when adaptive pixel density is enabled").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(InHMDPtr, &FOculusHMD::PixelDensityMinCommandHandler))
	, PixelDensityMaxCommand(TEXT("vr.oculus.PixelDensity.max"),
		*NSLOCTEXT("OculusRift", "CCommandText_PixelDensityMax",
			"Oculus Rift specific extension.\nMaximum pixel density when adaptive pixel density is enabled").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(InHMDPtr, &FOculusHMD::PixelDensityMaxCommandHandler))
	, PixelDensityAdaptiveCommand(TEXT("vr.oculus.PixelDensity.adaptive"),
		*NSLOCTEXT("OculusRift", "CCommandText_PixelDensityAdaptive",
			"Oculus Rift specific extension.\nEnable or disable adaptive pixel density.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(InHMDPtr, &FOculusHMD::PixelDensityAdaptiveCommandHandler))
	, HQBufferCommand(TEXT("vr.oculus.bHQBuffer"),
		*NSLOCTEXT("OculusRift", "CCommandText_HQBuffer",
			"Oculus Rift specific extension.\nEnable or disable using floating point texture format for the eye layer.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(InHMDPtr, &FOculusHMD::HQBufferCommandHandler))
	, HQDistortionCommand(TEXT("vr.oculus.bHQDistortion"),
		*NSLOCTEXT("OculusRift", "CCommandText_HQDistortion",
			"Oculus Rift specific extension.\nEnable or disable using multiple mipmap levels for the eye layer.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(InHMDPtr, &FOculusHMD::HQDistortionCommandHandler))
	, ShowGlobalMenuCommand(TEXT("vr.oculus.ShowGlobalMenu"),
		*NSLOCTEXT("OculusRift", "CCommandText_GlobalMenu",
			"Oculus Rift specific extension.\nOpens the global menu.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(InHMDPtr, &FOculusHMD::ShowGlobalMenuCommandHandler))
	, ShowQuitMenuCommand(TEXT("vr.oculus.ShowQuitMenu"),
		*NSLOCTEXT("OculusRift", "CCommandText_QuitMenu",
			"Oculus Rift specific extension.\nOpens the quit menu.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(InHMDPtr, &FOculusHMD::ShowQuitMenuCommandHandler))

#if !UE_BUILD_SHIPPING
	, EnforceHeadTrackingCommand(TEXT("vr.oculus.Debug.EnforceHeadTracking"),
		*NSLOCTEXT("OculusRift", "CCommandText_EnforceTracking",
			"Oculus Rift specific extension.\nSet to on to enforce head tracking even when not in stereo mode.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(InHMDPtr, &FOculusHMD::EnforceHeadTrackingCommandHandler))
	, StatsCommand(TEXT("vr.oculus.Debug.bShowStats"),
		*NSLOCTEXT("OculusRift", "CCommandText_Stats",
			"Oculus Rift specific extension.\nEnable or disable rendering of stats.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(InHMDPtr, &FOculusHMD::StatsCommandHandler))
	, CubemapCommand(TEXT("vr.oculus.Debug.CaptureCubemap"),
		*NSLOCTEXT("OculusRift", "CCommandText_Cubemap",
			"Oculus Rift specific extension.\nCaptures a cubemap for Oculus Home.\nOptional arguments (default is zero for all numeric arguments):\n  xoff=<float> -- X axis offset from the origin\n  yoff=<float> -- Y axis offset\n  zoff=<float> -- Z axis offset\n  yaw=<float>  -- the direction to look into (roll and pitch is fixed to zero)\n  gearvr       -- Generate a GearVR format cubemap\n    (height of the captured cubemap will be 1024 instead of 2048 pixels)\n").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&UOculusSceneCaptureCubemap::CaptureCubemapCommandHandler))
	, ShowSettingsCommand(TEXT("vr.oculus.Debug.Show"),
		*NSLOCTEXT("OculusRift", "CCommandText_Show",
			"Oculus Rift specific extension.\nShows the current value of various stereo rendering params.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(InHMDPtr, &FOculusHMD::ShowSettingsCommandHandler))
	, ResetSettingsCommand(TEXT("vr.oculus.Debug.Reset"),
		*NSLOCTEXT("OculusRift", "CCommandText_Reset",
			"Oculus Rift specific extension.\nResets various stereo rendering params back to the original setting.").ToString(),
		FConsoleCommandDelegate::CreateRaw(InHMDPtr, &FOculusHMD::ResetStereoRenderingParams))
	, IPDCommand(TEXT("vr.oculus.Debug.IPD"),
		*NSLOCTEXT("OculusRift", "CCommandText_IPD",
			"Oculus Rift specific extension.\nShows or changes the current interpupillary distance in meters.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(InHMDPtr, &FOculusHMD::IPDCommandHandler))
	, FCPCommand(TEXT("vr.oculus.Debug.FCP"),
		*NSLOCTEXT("OculusRift", "CCommandText_FCP",
			"Oculus Rift specific extension.\nShows or overrides the current far clipping plane.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(InHMDPtr, &FOculusHMD::FCPCommandHandler))
	, NCPCommand(TEXT("vr.oculus.Debug.NCP"),
		*NSLOCTEXT("OculusRift", "CCommandText_NCP",
			"Oculus Rift specific extension.\nShows or overrides the current near clipping plane.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(InHMDPtr, &FOculusHMD::NCPCommandHandler))
#endif // !UE_BUILD_SHIPPING
{
}

bool FConsoleCommands::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	const TCHAR* OrigCmd = Cmd;
	FString AliasedCommand;
	
	if (FParse::Command(&Cmd, TEXT("OVRGLOBALMENU")))
	{
		AliasedCommand = TEXT("vr.oculus.ShowGlobalMenu");
	}
	else if (FParse::Command(&Cmd, TEXT("OVRQUITMENU")))
	{
		AliasedCommand = TEXT("vr.oculus.ShowQuitMenu");
	}

	if (!AliasedCommand.IsEmpty())
	{
		Ar.Logf(ELogVerbosity::Warning, TEXT("%s is deprecated. Use %s instead"), OrigCmd, *AliasedCommand);
		return IConsoleManager::Get().ProcessUserConsoleInput(*AliasedCommand, Ar, InWorld);
	}
	return false;
}

/// @endcond

} // namespace OculusHmd

#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
