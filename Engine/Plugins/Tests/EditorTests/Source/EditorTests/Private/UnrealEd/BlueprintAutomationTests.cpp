// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/MetaData.h"
#include "UObject/UnrealType.h"
#include "Serialization/ObjectWriter.h"
#include "Serialization/ObjectReader.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "Misc/PackageName.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "AssetData.h"
#include "Animation/AnimBlueprint.h"
#include "GameFramework/SaveGame.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "FileHelpers.h"

#include "ObjectTools.h"
#include "ARFilter.h"
#include "AssetRegistryModule.h"
#include "Toolkits/AssetEditorManager.h"

#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "DiffResults.h"
#include "GraphDiffControl.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlueprintAutomationTests, Log, All);

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FBlueprintCompileOnLoadTest, "Project.Blueprints.Compile-On-Load", EAutomationTestFlags::EditorContext | EAutomationTestFlags::StressFilter)
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FBlueprintInstancesTest, "Project.Blueprints.Instance Test", EAutomationTestFlags::EditorContext | EAutomationTestFlags::StressFilter)
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FBlueprintReparentTest, "System.Blueprints.Reparent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::StressFilter)
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FBlueprintRenameAndCloneTest, "Project.Blueprints.Rename And Clone", (EAutomationTestFlags::EditorContext | EAutomationTestFlags::RequiresUser | EAutomationTestFlags::StressFilter))
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FCompileBlueprintsTest, "Project.Blueprints.Compile Blueprints", EAutomationTestFlags::EditorContext | EAutomationTestFlags::StressFilter)
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FCompileAnimBlueprintsTest, "Project.Blueprints.Compile Anims", EAutomationTestFlags::EditorContext | EAutomationTestFlags::StressFilter)

class FBlueprintAutomationTestUtilities
{
	/** An incrementing number that can be used to tack on to save files, etc. (for avoiding naming conflicts)*/
	static uint32 QueuedTempId;

	/** List of packages touched by automation tests that can no longer be saved */
	static TArray<FName> DontSavePackagesList;

	/** Callback to check if package is ok to save. */
	static bool IsPackageOKToSave(UPackage* InPackage, const FString& InFilename, FOutputDevice* Error)
	{
		return !DontSavePackagesList.Contains(InPackage->GetFName());
	}

	/**
	 * Gets a unique int (this run) for automation purposes (to avoid temp save 
	 * file collisions, etc.)
	 * 
	 * @return A id for unique identification that can be used to tack on to save files, etc. (for avoiding naming conflicts)
	 */
	static uint32 GenTempUid()
	{
		return QueuedTempId++;
	}

public:

	typedef TMap< FString, FString >	FPropertiesMap;

	/** 
	 * Helper struct to ensure that a package is not inadvertently left in
	 * a dirty state by automation tests
	 */
	struct FPackageCleaner
	{
		FPackageCleaner(UPackage* InPackage) : Package(InPackage)
		{
			bIsDirty = Package ? Package->IsDirty() : false;
		}

		~FPackageCleaner()
		{
			// reset the dirty flag
			if (Package) 
			{
				Package->SetDirtyFlag(bIsDirty);
			}
		}

	private:
		bool bIsDirty;
		UPackage* Package;
	};

	/**
	 * Loads the map specified by an automation test
	 * 
	 * @param MapName - Map to load
	 */
	static void LoadMap(const FString& MapName)
	{
		const bool bLoadAsTemplate = false;
		const bool bShowProgress = false;

		FEditorFileUtils::LoadMap(MapName, bLoadAsTemplate, bShowProgress);
	}

	/** 
	 * Filter used to test to see if a UProperty is candidate for comparison.
	 * @param Property	The property to test
	 *
	 * @return True if UProperty should be compared, false otherwise
	 */
	static bool ShouldCompareProperty (const UProperty* Property)
	{
		// Ignore components & transient properties
		const bool bIsTransient = !!( Property->PropertyFlags & CPF_Transient );
		const bool bIsComponent = !!( Property->PropertyFlags & (CPF_InstancedReference | CPF_ContainsInstancedReference) );
		const bool bShouldCompare = !(bIsTransient || bIsComponent);

		return (bShouldCompare && Property->HasAnyPropertyFlags(CPF_BlueprintVisible));
	}
	
	/** 
	 * Get a given UObject's properties in simple key/value string map
	 *
	 * @param Obj			The UObject to extract properties for 
	 * @param ObjProperties	[OUT] Property map to be filled
	 */
	static void GetObjProperties (UObject* Obj, FPropertiesMap& ObjProperties)
	{
		for (TFieldIterator<UProperty> PropIt(Obj->GetClass(), EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
		{
			UProperty* Prop = *PropIt;

			if ( ShouldCompareProperty(Prop) )
			{
				for (int32 Index = 0; Index < Prop->ArrayDim; Index++)
				{
					FString PropName = (Prop->ArrayDim > 1) ? FString::Printf(TEXT("%s[%d]"), *Prop->GetName(), Index) : *Prop->GetName();
					FString PropText;
					Prop->ExportText_InContainer(Index, PropText, Obj, Obj, Obj, PPF_SimpleObjectText);
					ObjProperties.Add(PropName, PropText);
				}
			}
		}
	}

	/** 
	 * Compare two object property maps
	 * @param OrigName		The name of the original object being compared against
	 * @param OrigMap		The property map for the object
	 * @param CmpName		The name of the object to compare
	 * @param CmpMap		The property map for the object to compare
	 */
	static bool ComparePropertyMaps(FName OrigName, TMap<FString, FString>& OrigMap, FName CmpName, FPropertiesMap& CmpMap, FCompilerResultsLog& Results)
	{
		if (OrigMap.Num() != CmpMap.Num())
		{
			Results.Error( *FString::Printf(TEXT("Objects have a different number of properties (%d vs %d)"), OrigMap.Num(), CmpMap.Num()) );
			return false;
		}

		bool bMatch = true;
		for (auto PropIt = OrigMap.CreateIterator(); PropIt; ++PropIt)
		{
			FString Key = PropIt.Key();
			FString Val = PropIt.Value();

			const FString* CmpValue = CmpMap.Find(Key);

			// Value is missing
			if (CmpValue == NULL)
			{
				bMatch = false;
				Results.Error( *FString::Printf(TEXT("Property is missing in object being compared: (%s %s)"), *Key, *Val) );
				break;
			}
			else if (Val != *CmpValue)
			{
				// string out object names and retest
				FString TmpCmp(*CmpValue);
				TmpCmp.ReplaceInline(*CmpName.ToString(), TEXT(""));
				FString TmpVal(Val);
				TmpVal.ReplaceInline(*OrigName.ToString(), TEXT(""));

				if (TmpCmp != TmpVal)
				{
					bMatch = false;
					Results.Error( *FString::Printf(TEXT("Object properties do not match: %s (%s vs %s)"), *Key, *Val, *(*CmpValue)) );
					break;
				}
			}
		}
		return bMatch;
	}

	/** 
	 * Compares the properties of two UObject instances

	 * @param OriginalObj		The first UObject to compare
	 * @param CompareObj		The second UObject to compare
	 * @param Results			The results log to record errors or warnings
	 *
	 * @return True of the blueprints are the same, false otherwise (see the Results log for more details)
	 */
	static bool CompareObjects(UObject* OriginalObj, UObject* CompareObj, FCompilerResultsLog& Results)
	{
		bool bMatch = false;

		// ensure we have something sensible to compare
		if (OriginalObj == NULL)
		{
			Results.Error( TEXT("Original object is null") );
			return false;
		}
		else if (CompareObj == NULL)
		{	
			Results.Error( TEXT("Compare object is null") );
			return false;
		}
		else if (OriginalObj == CompareObj)
		{
			Results.Error( TEXT("Objects to compare are the same") );
			return false;
		}

		TMap<FString, FString> ObjProperties;
		GetObjProperties(OriginalObj, ObjProperties);

		TMap<FString, FString> CmpProperties;
		GetObjProperties(CompareObj, CmpProperties);

		return ComparePropertyMaps(OriginalObj->GetFName(), ObjProperties, CompareObj->GetFName(), CmpProperties, Results);
	}

	/** 
	 * Runs over all the assets looking for ones that can be used by this test
	 *
	 * @param Class					The UClass of assets to load for the test
	 * @param OutBeautifiedNames	[Filled by method] The beautified filenames of the assets to use in the test
	 * @oaram OutTestCommands		[Filled by method] The asset name in a form suitable for passing as a param to the test
	 * @param bIgnoreLoaded			If true, ignore any already loaded assets
	 */
	static void CollectTestsByClass(UClass * Class, TArray< FString >& OutBeautifiedNames, TArray< FString >& OutTestCommands, bool bIgnoreLoaded) 
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		TArray<FAssetData> ObjectList;
		AssetRegistryModule.Get().GetAssetsByClass(Class->GetFName(), ObjectList);

		for (auto ObjIter=ObjectList.CreateConstIterator(); ObjIter; ++ObjIter)
		{
			const FAssetData& Asset = *ObjIter;
			FString Filename = Asset.ObjectPath.ToString();
			//convert to full paths
			Filename = FPackageName::LongPackageNameToFilename(Filename);
			if (FAutomationTestFramework::Get().ShouldTestContent(Filename))
			{
				// optionally discount already loaded assets
				if (!bIgnoreLoaded || !Asset.IsAssetLoaded())
				{
					FString BeautifiedFilename = Asset.AssetName.ToString();
					OutBeautifiedNames.Add(BeautifiedFilename);
					OutTestCommands.Add(Asset.ObjectPath.ToString());
				}
			}
		}
	}

	/**
	 * Adds a package to a list of packages that can no longer be saved.
	 *
	 * @param Package Package to prevent from being saved to disk.
	 */
	static void DontSavePackage(UPackage* Package)
	{
		if (DontSavePackagesList.Num() == 0)
		{
			FCoreUObjectDelegates::IsPackageOKToSaveDelegate.BindStatic(&IsPackageOKToSave);
		}
		DontSavePackagesList.AddUnique(Package->GetFName());
	}

	/**
	 * A helper method that will reset a package for reload, and flag it as 
	 * unsavable (meant to be used after you've messed with a package for testing 
	 * purposes, leaving it in a questionable state).
	 * 
	 * @param  Package	The package you wish to invalidate.
	 */
	static void InvalidatePackage(UPackage* const Package)
	{
		// reset the blueprint's original package/linker so that we can get by
		// any early returns (in the load code), and reload its exports as if new 
		ResetLoaders(Package);
		Package->ClearFlags(RF_WasLoaded);	
		Package->bHasBeenFullyLoaded = false;

		Package->GetMetaData()->RemoveMetaDataOutsidePackage();
		// we've mucked around with the package manually, we should probably prevent 
		// people from saving it in this state (this means you won't be able to save 
		// the blueprints that these tests were run on until you relaunch the editor)
		DontSavePackage(Package);
	}

	/**
	 * Helper method to close a specified blueprint (if it is open in the blueprint-editor).
	 * 
	 * @param  BlueprintObj		The blueprint you want the editor closed for.
	 */
	static void CloseBlueprint(UBlueprint* const BlueprintObj)
	{
		IAssetEditorInstance* EditorInst = FAssetEditorManager::Get().FindEditorForAsset(BlueprintObj, /*bool bFocusIfOpen =*/false);
		if (EditorInst != nullptr)
		{
			UE_LOG(LogBlueprintAutomationTests, Log, TEXT("Closing '%s' so we don't invalidate the open version when unloading it."), *BlueprintObj->GetName());
			EditorInst->CloseWindow();
		}
	}

	/** 
	 * Helper method to unload loaded blueprints. Use with caution.
	 *
	 * @param BlueprintObj	The blueprint to unload
	 * @param bForceFlush   If true, will force garbage collection on everything but native objects (defaults to false)
	 */
	static void UnloadBlueprint(UBlueprint* const BlueprintObj, bool bForceFlush = false)
	{
		// have to grab the blueprint's package before we move it to the transient package
		UPackage* const OldPackage = BlueprintObj->GetOutermost();

		UPackage* const TransientPackage = GetTransientPackage();
		if (OldPackage == TransientPackage)
		{
			UE_LOG(LogBlueprintAutomationTests, Log, TEXT("No need to unload '%s' from the transient package."), *BlueprintObj->GetName());
		}
		else if (OldPackage->IsRooted() || BlueprintObj->IsRooted())
		{
			UE_LOG(LogBlueprintAutomationTests, Error, TEXT("Cannot unload '%s' when its root is set (it will not be garbage collected, leaving it in an erroneous state)."), *OldPackage->GetName());
		}
		else if (OldPackage->IsDirty())
		{
			UE_LOG(LogBlueprintAutomationTests, Error, TEXT("Cannot unload '%s' when it has unsaved changes (save the asset and then try again)."), *OldPackage->GetName());
		}
		else 
		{
			// prevent users from modifying an open blueprint, after it has been unloaded
			CloseBlueprint(BlueprintObj);

			UPackage* NewPackage = TransientPackage;
			// move the blueprint to the transient package (to be picked up by garbage collection later)
			FName UnloadedName = MakeUniqueObjectName(NewPackage, UBlueprint::StaticClass(), BlueprintObj->GetFName());
			BlueprintObj->Rename(*UnloadedName.ToString(), NewPackage, REN_DontCreateRedirectors|REN_DoNotDirty);

			// Rename() will mark the OldPackage dirty (since it is removing the
			// blueprint from it), we don't want this to affect the dirty flag 
			// (for if/when we load it again)
			OldPackage->SetDirtyFlag(/*bIsDirty =*/false);

			// make sure the blueprint is properly trashed so we can rerun tests on it
			BlueprintObj->SetFlags(RF_Transient);
			BlueprintObj->ClearFlags(RF_Standalone | RF_Transactional);
			BlueprintObj->RemoveFromRoot();
			BlueprintObj->MarkPendingKill();

			InvalidatePackage(OldPackage);
		}

		// because we just emptied out an existing package, we may want to clean 
		// up garbage so an attempted load doesn't stick us with an invalid asset
		if (bForceFlush)
		{
#if WITH_EDITOR
			// clear undo history to ensure that the transaction buffer isn't 
			// holding onto any references to the blueprints we want unloaded
			GEditor->Trans->Reset(NSLOCTEXT("BpAutomation", "BpAutomationTest", "Blueprint Automation Test"));
#endif // #if WITH_EDITOR
			CollectGarbage(RF_NoFlags);
		}
	}

	/**
	 * A utility function to help separate a package name and asset name out 
	 * from a full asset object path.
	 * 
	 * @param  AssetObjPathIn	The asset object path you want split.
	 * @param  PackagePathOut	The first half of the in string (the package portion).
	 * @param  AssetNameOut		The second half of the in string (the asset name portion).
	 */
	static void SplitPackagePathAndAssetName(FString const& AssetObjPathIn, FString& PackagePathOut, FString& AssetNameOut)
	{
		AssetObjPathIn.Split(TEXT("."), &PackagePathOut, &AssetNameOut);
	}

	/**
	 * A utility function for looking up a package from an asset's full path (a 
	 * long package path).
	 * 
	 * @param  AssetPath	The object path for a package that you want to look up.
	 * @return A package containing the specified asset (NULL if the asset doesn't exist, or it isn't loaded).
	 */
	static UPackage* FindPackageForAsset(FString const& AssetPath)
	{
		FString PackagePath, AssetName;
		SplitPackagePathAndAssetName(AssetPath, PackagePath, AssetName);

		return FindPackage(NULL, *PackagePath);
	}

	/**
	 * Helper method for checking to see if a blueprint is currently loaded.
	 * 
	 * @param  AssetPath	A path detailing the asset in question (in the form of <PackageName>.<AssetName>)
	 * @return True if a blueprint can be found with a matching package/name, false if no corresponding blueprint was found.
	 */
	static bool IsBlueprintLoaded(FString const& AssetPath, UBlueprint** BlueprintOut = nullptr)
	{
		bool bIsLoaded = false;

		if (UPackage* ExistingPackage = FindPackageForAsset(AssetPath))
		{
			FString PackagePath, AssetName;
			SplitPackagePathAndAssetName(AssetPath, PackagePath, AssetName);

			if (UBlueprint* ExistingBP = Cast<UBlueprint>(StaticFindObject(UBlueprint::StaticClass(), ExistingPackage, *AssetName)))
			{
				bIsLoaded = true;
				if (BlueprintOut != nullptr)
				{
					*BlueprintOut = ExistingBP;
				}
			}
		}

		return bIsLoaded;
	}

	/** */
	static bool GetExternalReferences(UObject* Obj, TArray<FReferencerInformation>& ExternalRefsOut)
	{
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

		bool bHasReferences = false;

		FReferencerInformationList Refs;
		if (IsReferenced(Obj, RF_Public, EInternalObjectFlags::None, true, &Refs))
		{
			ExternalRefsOut = Refs.ExternalReferences;
			bHasReferences = true;
		}

		return bHasReferences;
	}

	/**
	 * Helper method for determining if the specified asset has pending changes.
	 * 
	 * @param  AssetPath	The object path to an asset you want checked.
	 * @return True if the package is unsaved, false if it is up to date.
	 */
	static bool IsAssetUnsaved(FString const& AssetPath)
	{
		bool bIsUnsaved = false;
		if (UPackage* ExistingPackage = FindPackageForAsset(AssetPath))
		{
			bIsUnsaved = ExistingPackage->IsDirty();
		}
		return bIsUnsaved;
	}

	/**
	 * Simulates the user pressing the blueprint's compile button (will load the
	 * blueprint first if it isn't already).
	 * 
	 * @param  BlueprintAssetPath	The asset object path that you wish to compile.
	 * @return False if we failed to load the blueprint, true otherwise
	 */
	static bool CompileBlueprint(const FString& BlueprintAssetPath)
	{
		UBlueprint* BlueprintObj = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), NULL, *BlueprintAssetPath));
		if (!BlueprintObj || !BlueprintObj->ParentClass)
		{
			UE_LOG(LogBlueprintAutomationTests, Error, TEXT("Failed to compile invalid blueprint, or blueprint parent no longer exists."));
			return false;
		}

		UPackage* const BlueprintPackage = BlueprintObj->GetOutermost();
		// compiling the blueprint will inherently dirty the package, but if there 
		// weren't any changes to save before, there shouldn't be after
		bool const bStartedWithUnsavedChanges = (BlueprintPackage != nullptr) ? BlueprintPackage->IsDirty() : true;

		FKismetEditorUtilities::CompileBlueprint(BlueprintObj, EBlueprintCompileOptions::SkipGarbageCollection);

		if (BlueprintPackage != nullptr)
		{
			BlueprintPackage->SetDirtyFlag(bStartedWithUnsavedChanges);
		}

		return true;
	}

	/**
	 * Takes two blueprints and compares them (as if we were running the in-editor 
	 * diff tool). Any discrepancies between the two graphs will be listed in the DiffsOut array.
	 * 
	 * @param  LhsBlueprint	The baseline blueprint you wish to compare against.
	 * @param  RhsBlueprint	The blueprint you wish to look for changes in.
	 * @param  DiffsOut		An output list that will contain any graph internal differences that were found.
	 * @return True if the two blueprints differ, false if they are identical.
	 */
	static bool DiffBlueprints(UBlueprint* const LhsBlueprint, UBlueprint* const RhsBlueprint, TArray<FDiffSingleResult>& DiffsOut)
	{
		TArray<UEdGraph*> LhsGraphs;
		LhsBlueprint->GetAllGraphs(LhsGraphs);
		TArray<UEdGraph*> RhsGraphs;
		RhsBlueprint->GetAllGraphs(RhsGraphs);

		bool bDiffsFound = false;
		// walk the graphs in the rhs blueprint (because, conceptually, it is the more up to date one)
		for (auto RhsGraphIt(RhsGraphs.CreateIterator()); RhsGraphIt; ++RhsGraphIt)
		{
			UEdGraph* RhsGraph = *RhsGraphIt;
			UEdGraph* LhsGraph = NULL;

			// search for the corresponding graph in the lhs blueprint
			for (auto LhsGraphIt(LhsGraphs.CreateIterator()); LhsGraphIt; ++LhsGraphIt)
			{
				// can't trust the guid until we've done a resave on every asset
				//if ((*LhsGraphIt)->GraphGuid == RhsGraph->GraphGuid)
				
				// name compares is probably sufficient, but just so we don't always do a string compare
				if (((*LhsGraphIt)->GetClass() == RhsGraph->GetClass()) &&
					((*LhsGraphIt)->GetName() == RhsGraph->GetName()))
				{
					LhsGraph = *LhsGraphIt;
					break;
				}
			}

			// if a matching graph wasn't found in the lhs blueprint, then that is a BIG inconsistency
			if (LhsGraph == NULL)
			{
				bDiffsFound = true;
				continue;
			}

			bDiffsFound |= FGraphDiffControl::DiffGraphs(LhsGraph, RhsGraph, DiffsOut);
		}

		return bDiffsFound;
	}

	/**
	 * Gathers a list of asset files corresponding to a config array (an array 
	 * of package paths).
	 * 
	 * @param  ConfigKey	A key to the config array you want to look up.
	 * @param  AssetsOut	An output array that will be filled with the desired asset data.
	 * @param  ClassType	If specified, will further filter the asset look up by class.
	 */
	static void GetAssetListingFromConfig(FString const& ConfigKey, TArray<FAssetData>& AssetsOut, UClass const* const ClassType = NULL)
	{
		check(GConfig != NULL);

		FARFilter AssetFilter;
		AssetFilter.bRecursivePaths = true;
		if (ClassType != NULL)
		{
			AssetFilter.ClassNames.Add(ClassType->GetFName());
		}
		
		TArray<FString> AssetPaths;
		GConfig->GetArray(TEXT("AutomationTesting.Blueprint"), *ConfigKey, AssetPaths, GEngineIni);
		for (FString const& AssetPath : AssetPaths)
		{
			AssetFilter.PackagePaths.Add(*AssetPath);
		}
		
		if (AssetFilter.PackagePaths.Num() > 0)
		{
			IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
			AssetRegistry.GetAssets(AssetFilter, AssetsOut);
		}
	}

	/**
	 * A utility function for spawning an empty temporary package, meant for test purposes.
	 * 
	 * @param  Name		A suggested string name to get the package (NOTE: it will further be decorated with things like "Temp", etc.) 
	 * @return A newly created package for throwaway use.
	 */
	static UPackage* CreateTempPackage(FString Name)
	{
		FString TempPackageName = FString::Printf(TEXT("/Temp/BpAutomation-%u-%s"), GenTempUid(), *Name);
		return CreatePackage(NULL, *TempPackageName);
	}

	/**
	 * A helper that will take a blueprint and copy it into a new, temporary 
	 * package (intended for throwaway purposes).
	 * 
	 * @param  BlueprintToClone		The blueprint you wish to duplicate.
	 * @return A new temporary blueprint copy of what was passed in.
	 */
	static UBlueprint* DuplicateBlueprint(UBlueprint const* const BlueprintToClone)
	{
		UPackage* TempPackage = CreateTempPackage(BlueprintToClone->GetName());

		const FName TempBlueprintName = MakeUniqueObjectName(TempPackage, UBlueprint::StaticClass(), BlueprintToClone->GetFName());
		return Cast<UBlueprint>(StaticDuplicateObject(BlueprintToClone, TempPackage, TempBlueprintName));
	}

	/**
	 * A getter function for coordinating between multiple tests, a place for 
	 * temporary test files to be saved.
	 * 
	 * @return A relative path to a directory meant for temp automation files.
	 */
	static FString GetTempDir()
	{
		return FPaths::ProjectSavedDir() + TEXT("Automation/");
	}

	/**
	 * Will save a blueprint package under a temp file and report on weather it succeeded or not.
	 * 
	 * @param  BlueprintObj		The blueprint you wish to save.
	 * @return True if the save was successful, false if it failed.
	 */
	static bool TestSaveBlueprint(UBlueprint* const BlueprintObj)
	{
		FString TempDir = GetTempDir();
		IFileManager::Get().MakeDirectory(*TempDir);

		FString SavePath = FString::Printf(TEXT("%sTemp-%u-%s"), *TempDir, GenTempUid(), *FPaths::GetCleanFilename(BlueprintObj->GetName()));

		UPackage* const AssetPackage = BlueprintObj->GetOutermost();
		return UPackage::SavePackage(AssetPackage, NULL, RF_Standalone, *SavePath, GWarn);
	}

	static void ResolveCircularDependencyDiffs(UBlueprint const* const BlueprintIn, TArray<FDiffSingleResult>& DiffsInOut)
	{
		UEdGraphSchema_K2 const* K2Schema = GetDefault<UEdGraphSchema_K2>();

		typedef TArray<FDiffSingleResult>::TIterator TDiffIt;
		TMap<UEdGraphPin*, TDiffIt> PinLinkDiffsForRepair;

		for (auto DiffIt(DiffsInOut.CreateIterator()); DiffIt; ++DiffIt)
		{
			// as far as we know, pin link diffs are the only ones that would be
			// affected by circular references pointing to an unloaded class 
			// 
			// NOTE: we only handle PIN_LINKEDTO_NUM_INC over PIN_LINKEDTO_NUM_DEC,
			//       this assumes that the diff was performed in a specific 
			//       order (the reloaded blueprint first).
			if (DiffIt->Diff != EDiffType::PIN_LINKEDTO_NUM_INC)
			{
				continue;
			}

			check(DiffIt->Pin1 != nullptr);
			check(DiffIt->Pin2 != nullptr);
			UEdGraphPin* MalformedPin = DiffIt->Pin1;
			
			FEdGraphPinType const& PinType = MalformedPin->PinType;
			// only object pins would reference the unloaded blueprint
			if (!PinType.PinSubCategoryObject.IsValid() || ((PinType.PinCategory != K2Schema->PC_Object) && 
				(PinType.PinCategory != K2Schema->PSC_Self) && (PinType.PinCategory != K2Schema->PC_Interface)))
			{
				continue;
			}

			UStruct const* PinObjType = Cast<UStruct>(PinType.PinSubCategoryObject.Get());
			// only pins that match the blueprint class would have been affected 
			// by the unload (assumes an FArchiveReplaceObjectRef() has since been 
			// ran to fix-up any references to the unloaded class... meaning the 
			// malformed pins now have the proper reference)
			if (!PinObjType->IsChildOf(BlueprintIn->GeneratedClass))
			{
				continue;
			}

			UEdGraphPin* LegitPin = DiffIt->Pin2;
			// make sure we interpreted which pin is which correctly
			check(LegitPin->LinkedTo.Num() > MalformedPin->LinkedTo.Num());

			for (UEdGraphPin* LinkedPin : LegitPin->LinkedTo)
			{
				// pin linked-to-count diffs always come in pairs (one for the
				// input pin, another for the output)... we use this to know 
				// which pins we should attempt to link again
				TDiffIt const* CorrespendingDiff = PinLinkDiffsForRepair.Find(LinkedPin);
				// we don't have the full pair yet, we'll have to wait till we have the other one
				if (CorrespendingDiff == nullptr)
				{
					continue;
				}

				UEdGraphPin* OtherMalformedPin = (*CorrespendingDiff)->Pin1;
				if (K2Schema->ArePinsCompatible(MalformedPin, OtherMalformedPin, BlueprintIn->GeneratedClass))
				{
					MalformedPin->MakeLinkTo(OtherMalformedPin);
				}
				// else pin types still aren't compatible (even after running 
				// FArchiveReplaceObjectRef), meaning this diff isn't fully resolvable
			}

			// track diffs that are in possible need of repair (so we know which  
			// two pins should attempt to relink)
			PinLinkDiffsForRepair.Add(LegitPin, DiffIt);
		}

		// remove any resolved diffs that no longer are valid (iterating backwards
		// so we can remove array items and not have to offset the index)
		for (int32 DiffIndex = DiffsInOut.Num()-1; DiffIndex >= 0; --DiffIndex)
		{
			FDiffSingleResult const& Diff = DiffsInOut[DiffIndex];
			if ((Diff.Diff == EDiffType::PIN_LINKEDTO_NUM_INC) || (Diff.Diff == EDiffType::PIN_LINKEDTO_NUM_DEC))
			{
				check(Diff.Pin1 && Diff.Pin2);
				// if this diff has been resolved (it's no longer valid)
				if (Diff.Pin1->LinkedTo.Num() == Diff.Pin2->LinkedTo.Num())
				{
					DiffsInOut.RemoveAt(DiffIndex);
				}
			}
		}
	}
};

uint32 FBlueprintAutomationTestUtilities::QueuedTempId = 0u;
TArray<FName> FBlueprintAutomationTestUtilities::DontSavePackagesList;

/************************************************************************/
/* FScopedBlueprintUnloader                                             */
/************************************************************************/

class FScopedBlueprintUnloader
{
public:
	FScopedBlueprintUnloader(bool bAutoOpenScope, bool bRunGCOnCloseIn)
		: bIsOpen(false)
		, bRunGCOnClose(bRunGCOnCloseIn)
	{
		if (bAutoOpenScope)
		{
			OpenScope();
		}
	}

	~FScopedBlueprintUnloader()
	{
		CloseScope();
	}

	/** Tracks currently loaded blueprints at the time of this object's creation */
	void OpenScope()
	{
		PreLoadedBlueprints.Empty();

		// keep a list of blueprints that were loaded at the start (so we can unload new ones after)
		for (TObjectIterator<UBlueprint> BpIt; BpIt; ++BpIt)
		{
			UBlueprint* Blueprint = *BpIt;
			PreLoadedBlueprints.Add(Blueprint);
		}
		bIsOpen = true;
	}

	/** Unloads any blueprints that weren't loaded when this object was created */
	void CloseScope()
	{
		if (bIsOpen)
		{
			// clean up any dependencies that we're loading in the scope of this object's lifetime
			for (TObjectIterator<UBlueprint> BpIt; BpIt; ++BpIt)
			{
				UBlueprint* Blueprint = *BpIt;
				if (PreLoadedBlueprints.Find(Blueprint) == nullptr)
				{
					FBlueprintAutomationTestUtilities::UnloadBlueprint(Blueprint);
				}
			}

			bIsOpen = false;
		}

		// run, even if it was not open (some tests may be relying on this, and 
		// not running it themselves)
		if (bRunGCOnClose)
		{
#if WITH_EDITOR
			// clear undo history to ensure that the transaction buffer isn't 
			// holding onto any references to the blueprints we want unloaded
			GEditor->Trans->Reset(NSLOCTEXT("BpAutomation", "BpAutomationTest", "Blueprint Automation Test"));
#endif // #if WITH_EDITOR
			CollectGarbage(RF_NoFlags);
		}
	}

	void ClearScope()
	{
		PreLoadedBlueprints.Empty();
		bIsOpen = false;
	}

private:
	bool bIsOpen;
	TSet<UBlueprint*> PreLoadedBlueprints;
	bool bRunGCOnClose;
};

/************************************************************************/
/* FBlueprintCompileOnLoadTest                                          */
/************************************************************************/

/** 
 * Gather the tests to run
 */
void FBlueprintCompileOnLoadTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	bool bTestLoadedBlueprints = false;
	GConfig->GetBool( TEXT("AutomationTesting.Blueprint"), TEXT("TestAllBlueprints"), /*out*/ bTestLoadedBlueprints, GEngineIni );
	FBlueprintAutomationTestUtilities::CollectTestsByClass(UBlueprint::StaticClass(), OutBeautifiedNames, OutTestCommands, !bTestLoadedBlueprints);
}

/** 
 * Runs compile-on-load test against all unloaded, and optionally loaded, blueprints
 * See the TestAllBlueprints config key in the [Automation.Blueprint] config sections
 */
bool FBlueprintCompileOnLoadTest::RunTest(const FString& BlueprintAssetPath)
{
	FCompilerResultsLog Results;

	UBlueprint* ExistingBP = nullptr;
	// if this blueprint was already loaded, then these tests are invalidated 
	// (because dependencies have already been loaded)
	if (FBlueprintAutomationTestUtilities::IsBlueprintLoaded(BlueprintAssetPath, &ExistingBP))
	{
		if (FBlueprintAutomationTestUtilities::IsAssetUnsaved(BlueprintAssetPath))
		{
			AddError(FString::Printf(TEXT("You have unsaved changes made to '%s', please save them before running this test."), *BlueprintAssetPath));
			return false;
		}
		else
		{
			AddWarning(FString::Printf(TEXT("Test may be invalid (the blueprint is already loaded): '%s'"), *BlueprintAssetPath));
			FBlueprintAutomationTestUtilities::UnloadBlueprint(ExistingBP);
		}
	}

	// tracks blueprints that were already loaded (and cleans up any that were 
	// loaded in its lifetime, once it is destroyed)
	FScopedBlueprintUnloader NewBlueprintUnloader(/*bAutoOpenScope =*/true, /*bRunGCOnCloseIn =*/true);

	// We load the blueprint twice and compare the two for discrepancies. This is 
	// to bring dependency load issues to light (among other things). If a blueprint's
	// dependencies are loaded too late, then this first object is the degenerate one.
	UBlueprint* InitialBlueprint = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), NULL, *BlueprintAssetPath));

	// if we failed to load it the first time, then there is no need to make a 
	// second attempt, leave them to fix up this issue first
	if (InitialBlueprint == NULL)
	{
		AddError(*FString::Printf(TEXT("Unable to load blueprint for: '%s'"), *BlueprintAssetPath));
		return false;
	}

	if (!InitialBlueprint->SkeletonGeneratedClass || !InitialBlueprint->GeneratedClass)
	{
		AddError(*FString::Printf(TEXT("Unable to load blueprint for: '%s'. Probably it derives from an invalid class."), *BlueprintAssetPath));
		return false;
	}

	// GATHER SUBOBJECTS
	TArray<TWeakObjectPtr<UObject>> InitialBlueprintSubobjects;
	{
		TArray<UObject*> InitialBlueprintSubobjectsPtr;
		GetObjectsWithOuter(InitialBlueprint, InitialBlueprintSubobjectsPtr);
		for (auto Obj : InitialBlueprintSubobjectsPtr)
		{
			InitialBlueprintSubobjects.Add(Obj);
		}
	}

	// GATHER DEPENDENCIES
	TSet<TWeakObjectPtr<UBlueprint>> BlueprintDependencies;
	{
		TArray<UBlueprint*> DependentBlueprints;
		FBlueprintEditorUtils::GetDependentBlueprints(InitialBlueprint, DependentBlueprints);
		for (auto BP : DependentBlueprints)
		{
			BlueprintDependencies.Add(BP);
		}
	}
	BlueprintDependencies.Add(InitialBlueprint);

	// GATHER DEPENDENCIES PERSISTENT DATA
	struct FReplaceInnerData
	{
		TWeakObjectPtr<UClass> Class;
		FSoftObjectPath BlueprintAsset;
	};
	TArray<FReplaceInnerData> ReplaceInnerData;
	for (auto BPToUnloadWP : BlueprintDependencies)
	{
		auto BPToUnload = BPToUnloadWP.Get();
		auto OldClass = BPToUnload ? *BPToUnload->GeneratedClass : NULL;
		if (OldClass)
		{
			FReplaceInnerData Data;
			Data.Class = OldClass;
			Data.BlueprintAsset = FSoftObjectPath(BPToUnload);
			ReplaceInnerData.Add(Data);
		}
	}

	// store off data for the initial blueprint so we can unload it (and reconstruct 
	// later to compare it with a second one)
	TArray<uint8> InitialLoadData;
	FObjectWriter(InitialBlueprint, InitialLoadData);

	// grab the name before we unload the blueprint
	FName const BlueprintName = InitialBlueprint->GetFName();
	// unload the blueprint so we can reload it (to catch any differences, now  
	// that all its dependencies should be loaded as well)

	//UNLOAD DEPENDENCIES, all circular dependencies will be loaded again 
	// unload the blueprint so we can reload it (to catch any differences, now  
	// that all its dependencies should be loaded as well)
	for (auto BPToUnloadWP : BlueprintDependencies)
	{
		if (auto BPToUnload = BPToUnloadWP.Get())
		{
			FBlueprintAutomationTestUtilities::UnloadBlueprint(BPToUnload);
		}
	}

	// this blueprint is now dead (will be destroyed next garbage-collection pass)
	UBlueprint* UnloadedBlueprint = InitialBlueprint;
	InitialBlueprint = NULL;

	// load the blueprint a second time; if the two separately loaded blueprints 
	// are different, then this one is most likely the choice one (it has all its 
	// dependencies loaded)

	UBlueprint* ReloadedBlueprint = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), NULL, *BlueprintAssetPath));

	UPackage* TransientPackage = GetTransientPackage();
	FName ReconstructedName = MakeUniqueObjectName(TransientPackage, UBlueprint::StaticClass(), BlueprintName);
	// reconstruct the initial blueprint (using the serialized data from its initial load)
	EObjectFlags const StandardBlueprintFlags = RF_Public | RF_Standalone | RF_Transactional;
	InitialBlueprint = NewObject<UBlueprint>(TransientPackage, ReconstructedName, StandardBlueprintFlags | RF_Transient);
	FObjectReader(InitialBlueprint, InitialLoadData);
	{
		TMap<UObject*, UObject*> ClassRedirects;
		for (auto& Data : ReplaceInnerData)
		{
			UClass* OriginalClass = Data.Class.Get();
			UBlueprint* NewBlueprint = Cast<UBlueprint>(Data.BlueprintAsset.ResolveObject());
			UClass* NewClass = NewBlueprint ? *NewBlueprint->GeneratedClass : NULL;
			if (OriginalClass && NewClass)
			{
				ClassRedirects.Add(OriginalClass, NewClass);
			}
		}
		// REPLACE OLD DATA
		FArchiveReplaceObjectRef<UObject>(InitialBlueprint, ClassRedirects, /*bNullPrivateRefs=*/false, /*bIgnoreOuterRef=*/true, /*bIgnoreArchetypeRef=*/false);
		for (auto SubobjWP : InitialBlueprintSubobjects)
		{
			if (auto Subobj = SubobjWP.Get())
			{
				FArchiveReplaceObjectRef<UObject>(Subobj, ClassRedirects, /*bNullPrivateRefs=*/false, /*bIgnoreOuterRef=*/true, /*bIgnoreArchetypeRef=*/false);
			}
		}
	}

	// look for diffs between subsequent loads and log them as errors
	TArray<FDiffSingleResult> BlueprintDiffs;
	bool bDiffsFound = FBlueprintAutomationTestUtilities::DiffBlueprints(InitialBlueprint, ReloadedBlueprint, BlueprintDiffs);
	if (bDiffsFound)
	{
		FBlueprintAutomationTestUtilities::ResolveCircularDependencyDiffs(ReloadedBlueprint, BlueprintDiffs);
		// if there are still diffs after resolving any the could have been from unloaded circular dependencies
		if (BlueprintDiffs.Num() > 0)
		{
			AddError(FString::Printf(TEXT("Inconsistencies between subsequent blueprint loads for: '%s' (was a dependency not preloaded?)"), *BlueprintAssetPath));
		}
		else 
		{
			bDiffsFound = false;
		}
		
		// list all the differences (so as to help identify what dependency was missing)
		for (auto DiffIt(BlueprintDiffs.CreateIterator()); DiffIt; ++DiffIt)
		{
			// will be presented in the context of "what changed between the initial load and the second?"
			FString DiffDescription = DiffIt->ToolTip.ToString();
			if (DiffDescription != DiffIt->DisplayString.ToString())
			{
				DiffDescription = FString::Printf(TEXT("%s (%s)"), *DiffDescription, *DiffIt->DisplayString.ToString());
			}

			const UEdGraphNode* NodeFromPin = DiffIt->Pin1 ? DiffIt->Pin1->GetOuter() : nullptr;
			const UEdGraphNode* Node = DiffIt->Node1 ? DiffIt->Node1 : NodeFromPin;
			const UEdGraph* Graph = Node ? Node->GetGraph() : NULL;
			const FString GraphName = Graph ? Graph->GetName() : FString(TEXT("Unknown Graph"));
			AddError(FString::Printf(TEXT("%s.%s differs between subsequent loads: %s"), *BlueprintName.ToString(), *GraphName, *DiffDescription));
		}
	}

	// At the close of this function, the FScopedBlueprintUnloader should prep 
	// for following tests by unloading any blueprint dependencies that were 
	// loaded for this one (should catch InitialBlueprint and ReloadedBlueprint) 
	// 
	// The FScopedBlueprintUnloader should also run garbage-collection after,
	// in hopes that the imports for this blueprint get destroyed so that they 
	// don't invalidate other tests that share the same dependencies
	return !bDiffsFound;
}


/************************************************************************/
/* FCompileBlueprintsTest                                              */
/************************************************************************/

/** Requests a enumeration of all blueprints to be loaded */
void FCompileBlueprintsTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const
{
	FBlueprintAutomationTestUtilities::CollectTestsByClass(UBlueprint::StaticClass(), OutBeautifiedNames, OutTestCommands, /*bool bIgnoreLoaded =*/false);
}


bool FCompileBlueprintsTest::RunTest(const FString& Parameters)
{
	UE_LOG(LogBlueprintAutomationTests, Log, TEXT("Beginning compile test for %s"), *Parameters);
	return FBlueprintAutomationTestUtilities::CompileBlueprint(Parameters);
}

/************************************************************************/
/* FCompileAnimBlueprintsTest                                           */
/************************************************************************/

/** Requests a enumeration of all blueprints to be loaded */
void FCompileAnimBlueprintsTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const
{
	FBlueprintAutomationTestUtilities::CollectTestsByClass(UAnimBlueprint::StaticClass(), OutBeautifiedNames, OutTestCommands, /*bool bIgnoreLoaded =*/false);
}

bool FCompileAnimBlueprintsTest::RunTest(const FString& Parameters)
{
	return FBlueprintAutomationTestUtilities::CompileBlueprint(Parameters);
}

/************************************************************************/
/* FBlueprintInstancesTest                                              */
/************************************************************************/

void FBlueprintInstancesTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	// Load the test maps
	check( GConfig );

	// Load from config file
	TArray<FString> MapsToLoad;
	GConfig->GetArray( TEXT("AutomationTesting.Blueprint"), TEXT("InstanceTestMaps"), MapsToLoad, GEngineIni);
	for (auto It = MapsToLoad.CreateConstIterator(); It; ++It)
	{
		const FString MapFileName = *It;
		if( IFileManager::Get().FileSize( *MapFileName ) > 0 )
		{
			OutBeautifiedNames.Add(FPaths::GetBaseFilename(MapFileName));
			OutTestCommands.Add(MapFileName);
		}
	}
}

/**
  * Wait for the given amount of time
  */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FDelayLatentCommand, float, Duration);

bool FDelayLatentCommand::Update()
{
	float NewTime = FPlatformTime::Seconds();
	if (NewTime - StartTime >= Duration)
	{
		return true;
	}
	return false;
}

/************************************************************************/
/**
 * Uses test maps in Engine and/or game content folder which are populated with a few blueprint instances
 * See InstanceTestMaps entries in the [Automation.Blueprint] config sections
 * For all blueprint instances in the map:
 *		Duplicates the instance 
 *		Compares the duplicated instance properties to the original instance properties
 */
bool FBlueprintInstancesTest::RunTest(const FString& InParameters)
{
	FBlueprintAutomationTestUtilities::LoadMap(InParameters);

	// Pause before running test
	ADD_LATENT_AUTOMATION_COMMAND(FDelayLatentCommand(2.f));

	// Grab BP instances from map
	TSet<AActor*> BlueprintInstances;
	for ( FActorIterator It(GWorld); It; ++It )
	{
		AActor* Actor = *It;

		UClass* ActorClass = Actor->GetClass();

		if (ActorClass->ClassGeneratedBy && ActorClass->ClassGeneratedBy->IsA( UBlueprint::StaticClass() ) )
		{
			BlueprintInstances.Add(Actor);
		}
	}

	bool bPropertiesMatch = true;
	FCompilerResultsLog ResultLog;
	TSet<UPackage*> PackagesUserRefusedToFullyLoad;
	ObjectTools::FPackageGroupName PGN;

	for (auto BpIter = BlueprintInstances.CreateIterator(); BpIter; ++BpIter )
	{
		AActor* BPInstance = *BpIter;
		UObject* BPInstanceOuter = BPInstance->GetOuter();

		TMap<FString,FString> BPNativePropertyValues;
		BPInstance->GetNativePropertyValues(BPNativePropertyValues);

		// Grab the package and save out its dirty state
		UPackage* ActorPackage = BPInstance->GetOutermost();
		FBlueprintAutomationTestUtilities::FPackageCleaner Cleaner(ActorPackage);

		// Use this when duplicating the object to keep a list of everything that was duplicated
		//TMap<UObject*, UObject*> DuplicatedObjectList;

		FObjectDuplicationParameters Parameters(BPInstance, BPInstanceOuter);
		//Parameters.CreatedObjects = &DuplicatedObjectList;
		Parameters.DestName = MakeUniqueObjectName( BPInstanceOuter, AActor::StaticClass(), BPInstance->GetFName() );

		// Duplicate the object
		AActor* ClonedInstance = Cast<AActor>(StaticDuplicateObjectEx(Parameters));

		if (!FBlueprintAutomationTestUtilities::CompareObjects(BPInstance, ClonedInstance, ResultLog))
		{
			bPropertiesMatch = false;
			break;
		}

		// Ensure we can't save package in editor
		FBlueprintAutomationTestUtilities::DontSavePackage(ActorPackage);
	}

	// Start a new map for now
	// @todo find a way return to previous map thats a 100% reliably
	GEditor->CreateNewMapForEditing();

	ADD_LATENT_AUTOMATION_COMMAND(FDelayLatentCommand(2.f));

	return bPropertiesMatch;
}

/*******************************************************************************
* FBlueprintReparentTest
*******************************************************************************/

void FBlueprintReparentTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	TArray<FAssetData> Assets;
	FBlueprintAutomationTestUtilities::GetAssetListingFromConfig(TEXT("ReparentTest.ChildrenPackagePaths"), Assets, UBlueprint::StaticClass());

	for (FAssetData const& AssetData : Assets)
	{
		OutBeautifiedNames.Add(AssetData.AssetName.ToString());
		OutTestCommands.Add(AssetData.ObjectPath.ToString());
	}
}

bool FBlueprintReparentTest::RunTest(const FString& BlueprintAssetPath)
{
	bool bTestFailed = false;

	UBlueprint * const BlueprintTemplate = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), NULL, *BlueprintAssetPath));
	if (BlueprintTemplate != NULL)
	{
		// want to explicitly test switching from actors->objects, and vise versa (objects->actors), 
		// also could cover the case of changing non-native parents to native ones
		TArray<UClass*> TestParentClasses;
		if (!BlueprintTemplate->ParentClass->IsChildOf(AActor::StaticClass()))
		{
			TestParentClasses.Add(AActor::StaticClass());
		}
		else
		{
			// not many engine level Blueprintable classes that aren't Actors
			TestParentClasses.Add(USaveGame::StaticClass());
		}

		TArray<FAssetData> Assets;
		FBlueprintAutomationTestUtilities::GetAssetListingFromConfig(TEXT("ReparentTest.ParentsPackagePaths"), Assets, UBlueprint::StaticClass());
		// additionally gather up any blueprints that we explicitly specify though the config
		for (FAssetData const& AssetData : Assets)
		{
			UClass* AssetClass = FindObject<UClass>(ANY_PACKAGE, *AssetData.AssetClass.ToString());
			TestParentClasses.Add(AssetClass);
		}

		for (UClass* Class : TestParentClasses)
		{		 
			UBlueprint* BlueprintObj = FBlueprintAutomationTestUtilities::DuplicateBlueprint(BlueprintTemplate);
			check(BlueprintObj != NULL);
			BlueprintObj->ParentClass = Class;

			if (!FBlueprintAutomationTestUtilities::TestSaveBlueprint(BlueprintObj))
			{
				AddError(FString::Printf(TEXT("Failed to save blueprint after reparenting with %s: '%s'"), *Class->GetName(), *BlueprintAssetPath));
				bTestFailed = true;
			}

			FBlueprintAutomationTestUtilities::UnloadBlueprint(BlueprintObj);
		}

#if WITH_EDITOR
		// clear undo history to ensure that the transaction buffer isn't 
		// holding onto any references to the blueprints we want unloaded
		GEditor->Trans->Reset(NSLOCTEXT("BpAutomation", "ReparentTest", "Reparent Blueprint Test"));
#endif // #if WITH_EDITOR
		// make sure the unloaded blueprints are properly flushed (for future tests)
		CollectGarbage(RF_NoFlags);
	}

	return !bTestFailed;
}

/*******************************************************************************
* FBlueprintRenameTest
*******************************************************************************/

void FBlueprintRenameAndCloneTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	
	TArray<FAssetData> ObjectList;
	AssetRegistryModule.Get().GetAssetsByClass(UBlueprint::StaticClass()->GetFName(), ObjectList);

	for (FAssetData const& Asset : ObjectList)
	{
		FString AssetObjPath = Asset.ObjectPath.ToString();

		FString const Filename = FPackageName::LongPackageNameToFilename(AssetObjPath);
		if (!FAutomationTestFramework::Get().ShouldTestContent(Filename))
		{
			continue;
		}

		FString PackageName, AssetName;
		FBlueprintAutomationTestUtilities::SplitPackagePathAndAssetName(AssetObjPath, PackageName, AssetName);

		if (UPackage* ExistingPackage = FindPackage(NULL, *PackageName))
		{
			if (ExistingPackage->IsRooted())
			{
				continue;
			}
		}

		OutBeautifiedNames.Add(Asset.AssetName.ToString());
		OutTestCommands.Add(AssetObjPath);
	}
}

bool FBlueprintRenameAndCloneTest::RunTest(const FString& BlueprintAssetPath)
{
	bool bTestFailed = false;
	if (FBlueprintAutomationTestUtilities::IsAssetUnsaved(BlueprintAssetPath))
	{
		bTestFailed = true;
		AddError(FString::Printf(TEXT("You have unsaved changes made to '%s', please save them before running this test."), *BlueprintAssetPath));
	}

	bool bIsAlreadyLoaded = false;
	if (FBlueprintAutomationTestUtilities::IsBlueprintLoaded(BlueprintAssetPath))
	{
		bIsAlreadyLoaded = true;
		AddWarning(FString::Printf(TEXT("'%s' is already loaded, and possibly referenced by external objects (unable to perform rename tests... please run again in an empty map)."), *BlueprintAssetPath));
	}

	// track the loaded blueprint (and any other BP dependencies) so we can 
	// unload them if we end up renaming it.
	FScopedBlueprintUnloader NewBlueprintUnloader(/*bAutoOpenScope =*/true, /*bRunGCOnCloseIn =*/false);

	UBlueprint* const OriginalBlueprint = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), NULL, *BlueprintAssetPath));
	if (OriginalBlueprint == NULL)
	{
		bTestFailed = true;
		AddError(FString::Printf(TEXT("Failed to load '%s' (has it been renamed?)."), *BlueprintAssetPath));
	}
	else if (!OriginalBlueprint->SkeletonGeneratedClass || !OriginalBlueprint->GeneratedClass)
	{
		bTestFailed = true;
		AddError(*FString::Printf(TEXT("Unable to load blueprint for: '%s'. Probably it derives from an invalid class."), *BlueprintAssetPath));
	}
	else if (!bTestFailed)
	{
		// duplicate
		{
			UBlueprint* DuplicateBluprint = FBlueprintAutomationTestUtilities::DuplicateBlueprint(OriginalBlueprint);
			if (!FBlueprintAutomationTestUtilities::TestSaveBlueprint(DuplicateBluprint))
			{
				AddError(FString::Printf(TEXT("Failed to save blueprint after duplication: '%s'"), *BlueprintAssetPath));
				bTestFailed = true;
			}
			FBlueprintAutomationTestUtilities::UnloadBlueprint(DuplicateBluprint);
		}

		// rename
		if (!bIsAlreadyLoaded)
		{
			// store the original package so we can manually invalidate it after the move
			UPackage* const OriginalPackage = OriginalBlueprint->GetOutermost();

			FString const BlueprintName = OriginalBlueprint->GetName();
			UPackage* TempPackage = FBlueprintAutomationTestUtilities::CreateTempPackage(BlueprintName);

			FString NewName = FString::Printf(TEXT("%s-Rename"), *BlueprintName);
			NewName = MakeUniqueObjectName(TempPackage, OriginalBlueprint->GetClass(), *NewName).ToString();

			OriginalBlueprint->Rename(*NewName, TempPackage, REN_None);

			if (!FBlueprintAutomationTestUtilities::TestSaveBlueprint(OriginalBlueprint))
			{
				AddError(FString::Printf(TEXT("Failed to save blueprint after rename: '%s'"), *BlueprintAssetPath));
				bTestFailed = true;
			}

			// the blueprint has been moved out of this package, invalidate it so 
			// we don't save it in this state and so we can reload the blueprint later
			FBlueprintAutomationTestUtilities::InvalidatePackage(OriginalPackage);

			// need to unload the renamed blueprint (and any other blueprints 
			// that were relying on it), so that the renamed blueprint doesn't get used by the user
			FBlueprintAutomationTestUtilities::UnloadBlueprint(OriginalBlueprint);
			NewBlueprintUnloader.CloseScope();
		}
		else 
		{
			// no need to unload the blueprint or any of its dependencies (since 
			// we didn't muck with it by renaming it) 
			NewBlueprintUnloader.ClearScope();
		}

#if WITH_EDITOR
		// clear undo history to ensure that the transaction buffer isn't 
		// holding onto any references to the blueprints we want unloaded
		GEditor->Trans->Reset(NSLOCTEXT("BpAutomation", "RenameCloneTest", "Rename and Clone Test"));
#endif // #if WITH_EDITOR
		// make sure the unloaded blueprints are properly flushed (for future tests)
		CollectGarbage(RF_NoFlags);
	}

	return !bTestFailed;
}
