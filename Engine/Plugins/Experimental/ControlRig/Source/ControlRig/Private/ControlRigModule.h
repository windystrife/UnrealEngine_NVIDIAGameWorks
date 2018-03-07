// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/IDelegateInstance.h"
#include "ModuleInterface.h"
#include "Materials/Material.h"

class FControlRigModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

public:
#if WITH_EDITOR
	UMaterial* ManipulatorMaterial;
#endif

private:
	FDelegateHandle OnCreateMovieSceneObjectSpawnerHandle;
};