// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_MaterialInstanceConstant.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "AssetTools.h"
#include "MaterialEditorModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_MaterialInstanceConstant::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	auto MICs = GetTypedWeakObjectPtrs<UMaterialInstanceConstant>(InObjects);

	FAssetTypeActions_MaterialInterface::GetActions(InObjects, MenuBuilder);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("MaterialInstanceConstant_FindParent", "Find Parent"),
		LOCTEXT("MaterialInstanceConstant_FindParentTooltip", "Finds the material this instance is based on in the content browser."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.GenericFind"),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_MaterialInstanceConstant::ExecuteFindParent, MICs ),
			FCanExecuteAction()
			)
		);
}

void FAssetTypeActions_MaterialInstanceConstant::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto MIC = Cast<UMaterialInstanceConstant>(*ObjIt);
		if (MIC != NULL)
		{
			IMaterialEditorModule* MaterialEditorModule = &FModuleManager::LoadModuleChecked<IMaterialEditorModule>( "MaterialEditor" );
			MaterialEditorModule->CreateMaterialInstanceEditor(Mode, EditWithinLevelEditor, MIC);
		}
	}
}

void FAssetTypeActions_MaterialInstanceConstant::ExecuteFindParent(TArray<TWeakObjectPtr<UMaterialInstanceConstant>> Objects)
{
	TArray<UObject*> ObjectsToSyncTo;

	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if ( Object )
		{
			ObjectsToSyncTo.AddUnique( Object->Parent );
		}
	}

	// Sync the respective browser to the valid parents
	if ( ObjectsToSyncTo.Num() > 0 )
	{
		FAssetTools::Get().SyncBrowserToAssets(ObjectsToSyncTo);
	}
}

#undef LOCTEXT_NAMESPACE
