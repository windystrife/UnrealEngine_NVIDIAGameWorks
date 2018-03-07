// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_StringTable.h"
#include "StringTableEditorModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_StringTable::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	FStringTableEditorModule& StringTableEditorModule = FModuleManager::LoadModuleChecked<FStringTableEditorModule>("StringTableEditor");
	for (UObject* ObjToEdit : InObjects)
	{
		if (UStringTable* StringTable = Cast<UStringTable>(ObjToEdit))
		{
			StringTableEditorModule.CreateStringTableEditor(EToolkitMode::Standalone, EditWithinLevelEditor, StringTable);
		}
	}
}

#undef LOCTEXT_NAMESPACE
