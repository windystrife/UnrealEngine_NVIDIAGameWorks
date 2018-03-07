// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

//////////////////////////////////////////////////////////////////////////
// FAnimGraphRuntimeModule

class FAnimGraphRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};

//////////////////////////////////////////////////////////////////////////

IMPLEMENT_MODULE(FAnimGraphRuntimeModule, AnimGraphRuntime);
