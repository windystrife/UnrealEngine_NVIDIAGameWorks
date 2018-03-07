// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "USDAssetImportFactory.h"
#include "USDImporter.h"
#include "IUSDImporterModule.h"
#include "ActorFactories/ActorFactoryStaticMesh.h"
#include "ScopedTimers.h"
#include "USDImportOptions.h"
#include "Engine/StaticMesh.h"
#include "Paths.h"
#include "JsonObjectConverter.h"

void FUSDAssetImportContext::Init(UObject* InParent, const FString& InName, class IUsdStage* InStage)
{
	FUsdImportContext::Init(InParent, InName, InStage);
}


UUSDAssetImportFactory::UUSDAssetImportFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = false;
	bEditAfterNew = true;
	SupportedClass = UStaticMesh::StaticClass();

	ImportOptions = ObjectInitializer.CreateDefaultSubobject<UUSDImportOptions>(this, TEXT("USDImportOptions"));

	bEditorImport = true;
	bText = false;

	Formats.Add(TEXT("usd;Universal Scene Descriptor files"));
	Formats.Add(TEXT("usda;Universal Scene Descriptor files"));
	Formats.Add(TEXT("usdc;Universal Scene Descriptor files"));
}

UObject* UUSDAssetImportFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	UObject* ImportedObject = nullptr;

	UUSDImporter* USDImporter = IUSDImporterModule::Get().GetImporter();

	if (IsAutomatedImport() || USDImporter->ShowImportOptions(*ImportOptions))
	{
		IUsdStage* Stage = USDImporter->ReadUSDFile(ImportContext, Filename);
		if (Stage)
		{
			ImportContext.Init(InParent, InName.ToString(), Stage);
			ImportContext.ImportOptions = ImportOptions;
			ImportContext.bApplyWorldTransformToGeometry = ImportOptions->bApplyWorldTransformToGeometry;

			TArray<FUsdPrimToImport> PrimsToImport;

			ImportContext.PrimResolver->FindPrimsToImport(ImportContext, PrimsToImport);

			ImportedObject = USDImporter->ImportMeshes(ImportContext, PrimsToImport);

			// Just return the first one imported
			ImportedObject = ImportContext.PathToImportAssetMap.Num() > 0 ? ImportContext.PathToImportAssetMap.CreateConstIterator().Value() : nullptr;
		}

		ImportContext.DisplayErrorMessages(IsAutomatedImport());
	}
	else
	{
		bOutOperationCanceled = true;
	}

	return ImportedObject;
}

bool UUSDAssetImportFactory::FactoryCanImport(const FString& Filename)
{
	const FString Extension = FPaths::GetExtension(Filename);

	if (Extension == TEXT("usd") || Extension == TEXT("usda") || Extension == TEXT("usdc"))
	{
		return true;
	}

	return false;
}

void UUSDAssetImportFactory::CleanUp()
{
	ImportContext = FUSDAssetImportContext();
	UnrealUSDWrapper::CleanUp();
}

void UUSDAssetImportFactory::ParseFromJson(TSharedRef<class FJsonObject> ImportSettingsJson)
{
	FJsonObjectConverter::JsonObjectToUStruct(ImportSettingsJson, ImportOptions->GetClass(), ImportOptions, 0, CPF_InstancedReference);
}
