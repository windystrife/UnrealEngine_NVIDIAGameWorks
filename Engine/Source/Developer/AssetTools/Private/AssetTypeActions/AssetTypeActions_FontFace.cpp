// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_FontFace.h"
#include "FontEditorModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "EditorReimportHandler.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_FontFace::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	auto FontFaces = GetTypedWeakObjectPtrs<UFontFace>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ReimportFontFaceLabel", "Reimport"),
		LOCTEXT("ReimportFontFaceTooltip", "Reimport the selected font(s)."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.ReimportAsset"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_FontFace::ExecuteReimport, FontFaces),
			FCanExecuteAction::CreateSP(this, &FAssetTypeActions_FontFace::CanExecuteReimport, FontFaces)
			)
		);
}

void FAssetTypeActions_FontFace::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	// Load the FontEditor module to ensure that FFontFaceDetailsCustomization is registered
	IFontEditorModule* FontEditorModule = &FModuleManager::LoadModuleChecked<IFontEditorModule>("FontEditor");
	FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, InObjects);
}

bool FAssetTypeActions_FontFace::CanExecuteReimport(const TArray<TWeakObjectPtr<UFontFace>> Objects) const
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if (Object)
		{
			// We allow a reimport if any of the fonts have a source filename
			if (!Object->SourceFilename.IsEmpty())
			{
				return true;
			}
		}
	}

	return false;
}

void FAssetTypeActions_FontFace::ExecuteReimport(const TArray<TWeakObjectPtr<UFontFace>> Objects) const
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if (Object)
		{
			// Skip fonts that don't have a source filename, as they can't be reimported
			if (!Object->SourceFilename.IsEmpty())
			{
				// Fonts fail to reimport if they ask for a new file if missing
				FReimportManager::Instance()->Reimport(Object, /*bAskForNewFileIfMissing=*/false);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
