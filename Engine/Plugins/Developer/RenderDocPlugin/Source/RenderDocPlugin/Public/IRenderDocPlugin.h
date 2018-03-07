// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IInputDeviceModule.h"

/**
 * The RenderDoc plugin works as an input plugin. Regular plugins built upon
 * IModuleInterface lack the ability of ticking, while IInputDeviceModule instantiates and
 * manages an IInputDevice object that is capable of ticking.
 *
 * By responding to tick events, the RenderDoc plugin is able to intercept the entire frame
 * activity, including Editor (Slate) UI rendering and SceneCapture updates.
 */
class IRenderDocPlugin : public IInputDeviceModule
{
public:
	static inline IRenderDocPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked<IRenderDocPlugin>("RenderDocPlugin");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("RenderDocPlugin");
	}
};
