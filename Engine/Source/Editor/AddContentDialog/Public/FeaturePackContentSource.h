// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Editor/AddContentDialog/Private/IContentSource.h"
#include "FeaturePackContentSource.generated.h"

class FJsonValue;
class FPakPlatformFile;
struct FSearchEntry;

struct FPackData
{
	FString PackSource;
	FString PackName;
	FString PackMap;
	TArray<UObject*>	ImportedObjects;
};

class FLocalizedTextArray
{
public:
	FLocalizedTextArray()
	{
	}

	/** Creates a new FLocalizedText
		@param InTwoLetterLanguage - The iso 2-letter language specifier.
		@param InText - The text in the language specified */
	FLocalizedTextArray(FString InTwoLetterLanguage, FString InText)
	{
		TwoLetterLanguage = InTwoLetterLanguage;
		TArray<FString> AsArray;
		InText.ParseIntoArray(AsArray,TEXT(","));
		for (int32 iString = 0; iString < AsArray.Num() ; iString++)
		{
			Tags.Add(FText::FromString(AsArray[iString]));
		}
	}

	/** Gets the iso 2-letter language specifier for this text. */
	FString GetTwoLetterLanguage() const
	{
		return TwoLetterLanguage;
	}

	/** Gets the array of tags in the language specified. */
	TArray<FText> GetTags() const
	{
		return Tags;
	}

private:
	FString TwoLetterLanguage;
	TArray<FText> Tags;
};


/** Defines categories for shared template resource levels. */
UENUM()
enum class EFeaturePackDetailLevel :uint8
{
	Standard,
	High,
};

/* Structure that defines a shared feature pack resource. */
USTRUCT()
struct FFeaturePackLevelSet
{
	GENERATED_BODY()

	FFeaturePackLevelSet(){};

	/** Creates a new FFeaturePackLevelSet
		@param InMountName - Name of the pack/folder to insert to 
		@param InDetailLevels - The levels available for this pack*/
	FFeaturePackLevelSet(FString InMountName, TArray<EFeaturePackDetailLevel> InDetailLevels)
	{
		MountName = InMountName;
		DetailLevels = InDetailLevels;
	}

	/* List of shared resource levels for this shared resource.*/
	UPROPERTY()
	TArray<EFeaturePackDetailLevel> DetailLevels;

	/* Mount name for the shared resource - this is the folder the resource will be copied to on project generation as well as the name of the folder that will appear in the content browser. */
	UPROPERTY()
	FString MountName;

	FString GetFeaturePackNameForLevel(EFeaturePackDetailLevel InLevel, bool bLevelRequired = false)
	{
		check(DetailLevels.Num()>0); // We need at least one detail level defined
		int32 Index = DetailLevels.Find(InLevel);
		FString DetailString;
		if( Index != INDEX_NONE)
		{			
			UEnum::GetValueAsString(TEXT("/Script/AddContentDialog.EFeaturePackDetailLevel"), InLevel, DetailString);					
		}
		else 
		{
			check(bLevelRequired==false); // The level is REQUIRED and we don't have it !
			// If we didn't have the requested level, use the first
			UEnum::GetValueAsString(TEXT("/Script/AddContentDialog.EFeaturePackDetailLevel"), DetailLevels[0], DetailString);
		}
		FString NameString = MountName+DetailString + TEXT(".upack");
		return NameString;
	}
};

/* Structure that defines a shared feature pack resource. */
USTRUCT()
struct FFeatureAdditionalFiles
{
	GENERATED_BODY()

	FFeatureAdditionalFiles(){};
	
	/* Name of the folder to insert the files to */
	UPROPERTY()
	FString DestinationFilesFolder;

	/* List of files to insert */
	UPROPERTY()
	TArray<FString> AdditionalFilesList;
};

class FPakPlatformFile;
struct FSearchEntry;

/** A content source which represents a content upack. */
class ADDCONTENTDIALOG_API FFeaturePackContentSource : public IContentSource
{
public:
	FFeaturePackContentSource();
	FFeaturePackContentSource(FString InFeaturePackPath);

	virtual ~FFeaturePackContentSource();

	virtual TArray<FLocalizedText> GetLocalizedNames() const override;
	virtual TArray<FLocalizedText> GetLocalizedDescriptions() const override;
	
	virtual EContentSourceCategory GetCategory() const override;
	virtual TArray<FLocalizedText> GetLocalizedAssetTypes() const override;
	virtual FString GetSortKey() const override;
	virtual FString GetClassTypesUsed() const override;
	
	virtual TSharedPtr<FImageData> GetIconData() const override;
	virtual TArray<TSharedPtr<FImageData>> GetScreenshotData() const override;
	FString GetFocusAssetName() const;

	virtual bool InstallToProject(FString InstallPath) override;

	void InsertAdditionalFeaturePacks();
	bool InsertAdditionalResources(TArray<FFeaturePackLevelSet> InAdditionalFeaturePacks,EFeaturePackDetailLevel RequiredLevel, const FString& InDestinationFolder,TArray<FString>& InFilesCopied);

	virtual bool IsDataValid() const override;
			
	/*
	 * Copies the list of files specified in 'AdditionFilesToInclude' section in the config.ini of the feature pack.
	 *
	 * @param DestinationFolder	Destination folder for the files
	 * @param FilesCopied		List of files copied
	 * @param bContainsSource 	Set to true if the file list contains any source files
	 * @returns true if config file was read and parsed successfully
	 */
	void CopyAdditionalFilesToFolder( const FString& DestinationFolder, TArray<FString>& FilesCopied, bool &bHasSourceFiles, FString InGameFolder = FString() );

	/*
	 * Returns a list of additional files (including the path) as specified in the config file if one exists in the pack file.
	 *
	 * @param FileList		  array to receive list of files
	 * @param bContainsSource did the file list contain any source files
	 * @returns true if config file was read and parsed successfully
	 */
	bool GetAdditionalFilesForPack(TArray<FString>& FileList, bool& bContainsSource);

	static void ImportPendingPacks();
	
	/* Errors found when parsing manifest (if any) */
	TArray<FString>	ParseErrors;
	
	void BuildListOfAdditionalFiles(TArray<FString>& AdditionalFileSourceList,TArray<FString>& FileList, bool& bContainsSourceFiles);

	/* Get the identifier of the pack. */
	virtual FString GetIdent() const;

private:
	static void ParseAndImportPacks();
	bool LoadPakFileToBuffer(FPakPlatformFile& PakPlatformFile, FString Path, TArray<uint8>& Buffer);
	
	
	/*
	 * Extract the list of additional files defined in config file to an array
	 *
	 * @param ConfigFile	  config file as a string
	 * @param FileList		  array to receive list of files
	 * @param bContainsSource did the file list contain any source files
	 */
	bool ExtractListOfAdditionalFiles(const FString& ConfigFile, TArray<FString>& FileList,bool& bContainsSource);

	void RecordAndLogError(const FString& ErrorString);

	/* Load the images for the icon and screen shots directly from disk */
	bool LoadFeaturePackImageData();
	
	/* extract the images for the icon and screen shots from a pak file */
	bool LoadFeaturePackImageDataFromPackFile(FPakPlatformFile& PakPlatformFile);
	
	/* Parse the manifest string describing this pack file */
	bool ParseManifestString(const FString& ManifestString);

	/** Selects an FLocalizedText from an array which matches either the supplied language code, or the default language code. */
	FLocalizedTextArray ChooseLocalizedTextArray(TArray<FLocalizedTextArray> Choices, FString LanguageCode);
	FLocalizedText ChooseLocalizedText(TArray<FLocalizedText> Choices, FString LanguageCode);

	/* The path of the file we used to create this feature pack instance */
	FString FeaturePackPath;
	
	/* Array of localised names */
	TArray<FLocalizedText> LocalizedNames;
	
	/* Array of localised descriptions */
	TArray<FLocalizedText> LocalizedDescriptions;
	
	/* Defines the type of feature pack this is */
	EContentSourceCategory Category;
	
	/* Filename of the icon */
	FString IconFilename;
	
	/* Image data for the icon */
	TSharedPtr<FImageData> IconData;
	
	/* Filenames of the preview screenshots */
	TArray<TSharedPtr<FJsonValue>> ScreenshotFilenameArray;
	
	/* Image data of the preview screenshots */
	TArray<TSharedPtr<FImageData>> ScreenshotData;
	
	/* Array of localised assset type names */
	TArray<FLocalizedText> LocalizedAssetTypesList;
	
	/* Comma delimited string listing the class types */
	FString ClassTypes;
	
	/* true if the pack is valid */
	bool bPackValid;
	
	/* Asset to focus after loading the pack */
	FString FocusAssetIdent;
	
	/* Key used when sorting in the add dialog */
	FString SortKey;
	
	/* Tags searched when typing in the super search box */
	TArray<FLocalizedTextArray> LocalizedSearchTags;
	
	/* Other feature packs this pack needs (shared assets) */
	TArray<FFeaturePackLevelSet> AdditionalFeaturePacks;
		
	/* Additional files to copy when installing this pack */
	FFeatureAdditionalFiles AdditionalFilesForPack;
	
	/* Are the contents in a pack file or did we just read a manifest for the pack */
	bool bContentsInPakFile;

	/* Feature pack mount point */
	FString MountPoint;

	FString Identity;
	FString VersionNumber;
};
