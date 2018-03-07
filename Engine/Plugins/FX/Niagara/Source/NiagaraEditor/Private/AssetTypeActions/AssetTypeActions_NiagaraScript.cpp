// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_NiagaraScript.h"
#include "NiagaraScript.h"
#include "NiagaraScriptToolkit.h"
#include "NiagaraEditorStyle.h"

FColor FAssetTypeActions_NiagaraScript::GetTypeColor() const
{ 
	return FNiagaraEditorStyle::Get().GetColor("NiagaraEditor.AssetColors.Script").ToFColor(true); 
}

void FAssetTypeActions_NiagaraScript::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Script = Cast<UNiagaraScript>(*ObjIt);
		if (Script != NULL)
		{
			TSharedRef< FNiagaraScriptToolkit > NewNiagaraScriptToolkit(new FNiagaraScriptToolkit());
			NewNiagaraScriptToolkit->Initialize(Mode, EditWithinLevelEditor, Script);
		}
	}
}

UClass* FAssetTypeActions_NiagaraScript::GetSupportedClass() const
{ 
	return UNiagaraScript::StaticClass(); 
}
