// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Commandlets/Commandlet.h"
#include "Engine/EngineTypes.h"
#include "ResavePackagesCommandlet.generated.h"


// Log category should be accessible by derived classes
UNREALED_API DECLARE_LOG_CATEGORY_EXTERN(LogContentCommandlet, Log, All);

UCLASS()
// Added UNREALED_API to expose this to the save packages test
class UNREALED_API UResavePackagesCommandlet : public UCommandlet
{
    GENERATED_UCLASS_BODY()

protected:

	enum EBrevity
	{
		VERY_VERBOSE,
		INFORMATIVE,
		ONLY_ERRORS
	};

	EBrevity Verbosity;

	/** only packages that have this version or higher will be resaved; a value of IGNORE_PACKAGE_VERSION indicates that there is no minimum package version */
	int32 MinResaveUE4Version;

	/**
	 * Limits resaving to packages with this UE4 package version or lower.
	 * A value of IGNORE_PACKAGE_VERSION (default) removes this limitation.
	 */
	int32 MaxResaveUE4Version;

	/**
	 * Limits resaving to packages with this licensee package version or lower.
	 * A value of IGNORE_PACKAGE_VERSION (default) removes this limitation.
	 */
	int32 MaxResaveLicenseeUE4Version;

	/** 
	 * Maximum number of packages to resave to avoid having a massive sync
	 * A value of -1 (default) removes this limitation.
	 */
	int32 MaxPackagesToResave;

	/** allows users to save only packages with a particular class in them (useful for fixing content) */
	TArray<FName> ResaveClasses;

	// If non-empty, this substring has to be present in the package name for the commandlet to process it
	FString PackageSubstring;

	// strip editor only content
	bool bStripEditorOnlyContent;

	// skip the assert when a package can not be opened
	bool bCanIgnoreFails;

	/** load all packages, and display warnings for those packages which would have been resaved but were read-only */
	bool bVerifyContent;

	/** if we should only save dirty packages **/
	bool bOnlySaveDirtyPackages;

	/** if we should auto checkout packages that need to be saved**/
	bool bAutoCheckOut;

	/** if we should auto checkin packages that were checked out**/
	bool bAutoCheckIn;

	/** Should we build lighting for the packages we are saving? **/
	bool bShouldBuildLighting;

	/** Should we build texture streaming for the packages we are saving? **/
	bool bShouldBuildTextureStreaming;

	/** Ignore package version changelist **/
	bool bIgnoreChangelist;

	/** Running count of packages that got modified and will need to be resaved */
	int32 PackagesRequiringResave;

	/** Only collect garbage after N packages */
	int32 GarbageCollectionFrequency;

 	/** Lighting Build Quality(default: Production) */
 	ELightingBuildQuality LightingBuildQuality;
 
	/** List of files to submit */
	TArray<FString> FilesToSubmit;

	/** The list of switches that were passed on the commandline */
	TArray<FString> Switches;

	/** List of redirector packages that should be fixed up at the end */
	TArray<FString> RedirectorsToFixup;

	/**
	 * Evaluates the command-line to determine which maps to check.  By default all maps are checked
	 * Provides child classes with a chance to initialize any variables, parse the command line, etc.
	 *
	 * @param	Tokens			the list of tokens that were passed to the commandlet
	 * @param	MapPathNames	receives the list of path names for the maps that will be checked.
	 *
	 * @return	0 to indicate that the commandlet should continue; otherwise, the error code that should be returned by Main()
	 */
	virtual int32 InitializeResaveParameters( const TArray<FString>& Tokens, TArray<FString>& MapPathNames );

	/** Loads and saves a single package */
	virtual void LoadAndSaveOnePackage(const FString& Filename);

	/** Checks to see if a package should be skipped */
	virtual bool ShouldSkipPackage(const FString& Filename);

	/** Deletes a single package */
	virtual void DeleteOnePackage(const FString& Filename);

	/**
	 * Allow the commandlet to perform any operations on the export/import table of the package before all objects in the package are loaded.
	 *
	 * @param	PackageLinker	the linker for the package about to be loaded
	 * @param	bSavePackage	[in]	indicates whether the package is currently going to be saved
	 *							[out]	set to true to resave the package
	 */
	virtual void PerformPreloadOperations( FLinkerLoad* PackageLinker, bool& bSavePackage );

	/**
	 * Allows the commandlet to perform any additional operations on the object before it is resaved.
	 *
	 * @param	Object			the object in the current package that is currently being processed
	 * @param	bSavePackage	[in]	indicates whether the package is currently going to be saved
	 *							[out]	set to true to resave the package
	 */
	virtual void PerformAdditionalOperations( class UObject* Object, bool& bSavePackage );

	/**
	 * Allows the commandlet to perform any additional operations on the package before it is resaved.
	 *
	 * @param	Package			the package that is currently being processed
	 * @param	bSavePackage	[in]	indicates whether the package is currently going to be saved
	 *							[out]	set to true to resave the package
	 */
	virtual void PerformAdditionalOperations( class UPackage* Package, bool& bSavePackage );

	/**
	* Allows the commandlet to perform any additional operations on the world before it is resaved.
	*
	* @param	World			the world that is currently being processed
	* @param	bSavePackage	[in]	indicates whether the package is currently going to be saved
	*							[out]	set to true to resave the package
	*/
	virtual void PerformAdditionalOperations(class UWorld* World, bool& bSavePackage);

	/**
	 * Removes any UClass exports from packages which aren't script packages.
	 *
	 * @param	Package			the package that is currently being processed
	 *
	 * @return	true to resave the package
	 */
	bool CleanClassesFromContentPackages( class UPackage* Package );

	// Get the changelist description to use if automatically checking packages out
	virtual FText GetChangelistDescription() const;

	bool CheckoutFile(const FString& Filename, bool bAddFile = false);

	// Print out a message only if running in very verbose mode
	void VerboseMessage(const FString& Message);

public:		
	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface
};
