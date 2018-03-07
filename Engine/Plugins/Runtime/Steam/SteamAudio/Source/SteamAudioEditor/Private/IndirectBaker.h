//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "Delegates/Delegate.h"
#include <atomic>

class UPhononSourceComponent;

namespace SteamAudio
{
	// True if a baking process is currently running.
	extern std::atomic<bool> GIsBaking;

	DECLARE_DELEGATE_OneParam(FBakedSourceUpdated, FName);
	
	void Bake(const TArray<UPhononSourceComponent*> PhononSourceComponents, const bool BakeReverb, SteamAudio::FBakedSourceUpdated BakedSourceUpdated);
}