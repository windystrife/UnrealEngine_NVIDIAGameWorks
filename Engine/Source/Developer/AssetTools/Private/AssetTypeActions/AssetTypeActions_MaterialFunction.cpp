// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_MaterialFunction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "EditorStyleSet.h"
#include "Materials/Material.h"
#include "ThumbnailRendering/SceneThumbnailInfoWithPrimitive.h"
#include "AssetTools.h"
#include "MaterialEditorModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_MaterialFunction::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	auto Materials = GetTypedWeakObjectPtrs<UMaterialFunction>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("MaterialFunction_FindMaterials", "Find Materials Using This"),
		LOCTEXT("MaterialFunction_FindMaterialsTooltip", "Finds the materials that reference this material function in the content browser."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.GenericFind"),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_MaterialFunction::ExecuteFindMaterials, Materials ),
			FCanExecuteAction()
			)
		);
}

void FAssetTypeActions_MaterialFunction::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Function = Cast<UMaterialFunction>(*ObjIt);
		if (Function != NULL)
		{
			IMaterialEditorModule* MaterialEditorModule = &FModuleManager::LoadModuleChecked<IMaterialEditorModule>( "MaterialEditor" );
			MaterialEditorModule->CreateMaterialEditor(Mode, EditWithinLevelEditor, Function);
		}
	}
}

void FAssetTypeActions_MaterialFunction::ExecuteFindMaterials(TArray<TWeakObjectPtr<UMaterialFunction>> Objects)
{
	TArray<UObject*> ObjectsToSync;

	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if ( Object )
		{
			// @todo This only considers loaded materials! Find a good way to make this use the asset registry.
			for (TObjectIterator<UMaterial> It; It; ++It)
			{
				UMaterial* CurrentMaterial = *It;

				for (int32 FunctionIndex = 0; FunctionIndex < CurrentMaterial->MaterialFunctionInfos.Num(); FunctionIndex++)
				{
					if (CurrentMaterial->MaterialFunctionInfos[FunctionIndex].Function == Object)
					{
						ObjectsToSync.Add(CurrentMaterial);
						break;
					}
				}
			}
		}
	}

	if (ObjectsToSync.Num() > 0)
	{
		FAssetTools::Get().SyncBrowserToAssets(ObjectsToSync);
	}
}

UThumbnailInfo* FAssetTypeActions_MaterialFunction::GetThumbnailInfo(UObject* Asset) const
{
	UMaterialFunction* MaterialFunc = CastChecked<UMaterialFunction>(Asset);
	UThumbnailInfo* ThumbnailInfo = NULL;
	if( MaterialFunc )
	{
		ThumbnailInfo = MaterialFunc->ThumbnailInfo;
		if ( ThumbnailInfo == NULL )
		{
			ThumbnailInfo = NewObject<USceneThumbnailInfoWithPrimitive>(MaterialFunc, NAME_None, RF_Transactional);
			MaterialFunc->ThumbnailInfo = ThumbnailInfo;
		}
	}

	return ThumbnailInfo;
}

#undef LOCTEXT_NAMESPACE
