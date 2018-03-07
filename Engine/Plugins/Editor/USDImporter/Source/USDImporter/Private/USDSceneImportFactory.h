// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Factories/SceneImportFactory.h"
#include "USDImporter.h"
#include "Editor/EditorEngine.h"
#include "Factories/ImportSettings.h"
#include "USDSceneImportFactory.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUSDSceneImport, Log, All);

class UWorld;

USTRUCT()
struct FUSDSceneImportContext : public FUsdImportContext
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	UWorld* World;

	UPROPERTY()
	TMap<FName, AActor*> ExistingActors;

	UPROPERTY()
	TArray<FName> ActorsToDestroy;

	UPROPERTY()
	class UActorFactory* EmptyActorFactory;

	UPROPERTY()
	TMap<UClass*, UActorFactory*> UsedFactories;

	FCachedActorLabels ActorLabels;

	virtual void Init(UObject* InParent, const FString& InName, class IUsdStage* InStage);
};

UCLASS(transient)
class UUSDSceneImportFactory : public USceneImportFactory, public IImportSettingsParser
{
	GENERATED_UCLASS_BODY()

public:
	/** UFactory Interface */
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual void CleanUp() override;
	virtual IImportSettingsParser* GetImportSettingsParser() override { return this; }

	/** IImportSettingsParser interface */
	virtual void ParseFromJson(TSharedRef<class FJsonObject> ImportSettingsJson) override;
private:
	void GenerateSpawnables(TArray<FActorSpawnData>& OutRootSpawnData, int32& OutTotalNumSpawnables);
	void RemoveExistingActors();
	void SpawnActors(const TArray<FActorSpawnData>& SpawnDatas, FScopedSlowTask& SlowTask);
	void OnActorSpawned(AActor* SpawnedActor, const FActorSpawnData& SpawnData);
private:
	UPROPERTY()
	FUSDSceneImportContext ImportContext;

	UPROPERTY()
	UUSDSceneImportOptions* ImportOptions;
};
