// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class INetcodeUnitTest : public IModuleInterface
{
public:
	static inline INetcodeUnitTest& Get()
	{
		return FModuleManager::LoadModuleChecked<INetcodeUnitTest>("NetcodeUnitTest");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("NetcodeUnitTest");
	}
};

