// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_DataTable.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/FileHelper.h"
#include "EditorFramework/AssetImportData.h"
#include "Dialogs/Dialogs.h"
#include "Framework/Application/SlateApplication.h"

#include "Editor/DataTableEditor/Public/DataTableEditorModule.h"
#include "DesktopPlatformModule.h"
#include "AssetToolsModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_DataTable::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	auto Tables = GetTypedWeakObjectPtrs<UObject>(InObjects);
	
	TArray<FString> ImportPaths;
	for (auto TableIter = Tables.CreateConstIterator(); TableIter; ++TableIter)
	{
		const UDataTable* CurTable = Cast<UDataTable>((*TableIter).Get());
		if (CurTable)
		{
			CurTable->AssetImportData->ExtractFilenames(ImportPaths);
		}
	}

	MenuBuilder.AddMenuEntry(
		LOCTEXT("DataTable_ExportAsCSV", "Export as CSV"),
		LOCTEXT("DataTable_ExportAsCSVTooltip", "Export the data table as a file containing CSV data."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_DataTable::ExecuteExportAsCSV, Tables ),
			FCanExecuteAction()
			)
		);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("DataTable_ExportAsJSON", "Export as JSON"),
		LOCTEXT("DataTable_ExportAsJSONTooltip", "Export the data table as a file containing JSON data."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_DataTable::ExecuteExportAsJSON, Tables ),
			FCanExecuteAction()
			)
		);

	TArray<FString> PotentialFileExtensions;
	PotentialFileExtensions.Add(TEXT(".xls"));
	PotentialFileExtensions.Add(TEXT(".xlsm"));
	PotentialFileExtensions.Add(TEXT(".csv"));
	PotentialFileExtensions.Add(TEXT(".json"));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("DataTable_OpenSourceData", "Open Source Data"),
		LOCTEXT("DataTable_OpenSourceDataTooltip", "Opens the data table's source data file in an external editor. It will search using the following extensions: .xls/.xlsm/.csv/.json"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_DataTable::ExecuteFindSourceFileInExplorer, ImportPaths, PotentialFileExtensions ),
			FCanExecuteAction::CreateSP(this, &FAssetTypeActions_DataTable::CanExecuteFindSourceFileInExplorer, ImportPaths, PotentialFileExtensions)
			)
		);
}

void FAssetTypeActions_DataTable::ExecuteExportAsCSV(TArray< TWeakObjectPtr<UObject> > Objects)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

	const void* ParentWindowWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto DataTable = Cast<UDataTable>((*ObjIt).Get());
		if (DataTable)
		{
			const FText Title = FText::Format(LOCTEXT("DataTable_ExportCSVDialogTitle", "Export '{0}' as CSV..."), FText::FromString(*DataTable->GetName()));
			const FString CurrentFilename = DataTable->AssetImportData->GetFirstFilename();
			const FString FileTypes = TEXT("Data Table CSV (*.csv)|*.csv");

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
				FFileHelper::SaveStringToFile(DataTable->GetTableAsCSV(EDataTableExportFlags::UsePrettyPropertyNames | EDataTableExportFlags::UsePrettyEnumNames), *OutFilenames[0]);
			}
		}
	}
}

void FAssetTypeActions_DataTable::ExecuteExportAsJSON(TArray< TWeakObjectPtr<UObject> > Objects)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

	const void* ParentWindowWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto DataTable = Cast<UDataTable>((*ObjIt).Get());
		if (DataTable)
		{
			const FText Title = FText::Format(LOCTEXT("DataTable_ExportJSONDialogTitle", "Export '{0}' as JSON..."), FText::FromString(*DataTable->GetName()));
			const FString CurrentFilename = DataTable->AssetImportData->GetFirstFilename();
			const FString FileTypes = TEXT("Data Table JSON (*.json)|*.json");

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
				FFileHelper::SaveStringToFile(DataTable->GetTableAsJSON(EDataTableExportFlags::UsePrettyPropertyNames | EDataTableExportFlags::UsePrettyEnumNames | EDataTableExportFlags::UseJsonObjectsForStructs), *OutFilenames[0]);
			}
		}
	}
}

void FAssetTypeActions_DataTable::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	TArray<UDataTable*> DataTablesToOpen;
	TArray<UDataTable*> InvalidDataTables;

	for (UObject* Obj : InObjects)
	{
		UDataTable* Table = Cast<UDataTable>(Obj);
		if (Table)
		{
			if (Table->RowStruct)
			{
				DataTablesToOpen.Add(Table);
			}
			else
			{
				InvalidDataTables.Add(Table);
			}
		}
	}

	if (InvalidDataTables.Num() > 0)
	{
		FTextBuilder DataTablesListText;
		DataTablesListText.Indent();
		for (UDataTable* Table : InvalidDataTables)
		{
			const FName ResolvedRowStructName = Table->GetRowStructName();
			DataTablesListText.AppendLineFormat(LOCTEXT("DataTable_MissingRowStructListEntry", "* {0} (Row Structure: {1})"), FText::FromString(Table->GetName()), FText::FromName(ResolvedRowStructName));
		}

		const EAppReturnType::Type DlgResult = OpenMsgDlgInt(
			EAppMsgType::YesNoCancel, 
			FText::Format(LOCTEXT("DataTable_MissingRowStructMsg", "The following Data Tables are missing their row structure and will not be editable.\n\n{0}\n\nDo you want to open these data tables?"), DataTablesListText.ToText()), 
			LOCTEXT("DataTable_MissingRowStructTitle", "Continue?")
			);

		switch(DlgResult)
		{
		case EAppReturnType::Yes:
			DataTablesToOpen.Append(InvalidDataTables);
			break;
		case EAppReturnType::Cancel:
			return;
		default:
			break;
		}
	}

	FDataTableEditorModule& DataTableEditorModule = FModuleManager::LoadModuleChecked<FDataTableEditorModule>("DataTableEditor");
	for (UDataTable* Table : DataTablesToOpen)
	{
		DataTableEditorModule.CreateDataTableEditor(EToolkitMode::Standalone, EditWithinLevelEditor, Table);
	}
}

void FAssetTypeActions_DataTable::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (auto& Asset : TypeAssets)
	{
		const auto DataTable = CastChecked<UDataTable>(Asset);
		DataTable->AssetImportData->ExtractFilenames(OutSourceFilePaths);
	}
}

// Attempts to export temporary CSV files and diff those. If that fails we fall back to diffing the data table assets directly.
void FAssetTypeActions_DataTable::PerformAssetDiff(UObject* OldAsset, UObject* NewAsset, const FRevisionInfo& OldRevision, const FRevisionInfo& NewRevision) const
{
	UDataTable* OldDataTable = CastChecked<UDataTable>(OldAsset);
	UDataTable* NewDataTable = CastChecked<UDataTable>(NewAsset);

	// Build names for temp csv files
	FString RelOldTempFileName = FString::Printf(TEXT("%sTemp%s-%s.csv"), *FPaths::DiffDir(), *OldAsset->GetName(), *OldRevision.Revision);
	FString AbsoluteOldTempFileName = FPaths::ConvertRelativePathToFull(RelOldTempFileName);
	FString RelNewTempFileName = FString::Printf(TEXT("%sTemp%s-%s.csv"), *FPaths::DiffDir(), *NewAsset->GetName(), *NewRevision.Revision);
	FString AbsoluteNewTempFileName = FPaths::ConvertRelativePathToFull(RelNewTempFileName);

	// save temp files
	bool OldResult = FFileHelper::SaveStringToFile(OldDataTable->GetTableAsCSV(EDataTableExportFlags::UsePrettyPropertyNames | EDataTableExportFlags::UsePrettyEnumNames), *AbsoluteOldTempFileName);
	bool NewResult = FFileHelper::SaveStringToFile(NewDataTable->GetTableAsCSV(EDataTableExportFlags::UsePrettyPropertyNames | EDataTableExportFlags::UsePrettyEnumNames), *AbsoluteNewTempFileName);

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
