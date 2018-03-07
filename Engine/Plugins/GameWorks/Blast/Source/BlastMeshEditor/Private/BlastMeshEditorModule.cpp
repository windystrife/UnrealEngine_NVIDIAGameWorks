// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlastMeshEditorModule.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "IBlastMeshEditor.h"
#include "BlastMeshEditor.h"
#include "Misc/MessageDialog.h"
#include "IPluginManager.h"
#include "PropertyEditorModule.h"

#include "IAssetTools.h"
#include "AssetToolsModule.h"

#include "BlastGlobals.h"
#include "BlastMesh.h"
#include "BlastFractureSettings.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionSpeedTree.h"

#include "ISettingsModule.h"

#include "BlastContentBrowserExtensions.h"

IMPLEMENT_MODULE( FBlastMeshEditorModule, BlastMeshEditor );
DEFINE_LOG_CATEGORY(LogBlastMeshEditor);

#define LOCTEXT_NAMESPACE "BlastMeshEditor"

const FName BlastMeshEditorAppIdentifier = FName(TEXT("BlastMeshEditorApp"));

const int32 FBlastMeshEditorModule::MaxChunkDepth = 0xFFFFFF;

void FBlastMeshEditorModule::StartupModule()
{
	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout("BlastVector", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FBlastVectorCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(UBlastFractureSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FBlastFractureSettingsComponentDetails::MakeInstance));

	FBlastContentBrowserExtensions::InstallHooks();

	// Register slate style overrides
	FBlastMeshEditorStyle::Initialize();
}


void FBlastMeshEditorModule::ShutdownModule()
{
	FBlastContentBrowserExtensions::RemoveHooks();

	MenuExtensibilityManager.Reset();
	ToolBarExtensibilityManager.Reset();

	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout(UBlastFractureSettings::StaticClass()->GetFName());
	}

	// Unregister slate style overrides
	FBlastMeshEditorStyle::Shutdown();
}


TSharedRef<IBlastMeshEditor> FBlastMeshEditorModule::CreateBlastMeshEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UBlastMesh* Table )
{
	TSharedRef< FBlastMeshEditor > NewBlastMeshEditor( new FBlastMeshEditor() );
	NewBlastMeshEditor->InitBlastMeshEditor( Mode, InitToolkitHost, Table );
	return NewBlastMeshEditor;
}

#undef LOCTEXT_NAMESPACE
