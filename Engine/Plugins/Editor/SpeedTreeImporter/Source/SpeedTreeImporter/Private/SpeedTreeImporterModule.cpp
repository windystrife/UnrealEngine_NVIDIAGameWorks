// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "ISpeedTreeImporter.h"

#include "PropertyEditorModule.h"
#include "SpeedTreeImportData.h"

/**
 * SpeedTreeImporter module implementation (private)
 */
class FSpeedTreeImporterModule : public ISpeedTreeImporter
{
public:
	virtual void StartupModule() override
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout(USpeedTreeImportData::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FSpeedTreeImportDataDetails::MakeInstance));
	}


	virtual void ShutdownModule() override
	{
	}

};

IMPLEMENT_MODULE(FSpeedTreeImporterModule, SpeedTreeImporter);
