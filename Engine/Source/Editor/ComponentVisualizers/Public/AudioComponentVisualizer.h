// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AttenuatedComponentVisualizer.h"

class COMPONENTVISUALIZERS_API FAudioComponentVisualizer : public TAttenuatedComponentVisualizer<UAudioComponent>
{
private:
	virtual bool IsVisualizerEnabled(const FEngineShowFlags& ShowFlags) const override
	{
		return ShowFlags.AudioRadius;
	}
};
