// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StringTableEditorModule.h"
#include "Modules/ModuleManager.h"
#include "IStringTableEditor.h"
#include "StringTableEditor.h"
#include "AssetTypeActions_StringTable.h"

IMPLEMENT_MODULE(FStringTableEditorModule, StringTableEditor);

const FName FStringTableEditorModule::StringTableEditorAppIdentifier("StringTableEditorApp");

void FStringTableEditorModule::StartupModule()
{
	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_StringTable()));
}

void FStringTableEditorModule::ShutdownModule()
{
	MenuExtensibilityManager.Reset();
}

TSharedRef<IStringTableEditor> FStringTableEditorModule::CreateStringTableEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UStringTable* StringTable)
{
	TSharedRef<FStringTableEditor> NewStringTableEditor(new FStringTableEditor());
	NewStringTableEditor->InitStringTableEditor(Mode, InitToolkitHost, StringTable);
	return NewStringTableEditor;
}
