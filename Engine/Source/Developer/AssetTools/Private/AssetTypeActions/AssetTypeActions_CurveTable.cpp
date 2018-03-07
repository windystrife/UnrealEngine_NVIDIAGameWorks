// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_CurveTable.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/FileHelper.h"
#include "EditorFramework/AssetImportData.h"
#include "Framework/Application/SlateApplication.h"
#include "AssetToolsModule.h"
#include "Editor/CurveTableEditor/Public/CurveTableEditorModule.h"
#include "DesktopPlatformModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_CurveTable::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	auto Tables = GetTypedWeakObjectPtrs<UObject>(InObjects);

	TArray<FString> ImportPaths;
	for (auto TableIter = Tables.CreateConstIterator(); TableIter; ++TableIter)
	{
		const UCurveTable* CurTable = Cast<UCurveTable>((*TableIter).Get());
		if (CurTable)
		{
			CurTable->AssetImportData->ExtractFilenames(ImportPaths);
		}
	}

	MenuBuilder.AddMenuEntry(
		LOCTEXT("CurveTable_ExportAsCSV", "Export as CSV"),
		LOCTEXT("CurveTable_ExportAsCSVTooltip", "Export the curve table as a file containing CSV data."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_CurveTable::ExecuteExportAsCSV, Tables ),
			FCanExecuteAction()
			)
		);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("CurveTable_ExportAsJSON", "Export as JSON"),
		LOCTEXT("CurveTable_ExportAsJSONTooltip", "Export the curve table as a file containing JSON data."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_CurveTable::ExecuteExportAsJSON, Tables ),
			FCanExecuteAction()
			)
		);

	TArray<FString> PotentialFileExtensions;
	PotentialFileExtensions.Add(TEXT(".xls"));
	PotentialFileExtensions.Add(TEXT(".xlsm"));
	PotentialFileExtensions.Add(TEXT(".csv"));
	PotentialFileExtensions.Add(TEXT(".json"));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("CurveTable_OpenSourceData", "Open Source Data"),
		LOCTEXT("CurveTable_OpenSourceDataTooltip", "Opens the curve table's source data file in an external editor. It will search using the following extensions: .xls/.xlsm/.csv/.json"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_CurveTable::ExecuteFindSourceFileInExplorer, ImportPaths, PotentialFileExtensions ),
			FCanExecuteAction::CreateSP(this, &FAssetTypeActions_CurveTable::CanExecuteFindSourceFileInExplorer, ImportPaths, PotentialFileExtensions)
			)
		);
}

void FAssetTypeActions_CurveTable::ExecuteExportAsCSV(TArray< TWeakObjectPtr<UObject> > Objects)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

	const void* ParentWindowWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto CurTable = Cast<UCurveTable>((*ObjIt).Get());
		if (CurTable)
		{
			const FText Title = FText::Format(LOCTEXT("CurveTable_ExportCSVDialogTitle", "Export '{0}' as CSV..."), FText::FromString(*CurTable->GetName()));
			const FString CurrentFilename = CurTable->AssetImportData->GetFirstFilename();
			const FString FileTypes = TEXT("Curve Table CSV (*.csv)|*.csv");

			TArray<FString> OutFilenames;
			DesktopPlatform->SaveFileDialog(
				ParentWindowWindowHandle,
				Title.ToString(),
				(CurrentFilename.IsEmpty()) ? TEXT("") : FPaths::GetPath(CurrentFilename),
				(CurrentFilename.IsEmpty()) ? TEXT("") : FPaths::GetBaseFilename(CurrentFilename) + TEXT(".csv"),
				FileTypes,
				EFileDialogFlags::None,
				OutFilenames
				);

			if (OutFilenames.Num() > 0)
			{
				FFileHelper::SaveStringToFile(CurTable->GetTableAsCSV(), *OutFilenames[0]);
			}
		}
	}
}

void FAssetTypeActions_CurveTable::ExecuteExportAsJSON(TArray< TWeakObjectPtr<UObject> > Objects)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

	const void* ParentWindowWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto CurTable = Cast<UCurveTable>((*ObjIt).Get());
		if (CurTable)
		{
			const FText Title = FText::Format(LOCTEXT("CurveTable_ExportJSONDialogTitle", "Export '{0}' as JSON..."), FText::FromString(*CurTable->GetName()));
			const FString CurrentFilename = CurTable->AssetImportData->GetFirstFilename();
			const FString FileTypes = TEXT("Curve Table JSON (*.json)|*.json");

			TArray<FString> OutFilenames;
			DesktopPlatform->SaveFileDialog(
				ParentWindowWindowHandle,
				Title.ToString(),
				(CurrentFilename.IsEmpty()) ? TEXT("") : FPaths::GetPath(CurrentFilename),
				(CurrentFilename.IsEmpty()) ? TEXT("") : FPaths::GetBaseFilename(CurrentFilename) + TEXT(".json"),
				FileTypes,
				EFileDialogFlags::None,
				OutFilenames
				);

			if (OutFilenames.Num() > 0)
			{
				FFileHelper::SaveStringToFile(CurTable->GetTableAsJSON(), *OutFilenames[0]);
			}
		}
	}
}

void FAssetTypeActions_CurveTable::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Table = Cast<UCurveTable>(*ObjIt);
		if (Table != NULL)
		{
			FCurveTableEditorModule& CurveTableEditorModule = FModuleManager::LoadModuleChecked<FCurveTableEditorModule>( "CurveTableEditor" );
			TSharedRef< ICurveTableEditor > NewCurveTableEditor = CurveTableEditorModule.CreateCurveTableEditor( EToolkitMode::Standalone, EditWithinLevelEditor, Table );
		}
	}
}

void FAssetTypeActions_CurveTable::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (auto& Asset : TypeAssets)
	{
		const auto CurveTable = CastChecked<UCurveTable>(Asset);
		CurveTable->AssetImportData->ExtractFilenames(OutSourceFilePaths);
	}
}

// Attempts to export temporary CSV files and diff those. If that fails we fall back to diffing the data table assets directly.
void FAssetTypeActions_CurveTable::PerformAssetDiff(UObject* OldAsset, UObject* NewAsset, const FRevisionInfo& OldRevision, const FRevisionInfo& NewRevision) const
{
	UCurveTable* OldCurveTable = CastChecked<UCurveTable>(OldAsset);
	UCurveTable* NewCurveTable = CastChecked<UCurveTable>(NewAsset);

	// Build names for temp csv files
	FString RelOldTempFileName = FString::Printf(TEXT("%sTemp%s-%s.csv"), *FPaths::DiffDir(), *OldAsset->GetName(), *OldRevision.Revision);
	FString AbsoluteOldTempFileName = FPaths::ConvertRelativePathToFull(RelOldTempFileName);
	FString RelNewTempFileName = FString::Printf(TEXT("%sTemp%s-%s.csv"), *FPaths::DiffDir(), *NewAsset->GetName(), *NewRevision.Revision);
	FString AbsoluteNewTempFileName = FPaths::ConvertRelativePathToFull(RelNewTempFileName);

	// save temp files
	bool OldResult = FFileHelper::SaveStringToFile(OldCurveTable->GetTableAsCSV(), *AbsoluteOldTempFileName);
	bool NewResult = FFileHelper::SaveStringToFile(NewCurveTable->GetTableAsCSV(), *AbsoluteNewTempFileName);

	if (OldResult && NewResult)
	{
		FString DiffCommand = GetDefault<UEditorLoadingSavingSettings>()->TextDiffToolPath.FilePath;

		FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
		AssetToolsModule.Get().CreateDiffProcess(DiffCommand, AbsoluteOldTempFileName, AbsoluteNewTempFileName);
	}
	else
	{
		FAssetTypeActions_CSVAssetBase::PerformAssetDiff(OldAsset, NewAsset, OldRevision, NewRevision);
	}
}

#undef LOCTEXT_NAMESPACE
