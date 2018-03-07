// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "IProfilerServiceModule.h"
#include "ProfilerServiceManager.h"


/**
 * Implements the ProfilerService module
 */
class FProfilerServiceModule
	: public IProfilerServiceModule
{
public:

	//~ IModuleInterface interface

	virtual void ShutdownModule() override
	{
		if (ProfilerServiceManager.IsValid())
		{
			((FProfilerServiceManager*)ProfilerServiceManager.Get())->Shutdown();
		}
		ProfilerServiceManager.Reset();
	}

public:

	//~ IProfilerServiceModule interface

	virtual TSharedPtr<IProfilerServiceManager> CreateProfilerServiceManager() override
	{
		if (!ProfilerServiceManager.IsValid())
		{
			ProfilerServiceManager = FProfilerServiceManager::CreateSharedServiceManager();
			((FProfilerServiceManager*)ProfilerServiceManager.Get())->Init();
		}

		return ProfilerServiceManager;
	}

private:

	/** The profiler service singleton. */
	static TSharedPtr<IProfilerServiceManager> ProfilerServiceManager;
};


TSharedPtr<IProfilerServiceManager> FProfilerServiceModule::ProfilerServiceManager = nullptr;


IMPLEMENT_MODULE(FProfilerServiceModule, ProfilerService);
