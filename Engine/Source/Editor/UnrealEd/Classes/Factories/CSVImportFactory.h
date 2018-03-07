// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * Factory for importing table from text (CSV)
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/RichCurve.h"
#include "Factories/Factory.h"
#include "Factories/ImportSettings.h"
#include "CSVImportFactory.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCSVImportFactory, Log, All);

/** Enum to indicate what to import CSV as */
UENUM()
enum class ECSVImportType : uint8
{
	/** Import as UDataTable */
	ECSV_DataTable,
	/** Import as UCurveTable */
	ECSV_CurveTable,
	/** Import as a UCurveFloat */
	ECSV_CurveFloat,
	/** Import as a UCurveVector */
	ECSV_CurveVector,
	/** Import as a UCurveLinearColor */
	ECSV_CurveLinearColor,
};

USTRUCT()
struct FCSVImportSettings
{
	GENERATED_BODY()

	FCSVImportSettings();

	UPROPERTY()
	UScriptStruct* ImportRowStruct;

	UPROPERTY()
	ECSVImportType ImportType;

	UPROPERTY()
	TEnumAsByte<ERichCurveInterpMode> ImportCurveInterpMode;
};

UCLASS(hidecategories=Object)
class UNREALED_API UCSVImportFactory : public UFactory, public IImportSettingsParser
{
	GENERATED_UCLASS_BODY()

	//~ Begin UFactory Interface
	virtual FText GetDisplayName() const override;
	virtual UObject* FactoryCreateText( UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn ) override;
	virtual bool DoesSupportClass(UClass * Class) override;
	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual	IImportSettingsParser* GetImportSettingsParser() override { return this; }


	/* Reimport an object that was created based on a CSV */
	bool ReimportCSV(UObject* Obj);
	
	/**
	 * IImportSettings interface
	 */
	virtual void ParseFromJson(TSharedRef<class FJsonObject> ImportSettingsJson) override;

protected:
	virtual TArray<FString> DoImportDataTable(class UDataTable* TargetDataTable, const FString& DataToImport);
	virtual TArray<FString> DoImportCurveTable(class UCurveTable* TargetCurveTable, const FString& DataToImport, const ERichCurveInterpMode ImportCurveInterpMode);
	virtual TArray<FString> DoImportCurve(class UCurveBase* TargetCurve, const FString& DataToImport);

private:
	/* Reimport object from the given path*/
	bool Reimport(UObject* Obj, const FString& Path  );

	UPROPERTY()
	FCSVImportSettings AutomatedImportSettings;
};

