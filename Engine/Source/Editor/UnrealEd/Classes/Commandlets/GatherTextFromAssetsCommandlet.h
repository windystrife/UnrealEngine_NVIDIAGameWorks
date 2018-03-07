// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Internationalization/GatherableTextData.h"
#include "Commandlets/GatherTextCommandletBase.h"
#include "GatherTextFromAssetsCommandlet.generated.h"

/**
 *	UGatherTextFromAssetsCommandlet: Localization commandlet that collects all text to be localized from the game assets.
 */
UCLASS()
class UGatherTextFromAssetsCommandlet : public UGatherTextCommandletBase
{
	GENERATED_UCLASS_BODY()

	void ProcessGatherableTextDataArray(const FString& PackageFilePath, const TArray<FGatherableTextData>& GatherableTextDataArray);
	void ProcessPackages( const TArray< UPackage* >& PackagesToProcess );

public:
	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;

	bool GetConfigurationScript(const TMap<FString, FString>& InCommandLineParameters, FString& OutFilePath, FString& OutStepSectionName);
	bool ConfigureFromScript(const FString& GatherTextConfigPath, const FString& SectionName);

	//~ End UCommandlet Interface

private:
	static const FString UsageText;

	TArray<FString> ModulesToPreload;
	TArray<FString> IncludePathFilters;
	TArray<FString> CollectionFilters;
	TArray<FString> ExcludePathFilters;
	TArray<FString> PackageFileNameFilters;
	TArray<FString> ExcludeClassNames;
	TArray<FString> ManifestDependenciesList;

	bool bSkipGatherCache;
	bool bFixBroken;
	bool ShouldGatherFromEditorOnlyData;
	bool ShouldExcludeDerivedClasses;
};
