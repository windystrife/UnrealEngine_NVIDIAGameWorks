// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MeshPaintModeModule.h"
#include "Modules/ModuleManager.h"
#include "Textures/SlateIcon.h"
#include "EditorStyleSet.h"
#include "EditorModeRegistry.h"
#include "EditorModes.h"
#include "MeshPaintEdMode.h"

#include "PropertyEditorModule.h"
#include "PaintModeSettingsCustomization.h"

#include "PaintModeSettings.h"

#include "ModuleManager.h"

IMPLEMENT_MODULE(FMeshPaintModeModule, MeshPaintMode );

void FMeshPaintModeModule::StartupModule()
{
	FEditorModeRegistry::Get().RegisterMode<FEdModeMeshPaint>(
		FBuiltinEditorModes::EM_MeshPaint,
		NSLOCTEXT("MeshPaint_Mode", "MeshPaint_ModeName", "Paint"),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.MeshPaintMode", "LevelEditor.MeshPaintMode.Small"),
		true, 200 );

	/** Register detail/property customization */
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout("PaintModeSettings", FOnGetDetailCustomizationInstance::CreateStatic(&FPaintModeSettingsCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("VertexPaintSettings", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FVertexPaintSettingsCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("TexturePaintSettings", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FTexturePaintSettingsCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("TexturePaintSettings", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FTexturePaintSettingsCustomization::MakeInstance));

	FModuleManager::Get().LoadModule("MeshPaint");
}

void FMeshPaintModeModule::ShutdownModule()
{
	FEditorModeRegistry::Get().UnregisterMode(FBuiltinEditorModes::EM_MeshPaint);

	/** De-register detail/property customization */
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.UnregisterCustomClassLayout("PaintModeSettings");
	PropertyModule.UnregisterCustomPropertyTypeLayout("VertexPaintSettings");
	PropertyModule.UnregisterCustomPropertyTypeLayout("TexturePaintSettings");
	PropertyModule.UnregisterCustomPropertyTypeLayout("TexturePaintSettings");
}
