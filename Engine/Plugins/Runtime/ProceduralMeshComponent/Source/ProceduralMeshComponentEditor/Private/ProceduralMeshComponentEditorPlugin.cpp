// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ProceduralMeshComponent.h"
#include "IProceduralMeshComponentEditorPlugin.h"
#include "ProceduralMeshComponentDetails.h"




class FProceduralMeshComponentEditorPlugin : public IProceduralMeshComponentEditorPlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FProceduralMeshComponentEditorPlugin, ProceduralMeshComponentEditor )



void FProceduralMeshComponentEditorPlugin::StartupModule()
{
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout(UProceduralMeshComponent::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FProceduralMeshComponentDetails::MakeInstance));
	}
}


void FProceduralMeshComponentEditorPlugin::ShutdownModule()
{
	
}



