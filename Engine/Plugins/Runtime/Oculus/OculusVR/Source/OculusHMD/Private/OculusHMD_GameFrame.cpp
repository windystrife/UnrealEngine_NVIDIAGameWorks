// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OculusHMD_GameFrame.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "GameFramework/WorldSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FGameFrame
//-------------------------------------------------------------------------------------------------

FGameFrame::FGameFrame() :
	FrameNumber(0),
	WorldToMetersScale(100.f),
	ShowFlags(ESFIM_All0),
	PlayerOrientation(FQuat::Identity),
	PlayerLocation(FVector::ZeroVector)
{
	Flags.Raw = 0;
}

TSharedPtr<FGameFrame, ESPMode::ThreadSafe> FGameFrame::Clone() const
{
	TSharedPtr<FGameFrame, ESPMode::ThreadSafe> NewFrame = MakeShareable(new FGameFrame(*this));
	return NewFrame;
}


} // namespace OculusHMD

#endif //OCULUS_HMD_SUPPORTED_PLATFORMS
