// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MeshPaintModule.h"
#include "Modules/ModuleManager.h"
#include "Textures/SlateIcon.h"
#include "EditorStyleSet.h"
#include "EditorModeRegistry.h"
#include "EditorModes.h"
#include "MeshPaintAdapterFactory.h"
#include "MeshPaintStaticMeshAdapter.h"
#include "MeshPaintSplineMeshAdapter.h"
#include "MeshPaintSkeletalMeshAdapter.h"

#include "ImportVertexColorOptionsCustomization.h"

#include "ModuleManager.h"
#include "PropertyEditorModule.h"
#include "MeshPaintSettings.h"


//////////////////////////////////////////////////////////////////////////
// FMeshPaintModule

class FMeshPaintModule : public IMeshPaintModule
{
public:
	
	/**
	 * Called right after the module's DLL has been loaded and the module object has been created
	 */
	virtual void StartupModule() override
	{
		RegisterGeometryAdapterFactory(MakeShareable(new FMeshPaintGeometryAdapterForSplineMeshesFactory));
		RegisterGeometryAdapterFactory(MakeShareable(new FMeshPaintGeometryAdapterForStaticMeshesFactory));
		RegisterGeometryAdapterFactory(MakeShareable(new FMeshPaintGeometryAdapterForSkeletalMeshesFactory));

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout("VertexColorImportOptions", FOnGetDetailCustomizationInstance::CreateStatic(&FVertexColorImportOptionsCustomization::MakeInstance));
	}

	virtual void ShutdownModule() override
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout("VertexColorImportOptions");
	}

	virtual void RegisterGeometryAdapterFactory(TSharedRef<IMeshPaintGeometryAdapterFactory> Factory) override
	{
		FMeshPaintAdapterFactory::FactoryList.Add(Factory);
	}

	virtual void UnregisterGeometryAdapterFactory(TSharedRef<IMeshPaintGeometryAdapterFactory> Factory) override
	{
		FMeshPaintAdapterFactory::FactoryList.Remove(Factory);
	}
};

IMPLEMENT_MODULE( FMeshPaintModule, MeshPaint );
