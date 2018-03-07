// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ClothingSystemEditorModule.h"
#include "ClothingSystemEditorInterfaceModule.h"

#include "ClothingSimulationFactory.h"
#include "SimulationEditorExtenderNv.h"
#include "ClothingAssetFactory.h"
#include "ModuleManager.h"
#include "Features/IModularFeatures.h"

IMPLEMENT_MODULE(FClothingSystemEditorModule, ClothingSystemEditor);

FClothingSystemEditorModule::FClothingSystemEditorModule()
{

}

void FClothingSystemEditorModule::StartupModule()
{
	IModularFeatures::Get().RegisterModularFeature(IClothingAssetFactoryProvider::FeatureName, this);

#if WITH_NVCLOTH
	IModularFeatures::Get().RegisterModularFeature(FClothingSystemEditorInterfaceModule::ExtenderFeatureName, &NvEditorExtender);
#endif
}

void FClothingSystemEditorModule::ShutdownModule()
{
	IModularFeatures::Get().UnregisterModularFeature(IClothingAssetFactoryProvider::FeatureName, this);

#if WITH_NVCLOTH
	IModularFeatures::Get().UnregisterModularFeature(FClothingSystemEditorInterfaceModule::ExtenderFeatureName, &NvEditorExtender);
#endif
}

UClothingAssetFactoryBase* FClothingSystemEditorModule::GetFactory()
{
	return UClothingAssetFactory::StaticClass()->GetDefaultObject<UClothingAssetFactoryBase>();
}
