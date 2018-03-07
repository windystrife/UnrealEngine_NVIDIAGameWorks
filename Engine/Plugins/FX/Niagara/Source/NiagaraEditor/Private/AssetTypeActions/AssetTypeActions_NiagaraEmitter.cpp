// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_NiagaraEmitter.h"
#include "NiagaraEmitter.h"
#include "NiagaraSystemToolkit.h"
#include "NiagaraEditorStyle.h"

class UNiagaraEmitter;

FColor FAssetTypeActions_NiagaraEmitter::GetTypeColor() const
{
	return FNiagaraEditorStyle::Get().GetColor("NiagaraEditor.AssetColors.Emitter").ToFColor(true);
}

void FAssetTypeActions_NiagaraEmitter::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Emitter = Cast<UNiagaraEmitter>(*ObjIt);
		if (Emitter != nullptr)
		{
			TSharedRef<FNiagaraSystemToolkit> SystemToolkit(new FNiagaraSystemToolkit());
			SystemToolkit->InitializeWithEmitter(Mode, EditWithinLevelEditor, *Emitter);
		}
	}
}

UClass* FAssetTypeActions_NiagaraEmitter::GetSupportedClass() const
{
	return UNiagaraEmitter::StaticClass();
}
