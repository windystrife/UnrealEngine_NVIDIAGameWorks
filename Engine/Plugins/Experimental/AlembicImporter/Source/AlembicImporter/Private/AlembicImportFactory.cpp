// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AlembicImportFactory.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Editor.h"
#include "EditorFramework/AssetImportData.h"
#include "HAL/FileManager.h"
#include "Framework/Application/SlateApplication.h"

#include "IMainFrameModule.h"

#include "AlembicImportOptions.h"

#include "AlembicLibraryModule.h"
#include "AbcImporter.h"
#include "AbcImportLogger.h"
#include "AbcImportSettings.h"
#include "AbcAssetImportData.h"

#include "GeometryCache.h"

#define LOCTEXT_NAMESPACE "AlembicImportFactory"

DEFINE_LOG_CATEGORY_STATIC(LogAlembic, Log, All);

UAlembicImportFactory::UAlembicImportFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = false;
	bEditAfterNew = true;
	SupportedClass = nullptr;

	bEditorImport = true;
	bText = false;

	bShowOption = true;

	Formats.Add(TEXT("abc;Alembic"));
}

void UAlembicImportFactory::PostInitProperties()
{
	Super::PostInitProperties();
	ImportSettings = UAbcImportSettings::Get();
}

FText UAlembicImportFactory::GetDisplayName() const
{
	return LOCTEXT("AlembicImportFactoryDescription", "Alembic");
}

bool UAlembicImportFactory::DoesSupportClass(UClass * Class)
{
	return (Class == UStaticMesh::StaticClass() || Class == UGeometryCache::StaticClass() || Class == USkeletalMesh::StaticClass() || Class == UAnimSequence::StaticClass());
}

UClass* UAlembicImportFactory::ResolveSupportedClass()
{
	return UStaticMesh::StaticClass();
}

UObject* UAlembicImportFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	FEditorDelegates::OnAssetPreImport.Broadcast(this, InClass, InParent, InName, TEXT("ABC"));

	FAbcImporter Importer;
	EAbcImportError ErrorCode = Importer.OpenAbcFileForImport(Filename);
	ImportSettings->bReimport = false;

	if (ErrorCode != AbcImportError_NoError)
	{
		// Failed to read the file info, fail the import
		FEditorDelegates::OnAssetPostImport.Broadcast(this, nullptr);
		return nullptr;
	}

	// Reset (possible) changed frame start value 
	ImportSettings->SamplingSettings.FrameStart = 0;
	ImportSettings->SamplingSettings.FrameEnd = Importer.GetEndFrameIndex();

	bOutOperationCanceled = false;

	if (bShowOption)
	{
		TSharedPtr<SAlembicImportOptions> Options;
	ShowImportOptionsWindow(Options, UFactory::CurrentFilename, Importer);
	// Set whether or not the user canceled
	bOutOperationCanceled = !Options->ShouldImport();
	}

	// Set up message log page name to separate different assets
	const FString PageName = "Importing " + InName.ToString() + ".abc";

	TArray<UObject*> ResultAssets;
	if (!bOutOperationCanceled)
	{
		FEditorDelegates::OnAssetPreImport.Broadcast(this, InClass, InParent, InName, TEXT("ABC"));

			int32 NumThreads = 1;
			if (FPlatformProcess::SupportsMultithreading())
			{
				NumThreads = FPlatformMisc::NumberOfCores();
			}

			// Import file		
			ErrorCode = Importer.ImportTrackData(NumThreads, ImportSettings);

			if (ErrorCode != AbcImportError_NoError)
			{
				// Failed to read the file info, fail the import
				FEditorDelegates::OnAssetPostImport.Broadcast(this, nullptr);
				FAbcImportLogger::OutputMessages(PageName);
				return nullptr;
			}
			else
			{
				if (ImportSettings->ImportType == EAlembicImportType::StaticMesh)
				{
					const TArray<UObject*> ResultStaticMeshes = ImportStaticMesh(Importer, InParent, Flags);
					ResultAssets.Append(ResultStaticMeshes);
				}
				else if (ImportSettings->ImportType == EAlembicImportType::GeometryCache)
				{
					UObject* GeometryCache = ImportGeometryCache(Importer, InParent, Flags);
					if (GeometryCache)
					{
						ResultAssets.Add(GeometryCache);
					}
				}
				else if (ImportSettings->ImportType == EAlembicImportType::Skeletal)
				{
					UObject* SkeletalMesh = ImportSkeletalMesh(Importer, InParent, Flags);
					if (SkeletalMesh)
					{
						ResultAssets.Add(SkeletalMesh);
					}
				}
			}


		for (UObject* Object : ResultAssets)
		{
			if (Object)
			{
				FEditorDelegates::OnAssetPostImport.Broadcast(this, Object);
				Object->MarkPackageDirty();
				Object->PostEditChange();
			}
		}

		FAbcImportLogger::OutputMessages(PageName);
	}

	// Determine out parent according to the generated assets outer
	UObject* OutParent = (ResultAssets.Num() > 0 && InParent != ResultAssets[0]->GetOutermost()) ? ResultAssets[0]->GetOutermost() : InParent;
	return (ResultAssets.Num() > 0) ? OutParent : nullptr;
}

TArray<UObject*> UAlembicImportFactory::ImportStaticMesh(FAbcImporter& Importer, UObject* InParent, EObjectFlags Flags)
{
	// Flush commands before importing
	FlushRenderingCommands();

	TArray<UObject*> Objects;

	const uint32 NumMeshes = Importer.GetNumMeshTracks();
	// Check if the alembic file contained any meshes
	if (NumMeshes > 0)
	{
		const TArray<UStaticMesh*>& StaticMeshes = Importer.ImportAsStaticMesh(InParent, Flags);

		for (UStaticMesh* StaticMesh : StaticMeshes)
		{
			if (StaticMesh)
			{
				// Setup asset import data
				if (!StaticMesh->AssetImportData || !StaticMesh->AssetImportData->IsA<UAbcAssetImportData>())
				{
					StaticMesh->AssetImportData = NewObject<UAbcAssetImportData>(StaticMesh);
				}
				StaticMesh->AssetImportData->Update(UFactory::CurrentFilename);

				UAbcAssetImportData* AssetImportData = Cast<UAbcAssetImportData>(StaticMesh->AssetImportData);
				if (AssetImportData)
				{
					Importer.UpdateAssetImportData(AssetImportData);
				}

				Objects.Add(StaticMesh);
			}
		}
	}

	return Objects;
}

UObject* UAlembicImportFactory::ImportGeometryCache(FAbcImporter& Importer, UObject* InParent, EObjectFlags Flags)
{
	// Flush commands before importing
	FlushRenderingCommands();

	const uint32 NumMeshes = Importer.GetNumMeshTracks();
	// Check if the alembic file contained any meshes
	if (NumMeshes > 0)
	{
		UGeometryCache* GeometryCache = Importer.ImportAsGeometryCache(InParent, Flags);

		if (!GeometryCache)
		{
			return nullptr;
		}

		// Setup asset import data
		if (!GeometryCache->AssetImportData || !GeometryCache->AssetImportData->IsA<UAbcAssetImportData>())
		{
			GeometryCache->AssetImportData = NewObject<UAbcAssetImportData>(GeometryCache);
		}
		GeometryCache->AssetImportData->Update(UFactory::CurrentFilename);
		UAbcAssetImportData* AssetImportData = Cast<UAbcAssetImportData>(GeometryCache->AssetImportData);
		if (AssetImportData)
		{
			Importer.UpdateAssetImportData(AssetImportData);
		}

		return GeometryCache;
	}
	else
	{
		// Not able to import a static mesh
		FEditorDelegates::OnAssetPostImport.Broadcast(this, nullptr);
		return nullptr;
	}
}

UObject* UAlembicImportFactory::ImportSkeletalMesh(FAbcImporter& Importer, UObject* InParent, EObjectFlags Flags)
{
	// Flush commands before importing
	FlushRenderingCommands();

	const uint32 NumMeshes = Importer.GetNumMeshTracks();
	// Check if the alembic file contained any meshes
	if (NumMeshes > 0)
	{
		TArray<UObject*> GeneratedObjects = Importer.ImportAsSkeletalMesh(InParent, Flags);

		if (!GeneratedObjects.Num())
		{
			return nullptr;
		}

		USkeletalMesh* SkeletalMesh = [&GeneratedObjects]()
		{
			UObject** FoundObject = GeneratedObjects.FindByPredicate([](UObject* Object) { return Object->IsA<USkeletalMesh>(); });
			return FoundObject ? CastChecked<USkeletalMesh>(*FoundObject) : nullptr;
		}();			
			
		if (SkeletalMesh)
		{
		// Setup asset import data
		if (!SkeletalMesh->AssetImportData || !SkeletalMesh->AssetImportData->IsA<UAbcAssetImportData>())
		{
			SkeletalMesh->AssetImportData = NewObject<UAbcAssetImportData>(SkeletalMesh);
		}
		SkeletalMesh->AssetImportData->Update(UFactory::CurrentFilename);
		UAbcAssetImportData* AssetImportData = Cast<UAbcAssetImportData>(SkeletalMesh->AssetImportData);
		if (AssetImportData)
		{
			Importer.UpdateAssetImportData(AssetImportData);
		}
		}

		UAnimSequence* AnimSequence = [&GeneratedObjects]()
		{
			UObject** FoundObject = GeneratedObjects.FindByPredicate([](UObject* Object) { return Object->IsA<UAnimSequence>(); });
			return FoundObject ? CastChecked<UAnimSequence>(*FoundObject) : nullptr;
		}();

		if (AnimSequence)
		{
			// Setup asset import data
			if (!AnimSequence->AssetImportData || !AnimSequence->AssetImportData->IsA<UAbcAssetImportData>())
			{
				AnimSequence->AssetImportData = NewObject<UAbcAssetImportData>(AnimSequence);
			}
			AnimSequence->AssetImportData->Update(UFactory::CurrentFilename);
			UAbcAssetImportData* AssetImportData = Cast<UAbcAssetImportData>(AnimSequence->AssetImportData);
			if (AssetImportData)
			{
				Importer.UpdateAssetImportData(AssetImportData);
			}
		}
		

		return SkeletalMesh;
	}
	else
	{
		// Not able to import a static mesh
		FEditorDelegates::OnAssetPostImport.Broadcast(this, nullptr);
		return nullptr;
	}
}

bool UAlembicImportFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UAssetImportData* ImportData = nullptr;
	if (Obj->GetClass() == UStaticMesh::StaticClass())
	{
		UStaticMesh* Mesh = Cast<UStaticMesh>(Obj);
		ImportData = Mesh->AssetImportData;
	}
	else if (Obj->GetClass() == UGeometryCache::StaticClass())
	{
		UGeometryCache* Cache = Cast<UGeometryCache>(Obj);
		ImportData = Cache->AssetImportData;
	}
	else if (Obj->GetClass() == USkeletalMesh::StaticClass())
	{
		USkeletalMesh* Cache = Cast<USkeletalMesh>(Obj);
		ImportData = Cache->AssetImportData;
	}
	else if (Obj->GetClass() == UAnimSequence::StaticClass())
	{
		UAnimSequence* Cache = Cast<UAnimSequence>(Obj);
		ImportData = Cache->AssetImportData;
	}
	
	if (ImportData)
	{
		if (FPaths::GetExtension(ImportData->GetFirstFilename()) == TEXT("abc") || ( Obj->GetClass() == UAnimSequence::StaticClass() && ImportData->GetFirstFilename().IsEmpty()))
		{
			ImportData->ExtractFilenames(OutFilenames);
			return true;
		}
	}
	return false;
}

void UAlembicImportFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UStaticMesh* Mesh = Cast<UStaticMesh>(Obj);
	if (Mesh && Mesh->AssetImportData && ensure(NewReimportPaths.Num() == 1))
	{
		Mesh->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}

	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Obj);
	if (SkeletalMesh && SkeletalMesh->AssetImportData && ensure(NewReimportPaths.Num() == 1))
	{
		SkeletalMesh->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}

	UAnimSequence* Sequence = Cast<UAnimSequence>(Obj);
	if (Sequence && Sequence->AssetImportData && ensure(NewReimportPaths.Num() == 1))
	{
		Sequence->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}

	UGeometryCache* GeometryCache = Cast<UGeometryCache>(Obj);
	if (GeometryCache && GeometryCache->AssetImportData && ensure(NewReimportPaths.Num() == 1))
	{
		GeometryCache->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}
}

EReimportResult::Type UAlembicImportFactory::Reimport(UObject* Obj)
{
	ImportSettings->bReimport = true;
	if (Obj->GetClass() == UStaticMesh::StaticClass())
	{
		UStaticMesh* Mesh = Cast<UStaticMesh>(Obj);
		if (!Mesh)
		{
			return EReimportResult::Failed;
		}

		CurrentFilename = Mesh->AssetImportData->GetFirstFilename();

		return ReimportStaticMesh(Mesh);
	}
	else if (Obj->GetClass() == UGeometryCache::StaticClass())
	{
		UGeometryCache* GeometryCache = Cast<UGeometryCache>(Obj);
		if (!GeometryCache)
		{
			return EReimportResult::Failed;
		}

		CurrentFilename = GeometryCache->AssetImportData->GetFirstFilename();

		EReimportResult::Type Result = ReimportGeometryCache(GeometryCache);

		if (GeometryCache->GetOuter())
		{
			GeometryCache->GetOuter()->MarkPackageDirty();
		}
		else
		{
			GeometryCache->MarkPackageDirty();
		}

		return Result;
	}
	else if (Obj->GetClass() == USkeletalMesh::StaticClass())
	{
		USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Obj);
		if (!SkeletalMesh)
		{
			return EReimportResult::Failed;
		}

		CurrentFilename = SkeletalMesh->AssetImportData->GetFirstFilename();

		EReimportResult::Type Result = ReimportSkeletalMesh(SkeletalMesh);

		if (SkeletalMesh->GetOuter())
		{
			SkeletalMesh->GetOuter()->MarkPackageDirty();
		}
		else
		{
			SkeletalMesh->MarkPackageDirty();
		}

		return Result;
	}
	else if (Obj->GetClass() == UAnimSequence::StaticClass())
	{
		UAnimSequence* AnimSequence = Cast<UAnimSequence>(Obj);
		if (!AnimSequence)
		{
			return EReimportResult::Failed;
		}

		CurrentFilename = AnimSequence->AssetImportData->GetFirstFilename();
		USkeletalMesh* SkeletalMesh = nullptr;
		for (TObjectIterator<USkeletalMesh> It; It; ++It)
		{
			// This works because the skeleton is unique for every imported alembic cache
			if (It->Skeleton == AnimSequence->GetSkeleton())
			{
				SkeletalMesh = *It;
				break;
			}
		}

		if (!SkeletalMesh)
		{
			return EReimportResult::Failed;
		}

		EReimportResult::Type Result = ReimportSkeletalMesh(SkeletalMesh);

		if (SkeletalMesh->GetOuter())
		{
			SkeletalMesh->GetOuter()->MarkPackageDirty();
		}
		else
		{
			SkeletalMesh->MarkPackageDirty();
		}

		return Result;
	}

	return EReimportResult::Failed;
}

void UAlembicImportFactory::ShowImportOptionsWindow(TSharedPtr<SAlembicImportOptions>& Options, FString FilePath, const FAbcImporter& Importer)
{

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Alembic Cache Import Options"))
		.SizingRule(ESizingRule::Autosized);

	Window->SetContent
		(
			SAssignNew(Options, SAlembicImportOptions).WidgetWindow(Window)
			.ImportSettings(ImportSettings)
			.WidgetWindow(Window)
			.PolyMeshes(Importer.GetPolyMeshes())
			.FullPath(FText::FromString(FilePath))
		);

	TSharedPtr<SWindow> ParentWindow;

	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);
}

EReimportResult::Type UAlembicImportFactory::ReimportGeometryCache(UGeometryCache* Cache)
{
	// Ensure that the file provided by the path exists
	if (IFileManager::Get().FileSize(*CurrentFilename) == INDEX_NONE)
	{
		return EReimportResult::Failed;
	}

	FAbcImporter Importer;
	EAbcImportError ErrorCode = Importer.OpenAbcFileForImport(CurrentFilename);

	if (ErrorCode != AbcImportError_NoError)
	{
		// Failed to read the file info, fail the re importing process 
		return EReimportResult::Failed;
	}



	if (Cache->AssetImportData && Cache->AssetImportData->IsA<UAbcAssetImportData>())
	{
		UAbcAssetImportData* ImportData = Cast<UAbcAssetImportData>(Cache->AssetImportData);
		PopulateOptionsWithImportData(ImportData);
		Importer.RetrieveAssetImportData(ImportData);
	}

	ImportSettings->ImportType = EAlembicImportType::GeometryCache;
	ImportSettings->SamplingSettings.FrameStart = 0;
	ImportSettings->SamplingSettings.FrameEnd = Importer.GetEndFrameIndex();

	if (bShowOption)
	{
		TSharedPtr<SAlembicImportOptions> Options;
	ShowImportOptionsWindow(Options, CurrentFilename, Importer);

	if (!Options->ShouldImport())
	{
		return EReimportResult::Cancelled;
	}
	}

	int32 NumThreads = 1;
	if (FPlatformProcess::SupportsMultithreading())
	{
		NumThreads = FPlatformMisc::NumberOfCores();
	}

	// Import file	
	ErrorCode = Importer.ImportTrackData(NumThreads, ImportSettings);

	if (ErrorCode != AbcImportError_NoError)
	{
		// Failed to read the file info, fail the re importing process 
		return EReimportResult::Failed;
	}

	UGeometryCache* GeometryCache = Importer.ReimportAsGeometryCache(Cache);

	if (!GeometryCache)
	{
		return EReimportResult::Failed;
	}
	else
	{
		// Update file path/timestamp (Path could change if user has to browse for it manually)
		if (!GeometryCache->AssetImportData || !GeometryCache->AssetImportData->IsA<UAbcAssetImportData>())
		{
			GeometryCache->AssetImportData = NewObject<UAbcAssetImportData>(GeometryCache);
		}

		GeometryCache->AssetImportData->Update(CurrentFilename);
		UAbcAssetImportData* AssetImportData = Cast<UAbcAssetImportData>(GeometryCache->AssetImportData);
		if (AssetImportData)
		{
			Importer.UpdateAssetImportData(AssetImportData);
		}
	}

	return EReimportResult::Succeeded;
}

EReimportResult::Type UAlembicImportFactory::ReimportSkeletalMesh(USkeletalMesh* SkeletalMesh)
{
	// Ensure that the file provided by the path exists
	if (IFileManager::Get().FileSize(*CurrentFilename) == INDEX_NONE)
	{
		return EReimportResult::Failed;
	}

	FAbcImporter Importer;
	EAbcImportError ErrorCode = Importer.OpenAbcFileForImport(CurrentFilename);

	if (ErrorCode != AbcImportError_NoError)
	{
		// Failed to read the file info, fail the re importing process 
		return EReimportResult::Failed;
	}

	if (SkeletalMesh->AssetImportData && SkeletalMesh->AssetImportData->IsA<UAbcAssetImportData>())
	{
		UAbcAssetImportData* ImportData = Cast<UAbcAssetImportData>(SkeletalMesh->AssetImportData);
		PopulateOptionsWithImportData(ImportData);
		Importer.RetrieveAssetImportData(ImportData);
	}

	ImportSettings->ImportType = EAlembicImportType::Skeletal;
	ImportSettings->SamplingSettings.FrameStart = 0;
	ImportSettings->SamplingSettings.FrameEnd = Importer.GetEndFrameIndex();

	if (bShowOption)
	{
		TSharedPtr<SAlembicImportOptions> Options;
	ShowImportOptionsWindow(Options, CurrentFilename, Importer);

	if (!Options->ShouldImport())
	{
		return EReimportResult::Cancelled;
	}
	}

	int32 NumThreads = 1;
	if (FPlatformProcess::SupportsMultithreading())
	{
		NumThreads = FPlatformMisc::NumberOfCores();
	}

	// Import file	
	ErrorCode = Importer.ImportTrackData(NumThreads, ImportSettings);

	if (ErrorCode != AbcImportError_NoError)
	{
		// Failed to read the file info, fail the re importing process 
		return EReimportResult::Failed;
	}

	TArray<UObject*> ReimportedObjects = Importer.ReimportAsSkeletalMesh(SkeletalMesh);
	USkeletalMesh* NewSkeletalMesh = [&ReimportedObjects]()
	{
		UObject** FoundObject = ReimportedObjects.FindByPredicate([](UObject* Object) { return Object->IsA<USkeletalMesh>(); });
		return FoundObject ? CastChecked<USkeletalMesh>(*FoundObject) : nullptr;
	}();

	if (!NewSkeletalMesh)
	{
		return EReimportResult::Failed;
	}
	else
	{
		// Update file path/timestamp (Path could change if user has to browse for it manually)
		if (!NewSkeletalMesh->AssetImportData || !NewSkeletalMesh->AssetImportData->IsA<UAbcAssetImportData>())
		{
			NewSkeletalMesh->AssetImportData = NewObject<UAbcAssetImportData>(NewSkeletalMesh);
		}

		NewSkeletalMesh->AssetImportData->Update(CurrentFilename);
		UAbcAssetImportData* AssetImportData = Cast<UAbcAssetImportData>(NewSkeletalMesh->AssetImportData);
		if (AssetImportData)
		{
			Importer.UpdateAssetImportData(AssetImportData);
		}
	}

	UAnimSequence* NewAnimSequence = [&ReimportedObjects]()
	{
		UObject** FoundObject = ReimportedObjects.FindByPredicate([](UObject* Object) { return Object->IsA<UAnimSequence>(); });
		return FoundObject ? CastChecked<UAnimSequence>(*FoundObject) : nullptr;
	}();

	if (!NewAnimSequence)
	{
		return EReimportResult::Failed;
	}
	else
	{
		// Update file path/timestamp (Path could change if user has to browse for it manually)
		if (!NewAnimSequence->AssetImportData || !NewAnimSequence->AssetImportData->IsA<UAbcAssetImportData>())
		{
			NewAnimSequence->AssetImportData = NewObject<UAbcAssetImportData>(NewAnimSequence);
		}

		NewAnimSequence->AssetImportData->Update(CurrentFilename);
		UAbcAssetImportData* AssetImportData = Cast<UAbcAssetImportData>(NewAnimSequence->AssetImportData);
		if (AssetImportData)
		{
			Importer.UpdateAssetImportData(AssetImportData);
		}
	}

	return EReimportResult::Succeeded;
}

void UAlembicImportFactory::PopulateOptionsWithImportData(UAbcAssetImportData* ImportData)
{

}

EReimportResult::Type UAlembicImportFactory::ReimportStaticMesh(UStaticMesh* Mesh)
{
	// Ensure that the file provided by the path exists
	if (IFileManager::Get().FileSize(*CurrentFilename) == INDEX_NONE)
	{
		return EReimportResult::Failed;
	}

	FAbcImporter Importer;
	EAbcImportError ErrorCode = Importer.OpenAbcFileForImport(CurrentFilename);

	if (ErrorCode != AbcImportError_NoError)
	{
		// Failed to read the file info, fail the re importing process 
		return EReimportResult::Failed;
	}

	if (Mesh->AssetImportData && Mesh->AssetImportData->IsA<UAbcAssetImportData>())
	{
		UAbcAssetImportData* ImportData = Cast<UAbcAssetImportData>(Mesh->AssetImportData);
		PopulateOptionsWithImportData(ImportData);
		Importer.RetrieveAssetImportData(ImportData);
	}

	ImportSettings->ImportType = EAlembicImportType::StaticMesh;
	ImportSettings->SamplingSettings.FrameStart = 0;
	ImportSettings->SamplingSettings.FrameEnd = Importer.GetEndFrameIndex();

	if (bShowOption)
	{
		TSharedPtr<SAlembicImportOptions> Options;
	ShowImportOptionsWindow(Options, CurrentFilename, Importer);

	if (!Options->ShouldImport())
	{
		return EReimportResult::Cancelled;
	}
	}

	int32 NumThreads = 1;
	if (FPlatformProcess::SupportsMultithreading())
	{
		NumThreads = FPlatformMisc::NumberOfCores();
	}

	ErrorCode = Importer.ImportTrackData(NumThreads, ImportSettings);

	if (ErrorCode != AbcImportError_NoError)
	{
		// Failed to read the file info, fail the re importing process 
		return EReimportResult::Failed;
	}
	else
	{
		const TArray<UStaticMesh*>& StaticMeshes = Importer.ReimportAsStaticMesh(Mesh);
		for (UStaticMesh* StaticMesh : StaticMeshes)
		{
			if (StaticMesh)
			{
				// Setup asset import data
				if (!StaticMesh->AssetImportData || !StaticMesh->AssetImportData->IsA<UAbcAssetImportData>())
				{
					StaticMesh->AssetImportData = NewObject<UAbcAssetImportData>(StaticMesh);
				}
				StaticMesh->AssetImportData->Update(UFactory::CurrentFilename);
				UAbcAssetImportData* AssetImportData = Cast<UAbcAssetImportData>(StaticMesh->AssetImportData);
				if (AssetImportData)
				{
					Importer.UpdateAssetImportData(AssetImportData);
				}
			}
		}

		if (!StaticMeshes.Num())
		{
			return EReimportResult::Failed;
		}
	}

	return EReimportResult::Succeeded;
}

int32 UAlembicImportFactory::GetPriority() const
{
	return ImportPriority;
}

#undef LOCTEXT_NAMESPACE
