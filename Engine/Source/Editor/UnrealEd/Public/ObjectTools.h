// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ObjectTools.h: Object-related utilities

=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Serialization/ArchiveUObject.h"
#include "AssetData.h"
#include "UObject/GCObject.h"
#include "CollectionManagerTypes.h"

class FTextureRenderTargetResource;
class SWindow;
class UExporter;
class UFactory;
class USoundWave;

namespace ObjectTools
{
	/** A simple struct to represent the package group name triplet */
	struct FPackageGroupName
	{
		FString PackageName;
		FString GroupName;
		FString ObjectName;
	};

	/** Returns true if the specified object can be displayed in a content browser */
	UNREALED_API bool IsObjectBrowsable( UObject* Obj );

	/**
	 * An archive for collecting object references that are top-level objects.
	 */
	class FArchiveTopLevelReferenceCollector : public FArchiveUObject
	{
	public:
		FArchiveTopLevelReferenceCollector(
			TArray<UObject*>* InObjectArray,
			const TArray<UObject*>& InIgnoreOuters,
			const TArray<UClass*>& InIgnoreClasses );


		/** @return		true if the specified objects should be serialized to determine asset references. */
		FORCEINLINE bool ShouldSearchForAssets(const UObject* Object) const
		{
			// Discard class default objects.
			if ( Object->IsTemplate(RF_ClassDefaultObject) )
			{
				return false;
			}
			// Check to see if we should be based on object class.
			if ( IsAnIgnoreClass(Object) )
			{
				return false;
			}
			// Discard sub-objects of outer objects to ignore.
			if ( IsInIgnoreOuter(Object) )
			{
				return false;
			}
			return true;
		}

		/** @return		true if the specified object is an 'IgnoreClasses' type. */
		FORCEINLINE bool IsAnIgnoreClass(const UObject* Object) const
		{
			for ( int32 ClassIndex = 0 ; ClassIndex < IgnoreClasses.Num() ; ++ClassIndex )
			{
				if ( Object->IsA(IgnoreClasses[ClassIndex]) )
				{
					return true;
				}
			}
			return false;
		}

		/** @return		true if the specified object is not a subobject of one of the IngoreOuters. */
		FORCEINLINE bool IsInIgnoreOuter(const UObject* Object) const
		{
			for ( int32 OuterIndex = 0 ; OuterIndex < IgnoreOuters.Num() ; ++OuterIndex )
			{
				if( ensure( IgnoreOuters[ OuterIndex ] != NULL ) )
				{
					if ( Object->IsIn(IgnoreOuters[OuterIndex]) )
					{
						return true;
					}
				}
			}
			return false;
		}

	private:
		/** 
		 * UObject serialize operator implementation
		 *
		 * @param Object	reference to Object reference
		 * @return reference to instance of this class
		 */
		FArchive& operator<<( UObject*& Obj );

		/** Stored pointer to array of objects we add object references to */
		TArray<UObject*>*		ObjectArray;

		/** Only objects not within these objects will be considered.*/
		const TArray<UObject*>&	IgnoreOuters;

		/** Only objects not of these types will be considered.*/
		const TArray<UClass*>&	IgnoreClasses;
	};

	/** Target package and object name for moving an asset. */
	class FMoveInfo
	{
	public:
		FString FullPackageName;
		FString NewObjName;

		void Set(const TCHAR* InFullPackageName, const TCHAR* InNewObjName);
		
		/** @return		true once valid (non-empty) move info exists. */
		bool IsValid() const;
	};

	/**
	 * Handles fully loading packages for a set of passed in objects.
	 *
	 * @param	Objects				Array of objects whose packages need to be fully loaded
	 * @param	OperationString		Localization key for a string describing the operation; appears in the warning string presented to the user.
	 * 
	 * @return true if all packages where fully loaded, false otherwise
	 */
	bool HandleFullyLoadingPackages( const TArray<UObject*>& Objects, const FText& OperationText );


	/** Duplicates a list of objects
	 *
	 * @param	SelectedObjects			The objects to delete.
	 * @param	SourcePath				A path to use to form a relative path for the copied objects.
	 * @param	DestinationPath			A path to use as a default destination for the copied objects.
	 * @param	OpenDialog				If true, a dialog will open to prompt the user for path information.
	 * @param	OutNewObjects	If non-NULL, returns the list of duplicated objects.
	 */
	UNREALED_API void DuplicateObjects( const TArray<UObject*>& SelectedObjects, const FString& SourcePath = TEXT(""), const FString& DestinationPath = TEXT(""), bool bOpenDialog = true, TArray<UObject*>* OutNewObjects = NULL );

	/** Duplicates a single object
	 *
	 * @param	Object									The objects to delete.
	 * @param	PGN										The new package, group, and name of the object.
	 * @param	InOutPackagesUserRefusedToFullyLoad		A set of packages the user opted out of fully loading. This is used internally to prevent asking multiple times.\
	 * @param	bPromptToOverwrite						If true the user will be prompted to overwrite if duplicating to an existing object.  If false, the duplication will always happen
	 * @retun	The duplicated object or NULL if a failure occurred.
	 */
	UNREALED_API UObject* DuplicateSingleObject(UObject* Object, const FPackageGroupName& PGN, TSet<UPackage*>& InOutPackagesUserRefusedToFullyLoad, bool bPromptToOverwrite = true);
	
	/** Helper struct to detail the results of a consolidation operation */
	struct UNREALED_API FConsolidationResults : public FGCObject
	{
		/** FGCObject interface; Serialize any object references */
		virtual void AddReferencedObjects( FReferenceCollector& Collector ) override
		{
			Collector.AddReferencedObjects( DirtiedPackages );
			Collector.AddReferencedObjects( InvalidConsolidationObjs );
			Collector.AddReferencedObjects( FailedConsolidationObjs );
		}

		/** Packages dirtied by a consolidation operation */
		TArray<UPackage*>	DirtiedPackages;

		/** Objects which were not valid for consolidation */
		TArray<UObject*>	InvalidConsolidationObjs;

		/** Objects which failed consolidation (partially consolidated) */
		TArray<UObject*>	FailedConsolidationObjs;
	};

	/**
	 * Consolidates objects by replacing all references/uses of the provided "objects to consolidate" with references to the "object to consolidate to." This is
	 * useful for situations such as when a particular asset is duplicated in multiple places and it would be handy to allow all uses to point to one particular copy
	 * of the asset. When executed, the function first attempts to directly replace all relevant references located within objects that are already loaded and in memory.
	 * Next, it deletes the "objects to consolidate," leaving behind object redirectors to the "object to consolidate to" in their wake.
	 *
	 * @param	ObjectToConsolidateTo	Object to which all references of the "objects to consolidate" will instead refer to after this operation completes
	 * @param	ObjectsToConsolidate	Objects which all references of which will be replaced with references to the "object to consolidate to"; each will also be deleted
	 *
	 * @note	This function performs NO type checking, by design. It is potentially dangerous to replace references of one type with another, so utilize caution.
	 * @note	The "objects to consolidate" are DELETED by this function.
	 *
	 * @return	Structure of consolidation results, specifying which packages were dirtied, which objects failed consolidation (if any), etc.
	 */
	UNREALED_API FConsolidationResults ConsolidateObjects( UObject* ObjectToConsolidateTo, TArray<UObject*>& ObjectsToConsolidate, bool bShowDeleteConfirmation = true );

	/**
	 * Copies references for selected generic browser objects to the clipboard.
	 */
	UNREALED_API void CopyReferences( const TArray< UObject* >& SelectedObjects ); // const

	UNREALED_API void ShowReferencers( const TArray< UObject* >& SelectedObjects ); // const

	/**
	 * Displays a tree(currently) of all assets which reference the passed in object.  
	 *
	 * @param ObjectToGraph		The object to find references to.
	 * @param InBrowsableTypes	A mapping of classes to browsable types.  The tool only shows browsable types or actors
	 */
	UNREALED_API void ShowReferenceGraph( UObject* ObjectToGraph );
	/**
	 * Displays all of the objects the passed in object references
	 *
	 * @param	Object			Object whose references should be displayed
	 * @param	CollectionName	Name of a collection that needs to be made with these referenced objects
	 * @param	ShareType		The share type of any created collection
	 */
	UNREALED_API void ShowReferencedObjs( UObject* Object, const FString& CollectionName = FString(), ECollectionShareType::Type ShareType = ECollectionShareType::CST_Private);

	/**
	 * Select the object referencers in the level
	 *
	 * @param	Object			Object whose references are to be selected
	 *
	 */
	UNREALED_API void SelectActorsInLevelDirectlyReferencingObject( UObject* RefObj );

	/**
	 * Select the object and it's external referencers' referencers in the level.
	 * This function calls AccumulateObjectReferencersForObjectRecursive to
	 * recursively build a list of objects to check for referencers in the level
	 *
	 * @param	Object				Object whose references are to be selected
	 * @param	bRecurseMaterial	Whether or not we're allowed to recurse the material
	 *
	 */
	UNREALED_API void SelectObjectAndExternalReferencersInLevel( UObject* Object, const bool bRecurseMaterial );

	/**
	 * Recursively add the objects referencers to a single array
	 *
	 * @param	Object				Object whose references are to be selected
	 * @param	Referencers			Array of objects being referenced in level
	 * @param	bRecurseMaterial	Whether or not we're allowed to recurse the material
	 *
	 */
	UNREALED_API void AccumulateObjectReferencersForObjectRecursive( UObject* Object, TArray<UObject*>& Referencers, const bool bRecurseMaterial );

	/**
	 * Shows a confirmation dialog asking the user if it is ok to delete the packages containing the supplied objects
	 *
	 * @param	ObjectsToDelete		The list of objects to delete
	 *
	 * @return true if the user accepted the dialog or no dialog was necessary
	 */
	bool ShowDeleteConfirmationDialog ( const TArray<UObject*>& ObjectsToDelete );

	/**
	 * Collects garbage and marks truely empty packages for delete
	 *
	 * @param	ObjectsDeletedSuccessfully		The list of objects that were recently deleted but not yet cleaned up
	 */
	UNREALED_API void CleanupAfterSuccessfulDelete ( const TArray<UPackage*>& ObjectsDeletedSuccessfully, bool bPerformReferenceCheck = true );

	/**
	 * Deletes the list of objects
	 *
	 * @param	ObjectsToDelete		The list of objects to delete
	 * @param	bShowConfirmation	True when a dialog should prompt the user that he/she is about to delete something
	 *
	 * @return The number of objects successfully deleted
	 */
	UNREALED_API int32 DeleteObjects( const TArray< UObject* >& ObjectsToDelete, bool bShowConfirmation = true );

	/**
	* Deletes the list of objects without checking if they are still being used.  This should not be called directly
	* this is primarily used by the delete system after it has done the work of making sure it's safe to delete.
	*
	* @param	ObjectsToDelete		The list of objects to delete
	*
	* @return The number of objects successfully deleted
	*/
	UNREALED_API int32 DeleteObjectsUnchecked( const TArray< UObject* >& ObjectsToDelete );

	/**
	* Deletes the list of objects
	*
	* @param	AssetsToDelete		The list of assets to delete
	* @param	bShowConfirmation	True when a dialog should prompt the user that he/she is about to delete something
	*
	* @return The number of assets successfully deleted
	*/
	UNREALED_API int32 DeleteAssets( const TArray<FAssetData>& AssetsToDelete, bool bShowConfirmation = true );

	/**
	 * Delete a single object
	 *
	 * @param	ObjectToDelete		The object to delete
	 *
	 * @return If the object was successfully
	 */
	UNREALED_API bool DeleteSingleObject( UObject* ObjectToDelete, bool bPerformReferenceCheck = true );

	/**
	 * Deletes the list of objects
	 *
	 * @param	ObjectsToDelete		The list of objects to delete
	 * @param   ShowConfirmation    Show the confirmation dialog.
	 *
	 * @return The number of objects successfully deleted
	 */
	UNREALED_API int32 ForceDeleteObjects( const TArray< UObject* >& ObjectsToDelete, bool ShowConfirmation = true );

	/**
	 * Utility function to compose a string list of referencing objects
	 *
	 * @param References			Array of references to the relevant object
	 * @param RefObjNames			String list of all objects
	 * @param DefObjNames			String list of all objects referenced in default properties
	 *
	 * @return Whether or not any objects are in default properties
	 */
	UNREALED_API bool ComposeStringOfReferencingObjects( TArray<FReferencerInformation>& References, FString& RefObjNames, FString& DefObjNames );

	/** Information that can be gathered from the move dialog. */
	struct FMoveDialogInfo
	{
		FPackageGroupName PGN;
		bool bOkToAll;
		bool bSavePackages;

		FMoveDialogInfo()
			: bOkToAll(0)
			, bSavePackages(0)
		{}
	};

	/** Sends the redirector to the deleted redirectors package where it will be cleaned up later */
	UNREALED_API void DeleteRedirector(UObjectRedirector* Redirector);

	/**
	 * Helper function for RenameObjectsInternal. This function Updates a FMoveDialogInfo with information from the user. If OkToAll was pressed, it simply updates the relevant information.
	 * 
	 * @param DialogTitle				The title text for the dialog, if it opens
	 * @param Object					The object for which to provide move information.
	 * @param bUniqueDefaultName		If true, when the user is prompted for a name the default supplied name will be unique.
	 * @param SourcePath				A path to use to form a relative path for the renamed objects.
	 * @param DestinationPath			The default target path for the objects. If empty string, the default will be based on the the package name.
	 * @param InOutInfo					The information gathered from the move dialog.
	 * @return true if the move information was successfully extracted from the dialog.
	 */
	bool GetMoveDialogInfo(const FText& DialogTitle, UObject* Object, bool bUniqueDefaultName, const FString& SourcePath, const FString& DestinationPath, FMoveDialogInfo& InOutInfo);

	/**
	 * Internal implementation of rename objects with refs
	 * 
	 * @param Objects					The objects to rename
	 * @param bLocPackages				If true, the objects belong in localized packages
	 * @param ObjectToLanguageExtMap	A mapping of object to matching language (for fixing up localization if the objects are moved ).  Note: Not used if bLocPackages is false
	 * @param SourcePath				A path to use to form a relative path for the renamed objects.
	 * @param DestinationPath			The default target path for the objects. If empty string, the default will be based on the the package name.
	 * @param	OpenDialog				If true, a dialog will open to prompt the user for path information.
	 * @return true when all objects were renamed successfully
	 */
	UNREALED_API bool RenameObjectsInternal( const TArray<UObject*>& Objects, bool bLocPackages, const TMap< UObject*, FString >* ObjectToLanguageExtMap, const FString& SourcePath, const FString& DestinationPath, bool bOpenDialog );

	/**
	 * Renames a single object
	 * 
	 * @param Object								The object to rename
	 * @param PGN									The new package, group, and name of the object.
	 * @param InOutPackagesUserRefusedToFullyLoad	A set of packages the user opted out of fully loading. This is used internally to prevent asking multiple times.
	 * @param InOutErrorMessage						A string with any errors that occurred during the rename.
	 * @param ObjectToLanguageExtMap				A mapping of object to matching language (for fixing up localization if the objects are moved ).  Note: Not used if bLocPackages is false
	 * @param bLeaveRedirector						If true, a redirector will be left to allow unloaded assets to maintain a reference to the renamed object
	 * @return true when the object was renamed successfully
	 */
	UNREALED_API bool RenameSingleObject(UObject* Object, FPackageGroupName& PGN, TSet<UPackage*>& InOutPackagesUserRefusedToFullyLoad, FText& InOutErrorMessage, const TMap< UObject*, FString >* ObjectToLanguageExtMap = NULL, bool bLeaveRedirector = true);

	/** 
	 * Finds all language variants for the passed in sound wave
	 * 
	 * @param OutObjects	A list of found localized sound wave objects
	 * @param OutObjectToLanguageExtMap	A mapping of sound wave objects to their language extension
	 * @param Wave	The sound wave to search for
	 */
	UNREALED_API void AddLanguageVariants( TArray<UObject*>& OutObjects, TMap< UObject*, FString >& OutObjectToLanguageExtMap, USoundWave* Wave );


	/**
	 * Renames an object and leaves redirectors so other content that references it does not break
	 * Also renames all loc instances of the same asset
	 * @param Objects					The objects to rename
	 * @param bIncludeLocInstances		If true, the objects belong in localized packages
	 * @param SourcePath				A path to use to form a relative path for the renamed objects.
	 * @param DestinationPath			The default target path for the objects. If empty string, the default will be based on the the package name.
	 * @param OpenDialog				If true, a dialog will open to prompt the user for path information.
	 * @return true when all objects were renamed successfully
	 */
	UNREALED_API bool RenameObjects( const TArray< UObject* >& SelectedObjects, bool bIncludeLocInstances, const FString& SourcePath = TEXT(""), const FString& DestinationPath = TEXT(""), bool bOpenDialog = true );

	/** Converts all invalid object name characters to _ */
	UNREALED_API FString SanitizeObjectName(const FString& InObjectName);

	/**
	 * Populates two strings with all of the file types and extensions the provided factory supports.
	 *
	 * @param	InFactory		Factory whose supported file types and extensions should be retrieved
	 * @param	out_Filetypes	File types supported by the provided factory, concatenated into a string
	 * @param	out_Extensions	Extensions supported by the provided factory, concatenated into a string
	 */
	UNREALED_API void GenerateFactoryFileExtensions( const UFactory* InFactory, FString& out_Filetypes, FString& out_Extensions, TMultiMap<uint32, UFactory*>& out_FilterIndexToFactory );
	
	/**
	 * Populates two strings with all of the file types and extensions the provided factories support.
	 *
	 * @param	InFactories		Factories whose supported file types and extensions should be retrieved
	 * @param	out_Filetypes	File types supported by the provided factory, concatenated into a string
	 * @param	out_Extensions	Extensions supported by the provided factory, concatenated into a string
	 */
	UNREALED_API void GenerateFactoryFileExtensions( const TArray<UFactory*>& InFactories, FString& out_Filetypes, FString& out_Extensions, TMultiMap<uint32, UFactory*>& out_FilterIndexToFactory );
	
	/**
	 * Generates a list of file types for a given class.
	 */
	UNREALED_API void AppendFactoryFileExtensions( UFactory* InFactory, FString& out_Filetypes, FString& out_Extensions );


	/**
	 * Iterates over all classes and assembles a list of non-abstract UExport-derived type instances.
	 */
	UNREALED_API void AssembleListOfExporters(TArray<UExporter*>& OutExporters);

	/**
	 * Assembles a path from the outer chain of the specified object.
	 */
	UNREALED_API void GetDirectoryFromObjectPath(const UObject* Obj, FString& OutResult);


	/**
	 * Exports the specified objects to file.
	 *
	 * @param	ObjectsToExport					The set of objects to export.
	 * @param	bPromptIndividualFilenames		If true, prompt individually for filenames.  If false, bulk export to a single directory.
	 * @param	ExportPath						receives the value of the path the user chose for exporting.
	 * @param	bUseProvidedExportPath			If true and out_ExportPath is specified, use the value in out_ExportPath as the export path w/o prompting for a directory when applicable
	 */
	DEPRECATED(4.17, "ObjectTools::ExportObjects is deprecated.  Use AssetTools::ExportObjects instead")
	UNREALED_API void ExportObjects( const TArray<UObject*>& ObjectsToExport, bool bPromptIndividualFilenames, FString* ExportPath = NULL, bool bUseProvidedExportPath = false );


	/** Options for in use object tagging */
	enum EInUseSearchOption
	{
		SO_CurrentLevel, // Searches for in use objects refrenced by the current level
		SO_VisibleLevels, // Searches for in use objects referenced by visible levels
		SO_LoadedLevels // Searches for in use objects referenced by all loaded levels
	};

	/**
	 * Tags objects which are in use by levels specified by the search option 
	 *
	 * @param SearchOption	 The search option for finding in use objects
	 */
	UNREALED_API void TagInUseObjects( EInUseSearchOption SearchOption );

	/**
	 * Opens a property window for the selected objects
	 *
	 * @param SelectedObjects	The objects to view in the property window
	 * @return Pointer to the new properties window.
	 */
	UNREALED_API TSharedPtr<SWindow> OpenPropertiesForSelectedObjects( const TArray<UObject*>& SelectedObjects );

	/**
	 * Removes deleted objects from open property windows
	 *
	 * @param DeletedObjects	The objects to remove
	 */
	UNREALED_API void RemoveDeletedObjectsFromPropertyWindows( TArray<UObject*>& DeletedObjects );

	/**
	 * Determines if the asset is placeable in a world.
	 *
	 * @param InWorld	The world.
	 * @param ObjectPath	Object path.
	 * 
	 * @return true if the asset can be placed in the world.
	 */
	UNREALED_API bool IsAssetValidForPlacing(UWorld* InWorld, const FString& ObjectPath);

	/**
	 * Determines if the class is placeable in a world.
	 *
	 * @param InClass	Class to test.
	 * 
	 * @return true if the class can be placed in the world.
	 */
	UNREALED_API bool IsClassValidForPlacing(const UClass* InClass);

	/**
	 * Determines if a given class is a redirector.
	 *
	 * @param Class	The class
	 * 
	 * @return true if the class is a redirector, otherwise false.
	 */
	UNREALED_API bool IsClassRedirector( const UClass* Class );

	/**
	 * Determines if an array of objects are all of interchangeable types.
	 *
	 * @param InProposedObjects	The objects to check.
	 * 
	 * @return true if all objects are interchangeable, otherwise false.
	 */
	UNREALED_API bool AreObjectsOfEquivalantType( const TArray<UObject*>& InProposedObjects );

	/**
	 * Determines if two classes are interchangeable.  This would tell you if you could substitute one object 
	 * reference for another.  For example, Material and MaterialInstances are interchangeable.
	 *
	 * @param ClassA	The first class
	 * @param ClassB	The second class
	 *
	 * @return true if they are interchangeable otherwise false.
	 */
	UNREALED_API bool AreClassesInterchangeable( const UClass* ClassA, const UClass* ClassB );
}




namespace ThumbnailTools
{
	/** Thumbnail texture flush mode */
	namespace EThumbnailTextureFlushMode
	{
		enum Type
		{
			/** Don't flush texture streaming at all */
			NeverFlush = 0,

			/** Aggressively stream resources before rendering thumbnail to avoid blurry textures */
			AlwaysFlush,
		};
	}

	/** Finds the file path/name of an existing package for the specified object full name, or false if the package is not valid.  */
	UNREALED_API bool QueryPackageFileNameForObject( const FString& InFullName, FString& OutPackageFileName );

	/**
	 * Renders a thumbnail for the specified object.
	 *
	 * @param	InObject				the rendered thumbnail will represent this object
	 * @param	InImageWidth			the maximum width of the thumbnail. It may be smaller when supplying a OutThumbnail.
	 * @param	InImageHeight			the maximum height of the thumbnail. It may be smaller when supplying a OutThumbnail.
	 * @param	InRenderTargetResource	if non-NULL, the render target that will receive the thumbnail.
	 * @param	OutThumbnail			if non-NULL, the FObjectThumbnail that will receive the thumbnail data.
	 */
	UNREALED_API void RenderThumbnail( UObject* InObject, const uint32 InImageWidth, const uint32 InImageHeight, EThumbnailTextureFlushMode::Type InFlushMode, FTextureRenderTargetResource* InRenderTargetResource = NULL, FObjectThumbnail* OutThumbnail = NULL );

	/** Generates a thumbnail for the specified object and caches it. Used to generate the thumbnail that gets saved in a package. */
	UNREALED_API FObjectThumbnail* GenerateThumbnailForObjectToSaveToDisk( UObject* InObject );

	/**
	 * Caches a thumbnail into a package's thumbnail map.
	 *
	 * @param	ObjectFullName	the full name for the object to associate with the thumbnail
	 * @param	Thumbnail		the thumbnail to cache; specify NULL to remove the current cached thumbnail
	 * @param	DestPackage		the package that will hold the cached thumbnail
	 *
	 * @return	pointer to the thumbnail data that was cached into the package
	 */
	UNREALED_API FObjectThumbnail* CacheThumbnail( const FString& ObjectFullName, FObjectThumbnail* Thumbnail, UPackage* DestPackage );

	/**
	 * Caches an empty thumbnail entry
	 *
	 * @param	ObjectFullName	the full name for the object to associate with the thumbnail
	 * @param	DestPackage		the package that will hold the cached thumbnail
	 */
	UNREALED_API void CacheEmptyThumbnail( const FString& ObjectFullName, UPackage* DestPackage );

	/** Searches for an object's thumbnail in memory and returns it if found */
	UNREALED_API const FObjectThumbnail* FindCachedThumbnail( const FString& InFullName );

	/** Returns the thumbnail for the specified object or NULL if one doesn't exist yet */
	UNREALED_API FObjectThumbnail* GetThumbnailForObject( UObject* InObject );

	/** Loads thumbnails from the specified package file name */
	UNREALED_API bool LoadThumbnailsFromPackage( const FString& InPackageFileName, const TSet<FName>& InObjectFullNames, FThumbnailMap& InOutThumbnails );

	/** Loads thumbnails from a package unless they're already cached in that package's thumbnail map */
	UNREALED_API bool ConditionallyLoadThumbnailsFromPackage( const FString& InPackageFileName, const TSet< FName >& InObjectFullNames, FThumbnailMap& InOutThumbnails );

	/** Loads thumbnails for the specified objects (or copies them from a cache, if they're already loaded.) */
	UNREALED_API bool ConditionallyLoadThumbnailsForObjects( const TArray< FName >& InObjectFullNames, FThumbnailMap& InOutThumbnails );
	/** Standard thumbnail height setting used by generation */
	const int32 DefaultThumbnailSize=256;

	/** If the given asset has a custom thumbnail cached or on the disk. */
	UNREALED_API bool AssetHasCustomThumbnail(const FAssetData& InAssetData);
}
