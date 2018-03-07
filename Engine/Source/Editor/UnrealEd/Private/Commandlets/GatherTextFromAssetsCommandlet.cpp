// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Commandlets/GatherTextFromAssetsCommandlet.h"
#include "UObject/Class.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/OutputDeviceHelper.h"
#include "Misc/FeedbackContext.h"
#include "UObject/EditorObjectVersion.h"
#include "Modules/ModuleManager.h"
#include "Serialization/PropertyLocalizationDataGathering.h"
#include "Misc/PackageName.h"
#include "UObject/PackageFileSummary.h"
#include "Framework/Commands/Commands.h"
#include "Commandlets/GatherTextFromSourceCommandlet.h"
#include "Templates/ScopedPointer.h"
#include "AssetData.h"
#include "Sound/DialogueWave.h"
#include "ARFilter.h"
#include "AssetRegistryModule.h"
#include "PackageHelperFunctions.h"
#include "UniquePtr.h"
#include "CollectionManagerModule.h"
#include "ICollectionManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGatherTextFromAssetsCommandlet, Log, All);

/** Special feedback context used to stop the commandlet to reporting failure due to a package load error */
class FLoadPackageLogOutputRedirector : public FFeedbackContext
{
public:
	struct FScopedCapture
	{
		FScopedCapture(FLoadPackageLogOutputRedirector* InLogOutputRedirector, const FString& InPackageContext)
			: LogOutputRedirector(InLogOutputRedirector)
		{
			LogOutputRedirector->BeginCapturingLogData(InPackageContext);
		}

		~FScopedCapture()
		{
			LogOutputRedirector->EndCapturingLogData();
		}

		FLoadPackageLogOutputRedirector* LogOutputRedirector;
	};

	FLoadPackageLogOutputRedirector()
		: ErrorCount(0)
		, WarningCount(0)
		, FormattedErrorsAndWarningsList()
		, PackageContext()
		, OriginalWarningContext(nullptr)
	{
	}

	virtual ~FLoadPackageLogOutputRedirector()
	{
	}

	void BeginCapturingLogData(const FString& InPackageContext)
	{
		// Override GWarn so that we can capture any log data
		check(!OriginalWarningContext);
		OriginalWarningContext = GWarn;
		GWarn = this;

		PackageContext = InPackageContext;

		// Reset the counts and previous log output
		ErrorCount = 0;
		WarningCount = 0;
		FormattedErrorsAndWarningsList.Reset();
	}

	void EndCapturingLogData()
	{
		// Restore the original GWarn now that we've finished capturing log data
		check(OriginalWarningContext);
		GWarn = OriginalWarningContext;
		OriginalWarningContext = nullptr;

		// Report any messages, and also report a warning if we silenced some warnings or errors when loading
		if (ErrorCount > 0 || WarningCount > 0)
		{
			static const FString LogIndentation = TEXT("    ");

			UE_LOG(LogGatherTextFromAssetsCommandlet, Log, TEXT("Package '%s' produced %d error(s) and %d warning(s) while loading. Please verify that your text has gathered correctly."), *PackageContext, ErrorCount, WarningCount);

			GWarn->Log(NAME_None, ELogVerbosity::Log, FString::Printf(TEXT("The following errors and warnings were reported while loading '%s':"), *PackageContext));
			for (const auto& FormattedOutput : FormattedErrorsAndWarningsList)
			{
				GWarn->Log(NAME_None, ELogVerbosity::Log, LogIndentation + FormattedOutput);
			}
		}
	}

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		if (Verbosity == ELogVerbosity::Error)
		{
			++ErrorCount;
			FormattedErrorsAndWarningsList.Add(FOutputDeviceHelper::FormatLogLine(Verbosity, Category, V));
		}
		else if (Verbosity == ELogVerbosity::Warning)
		{
			++WarningCount;
			FormattedErrorsAndWarningsList.Add(FOutputDeviceHelper::FormatLogLine(Verbosity, Category, V));
		}
		else
		{
			// Pass anything else on to GWarn so that it can handle them appropriately
			OriginalWarningContext->Serialize(V, Verbosity, Category);
		}
	}

private:
	int32 ErrorCount;
	int32 WarningCount;
	TArray<FString> FormattedErrorsAndWarningsList;

	FString PackageContext;
	FFeedbackContext* OriginalWarningContext;
};

#define LOC_DEFINE_REGION

//////////////////////////////////////////////////////////////////////////
//UGatherTextFromAssetsCommandlet

const FString UGatherTextFromAssetsCommandlet::UsageText
(
	TEXT("GatherTextFromAssetsCommandlet usage...\r\n")
	TEXT("    <GameName> UGatherTextFromAssetsCommandlet -root=<parsed code root folder> -exclude=<paths to exclude>\r\n")
	TEXT("    \r\n")
	TEXT("    <paths to include> Paths to include. Delimited with ';'. Accepts wildcards. eg \"*Content/Developers/*;*/TestMaps/*\" OPTIONAL: If not present, everything will be included. \r\n")
	TEXT("    <paths to exclude> Paths to exclude. Delimited with ';'. Accepts wildcards. eg \"*Content/Developers/*;*/TestMaps/*\" OPTIONAL: If not present, nothing will be excluded.\r\n")
);

UGatherTextFromAssetsCommandlet::UGatherTextFromAssetsCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bSkipGatherCache(false)
	, bFixBroken(false)
	, ShouldGatherFromEditorOnlyData(false)
	, ShouldExcludeDerivedClasses(false)
{
}

void UGatherTextFromAssetsCommandlet::ProcessGatherableTextDataArray(const FString& PackageFilePath, const TArray<FGatherableTextData>& GatherableTextDataArray)
{
	for (const FGatherableTextData& GatherableTextData : GatherableTextDataArray)
	{
		for (const FTextSourceSiteContext& TextSourceSiteContext : GatherableTextData.SourceSiteContexts)
		{
			if (!TextSourceSiteContext.IsEditorOnly || ShouldGatherFromEditorOnlyData)
			{
				if (TextSourceSiteContext.KeyName.IsEmpty())
				{
					UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("Detected missing key on asset \"%s\"."), *TextSourceSiteContext.SiteDescription);
					continue;
				}

				static const FLocMetadataObject DefaultMetadataObject;

				FManifestContext Context;
				Context.Key = TextSourceSiteContext.KeyName;
				Context.KeyMetadataObj = !(FLocMetadataObject::IsMetadataExactMatch(&TextSourceSiteContext.KeyMetaData, &DefaultMetadataObject)) ? MakeShareable(new FLocMetadataObject(TextSourceSiteContext.KeyMetaData)) : nullptr;
				Context.InfoMetadataObj = !(FLocMetadataObject::IsMetadataExactMatch(&TextSourceSiteContext.InfoMetaData, &DefaultMetadataObject)) ? MakeShareable(new FLocMetadataObject(TextSourceSiteContext.InfoMetaData)) : nullptr;
				Context.bIsOptional = TextSourceSiteContext.IsOptional;
				Context.SourceLocation = TextSourceSiteContext.SiteDescription;

				FLocItem Source(GatherableTextData.SourceData.SourceString);

				GatherManifestHelper->AddSourceText(GatherableTextData.NamespaceName, Source, Context, &TextSourceSiteContext.SiteDescription);
			}
		}
	}
}

void UGatherTextFromAssetsCommandlet::ProcessPackages(const TArray< UPackage* >& PackagesToProcess)
{
	TArray<FGatherableTextData> GatherableTextDataArray;

	for (const UPackage* const Package : PackagesToProcess)
	{
		GatherableTextDataArray.Reset();

		// Gathers from the given package
		EPropertyLocalizationGathererResultFlags GatherableTextResultFlags = EPropertyLocalizationGathererResultFlags::Empty;
		FPropertyLocalizationDataGatherer(GatherableTextDataArray, Package, GatherableTextResultFlags);

		ProcessGatherableTextDataArray(Package->FileName.ToString(), GatherableTextDataArray);
	}
}

int32 UGatherTextFromAssetsCommandlet::Main(const FString& Params)
{
	// Parse command line.
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);

	FString GatherTextConfigPath;
	FString SectionName;
	if (!GetConfigurationScript(ParamVals, GatherTextConfigPath, SectionName))
	{
		return -1;
	}

	if (!ConfigureFromScript(GatherTextConfigPath, SectionName))
	{
		return -1;
	}

	// Add any manifest dependencies if they were provided
	{
		bool HasFailedToAddManifestDependency = false;
		for (const FString& ManifestDependency : ManifestDependenciesList)
		{
			FText OutError;
			if (!GatherManifestHelper->AddDependency(ManifestDependency, &OutError))
			{
				UE_LOG(LogGatherTextFromAssetsCommandlet, Error, TEXT("The GatherTextFromAssets commandlet couldn't load the specified manifest dependency: '%'. %s"), *ManifestDependency, *OutError.ToString());
				HasFailedToAddManifestDependency = true;
			}
		}
		if (HasFailedToAddManifestDependency)
		{
			return -1;
		}
	}

	// Preload necessary modules.
	{
		bool HasFailedToPreloadAnyModules = false;
		for (const FString& ModuleName : ModulesToPreload)
		{
			EModuleLoadResult ModuleLoadResult;
			FModuleManager::Get().LoadModuleWithFailureReason(*ModuleName, ModuleLoadResult);

			if (ModuleLoadResult != EModuleLoadResult::Success)
			{
				HasFailedToPreloadAnyModules = true;
				continue;
			}
		}

		if (HasFailedToPreloadAnyModules)
		{
			return -1;
		}
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	AssetRegistry.SearchAllAssets(true);
	TArray<FAssetData> AssetDataArray;

	{
		FARFilter FirstPassFilter;

		// Filter object paths to only those in any of the specified collections.
		{
			bool HasFailedToGetACollection = false;
			FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
			ICollectionManager& CollectionManager = CollectionManagerModule.Get();
			for (const FString& CollectionName : CollectionFilters)
			{
				if (!CollectionManager.GetObjectsInCollection(FName(*CollectionName), ECollectionShareType::CST_All, FirstPassFilter.ObjectPaths, ECollectionRecursionFlags::SelfAndChildren))
				{
					UE_LOG(LogGatherTextFromAssetsCommandlet, Error, TEXT("Failed get objects in specified collection: %s"), *CollectionName);
					HasFailedToGetACollection = true;
				}
			}
			if (HasFailedToGetACollection)
			{
				return -1;
			}
		}

		// Filter out any objects of the specified classes and their children at this point.
		if (ShouldExcludeDerivedClasses)
		{
			FirstPassFilter.bRecursiveClasses = true;
			for (const FString& ExcludeClassName : ExcludeClassNames)
			{
				// Note: Can't necessarily validate these class names here, as the class may be a generated blueprint class that hasn't been loaded yet.
				FirstPassFilter.RecursiveClassesExclusionSet.Add(*ExcludeClassName);
			}
		}

		// Apply filter if valid to do so, get all assets otherwise.
		if (FirstPassFilter.IsEmpty())
		{
			AssetRegistry.GetAllAssets(AssetDataArray);
		}
		else
		{
			AssetRegistry.GetAssets(FirstPassFilter, AssetDataArray);
		}
	}

	if (!ShouldExcludeDerivedClasses)
	{
		// Filter out any objects of the specified classes.
		FARFilter ExcludeExactClassesFilter;
		ExcludeExactClassesFilter.bRecursiveClasses = false;
		for (const FString& ExcludeClassName : ExcludeClassNames)
		{
			// Note: Can't necessarily validate these class names here, as the class may be a generated blueprint class that hasn't been loaded yet.
			ExcludeExactClassesFilter.ClassNames.Add(*ExcludeClassName);
		}

		// Reapply filter over the current set of assets.
		if (!ExcludeExactClassesFilter.IsEmpty())
		{
			// NOTE: The filter applied is actually the inverse, due to API limitations, so the resultant set must be removed from the current set.
			TArray<FAssetData> AssetsToExclude = AssetDataArray;
			AssetRegistry.RunAssetsThroughFilter(AssetsToExclude, ExcludeExactClassesFilter);
			AssetDataArray.RemoveAll([&](const FAssetData& AssetData)
			{
				return AssetsToExclude.Contains(AssetData);
			});
		}
	}

	// Note: AssetDataArray now contains all assets in the specified collections that are not instances of the specified excluded classes.

	const FFuzzyPathMatcher FuzzyPathMatcher = FFuzzyPathMatcher(IncludePathFilters, ExcludePathFilters);
	AssetDataArray.RemoveAll([&](const FAssetData& PartiallyFilteredAssetData) -> bool
	{
		FString PackageFilePath;
		if (!FPackageName::FindPackageFileWithoutExtension(FPackageName::LongPackageNameToFilename(PartiallyFilteredAssetData.PackageName.ToString()), PackageFilePath))
		{
			return true;
		}
		PackageFilePath = FPaths::ConvertRelativePathToFull(PackageFilePath);
		const FString PackageFileName = FPaths::GetCleanFilename(PackageFilePath);

		// Filter out assets whose package file names DO NOT match any of the package file name filters.
		{
			bool HasPassedAnyFileNameFilter = false;
			for (const FString& PackageFileNameFilter : PackageFileNameFilters)
			{
				if (PackageFileName.MatchesWildcard(PackageFileNameFilter))
				{
					HasPassedAnyFileNameFilter = true;
					break;
				}
			}
			if (!HasPassedAnyFileNameFilter)
			{
				return true;
			}
		}

		// Filter out assets whose package file paths do not pass the "fuzzy path" filters.
		if (FuzzyPathMatcher.TestPath(PackageFilePath) != FFuzzyPathMatcher::Included)
		{
			return true;
		}

		return false;
	});

	if (AssetDataArray.Num() == 0)
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("No assets matched the specified criteria."));
		return 0;
	}

	// Collect the names of all the packages that need to be processed.
	TArray<FString> FilePathsOfPackagesToProcess;
	for (const FAssetData& AssetData : AssetDataArray)
	{
		FString PackageFilePath;
		if (!FPackageName::FindPackageFileWithoutExtension(FPackageName::LongPackageNameToFilename(AssetData.PackageName.ToString()), PackageFilePath))
		{
			continue;
		}
		PackageFilePath = FPaths::ConvertRelativePathToFull(PackageFilePath);
		FilePathsOfPackagesToProcess.AddUnique(PackageFilePath);
	}
	AssetDataArray.Empty();

	// Process all packages that do not need to be loaded. Remove processed packages from the list.
	FilePathsOfPackagesToProcess.RemoveAll([&](const FString& PackageFilePath) -> bool
	{
		TUniquePtr<FArchive> FileReader(IFileManager::Get().CreateFileReader(*PackageFilePath));
		if (!FileReader)
		{
			return false;
		}

		// Read package file summary from the file.
		FPackageFileSummary PackageFileSummary;
		*FileReader << PackageFileSummary;

		bool MustLoadForGather = false;

		// Have we been asked to skip the cache of text that exists in the header of newer packages?
		if (bSkipGatherCache && PackageFileSummary.GetFileVersionUE4() >= VER_UE4_SERIALIZE_TEXT_IN_PACKAGES)
		{
			// Fallback on the old package flag check.
			if (PackageFileSummary.PackageFlags & PKG_RequiresLocalizationGather)
			{
				MustLoadForGather = true;
			}
		}

		const FCustomVersion* const EditorVersion = PackageFileSummary.GetCustomVersionContainer().GetVersion(FEditorObjectVersion::GUID);

		// Packages not resaved since localization gathering flagging was added to packages must be loaded.
		if (PackageFileSummary.GetFileVersionUE4() < VER_UE4_PACKAGE_REQUIRES_LOCALIZATION_GATHER_FLAGGING)
		{
			MustLoadForGather = true;
		}
		// Package not resaved since gatherable text data was added to package headers must be loaded, since their package header won't contain pregathered text data.
		else if (PackageFileSummary.GetFileVersionUE4() < VER_UE4_SERIALIZE_TEXT_IN_PACKAGES || (!EditorVersion || EditorVersion->Version < FEditorObjectVersion::GatheredTextEditorOnlyPackageLocId))
		{
			// Fallback on the old package flag check.
			if (PackageFileSummary.PackageFlags & PKG_RequiresLocalizationGather)
			{
				MustLoadForGather = true;
			}
		}
		else if (PackageFileSummary.GetFileVersionUE4() < VER_UE4_DIALOGUE_WAVE_NAMESPACE_AND_CONTEXT_CHANGES)
		{
			const FString LongPackageName = FPackageName::FilenameToLongPackageName(PackageFilePath);
			TArray<FAssetData> AllAssetDataInSamePackage;
			AssetRegistry.GetAssetsByPackageName(*LongPackageName, AllAssetDataInSamePackage);
			for (const FAssetData& AssetData : AllAssetDataInSamePackage)
			{
				if (AssetData.AssetClass == UDialogueWave::StaticClass()->GetFName())
				{
					MustLoadForGather = true;
				}
			}
		}

		// If this package doesn't have any cached data, then we have to load it for gather
		if (PackageFileSummary.GetFileVersionUE4() >= VER_UE4_SERIALIZE_TEXT_IN_PACKAGES && PackageFileSummary.GatherableTextDataOffset == 0 && (PackageFileSummary.PackageFlags & PKG_RequiresLocalizationGather))
		{
			MustLoadForGather = true;
		}

		if (MustLoadForGather)
		{
			return false;
		}

		// Process packages that don't require loading to process.
		if (PackageFileSummary.GatherableTextDataOffset > 0)
		{
			FileReader->Seek(PackageFileSummary.GatherableTextDataOffset);

			TArray<FGatherableTextData> GatherableTextDataArray;
			GatherableTextDataArray.SetNum(PackageFileSummary.GatherableTextDataCount);

			for (int32 GatherableTextDataIndex = 0; GatherableTextDataIndex < PackageFileSummary.GatherableTextDataCount; ++GatherableTextDataIndex)
			{
				(*FileReader) << GatherableTextDataArray[GatherableTextDataIndex];
			}

			ProcessGatherableTextDataArray(PackageFilePath, GatherableTextDataArray);
		}

		return true;
	});

	// Collect garbage before beginning to load packages for processing.
	CollectGarbage(RF_NoFlags);

	const int32 PackagesPerBatchCount = 100;
	const int32 PackageCount = FilePathsOfPackagesToProcess.Num();
	const int32 BatchCount = PackageCount / PackagesPerBatchCount + (PackageCount % PackagesPerBatchCount > 0 ? 1 : 0); // Add an extra batch for any remainder if necessary.
	if (PackageCount > 0)
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Log, TEXT("Loading %i packages in %i batches of %i."), PackageCount, BatchCount, PackagesPerBatchCount);
	}
	FLoadPackageLogOutputRedirector LogOutputRedirector;

	//Now go through the remaining packages in the main array and process them in batches.
	TArray< UPackage* > LoadedPackages;
	TArray< FString > LoadedPackageFileNames;
	TArray< FString > FailedPackageFileNames;
	TArray< UPackage* > LoadedPackagesToProcess;

	//Load the packages in batches
	int32 PackageIndex = 0;
	for (int32 BatchIndex = 0; BatchIndex < BatchCount; ++BatchIndex)
	{
		int32 PackagesInThisBatch = 0;
		int32 FailuresInThisBatch = 0;
		for (; PackageIndex < PackageCount && PackagesInThisBatch < PackagesPerBatchCount; ++PackageIndex)
		{
			const FString& PackageFileName = FilePathsOfPackagesToProcess[PackageIndex];

			UE_LOG(LogGatherTextFromAssetsCommandlet, Verbose, TEXT("Loading package: '%s'."), *PackageFileName);

			UPackage* Package = nullptr;
			{
				FString LongPackageName;
				if (!FPackageName::TryConvertFilenameToLongPackageName(PackageFileName, LongPackageName))
				{
					LongPackageName = FPaths::GetCleanFilename(PackageFileName);
				}

				FLoadPackageLogOutputRedirector::FScopedCapture ScopedCapture(&LogOutputRedirector, LongPackageName);
				Package = LoadPackage(NULL, *PackageFileName, LOAD_NoWarn | LOAD_Quiet);
			}

			if (Package)
			{
				// Because packages may not have been resaved after this flagging was implemented, we may have added packages to load that weren't flagged - potential false positives.
				// The loading process should have reflagged said packages so that only true positives will have this flag.
				if (Package->RequiresLocalizationGather())
				{
					LoadedPackagesToProcess.Add(Package);
				}
			}
			else
			{
				FailedPackageFileNames.Add(PackageFileName);
				++FailuresInThisBatch;
				continue;
			}

			++PackagesInThisBatch;
		}

		UE_LOG(LogGatherTextFromAssetsCommandlet, Log, TEXT("Loaded %i packages in batch %i of %i. %i failed."), PackagesInThisBatch, BatchIndex + 1, BatchCount, FailuresInThisBatch);

		ProcessPackages(LoadedPackagesToProcess);
		LoadedPackagesToProcess.Empty(PackagesPerBatchCount);

		if (bFixBroken)
		{
			for (int32 LoadedPackageIndex = 0; LoadedPackageIndex < LoadedPackages.Num(); ++LoadedPackageIndex)
			{
				UPackage *Package = LoadedPackages[LoadedPackageIndex];
				const FString PackageName = LoadedPackageFileNames[LoadedPackageIndex];

				//Todo - link with source control.
				if (Package)
				{
					if (Package->IsDirty())
					{
						if (SavePackageHelper(Package, *PackageName))
						{
							UE_LOG(LogGatherTextFromAssetsCommandlet, Log, TEXT("Saved Package %s."), *PackageName);
						}
						else
						{
							//TODO - Work out how to integrate with source control. The code from the source gatherer doesn't work.
							UE_LOG(LogGatherTextFromAssetsCommandlet, Log, TEXT("Could not save package %s. Probably due to source control. "), *PackageName);
						}
					}
				}
				else
				{
					UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("Failed to find one of the loaded packages."));
				}
			}
		}

		CollectGarbage(RF_NoFlags);
		LoadedPackages.Empty(PackagesPerBatchCount);
		LoadedPackageFileNames.Empty(PackagesPerBatchCount);
	}

	return 0;
}

bool UGatherTextFromAssetsCommandlet::GetConfigurationScript(const TMap<FString, FString>& InCommandLineParameters, FString& OutFilePath, FString& OutStepSectionName)
{
	//Set config file
	const FString* ParamVal = InCommandLineParameters.Find(FString(TEXT("Config")));
	if (ParamVal)
	{
		OutFilePath = *ParamVal;
	}
	else
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Error, TEXT("No config specified."));
		return false;
	}

	//Set config section
	ParamVal = InCommandLineParameters.Find(FString(TEXT("Section")));
	if (ParamVal)
	{
		OutStepSectionName = *ParamVal;
	}
	else
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Error, TEXT("No config section specified."));
		return false;
	}

	return true;
}

bool UGatherTextFromAssetsCommandlet::ConfigureFromScript(const FString& GatherTextConfigPath, const FString& SectionName)
{
	bool HasFatalError = false;

	// Modules to Preload
	GetStringArrayFromConfig(*SectionName, TEXT("ModulesToPreload"), ModulesToPreload, GatherTextConfigPath);

	// IncludePathFilters
	GetPathArrayFromConfig(*SectionName, TEXT("IncludePathFilters"), IncludePathFilters, GatherTextConfigPath);

	// IncludePaths (DEPRECATED)
	{
		TArray<FString> IncludePaths;
		GetPathArrayFromConfig(*SectionName, TEXT("IncludePaths"), IncludePaths, GatherTextConfigPath);
		if (IncludePaths.Num())
		{
			IncludePathFilters.Append(IncludePaths);
			UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("IncludePaths detected in section %s. IncludePaths is deprecated, please use IncludePathFilters."), *SectionName);
		}
	}

	if (IncludePathFilters.Num() == 0)
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Error, TEXT("No include path filters in section %s."), *SectionName);
		HasFatalError = true;
	}

	// Collections
	GetStringArrayFromConfig(*SectionName, TEXT("CollectionFilters"), CollectionFilters, GatherTextConfigPath);
	for (const FString& CollectionName : CollectionFilters)
	{
		FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
		ICollectionManager& CollectionManager = CollectionManagerModule.Get();

		const bool DoesCollectionExist = CollectionManager.CollectionExists(FName(*CollectionName), ECollectionShareType::CST_All);
		if (!DoesCollectionExist)
		{
			UE_LOG(LogGatherTextFromAssetsCommandlet, Error, TEXT("Failed to find a collection with name \"%s\", collection does not exist."), *CollectionName);
			HasFatalError = true;
		}
	}

	// ExcludePathFilters
	GetPathArrayFromConfig(*SectionName, TEXT("ExcludePathFilters"), ExcludePathFilters, GatherTextConfigPath);

	// ExcludePaths (DEPRECATED)
	{
		TArray<FString> ExcludePaths;
		GetPathArrayFromConfig(*SectionName, TEXT("ExcludePaths"), ExcludePaths, GatherTextConfigPath);
		if (ExcludePaths.Num())
		{
			ExcludePathFilters.Append(ExcludePaths);
			UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("ExcludePaths detected in section %s. ExcludePaths is deprecated, please use ExcludePathFilters."), *SectionName);
		}
	}

	// PackageNameFilters
	GetStringArrayFromConfig(*SectionName, TEXT("PackageFileNameFilters"), PackageFileNameFilters, GatherTextConfigPath);

	// PackageExtensions (DEPRECATED)
	{
		TArray<FString> PackageExtensions;
		GetStringArrayFromConfig(*SectionName, TEXT("PackageExtensions"), PackageExtensions, GatherTextConfigPath);
		if (PackageExtensions.Num())
		{
			PackageFileNameFilters.Append(PackageExtensions);
			UE_LOG(LogGatherTextFromAssetsCommandlet, Warning, TEXT("PackageExtensions detected in section %s. PackageExtensions is deprecated, please use PackageFileNameFilters."), *SectionName);
		}
	}

	if (PackageFileNameFilters.Num() == 0)
	{
		UE_LOG(LogGatherTextFromAssetsCommandlet, Error, TEXT("No package file name filters in section %s."), *SectionName);
		HasFatalError = true;
	}

	// Recursive asset class exclusion
	if (!GetBoolFromConfig(*SectionName, TEXT("ShouldExcludeDerivedClasses"), ShouldExcludeDerivedClasses, GatherTextConfigPath))
	{
		ShouldExcludeDerivedClasses = false;
	}

	// Asset class exclude
	GetStringArrayFromConfig(*SectionName, TEXT("ExcludeClasses"), ExcludeClassNames, GatherTextConfigPath);

	GetPathArrayFromConfig(*SectionName, TEXT("ManifestDependencies"), ManifestDependenciesList, GatherTextConfigPath);

	//Get whether we should fix broken properties that we find.
	if (!GetBoolFromConfig(*SectionName, TEXT("bFixBroken"), bFixBroken, GatherTextConfigPath))
	{
		bFixBroken = false;
	}

	// Get whether we should gather editor-only data. Typically only useful for the localization of UE4 itself.
	if (!GetBoolFromConfig(*SectionName, TEXT("ShouldGatherFromEditorOnlyData"), ShouldGatherFromEditorOnlyData, GatherTextConfigPath))
	{
		ShouldGatherFromEditorOnlyData = false;
	}

	bSkipGatherCache = FParse::Param(FCommandLine::Get(), TEXT("SkipGatherCache"));
	if (!bSkipGatherCache)
	{
		GetBoolFromConfig(*SectionName, TEXT("SkipGatherCache"), bSkipGatherCache, GatherTextConfigPath);
	}
	UE_LOG(LogGatherTextFromAssetsCommandlet, Log, TEXT("SkipGatherCache: %s"), bSkipGatherCache ? TEXT("true") : TEXT("false"));

	return !HasFatalError;
}

#undef LOC_DEFINE_REGION

//////////////////////////////////////////////////////////////////////////
