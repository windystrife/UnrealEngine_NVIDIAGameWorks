// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "InternationalizationExportSettings.generated.h"

UCLASS(hidecategories = Object, MinimalAPI, config=InternationalizationExport)
class UInternationalizationExportSettings : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Which cultures should be exported */
	UPROPERTY(Category = CommonSettings, EditAnywhere, config)
	TArray<FString> CulturesToGenerate;

	/** The commandlet to run */
	UPROPERTY(Category = GatherTextStep, EditAnywhere, config)
	FString CommandletClass;

	/** Source for the localization data */
	UPROPERTY(Category = GatherTextStep, EditAnywhere, config)
	FString SourcePath;

	/** Destination for the localization data */
	UPROPERTY(Category = GatherTextStep, EditAnywhere, config)
	FString DestinationPath;

	/** Filename for the Portable Object format file */
	UPROPERTY(Category = GatherTextStep, EditAnywhere, config)
	FString PortableObjectName;

	/** Name of the manifest file */
	UPROPERTY(Category = GatherTextStep, EditAnywhere, config)
	FString ManifestName;

	/** Name of the archive file */
	UPROPERTY(Category = GatherTextStep, EditAnywhere, config)
	FString ArchiveName;

	/** Whether or not to export localization data */
	UPROPERTY(Category = GatherTextStep, EditAnywhere, config)
	bool bExportLoc;

	/** Whether or not to import localization data */
	UPROPERTY(Category = GatherTextStep, EditAnywhere, config)
	bool bImportLoc;

	/** Whether or not to use culture path */
	UPROPERTY(Category = GatherTextStep, EditAnywhere, config)
	bool bUseCultureDirectory;
};
