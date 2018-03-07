// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "Engine/EngineTypes.h"
#include "PortableObjectPipeline.h"
#include "LocalizationTargetTypes.generated.h"

struct FPropertyChangedEvent;

UENUM()
enum class ELocalizationTargetConflictStatus : uint8
{
	/** The status of conflicts in this localization target could not be determined. */
	Unknown,
	/** The are outstanding conflicts present in this localization target. */
	ConflictsPresent,
	/** The localization target is clear of conflicts. */
	Clear,
};

USTRUCT()
struct FGatherTextSearchDirectory
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(config, EditAnywhere, Category="Path")
	FString Path;

	LOCALIZATION_API bool Validate(const FString& RootDirectory, FText& OutError) const;
};

USTRUCT()
struct FGatherTextIncludePath
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(config, EditAnywhere, Category="Pattern")
	FString Pattern;

	LOCALIZATION_API bool Validate(const FString& RootDirectory, FText& OutError) const;
};

USTRUCT()
struct FGatherTextExcludePath
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(config, EditAnywhere, Category="Pattern")
	FString Pattern;

	LOCALIZATION_API bool Validate(FText& OutError) const;
};

USTRUCT()
struct FGatherTextFileExtension
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(config, EditAnywhere, Category="Pattern")
	FString Pattern;

	LOCALIZATION_API bool Validate(FText& OutError) const;
};

USTRUCT()
struct FGatherTextFromTextFilesConfiguration
{
	GENERATED_USTRUCT_BODY()

	LOCALIZATION_API static const TArray<FGatherTextFileExtension>& GetDefaultTextFileExtensions();

	FGatherTextFromTextFilesConfiguration()
		: IsEnabled(true)
		, FileExtensions(GetDefaultTextFileExtensions())
		, ShouldGatherFromEditorOnlyData(false)
	{
	}

	/* If enabled, text from text files will be gathered according to this configuration. */
	UPROPERTY(config, EditAnywhere, Category = "Execution")
	bool IsEnabled;

	/* The paths of directories to be searched recursively for text files, specified relative to the project's root, which may be parsed for text to gather. */
	UPROPERTY(config, EditAnywhere, Category = "Filter")
	TArray<FGatherTextSearchDirectory> SearchDirectories;

	/* Text files whose paths match these wildcard patterns will be excluded from gathering. */
	UPROPERTY(config, EditAnywhere, Category = "Filter")
	TArray<FGatherTextExcludePath> ExcludePathWildcards;

	/* Text files whose names match these wildcard patterns may be parsed for text to gather. */
	UPROPERTY(config, EditAnywhere, Category = "Filter")
	TArray<FGatherTextFileExtension> FileExtensions;

	/* If enabled, data that is specified as editor-only may be processed for gathering. */
	UPROPERTY(config, EditAnywhere, Category = "Filter")
	bool ShouldGatherFromEditorOnlyData;

	LOCALIZATION_API bool Validate(const FString& RootDirectory, FText& OutError) const;
};


USTRUCT()
struct FGatherTextFromPackagesConfiguration
{
	GENERATED_USTRUCT_BODY()

	LOCALIZATION_API static const TArray<FGatherTextFileExtension>& GetDefaultPackageFileExtensions();

	FGatherTextFromPackagesConfiguration()
		: IsEnabled(true)
		, FileExtensions(GetDefaultPackageFileExtensions())
		, ShouldGatherFromEditorOnlyData(false)
		, SkipGatherCache(false)
	{
		{
			FGatherTextExcludePath L10NPath;
			L10NPath.Pattern = TEXT("Content/L10N/*");
			ExcludePathWildcards.Add(L10NPath);
		}
	}

	/* If enabled, text from packages will be gathered according to this configuration. */
	UPROPERTY(config, EditAnywhere, Category = "Execution")
	bool IsEnabled;

	/* Packages whose paths match these wildcard patterns, specified relative to the project's root, may be processed for gathering. */
	UPROPERTY(config, EditAnywhere, Category = "Filter")
	TArray<FGatherTextIncludePath> IncludePathWildcards;

	/* Packages whose paths match these wildcard patterns will be excluded from gathering. */
	UPROPERTY(config, EditAnywhere, Category = "Filter")
	TArray<FGatherTextExcludePath> ExcludePathWildcards;

	/* Packages whose names match these wildcard patterns may be processed for text to gather. */
	UPROPERTY(config, EditAnywhere, Category = "Filter")
	TArray<FGatherTextFileExtension> FileExtensions;

	/* Packages in these collections may be processed for gathering. */
	UPROPERTY(config, EditAnywhere, Category = "Filter")
	TArray<FName> Collections;

	/* If enabled, data that is specified as editor-only may be processed for gathering. */
	UPROPERTY(config, EditAnywhere, Category = "Gather Text")
	bool ShouldGatherFromEditorOnlyData;

	/* Should we ignore the cached text in the package header and perform a full package load instead? */
	UPROPERTY(config, EditAnywhere, Category = "Gather Text", AdvancedDisplay)
	bool SkipGatherCache;

	LOCALIZATION_API bool Validate(const FString& RootDirectory, FText& OutError) const;
};

USTRUCT()
struct FMetaDataTextKeyPattern
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(config, EditAnywhere, Category="Pattern")
	FString Pattern;

	LOCALIZATION_API bool Validate(FText& OutError) const;

	LOCALIZATION_API static const TArray<FString>& GetPossiblePlaceHolders();
};

USTRUCT()
struct FMetaDataKeyName
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(config, EditAnywhere, Category="Name")
	FString Name;

	LOCALIZATION_API bool Validate(FText& OutError) const;
};

USTRUCT()
struct FMetaDataKeyGatherSpecification
{
	GENERATED_USTRUCT_BODY()

	/*  The metadata key for which values will be gathered as text. */
	UPROPERTY(config, EditAnywhere, Category = "Input")
	FMetaDataKeyName MetaDataKey;

	/* The localization namespace in which the gathered text will be output. */
	UPROPERTY(config, EditAnywhere, Category = "Output")
	FString TextNamespace;

	/* The pattern which will be formatted to form the localization key for the metadata value gathered as text.
	Placeholder - Description
	{FieldPath} - The fully qualified name of the object upon which the metadata resides.
	{MetaDataValue} - The value associated with the metadata key. */
	UPROPERTY(config, EditAnywhere, Category = "Output")
	FMetaDataTextKeyPattern TextKeyPattern;

	LOCALIZATION_API bool Validate(FText& OutError) const;
};

USTRUCT()
struct FGatherTextFromMetaDataConfiguration
{
	GENERATED_USTRUCT_BODY()

	FGatherTextFromMetaDataConfiguration()
		: IsEnabled(false)
		, ShouldGatherFromEditorOnlyData(false)
	{
	}

	/* If enabled, metadata will be gathered according to this configuration. */
	UPROPERTY(config, EditAnywhere, Category = "Execution")
	bool IsEnabled;

	/* Metadata from source files whose paths match these wildcard patterns, specified relative to the project's root, may be processed for gathering. */
	UPROPERTY(config, EditAnywhere, Category = "Filter")
	TArray<FGatherTextIncludePath> IncludePathWildcards;

	/* Metadata from source files whose paths match these wildcard patterns will be excluded from gathering. */
	UPROPERTY(config, EditAnywhere, Category = "Filter")
	TArray<FGatherTextExcludePath> ExcludePathWildcards;

	/* Specifications for how to gather text from specific metadata keys. */
	UPROPERTY(config, EditAnywhere, Category = "MetaData")
	TArray<FMetaDataKeyGatherSpecification> KeySpecifications;

	/* If enabled, data that is specified as editor-only may be processed for gathering. */
	UPROPERTY(config, EditAnywhere, Category = "Filter")
	bool ShouldGatherFromEditorOnlyData;

	LOCALIZATION_API bool Validate(const FString& RootDirectory, FText& OutError) const;
};

USTRUCT()
struct FLocalizationExportingSettings
{
	GENERATED_BODY()

	FLocalizationExportingSettings()
		: CollapseMode(ELocalizedTextCollapseMode::IdenticalTextIdAndSource)
		, ShouldPersistCommentsOnExport(false)
		, ShouldAddSourceLocationsAsComments(true)
	{
	}

	/* How should we collapse down text when exporting to PO? */
	UPROPERTY(config, EditAnywhere, Category = "Collapsing")
	ELocalizedTextCollapseMode CollapseMode;

	/* Should user comments in existing PO files be persisted after export? Useful if using a third party service that stores editor/translator notes in the PO format's comment fields. */
	UPROPERTY(config, EditAnywhere, Category = "Comments")
	bool ShouldPersistCommentsOnExport;

	/* Should source locations be added to PO file entries as comments? Useful if a third party service doesn't expose PO file reference comments, which typically store the source location. */
	UPROPERTY(config, EditAnywhere, Category = "Comments")
	bool ShouldAddSourceLocationsAsComments;
};

USTRUCT()
struct FLocalizationCompilationSettings
{
	GENERATED_BODY()

	FLocalizationCompilationSettings()
		: SkipSourceCheck(false)
	{
	}

	/* Should we skip the source check when compiling translations? This will allow translations whose source no longer matches the active source to still be used by the game at runtime. */
	UPROPERTY(config, EditAnywhere, Category="Source")
	bool SkipSourceCheck;
};

USTRUCT()
struct FLocalizationImportDialogueSettings
{
	GENERATED_BODY()

	FLocalizationImportDialogueSettings()
		: ImportedDialogueFolder(TEXT("ImportedDialogue"))
		, bImportNativeAsSource(false)
	{
	}

	/** Path to the folder to import the audio from. This folder is expected to contain culture sub-folders, which in turn contain the raw WAV files to import. */
	UPROPERTY(config, EditAnywhere, Category="Dialogue")
	FDirectoryPath RawAudioPath;

	/** Folder in which to create the generated sound waves. This is relative to the root of the L10N culture folder (or the root content folder if importing native dialogue as source dialogue). */
	UPROPERTY(config, EditAnywhere, Category="Dialogue")
	FString ImportedDialogueFolder;

	/** Should the dialogue for the native culture be imported as if it were source audio? If false, the native culture dialogue will be imported as localized data for the native culture. */
	UPROPERTY(config, EditAnywhere, Category="Dialogue")
	bool bImportNativeAsSource;
};

USTRUCT()
struct FCultureStatistics
{
	GENERATED_USTRUCT_BODY()

	FCultureStatistics()
		: WordCount(0)
	{
	}

	FCultureStatistics(const FString& InCultureName)
		: CultureName(InCultureName)
		, WordCount(0)
	{
	}


	/* The ISO name for this culture. */
	UPROPERTY(config, EditAnywhere, Category = "Culture")
	FString CultureName;

	/* The estimated number of words that have been localized for this culture. */
	UPROPERTY(Transient, EditAnywhere, Category = "Statistics")
	uint32 WordCount;
};

UENUM()
enum class ELocalizationTargetLoadingPolicy : uint8
{
	/** This target's localization data will never be loaded automatically. */
	Never,
	/** This target's localization data will always be loaded automatically. */
	Always,
	/** This target's localization data will only be loaded when running the editor. Use if this target localizes the editor. */
	Editor,
	/** This target's localization data will only be loaded when running the game. Use if this target localizes your game. */
	Game,
	/** This target's localization data will only be loaded if the editor is displaying localized property names. */
	PropertyNames,
	/** This target's localization data will only be loaded if the editor is displaying localized tool tips. */
	ToolTips,
};

USTRUCT()
struct FLocalizationTargetSettings
{
	GENERATED_USTRUCT_BODY()

	FLocalizationTargetSettings()
		: Guid(FGuid::NewGuid())
		, ConflictStatus(ELocalizationTargetConflictStatus::Unknown)
		, NativeCultureIndex(INDEX_NONE)
	{
	}

	/* Unique name for the target. */
	UPROPERTY(config, EditAnywhere, Category = "Target")
	FString Name;

	UPROPERTY(config)
	FGuid Guid;

	/* Whether the target has outstanding conflicts that require resolution. */
	UPROPERTY(Transient, EditAnywhere, Category = "Target")
	ELocalizationTargetConflictStatus ConflictStatus;

	/* Text present in these targets will not be duplicated in this target. */
	UPROPERTY(config, EditAnywhere, Category = "Gather Text")
	TArray<FGuid> TargetDependencies;

	/* Text present in these manifests will not be duplicated in this target. */
	UPROPERTY(config, EditAnywhere, Category = "Gather Text", AdvancedDisplay, meta=(FilePathFilter="manifest"))
	TArray<FFilePath> AdditionalManifestDependencies;

	/* The names of modules which must be loaded when gathering text. Use to gather from packages or metadata sourced from a module that isn't the primary game module. */
	UPROPERTY(config, EditAnywhere, Category = "Gather Text", AdvancedDisplay)
	TArray<FString> RequiredModuleNames;

	/* Parameters for defining what text is gathered from text files and how. */
	UPROPERTY(config, EditAnywhere, Category = "Gather Text")
	FGatherTextFromTextFilesConfiguration GatherFromTextFiles;

	/* Parameters for defining what text is gathered from packages and how. */
	UPROPERTY(config, EditAnywhere, Category = "Gather Text")
	FGatherTextFromPackagesConfiguration GatherFromPackages;

	/* Parameters for defining what text is gathered from metadata and how. */
	UPROPERTY(config, EditAnywhere, Category = "Gather Text")
	FGatherTextFromMetaDataConfiguration GatherFromMetaData;

	/* Settings for exporting translations. */
	UPROPERTY(config, EditAnywhere, Category = "Export Text", meta=(ShowOnlyInnerProperties))
	FLocalizationExportingSettings ExportSettings;

	/* Settings for compiling translations. */
	UPROPERTY(config, EditAnywhere, Category = "Compile Text", meta=(ShowOnlyInnerProperties))
	FLocalizationCompilationSettings CompileSettings;

	/* Settings for importing dialogue from WAV files. */
	UPROPERTY(config, EditAnywhere, Category = "Import Dialogue", meta=(ShowOnlyInnerProperties))
	FLocalizationImportDialogueSettings ImportDialogueSettings;

	/* The index of the native culture among the supported cultures. */
	UPROPERTY(config, EditAnywhere, Category = "Cultures")
	int32 NativeCultureIndex;

	/* Cultures for which the source text is being localized for.*/
	UPROPERTY(config, EditAnywhere, Category = "Cultures")
	TArray<FCultureStatistics> SupportedCulturesStatistics;
};

UCLASS(Within=LocalizationTargetSet)
class LOCALIZATION_API ULocalizationTarget : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Target", meta=(ShowOnlyInnerProperties))
	FLocalizationTargetSettings Settings;

public:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	bool IsMemberOfEngineTargetSet() const;
	bool UpdateWordCountsFromCSV();
	void UpdateStatusFromConflictReport();
	bool RenameTargetAndFiles(const FString& NewName);
	bool DeleteFiles(const FString* const Culture = nullptr) const;
};

UCLASS(Within=LocalizationSettings)
class LOCALIZATION_API ULocalizationTargetSet : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Targets")
	TArray<ULocalizationTarget*> TargetObjects;

public:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
