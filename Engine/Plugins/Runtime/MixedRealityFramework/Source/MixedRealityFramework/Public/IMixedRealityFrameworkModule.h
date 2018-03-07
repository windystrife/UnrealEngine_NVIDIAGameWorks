// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "IConsoleManager.h" // for TAutoConsoleVariable<>

class IConsoleVariable;

/**
 * 
 */
class IMixedRealityFrameworkModule : public IModuleInterface
{
public:
	/** Virtual destructor. */
	virtual ~IMixedRealityFrameworkModule() {}
};

// class FMixedRealityBootManager
// {
// 
// };
// 
// class FMRBaseCommandSuite
// {
// 	virtual void OnBootstrapCommand() = 0;
// 	virtual void OnCastingModeChanged(IConsoleVariable* CastingModeVar) = 0;
// 
// 	FAutoConsoleCommand MRBootstrapCmd;
// 	TAutoConsoleVariable<int32> MRCastingMode;
// };
// 
// class IMRCameraFeedCommands
// {
// 	TAutoConsoleVariable<int32> MRCameraSource;
// 	TAutoConsoleVariable<int32> MRChromaR;
// 	TAutoConsoleVariable<int32> MRChromaG;
// 	TAutoConsoleVariable<int32> MRChromaB;
// 	TAutoConsoleVariable<int32> MRGarbageMatte;
// };
// 
// class FMRCamCalibrator
// {
// 	virtual void OnCamFOVChange(IConsoleVariable* FOVVar) = 0;
// 	virtual void OnCamOffsetChange(IConsoleVariable* VecComponentVar) = 0;
// 	virtual void OnCamRotChange(IConsoleVariable* VecComponentVar) = 0;
// 
// 	TAutoConsoleVariable<int32> CamDevicePairing;
// 
// 	TAutoConsoleVariable<float> CamFOV;
// 
// 	TAutoConsoleVariable<float> CamOffsetX;
// 	TAutoConsoleVariable<float> CamOffsetY;
// 	TAutoConsoleVariable<float> CamOffsetZ;
// 
// 	TAutoConsoleVariable<float> CamRotYaw;
// 	TAutoConsoleVariable<float> CamRotPitch;
// 	TAutoConsoleVariable<float> CamRotRoll;
// };
