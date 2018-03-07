// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Factories/CSVImportFactory.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Curves/CurveBase.h"
#include "Curves/CurveFloat.h"
#include "Factories/ReimportCurveFactory.h"
#include "Factories/ReimportCurveTableFactory.h"
#include "Factories/ReimportDataTableFactory.h"
#include "EditorFramework/AssetImportData.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveVector.h"
#include "Engine/CurveTable.h"
#include "Engine/DataTable.h"
#include "Editor.h"

#include "Interfaces/IMainFrameModule.h"
#include "SCSVImportOptions.h"
#include "DataTableEditorUtils.h"
#include "JsonObjectConverter.h"

DEFINE_LOG_CATEGORY(LogCSVImportFactory);

#define LOCTEXT_NAMESPACE "CSVImportFactory"

//////////////////////////////////////////////////////////////////////////

FCSVImportSettings::FCSVImportSettings()
{
	ImportRowStruct = nullptr;
	ImportType = ECSVImportType::ECSV_DataTable;
	ImportCurveInterpMode = ERichCurveInterpMode::RCIM_Linear;
}


static UClass* GetCurveClass( ECSVImportType ImportType )
{
	switch( ImportType )
	{
	case ECSVImportType::ECSV_CurveFloat:
		return UCurveFloat::StaticClass();
		break;
	case ECSVImportType::ECSV_CurveVector:
		return UCurveVector::StaticClass();
		break;
	case ECSVImportType::ECSV_CurveLinearColor:
		return UCurveLinearColor::StaticClass();
		break;
	default:
		return UCurveVector::StaticClass();
		break;
	}
}


UCSVImportFactory::UCSVImportFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	bCreateNew = false;
	bEditAfterNew = true;
	SupportedClass = UDataTable::StaticClass();

	bEditorImport = true;
	bText = true;

	// Give this factory a lower than normal import priority, as CSV and JSON can be commonly used and we'd like to give the other import factories a shot first
	--ImportPriority;

	Formats.Add(TEXT("csv;Comma-separated values"));
}

FText UCSVImportFactory::GetDisplayName() const
{
	return LOCTEXT("CSVImportFactoryDescription", "Comma Separated Values");
}

bool UCSVImportFactory::DoesSupportClass(UClass * Class)
{
	return (Class == UDataTable::StaticClass() || Class == UCurveTable::StaticClass() || Class == UCurveFloat::StaticClass() || Class == UCurveVector::StaticClass() || Class == UCurveLinearColor::StaticClass() );
}

bool UCSVImportFactory::FactoryCanImport(const FString& Filename)
{
	const FString Extension = FPaths::GetExtension(Filename);

	if(Extension == TEXT("csv"))
	{
		return true;
	}
	return false;
}

UObject* UCSVImportFactory::FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{
	FEditorDelegates::OnAssetPreImport.Broadcast(this, InClass, InParent, InName, Type);

	// See if table/curve already exists
	UDataTable* ExistingTable = FindObject<UDataTable>(InParent, *InName.ToString());
	UCurveTable* ExistingCurveTable = FindObject<UCurveTable>(InParent, *InName.ToString());
	UCurveBase* ExistingCurve = FindObject<UCurveBase>(InParent, *InName.ToString());

	// Save off information if so
	bool bHaveInfo = false;
	UScriptStruct* ImportRowStruct = NULL;
	ERichCurveInterpMode ImportCurveInterpMode = RCIM_Linear;
	ECSVImportType ImportType = ECSVImportType::ECSV_DataTable;

	if( IsAutomatedImport() )
	{
		ImportRowStruct = AutomatedImportSettings.ImportRowStruct;
		ImportCurveInterpMode = AutomatedImportSettings.ImportCurveInterpMode;
		ImportType = AutomatedImportSettings.ImportType;

		// For automated import to work a row struct must be specified for a datatable type or a curve type must be specified
		bHaveInfo = ImportRowStruct != nullptr || ImportType != ECSVImportType::ECSV_DataTable;
	}
	else if(ExistingTable != NULL)
	{
		ImportRowStruct = ExistingTable->RowStruct;
		bHaveInfo = true;
	}
	else if(ExistingCurveTable != NULL)
	{
		ImportType = ECSVImportType::ECSV_CurveTable;
		bHaveInfo = true;
	}
	else if(ExistingCurve != NULL)
	{
		ImportType = ExistingCurve->IsA(UCurveFloat::StaticClass()) ? ECSVImportType::ECSV_CurveFloat : ECSVImportType::ECSV_CurveVector;
		bHaveInfo = true;
	}

	bool bDoImport = true;

	// If we do not have the info we need, pop up window to ask for things
	if(!bHaveInfo && !IsAutomatedImport())
	{
		TSharedPtr<SWindow> ParentWindow;
		// Check if the main frame is loaded.  When using the old main frame it may not be.
		if( FModuleManager::Get().IsModuleLoaded( "MainFrame" ) )
		{
			IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>( "MainFrame" );
			ParentWindow = MainFrame.GetParentWindow();
		}

		TSharedPtr<SCSVImportOptions> ImportOptionsWindow;

		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title( LOCTEXT("DataTableOptionsWindowTitle", "DataTable Options" ))
			.SizingRule( ESizingRule::Autosized );
		
		FString ParentFullPath;

		if (InParent)
		{
			ParentFullPath = InParent->GetPathName();
		}

		Window->SetContent
		(
			SAssignNew(ImportOptionsWindow, SCSVImportOptions)
				.WidgetWindow(Window)
				.FullPath(FText::FromString(ParentFullPath))
		);

		FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

		ImportType = ImportOptionsWindow->GetSelectedImportType();
		ImportRowStruct = ImportOptionsWindow->GetSelectedRowStruct();
		ImportCurveInterpMode = ImportOptionsWindow->GetSelectedCurveIterpMode();
		bDoImport = ImportOptionsWindow->ShouldImport();
	}
	else if(!bHaveInfo && IsAutomatedImport())
	{
		if(ImportType == ECSVImportType::ECSV_DataTable && !ImportRowStruct)
		{
			UE_LOG(LogCSVImportFactory, Error, TEXT("A Data table row type must be specified in the import settings json file for automated import"));
		}
		bDoImport = false;
	}

	UObject* NewAsset = NULL;
	if(bDoImport)
	{
		// Convert buffer to an FString (will this be slow with big tables?)
		FString String;
		//const int32 BufferSize = BufferEnd - Buffer;
		//appBufferToString( String, Buffer, BufferSize );
		int32 NumChars = (BufferEnd - Buffer);
		TArray<TCHAR>& StringChars = String.GetCharArray();
		StringChars.AddUninitialized(NumChars+1);
		FMemory::Memcpy(StringChars.GetData(), Buffer, NumChars*sizeof(TCHAR));
		StringChars.Last() = 0;

		TArray<FString> Problems;

		if (ImportType == ECSVImportType::ECSV_DataTable)
		{
			UClass* DataTableClass = UDataTable::StaticClass();

			// If there is an existing table, need to call this to free data memory before recreating object
			if(ExistingTable != NULL)
			{
				DataTableClass = ExistingTable->GetClass();
				ExistingTable->EmptyTable();
			}

			// Create/reset table
			UDataTable* NewTable = NewObject<UDataTable>(InParent, DataTableClass, InName, Flags);
			NewTable->RowStruct = ImportRowStruct;
			NewTable->AssetImportData->Update(CurrentFilename);
			// Go ahead and create table from string
			Problems = DoImportDataTable(NewTable, String);

			// Print out
			UE_LOG(LogCSVImportFactory, Log, TEXT("Imported DataTable '%s' - %d Problems"), *InName.ToString(), Problems.Num());
			NewAsset = NewTable;
		}
		else if (ImportType == ECSVImportType::ECSV_CurveTable)
		{
			UClass* CurveTableClass = UCurveTable::StaticClass();

			// If there is an existing table, need to call this to free data memory before recreating object
			if(ExistingCurveTable != NULL)
			{
				CurveTableClass = ExistingCurveTable->GetClass();
				ExistingCurveTable->EmptyTable();
			}

			// Create/reset table
			UCurveTable* NewTable = NewObject<UCurveTable>(InParent, CurveTableClass, InName, Flags);
			NewTable->AssetImportData->Update(CurrentFilename);

			// Go ahead and create table from string
			Problems = DoImportCurveTable(NewTable, String, ImportCurveInterpMode);

			// Print out
			UE_LOG(LogCSVImportFactory, Log, TEXT("Imported CurveTable '%s' - %d Problems"), *InName.ToString(), Problems.Num());
			NewAsset = NewTable;
		}
		else if (ImportType == ECSVImportType::ECSV_CurveFloat || ImportType == ECSVImportType::ECSV_CurveVector || ImportType == ECSVImportType::ECSV_CurveLinearColor)
		{
			UClass* CurveClass = ExistingCurve ? ExistingCurve->GetClass() : GetCurveClass( ImportType );

			// Create/reset curve
			UCurveBase* NewCurve = NewObject<UCurveBase>(InParent, CurveClass, InName, Flags);

			Problems = DoImportCurve(NewCurve, String);

			UE_LOG(LogCSVImportFactory, Log, TEXT("Imported Curve '%s' - %d Problems"), *InName.ToString(), Problems.Num());
			NewCurve->AssetImportData->Update(CurrentFilename);
			NewAsset = NewCurve;
		}
		
		if(Problems.Num() > 0)
		{
			FString AllProblems;

			for(int32 ProbIdx=0; ProbIdx<Problems.Num(); ProbIdx++)
			{
				// Output problems to log
				UE_LOG(LogCSVImportFactory, Log, TEXT("%d:%s"), ProbIdx, *Problems[ProbIdx]);
				AllProblems += Problems[ProbIdx];
				AllProblems += TEXT("\n");
			}

			if(!IsAutomatedImport())
			{
				// Pop up any problems for user
				FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(AllProblems));
			}
		}
	}

	FEditorDelegates::OnAssetPostImport.Broadcast(this, NewAsset);

	return NewAsset;
}

bool UCSVImportFactory::ReimportCSV( UObject* Obj )
{
	bool bHandled = false;
	if(UCurveBase* Curve = Cast<UCurveBase>(Obj))
	{
		bHandled = Reimport(Curve, Curve->AssetImportData->GetFirstFilename());
	}
	else if(UCurveTable* CurveTable = Cast<UCurveTable>(Obj))
	{
		bHandled = Reimport(CurveTable, CurveTable->AssetImportData->GetFirstFilename());
	}
	else if(UDataTable* DataTable = Cast<UDataTable>(Obj))
	{
		bHandled = Reimport(DataTable, DataTable->AssetImportData->GetFirstFilename());
	}
	return bHandled;
}

void UCSVImportFactory::ParseFromJson(TSharedRef<class FJsonObject> ImportSettingsJson)
{	
	FJsonObjectConverter::JsonObjectToUStruct(ImportSettingsJson, FCSVImportSettings::StaticStruct(), &AutomatedImportSettings, 0, 0);
}

bool UCSVImportFactory::Reimport(UObject* Obj, const FString& Path)
{
	if(Path.IsEmpty() == false)
	{ 
		FString FilePath = IFileManager::Get().ConvertToRelativePath(*Path);

		FString Data;
		if( FFileHelper::LoadFileToString( Data, *FilePath) )
		{
			const TCHAR* Ptr = *Data;
			CurrentFilename = FilePath; //not thread safe but seems to be how it is done..
			auto Result = FactoryCreateText( Obj->GetClass(), Obj->GetOuter(), Obj->GetFName(), Obj->GetFlags(), NULL, *FPaths::GetExtension(FilePath), Ptr, Ptr+Data.Len(), NULL );
			return true;
		}
	}
	return false;
}

TArray<FString> UCSVImportFactory::DoImportDataTable(UDataTable* TargetDataTable, const FString& DataToImport)
{
	// Are we importing JSON data?
	const bool bIsJSON = CurrentFilename.EndsWith(TEXT(".json"));
	if (bIsJSON)
	{
		return TargetDataTable->CreateTableFromJSONString(DataToImport);
	}

	return TargetDataTable->CreateTableFromCSVString(DataToImport);
}

TArray<FString> UCSVImportFactory::DoImportCurveTable(UCurveTable* TargetCurveTable, const FString& DataToImport, const ERichCurveInterpMode InImportCurveInterpMode)
{
	// Are we importing JSON data?
	const bool bIsJSON = CurrentFilename.EndsWith(TEXT(".json"));
	if (bIsJSON)
	{
		return TargetCurveTable->CreateTableFromJSONString(DataToImport, InImportCurveInterpMode);
	}

	return TargetCurveTable->CreateTableFromCSVString(DataToImport, InImportCurveInterpMode);
}

TArray<FString> UCSVImportFactory::DoImportCurve(UCurveBase* TargetCurve, const FString& DataToImport)
{
	// Are we importing JSON data?
	const bool bIsJSON = CurrentFilename.EndsWith(TEXT(".json"));
	if (bIsJSON)
	{
		TArray<FString> Result;
		Result.Add(LOCTEXT("Error_CannotImportCurveFromJSON", "Cannot import a curve from JSON. Please use CSV instead.").ToString());
		return Result;
	}

	return TargetCurve->CreateCurveFromCSVString(DataToImport);
}

//////////////////////////////////////////////////////////////////////////

UReimportDataTableFactory::UReimportDataTableFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Formats.Add(TEXT("json;JavaScript Object Notation"));
}

bool UReimportDataTableFactory::FactoryCanImport( const FString& Filename )
{
	return true;
}

bool UReimportDataTableFactory::CanReimport( UObject* Obj, TArray<FString>& OutFilenames )
{	
	UDataTable* DataTable = Cast<UDataTable>(Obj);
	if(DataTable)
	{
		DataTable->AssetImportData->ExtractFilenames(OutFilenames);
		return true;
	}
	return false;
}

void UReimportDataTableFactory::SetReimportPaths( UObject* Obj, const TArray<FString>& NewReimportPaths )
{	
	UDataTable* DataTable = Cast<UDataTable>(Obj);
	if(DataTable && ensure(NewReimportPaths.Num() == 1))
	{
		DataTable->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}
}

EReimportResult::Type UReimportDataTableFactory::Reimport( UObject* Obj )
{	
	auto Result = EReimportResult::Failed;
	if(auto DataTable = Cast<UDataTable>(Obj))
	{
		FDataTableEditorUtils::BroadcastPreChange(DataTable, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
		Result = UCSVImportFactory::ReimportCSV(DataTable) ? EReimportResult::Succeeded : EReimportResult::Failed;
		FDataTableEditorUtils::BroadcastPostChange(DataTable, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
	}
	return Result;
}

int32 UReimportDataTableFactory::GetPriority() const
{
	return ImportPriority;
}

////////////////////////////////////////////////////////////////////////////
//
UReimportCurveTableFactory::UReimportCurveTableFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Formats.Add(TEXT("json;JavaScript Object Notation"));
}

bool UReimportCurveTableFactory::CanReimport( UObject* Obj, TArray<FString>& OutFilenames )
{	
	UCurveTable* CurveTable = Cast<UCurveTable>(Obj);
	if(CurveTable)
	{
		CurveTable->AssetImportData->ExtractFilenames(OutFilenames);
		return true;
	}
	return false;
}

void UReimportCurveTableFactory::SetReimportPaths( UObject* Obj, const TArray<FString>& NewReimportPaths )
{	
	UCurveTable* CurveTable = Cast<UCurveTable>(Obj);
	if(CurveTable && ensure(NewReimportPaths.Num() == 1))
	{
		CurveTable->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}
}

EReimportResult::Type UReimportCurveTableFactory::Reimport( UObject* Obj )
{	
	if(Cast<UCurveTable>(Obj))
	{
		return UCSVImportFactory::ReimportCSV(Obj) ? EReimportResult::Succeeded : EReimportResult::Failed;
	}
	return EReimportResult::Failed;
}

int32 UReimportCurveTableFactory::GetPriority() const
{
	return ImportPriority;
}

////////////////////////////////////////////////////////////////////////////
//
UReimportCurveFactory::UReimportCurveFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UCurveBase::StaticClass();
}

bool UReimportCurveFactory::CanReimport( UObject* Obj, TArray<FString>& OutFilenames )
{	
	UCurveBase* CurveBase = Cast<UCurveBase>(Obj);
	if(CurveBase)
	{
		CurveBase->AssetImportData->ExtractFilenames(OutFilenames);
		return true;
	}
	return false;
}

void UReimportCurveFactory::SetReimportPaths( UObject* Obj, const TArray<FString>& NewReimportPaths )
{	
	UCurveBase* CurveBase = Cast<UCurveBase>(Obj);
	if(CurveBase && ensure(NewReimportPaths.Num() == 1))
	{
		CurveBase->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}
}

EReimportResult::Type UReimportCurveFactory::Reimport( UObject* Obj )
{	
	if(Cast<UCurveBase>(Obj))
	{
		return UCSVImportFactory::ReimportCSV(Obj) ? EReimportResult::Succeeded : EReimportResult::Failed;
	}
	return EReimportResult::Failed;
}

int32 UReimportCurveFactory::GetPriority() const
{
	return ImportPriority;
}


#undef LOCTEXT_NAMESPACE

