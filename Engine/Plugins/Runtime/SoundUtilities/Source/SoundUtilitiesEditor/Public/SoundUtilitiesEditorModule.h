// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "SoundWaveAssetActionExtender.h"

class ISoundWaveAssetActionExtensions;

class FSoundUtilitiesEditorModule : public IModuleInterface
{
public:

	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	TSharedPtr<ISoundWaveAssetActionExtensions> SoundWaveAssetActionExtender;
};
