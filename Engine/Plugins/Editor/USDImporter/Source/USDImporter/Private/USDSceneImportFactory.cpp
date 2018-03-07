// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "USDSceneImportFactory.h"
#include "USDImportOptions.h"
#include "ActorFactories/ActorFactoryEmptyActor.h"
#include "Engine/Selection.h"
#include "ScopedTransaction.h"
#include "ILayers.h"
#include "IAssetRegistry.h"
#include "AssetRegistryModule.h"
#include "IUSDImporterModule.h"
#include "USDConversionUtils.h"
#include "ObjectTools.h"
#include "ScopedSlowTask.h"
#include "PackageTools.h"
#include "Editor.h"
#include "PropertySetter.h"
#include "AssetSelection.h"
#include "JsonObjectConverter.h"
#include "USDPrimResolver.h"

#define LOCTEXT_NAMESPACE "USDImportPlugin"

UUSDSceneImportFactory::UUSDSceneImportFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = false;
	bEditAfterNew = true;
	SupportedClass = UWorld::StaticClass();

	bEditorImport = true;
	bText = false;

	ImportOptions = ObjectInitializer.CreateDefaultSubobject<UUSDSceneImportOptions>(this, TEXT("USDSceneImportOptions"));

	Formats.Add(TEXT("usd;Universal Scene Descriptor files"));
	Formats.Add(TEXT("usda;Universal Scene Descriptor files"));
	Formats.Add(TEXT("usdc;Universal Scene Descriptor files"));
}

UObject* UUSDSceneImportFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	UUSDImporter* USDImporter = IUSDImporterModule::Get().GetImporter();

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	TArray<UObject*> AllAssets;

	if(IsAutomatedImport() || USDImporter->ShowImportOptions(*ImportOptions))
	{
		// @todo: Disabled.  This messes with the ability to replace existing actors since actors with this name could still be in the transaction buffer
		//FScopedTransaction ImportUSDScene(LOCTEXT("ImportUSDSceneTransaction", "Import USD Scene"));

		IUsdStage* Stage = USDImporter->ReadUSDFile(ImportContext, Filename);
		if (Stage)
		{
			IUsdPrim* RootPrim = Stage->GetRootPrim();

			ImportContext.Init(InParent, InName.ToString(), Stage);
			ImportContext.ImportOptions = ImportOptions;

			if (IsAutomatedImport() && InParent && ImportOptions->PathForAssets.Path == TEXT("/Game"))
			{
				ImportOptions->PathForAssets.Path = ImportContext.ImportPathName;
			}

			ImportContext.ImportPathName = ImportOptions->PathForAssets.Path;
	
			// Actors will have the transform
			ImportContext.bApplyWorldTransformToGeometry = false;

			TArray<FUsdPrimToImport> PrimsToImport;

			UUSDPrimResolver* PrimResolver = ImportContext.PrimResolver;
		
			EExistingActorPolicy ExistingActorPolicy = ImportOptions->ExistingActorPolicy;

			TArray<FActorSpawnData> SpawnDatas;

			FScopedSlowTask SlowTask(3.0f, LOCTEXT("ImportingUSDScene", "Importing USD Scene"));
	
			SlowTask.EnterProgressFrame(1.0f, LOCTEXT("FindingActorsToSpawn", "Finding Actors To Spawn"));
			PrimResolver->FindActorsToSpawn(ImportContext, SpawnDatas);

			SlowTask.EnterProgressFrame(1.0f, LOCTEXT("SpawningActors", "SpawningActors"));
			RemoveExistingActors();

			SpawnActors(SpawnDatas, SlowTask);
		}

		FEditorDelegates::OnAssetPostImport.Broadcast(this, ImportContext.World);

		GEditor->BroadcastLevelActorListChanged();

		ImportContext.DisplayErrorMessages(IsAutomatedImport());

		return ImportContext.World;
	}
	else
	{
		bOutOperationCanceled = true;
		return nullptr;
	}
}

bool UUSDSceneImportFactory::FactoryCanImport(const FString& Filename)
{
	const FString Extension = FPaths::GetExtension(Filename);

	if (Extension == TEXT("usd") || Extension == TEXT("usda") || Extension == TEXT("usdc"))
	{
		return true;
	}

	return false;
}

void UUSDSceneImportFactory::CleanUp()
{
	ImportContext = FUSDSceneImportContext();
	UnrealUSDWrapper::CleanUp();
}

void UUSDSceneImportFactory::ParseFromJson(TSharedRef<class FJsonObject> ImportSettingsJson)
{
	FJsonObjectConverter::JsonObjectToUStruct(ImportSettingsJson, ImportOptions->GetClass(), ImportOptions, 0, CPF_InstancedReference);
}

void UUSDSceneImportFactory::SpawnActors(const TArray<FActorSpawnData>& SpawnDatas, FScopedSlowTask& SlowTask)
{
	if(SpawnDatas.Num() > 0)
	{
		int32 SpawnCount = 0;

		FText NumActorsToSpawn = FText::AsNumber(SpawnDatas.Num());

		const float WorkAmount = 1.0f / SpawnDatas.Num();

		for (const FActorSpawnData& SpawnData : SpawnDatas)
		{
			++SpawnCount;
			SlowTask.EnterProgressFrame(WorkAmount, FText::Format(LOCTEXT("SpawningActor", "SpawningActor {0}/{1}"), FText::AsNumber(SpawnCount), NumActorsToSpawn));

			AActor* SpawnedActor = ImportContext.PrimResolver->SpawnActor(ImportContext, SpawnData);

			OnActorSpawned(SpawnedActor, SpawnData);
		}
	}
}

void UUSDSceneImportFactory::RemoveExistingActors()
{
	// We need to check here for any actors that exist that need to be deleted before we continue (they are getting replaced)
	{
		bool bDeletedActors = false;

		USelection* ActorSelection = GEditor->GetSelectedActors();
		ActorSelection->BeginBatchSelectOperation();

		EExistingActorPolicy ExistingActorPolicy = ImportOptions->ExistingActorPolicy;

		if (ExistingActorPolicy == EExistingActorPolicy::Replace && ImportContext.ActorsToDestroy.Num())
		{
			for (FName ExistingActorName : ImportContext.ActorsToDestroy)
			{
				AActor* ExistingActor = ImportContext.ExistingActors.FindAndRemoveChecked(ExistingActorName);
				if (ExistingActor)
				{
					bDeletedActors = true;
					if (ExistingActor->IsSelected())
					{
						GEditor->SelectActor(ExistingActor, false, false);
					}
					ImportContext.World->DestroyActor(ExistingActor);
				}
			}
		}

		ActorSelection->EndBatchSelectOperation();

		if (!IsAutomatedImport())
		{
			GEditor->NoteSelectionChange();
		}

		if (bDeletedActors)
		{
			// We need to make sure the actors are really gone before we start replacing them
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}
	}

	// Refresh actor labels as we deleted actors which were cached
	ULevel* CurrentLevel = ImportContext.World->GetCurrentLevel();
	check(CurrentLevel);

	for (AActor* Actor : CurrentLevel->Actors)
	{
		if (Actor)
		{
			ImportContext.ActorLabels.Add(Actor->GetActorLabel());
		}
	}
}

void UUSDSceneImportFactory::OnActorSpawned(AActor* SpawnedActor, const FActorSpawnData& SpawnData)
{
	if(Cast<UUSDSceneImportOptions>(ImportContext.ImportOptions)->bImportProperties)
	{
		FUSDPropertySetter PropertySetter(ImportContext);

		PropertySetter.ApplyPropertiesToActor(SpawnedActor, SpawnData.ActorPrim, TEXT(""));
	}
}

void FUSDSceneImportContext::Init(UObject* InParent, const FString& InName, IUsdStage* InStage)
{
	FUsdImportContext::Init(InParent, InName, InStage);

	World = GEditor->GetEditorWorldContext().World();

	ULevel* CurrentLevel = World->GetCurrentLevel();
	check(CurrentLevel);

	for (AActor* Actor : CurrentLevel->Actors)
	{
		if (Actor)
		{
			ExistingActors.Add(Actor->GetFName(), Actor);
		}
	}


	UActorFactoryEmptyActor* NewEmptyActorFactory = NewObject<UActorFactoryEmptyActor>();
	// Do not create sprites for empty actors.  These will likely just be parents of mesh actors;
	NewEmptyActorFactory->bVisualizeActor = false;

	EmptyActorFactory = NewEmptyActorFactory;

	bFindUnrealAssetReferences = true;
}


#undef LOCTEXT_NAMESPACE
