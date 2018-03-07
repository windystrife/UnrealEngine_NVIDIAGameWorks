// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class INUTUnrealEngine4 : public IModuleInterface
{
public:
	static inline INUTUnrealEngine4& Get()
	{
		return FModuleManager::LoadModuleChecked<INUTUnrealEngine4>("NUTUnrealEngine4");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("NUTUnrealEngine4");
	}
};

