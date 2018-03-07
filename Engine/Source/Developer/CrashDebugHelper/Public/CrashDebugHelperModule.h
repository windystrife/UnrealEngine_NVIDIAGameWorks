// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class ICrashDebugHelper;

class FCrashDebugHelperModule : public IModuleInterface
{
public:
	virtual void StartupModule();
	virtual void ShutdownModule();

	/** Gets the crash debug helper singleton or returns NULL */
	CRASHDEBUGHELPER_API ICrashDebugHelper* Get();

private:
	class ICrashDebugHelper* CrashDebugHelper;
};
