// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_NiagaraSystem.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemToolkit.h"
#include "NiagaraEditorStyle.h"

FColor FAssetTypeActions_NiagaraSystem::GetTypeColor() const
{
	return FNiagaraEditorStyle::Get().GetColor("NiagaraEditor.AssetColors.System").ToFColor(true);
}

void FAssetTypeActions_NiagaraSystem::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto System = Cast<UNiagaraSystem>(*ObjIt);
		if (System != NULL)
		{
			TSharedRef< FNiagaraSystemToolkit > NewNiagaraSystemToolkit(new FNiagaraSystemToolkit());
			NewNiagaraSystemToolkit->InitializeWithSystem(Mode, EditWithinLevelEditor, *System);
		}
	}
}

UClass* FAssetTypeActions_NiagaraSystem::GetSupportedClass() const
{
	return UNiagaraSystem::StaticClass();
}
