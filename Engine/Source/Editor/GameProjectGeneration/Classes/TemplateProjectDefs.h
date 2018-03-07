// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "FeaturePackContentSource.h"
#include "TemplateProjectDefs.generated.h"

// does not require reflection exposure
struct FTemplateConfigValue
{
	FString ConfigFile;
	FString ConfigSection;
	FString ConfigKey;
	FString ConfigValue;
	bool bShouldReplaceExistingValue;

	GAMEPROJECTGENERATION_API FTemplateConfigValue(const FString& InFile, const FString& InSection, const FString& InKey, const FString& InValue, bool InShouldReplaceExistingValue);
};

USTRUCT()
struct FTemplateReplacement
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FString> Extensions;

	UPROPERTY()
	FString From;

	UPROPERTY()
	FString To;

	UPROPERTY()
	bool bCaseSensitive;
};

USTRUCT()
struct FTemplateFolderRename
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString From;

	UPROPERTY()
	FString To;
};

USTRUCT()
struct FLocalizedTemplateString
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString Language;

	UPROPERTY()
	FString Text;
};

UCLASS(abstract,config=TemplateDefs,MinimalAPI)
class UTemplateProjectDefs : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(config)
	TArray<FLocalizedTemplateString> LocalizedDisplayNames;

	UPROPERTY(config)
	TArray<FLocalizedTemplateString> LocalizedDescriptions;

	UPROPERTY(config)
	TArray<FString> FoldersToIgnore;

	UPROPERTY(config)
	TArray<FString> FilesToIgnore;

	UPROPERTY(config)
	TArray<FTemplateFolderRename> FolderRenames;

	UPROPERTY(config)
	TArray<FTemplateReplacement> FilenameReplacements;

	UPROPERTY(config)
	TArray<FTemplateReplacement> ReplacementsInFiles;

	UPROPERTY(config)
	FString SortKey;

	UPROPERTY(config)
	FName Category;

	UPROPERTY(config)
	FString ClassTypes;

	UPROPERTY(config)
	FString AssetTypes;

	/* Should we allow creation of a project from this template */
	UPROPERTY(config)
	bool bAllowProjectCreation;
	
	/* Optional list of feature packs to include */
	UPROPERTY(config)
	TArray<FString> PacksToInclude;

	/** What detail level to edit when editing shared template resources  */
	UPROPERTY(config)
	EFeaturePackDetailLevel EditDetailLevelPreference;

	/* Shared feature packs. The files in these packs listed in these structures marked as 'additionalfiles' will be copied on project generation*/
	UPROPERTY(config)
	TArray<FFeaturePackLevelSet> SharedContentPacks;

	/** Fixes up all strings in this definitions object to replace \%TEMPLATENAME\% with the supplied template name and \%PROJECTNAME\% with the supplied project name */
	void FixupStrings(const FString& TemplateName, const FString& ProjectName);

	/** Returns the display name for the current culture, or English if the current culture has no translation */
	FText GetDisplayNameText();

	/** Returns the display name for the current culture, or English if the current culture has no translation */
	FText GetLocalizedDescription();

	/** Does this template generate C++ source? */
	virtual bool GeneratesCode(const FString& ProjectTemplatePath) const PURE_VIRTUAL(UTemplateProjectDefs::GeneratesCode, return false;)

	/** Callback for each file rename, so class renames can be extracted*/
	virtual bool IsClassRename(const FString& DestFilename, const FString& SrcFilename, const FString& FileExtension) const { return false; }

	/** Callback for adding config values */
	virtual void AddConfigValues(TArray<FTemplateConfigValue>& ConfigValuesToSet, const FString& TemplateName, const FString& ProjectName, bool bShouldGenerateCode) const { }

	/** Callback after project generation is done, allowing for custom project generation behavior */
	virtual bool PostGenerateProject(const FString& DestFolder, const FString& SrcFolder, const FString& NewProjectFile, const FString& TemplateFile, bool bShouldGenerateCode, FText& OutFailReason) { return true;  }

private:
	void FixString(FString& InOutStringToFix, const FString& TemplateName, const FString& ProjectName);
};

