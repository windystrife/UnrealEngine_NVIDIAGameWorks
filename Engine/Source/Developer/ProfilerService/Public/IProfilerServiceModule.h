// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "IProfilerServiceManager.h"

/**
 * Interface for ProfilerService modules.
 */
class IProfilerServiceModule
	: public IModuleInterface
{
public:

	/** 
	 * Creates a profiler service.
	 *
	 * @param Capture Specifies whether to start capturing immediately.
	 * @return A new profiler service.
	 */
	virtual TSharedPtr<IProfilerServiceManager> CreateProfilerServiceManager() = 0;

protected:

	/** Virtual destructor */
	virtual ~IProfilerServiceModule() { }
};
