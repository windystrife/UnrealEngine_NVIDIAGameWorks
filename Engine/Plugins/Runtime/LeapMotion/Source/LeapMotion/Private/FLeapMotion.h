// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../Public/ILeapMotion.h"

/**
 * The public interface to this module.  In most cases, this interface is only public to sibling modules 
 * within this plugin.
 */
class FLeapMotion : public ILeapMotion
{
	void* LeapMotionDLLHandle = nullptr;
	Leap::Controller* LeapController = nullptr;
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	Leap::Controller* Controller();
};
