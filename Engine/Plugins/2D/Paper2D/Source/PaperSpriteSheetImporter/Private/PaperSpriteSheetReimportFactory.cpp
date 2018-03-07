// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PaperSpriteSheetReimportFactory.h"
#include "PaperSpriteSheetImporterLog.h"
#include "HAL/FileManager.h"
#include "EditorFramework/AssetImportData.h"
#include "PaperSpriteSheet.h"

#define LOCTEXT_NAMESPACE "PaperJsonImporter"

//////////////////////////////////////////////////////////////////////////
// UPaperSpriteSheetReimportFactory

UPaperSpriteSheetReimportFactory::UPaperSpriteSheetReimportFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UPaperSpriteSheet::StaticClass();

	bCreateNew = false;
}

bool UPaperSpriteSheetReimportFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UPaperSpriteSheet* SpriteSheet = Cast<UPaperSpriteSheet>(Obj);
	if (SpriteSheet && SpriteSheet->AssetImportData)
	{
		SpriteSheet->AssetImportData->ExtractFilenames(OutFilenames);
		return true;
	}
	return false;
}

void UPaperSpriteSheetReimportFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UPaperSpriteSheet* SpriteSheet = Cast<UPaperSpriteSheet>(Obj);
	if (SpriteSheet && ensure(NewReimportPaths.Num() == 1))
	{
		SpriteSheet->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}
}

EReimportResult::Type UPaperSpriteSheetReimportFactory::Reimport(UObject* Obj)
{
	UPaperSpriteSheet* SpriteSheet = Cast<UPaperSpriteSheet>(Obj);
	if (!SpriteSheet)
	{
		return EReimportResult::Failed;
	}

	// Make sure file is valid and exists
	const FString Filename = SpriteSheet->AssetImportData->GetFirstFilename();
	if (!Filename.Len() || IFileManager::Get().FileSize(*Filename) == INDEX_NONE)
	{
		return EReimportResult::Failed;
	}

	// Configure the importer with the reimport settings
	Importer.SetReimportData(SpriteSheet->SpriteNames, SpriteSheet->Sprites);
	Importer.ExistingBaseTextureName = SpriteSheet->TextureName;
	Importer.ExistingBaseTexture = SpriteSheet->Texture;
	Importer.ExistingNormalMapTextureName = SpriteSheet->NormalMapTextureName;
	Importer.ExistingNormalMapTexture = SpriteSheet->NormalMapTexture;

	// Run the import again
	EReimportResult::Type Result = EReimportResult::Failed;
	bool OutCanceled = false;

	if (ImportObject(SpriteSheet->GetClass(), SpriteSheet->GetOuter(), *SpriteSheet->GetName(), RF_Public | RF_Standalone, Filename, nullptr, OutCanceled) != nullptr)
	{
		UE_LOG(LogPaperSpriteSheetImporter, Log, TEXT("Imported successfully"));

		SpriteSheet->AssetImportData->Update(Filename);
		
		// Try to find the outer package so we can dirty it up
		if (SpriteSheet->GetOuter())
		{
			SpriteSheet->GetOuter()->MarkPackageDirty();
		}
		else
		{
			SpriteSheet->MarkPackageDirty();
		}
		Result = EReimportResult::Succeeded;
	}
	else
	{
		if (OutCanceled)
		{
			UE_LOG(LogPaperSpriteSheetImporter, Warning, TEXT("-- import canceled"));
		}
		else
		{
			UE_LOG(LogPaperSpriteSheetImporter, Warning, TEXT("-- import failed"));
		}

		Result = EReimportResult::Failed;
	}

	return Result;
}

int32 UPaperSpriteSheetReimportFactory::GetPriority() const
{
	return ImportPriority;
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
