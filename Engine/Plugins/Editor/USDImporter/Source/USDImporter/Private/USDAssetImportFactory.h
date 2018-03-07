// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Factories/Factory.h"
#include "USDImporter.h"
#include "Factories/ImportSettings.h"

#include "USDAssetImportFactory.generated.h"

USTRUCT()
struct FUSDAssetImportContext : public FUsdImportContext
{
	GENERATED_USTRUCT_BODY()

	virtual void Init(UObject* InParent, const FString& InName, class IUsdStage* InStage);
};

UCLASS(transient)
class UUSDAssetImportFactory : public UFactory, public IImportSettingsParser
{
	GENERATED_UCLASS_BODY()

public:
	/** UFactory interface */
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual void CleanUp() override;
	virtual IImportSettingsParser* GetImportSettingsParser() override { return this; }

	/** IImportSettingsParser interface */
	virtual void ParseFromJson(TSharedRef<class FJsonObject> ImportSettingsJson) override;
private:
	UPROPERTY()
	FUSDAssetImportContext ImportContext;

	UPROPERTY()
	UUSDImportOptions* ImportOptions;
};
