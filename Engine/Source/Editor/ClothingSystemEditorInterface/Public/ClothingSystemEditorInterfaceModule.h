// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"
#include "NameTypes.h"

class ISimulationEditorExtender;
class UClothingAssetFactoryBase;

class CLOTHINGSYSTEMEDITORINTERFACE_API FClothingSystemEditorInterfaceModule : public IModuleInterface
{

public:

	const static FName ExtenderFeatureName;

	FClothingSystemEditorInterfaceModule();

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	UClothingAssetFactoryBase* GetClothingAssetFactory();
	ISimulationEditorExtender* GetSimulationEditorExtender(FName InSimulationFactoryClassName);

private:

};
