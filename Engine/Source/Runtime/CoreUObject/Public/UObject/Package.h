// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "Misc/WorldCompositionUtility.h"
#include "Templates/ScopedPointer.h"
#include "Misc/OutputDeviceError.h"
#include "Misc/ObjectThumbnail.h"
#include "Serialization/CustomVersion.h"
#include "UniquePtr.h"

class Error;

/**
 * Represents the result of saving a package
 */
enum class ESavePackageResult
{
	/** Package was saved successfully */
	Success, 
	/** Unknown error occured when saving package */
	Error,
	/** Canceled by user */
	Canceled,
	/** [When cooking] Package was not saved because it contained editor-only data */
	ContainsEditorOnlyData,
	/** [When cooking] Package was not saved because it was referenced by editor-only properties */
	ReferencedOnlyByEditorOnlyData, 
	/** [When cooking] Package was not saved because it contains assets that were converted into native code */
	ReplaceCompletely,
	/** [When cooking] Package was saved, but we should generate a stub so that other converted packages can interface with it*/
	GenerateStub
};

/**
 * Struct returned from save package, contains the enum as well as extra data about what was written
 */
struct FSavePackageResultStruct
{
	/** Success/failure of the save operation */
	ESavePackageResult Result;

	/** Total size of all files written out, including bulk data */
	int64 TotalFileSize;

	/** Constructors, it will implicitly construct from the result enum */
	FSavePackageResultStruct() : Result(ESavePackageResult::Error), TotalFileSize(0) {}
	FSavePackageResultStruct(ESavePackageResult InResult) : Result(InResult), TotalFileSize(0) {}
	FSavePackageResultStruct(ESavePackageResult InResult, int64 InTotalFileSize) : Result(InResult), TotalFileSize(InTotalFileSize) {}

	bool operator==(const FSavePackageResultStruct& Other) const
	{
		return Result == Other.Result;
	}

	bool operator!=(const FSavePackageResultStruct& Other) const
	{
		return Result != Other.Result;
	}

};

COREUOBJECT_API void StartSavingEDLCookInfoForVerification();
COREUOBJECT_API void VerifyEDLCookInfo();

/**
 * A package.
 */
PRAGMA_DISABLE_DEPRECATION_WARNINGS // Required for auto-generated functions referencing PackageFlags
class COREUOBJECT_API UPackage : public UObject
{
	// Have to unwind this macro to support the reference variable, can go back to commented declaration when removing the deprecated variable
	// DECLARE_CASTED_CLASS_INTRINSIC(UPackage, UObject, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UPackage)

	DECLARE_CASTED_CLASS_INTRINSIC_NO_CTOR_NO_VTABLE_CTOR( UPackage, UObject, 0, TEXT("/Script/CoreUObject"), CASTCLASS_UPackage, NO_API )
	/** DO NOT USE. This constructor is for internal usage only for hot-reload purposes. */
	UPackage(FVTableHelper& Helper)
		: Super(Helper)
		, PackageFlagsPrivate(PackageFlags) 
	{
	};


public:

	UPackage(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get())
		: Super(ObjectInitializer)
		, PackageFlagsPrivate(PackageFlags)
	{
	}

	/** delegate type for package dirty state events.  ( Params: UPackage* ModifiedPackage ) */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPackageDirtyStateChanged, class UPackage*);
	/** delegate type for package saved events ( Params: const FString& PackageFileName, UObject* Outer ) */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPackageSaved, const FString&, UObject*);					
	/** delegate type for when a package is marked as dirty via UObjectBaseUtilty::MarkPackageDirty ( Params: UPackage* ModifiedPackage, bool bWasDirty ) */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPackageMarkedDirty, class UPackage*, bool);
	/** delegate type for when a package is about to be saved */
	DECLARE_MULTICAST_DELEGATE_OneParam(FPreSavePackage, class UPackage*);

	/** Delegate to notify subscribers when a package is about to be saved. */
	static FPreSavePackage PreSavePackageEvent;
	/** Delegate to notify subscribers when a package has been saved. This is triggered when the package saving
	 *  has completed and was successful. */
	static FOnPackageSaved PackageSavedEvent;
	/** Delegate to notify subscribers when the dirty state of a package is changed.
	 *  Allows the editor to register the modified package as one that should be prompted for source control checkout. 
	 *  Use Package->IsDirty() to get the updated dirty state of the package */
	static FOnPackageDirtyStateChanged PackageDirtyStateChangedEvent;
	/** 
	 * Delegate to notify subscribers when a package is marked as dirty via UObjectBaseUtilty::MarkPackageDirty 
	 * Note: Unlike FOnPackageDirtyStateChanged, this is always called, even when the package is already dirty
	 * Use bWasDirty to check the previous dirty state of the package
	 * Use Package->IsDirty() to get the updated dirty state of the package
	 */
	static FOnPackageMarkedDirty PackageMarkedDirtyEvent;

private:
	/** Used by the editor to determine if a package has been changed.																							*/
	bool	bDirty;
#if WITH_EDITORONLY_DATA
	/** True if this package is only referenced by editor-only properties */
	bool bLoadedByEditorPropertiesOnly;
#endif
public:
	/** Whether this package has been fully loaded (aka had all it's exports created) at some point.															*/
	mutable bool	bHasBeenFullyLoaded;
private:
	/** Indicates which folder to display this package under in the Generic Browser's list of packages. If not specified, package is added to the root level.	*/
	FName	FolderName;
	/** Time in seconds it took to fully load this package. 0 if package is either in process of being loaded or has never been fully loaded.					*/
	float LoadTime;

	/** GUID of package if it was loaded from disk; used by netcode to make sure packages match between client and server */
	FGuid Guid;
	/** Chunk IDs for the streaming install chunks this package will be placed in.  Empty for no chunk */
	TArray<int32> ChunkIDs;
	/** for packages that were a forced export in another package (seekfree loading), the name of that base package, otherwise NAME_None */
	FName ForcedExportBasePackageName;
public:

	virtual bool IsNameStableForNetworking() const override { return true; }		// For now, assume all packages have stable net names

#if WITH_EDITORONLY_DATA
	/** Sets the bLoadedByEditorPropertiesOnly flag */
	void SetLoadedByEditorPropertiesOnly(bool bIsEditorOnly, bool bRecursive = false);
	/** returns true when the package is only referenced by editor-only flag */
	bool IsLoadedByEditorPropertiesOnly() const { return bLoadedByEditorPropertiesOnly; }
#endif

private:
	/** New private Package Flags. When deprecated PackageFlags removed, make this a member instead of a reference */
	uint32&	PackageFlagsPrivate;

public:

	/** Old deprecated public Package flags, serialized. When deprecation removed make PackageFlagsPrivate no longer a reference */
	DEPRECATED(4.10, "Direct access of PackageFlags is deprecated, use Get/Set/Clear/HasAny/HasAll PackageFlags instead.")
	uint32	PackageFlags;

	/** The name of the file that this package was loaded from */
	FName	FileName;

	/** Linker load associated with this package */
	class FLinkerLoad* LinkerLoad;

	/** Linker package version this package has been serialized with. This is mostly used by PostLoad **/
	int32 LinkerPackageVersion;

	/** Linker licensee version this package has been serialized with. This is mostly used by PostLoad **/
	int32 LinkerLicenseeVersion;

	/** Linker custom version container this package has been serialized with. This is mostly used by PostLoad **/
	FCustomVersionContainer LinkerCustomVersion;

	/** size of the file for this package; if the package was not loaded from a file or was a forced export in another package, this will be zero */
	int64 FileSize;
	
	/** Editor only: Thumbnails stored in this package */
	TUniquePtr< FThumbnailMap > ThumbnailMap;

	// MetaData for the editor, or NULL in the game
	class UMetaData*	MetaData;
	
	// World browser information
	TUniquePtr< FWorldTileInfo > WorldTileInfo;

	TMap<FName, int32> ClassUniqueNameIndexMap;

	/** Editor only: PIE instance ID this package belongs to, INDEX_NONE otherwise */
	int32 PIEInstanceID;

#if WITH_EDITORONLY_DATA
	/** True if this packages has been cooked for the editor / opened cooked by the editor */
	bool bIsCookedForEditor;
#endif

	/**
	 * Called after the C++ constructor and after the properties have been initialized, but before the config has been loaded, etc.
	 * mainly this is to emulate some behavior of when the constructor was called after the properties were initialized.
	 */
	virtual void PostInitProperties() override;

	virtual void BeginDestroy() override;

	/** Serializer */
	virtual void Serialize( FArchive& Ar ) override;

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	/** Packages are never assets */
	virtual bool IsAsset() const override { return false; }

	// UPackage interface.

	/**
	 * Sets the time it took to load this package.
	 */
	void SetLoadTime( float InLoadTime )
	{
		LoadTime = InLoadTime;
	}

	/**
	 * Returns the time it took the last time this package was fully loaded, 0 otherwise.
	 *
	 * @return Time it took to load.
	 */
	float GetLoadTime()
	{
		return LoadTime;
	}

	/**
	 * Get the package's folder name
	 * @return		Folder name
	 */
	FName GetFolderName() const
	{
		return FolderName;
	}

	/**
	 * Set the package's folder name
	 */
	void SetFolderName (FName name)
	{
		FolderName = name;
	}

	/**
	 * Marks/Unmarks the package's bDirty flag
	 */
	void SetDirtyFlag( bool bIsDirty );

	/**
	 * Returns whether the package needs to be saved.
	 *
	 * @return		true if the package is dirty and needs to be saved, false otherwise.
	 */
	bool IsDirty() const
	{
		return bDirty;
	}

	/**
	 * Marks this package as being fully loaded.
	 */
	void MarkAsFullyLoaded()
	{
		bHasBeenFullyLoaded = true;
	}

	/**
	 * Returns whether the package is fully loaded.
	 *
	 * @return true if fully loaded or no file associated on disk, false otherwise
	 */
	bool IsFullyLoaded() const;

	/**
	 * Fully loads this package. Safe to call multiple times and won't clobber already loaded assets.
	 */
	void FullyLoad();

	/**
	 * Tags the Package's metadata
	 */
	virtual void TagSubobjects(EObjectFlags NewFlags) override;

	/**
	 * Called to indicate that this package contains a ULevel or UWorld object.
	 */
	void ThisContainsMap() 
	{
		SetPackageFlags(PKG_ContainsMap);
	}

	/**
	 * Returns whether this package contains a ULevel or UWorld object.
	 *
	 * @return		true if package contains ULevel/ UWorld object, false otherwise.
	 */
	bool ContainsMap() const
	{
		return HasAnyPackageFlags(PKG_ContainsMap);
	}

	/**
	 * Called to indicate that this package contains data required to be gathered for localization.
	 */
	void ThisRequiresLocalizationGather(bool Value)
	{
		if(Value)
		{
			SetPackageFlags(PKG_RequiresLocalizationGather);
		}
		else
		{
			ClearPackageFlags(PKG_RequiresLocalizationGather);
		}
	}

	/**
	 * Returns whether this package contains data required to be gathered for localization.
	 *
	 * @return		true if package contains contains data required to be gathered for localization, false otherwise.
	 */
	bool RequiresLocalizationGather() const
	{
		return HasAnyPackageFlags(PKG_RequiresLocalizationGather);
	}

// UE-21181 - trying to track when a flag gets set on a package due to PIE
#if WITH_EDITOR
	static UPackage* EditorPackage;
	void SetPackageFlagsTo( uint32 NewFlags );
#else
	/**
	 * Sets all package flags to the specified values.
	 *
	 * @param	NewFlags		New value for package flags
	 */
	FORCEINLINE void SetPackageFlagsTo( uint32 NewFlags )
	{
		PackageFlagsPrivate = NewFlags;
	}
#endif

	/**
	 * Set the specified flags to true. Does not affect any other flags.
	 *
	 * @param	NewFlags		Package flags to enable
	 */
	FORCEINLINE void SetPackageFlags( uint32 NewFlags )
	{
		SetPackageFlagsTo(PackageFlagsPrivate | NewFlags);
	}

	/**
	 * Set the specified flags to false. Does not affect any other flags.
	 *
	 * @param	NewFlags		Package flags to disable
	 */
	FORCEINLINE void ClearPackageFlags( uint32 NewFlags )
	{
		SetPackageFlagsTo(PackageFlagsPrivate & ~NewFlags);
	}

	/**
	 * Used to safely check whether the passed in flag is set.
	 *
	 * @param	FlagsToCheck		Package flags to check for
	 *
	 * @return	true if the passed in flag is set, false otherwise
	 *			(including no flag passed in, unless the FlagsToCheck is CLASS_AllFlags)
	 */
	FORCEINLINE bool HasAnyPackageFlags( uint32 FlagsToCheck ) const
	{
		return (PackageFlagsPrivate & FlagsToCheck) != 0;
	}

	/**
	 * Used to safely check whether all of the passed in flags are set.
	 *
	 * @param FlagsToCheck	Package flags to check for
	 * @return true if all of the passed in flags are set (including no flags passed in), false otherwise
	 */
	FORCEINLINE bool HasAllPackagesFlags( uint32 FlagsToCheck ) const
	{
		return ((PackageFlagsPrivate & FlagsToCheck) == FlagsToCheck);
	}

	/**
	 * Gets the package flags.
	 *
	 * @return	The package flags.
	 */
	FORCEINLINE uint32 GetPackageFlags() const
	{
		return PackageFlagsPrivate;
	}

	/** Returns true if this package has a thumbnail map */
	bool HasThumbnailMap() const
	{
		return ThumbnailMap.IsValid();
	}
	
	/** Returns the thumbnail map for this package (const).  Only call this if HasThumbnailMap returns true! */
	const FThumbnailMap& GetThumbnailMap() const
	{
		check( HasThumbnailMap() );
		return *ThumbnailMap;
	}

	/** Access the thumbnail map for this package.  Only call this if HasThumbnailMap returns true! */
	FThumbnailMap& AccessThumbnailMap()
	{
		check( HasThumbnailMap() );
		return *ThumbnailMap;
	}
	
	/** returns our Guid */
	FORCEINLINE FGuid GetGuid() const
	{
		return Guid;
	}
	/** makes our a new fresh Guid */
	FORCEINLINE void MakeNewGuid()
	{
		Guid = FGuid::NewGuid();
	}
	/** sets a specific Guid */
	FORCEINLINE void SetGuid(FGuid NewGuid)
	{
		Guid = NewGuid;
	}

	/** returns our FileSize */
	FORCEINLINE int64 GetFileSize()
	{
		return FileSize;
	}

	/** returns our ChunkIDs */
	FORCEINLINE const TArray<int32>& GetChunkIDs() const
	{
		return ChunkIDs;
	}
	/** sets our ChunkIDs */
	FORCEINLINE void SetChunkIDs(const TArray<int32>& InChunkIDs)
	{
		ChunkIDs = InChunkIDs;
	}

	/** returns ForcedExportBasePackageName */
	FORCEINLINE FName GetForcedExportBasePackageName()
	{
		return ForcedExportBasePackageName;
	}

	////////////////////////////////////////////////////////
	// MetaData 

	/**
	 * Gets (after possibly creating) a metadata object for this package
	 *
	 * @param	bAllowLoadObject				Can load an object to find it's UMetaData if not currently loaded.
	 *
	 * @return A valid UMetaData pointer for all objects in this package
	 */
	class UMetaData* GetMetaData();

	/**
	 * Save one specific object (along with any objects it references contained within the same Outer) into an Unreal package.
	 * 
	 * @param	InOuter							the outer to use for the new package
	 * @param	Base							the object that should be saved into the package
	 * @param	TopLevelFlags					For all objects which are not referenced [either directly, or indirectly] through Base, only objects
	 *											that contain any of these flags will be saved.  If 0 is specified, only objects which are referenced
	 *											by Base will be saved into the package.
	 * @param	Filename						the name to use for the new package file
	 * @param	Error							error output
	 * @param	Conform							if non-NULL, all index tables for this will be sorted to match the order of the corresponding index table
	 *											in the conform package
	 * @param	bForceByteSwapping				whether we should forcefully byte swap before writing to disk
	 * @param	bWarnOfLongFilename				[opt] If true (the default), warn when saving to a long filename.
	 * @param	SaveFlags						Flags to control saving
	 * @param	TargetPlatform					The platform being saved for
	 * @param	FinalTimeStamp					If not FDateTime::MinValue(), the timestamp the saved file should be set to. (Intended for cooking only...)
	 *
	 * @return	FSavePackageResultStruct enum value with the result of saving a package as well as extra data
	 */
	static FSavePackageResultStruct Save(UPackage* InOuter, UObject* Base, EObjectFlags TopLevelFlags, const TCHAR* Filename,
		FOutputDevice* Error=GError, FLinkerLoad* Conform=NULL, bool bForceByteSwapping=false, bool bWarnOfLongFilename=true, 
		uint32 SaveFlags=SAVE_None, const class ITargetPlatform* TargetPlatform = NULL, const FDateTime& FinalTimeStamp = FDateTime::MinValue(), bool bSlowTask = true );

	/**
	* Save one specific object (along with any objects it references contained within the same Outer) into an Unreal package.
	*
	* @param	InOuter							the outer to use for the new package
	* @param	Base							the object that should be saved into the package
	* @param	TopLevelFlags					For all objects which are not referenced [either directly, or indirectly] through Base, only objects
	*											that contain any of these flags will be saved.  If 0 is specified, only objects which are referenced
	*											by Base will be saved into the package.
	* @param	Filename						the name to use for the new package file
	* @param	Error							error output
	* @param	Conform							if non-NULL, all index tables for this will be sorted to match the order of the corresponding index table
	*											in the conform package
	* @param	bForceByteSwapping				whether we should forcefully byte swap before writing to disk
	* @param	bWarnOfLongFilename				[opt] If true (the default), warn when saving to a long filename.
	* @param	SaveFlags						Flags to control saving
	* @param	TargetPlatform					The platform being saved for
	* @param	FinalTimeStamp					If not FDateTime::MinValue(), the timestamp the saved file should be set to. (Intended for cooking only...)
	*
	* @return	true if the package was saved successfully.
	*/
	static bool SavePackage(UPackage* InOuter, UObject* Base, EObjectFlags TopLevelFlags, const TCHAR* Filename,
		FOutputDevice* Error = GError, FLinkerLoad* Conform = NULL, bool bForceByteSwapping = false, bool bWarnOfLongFilename = true,
		uint32 SaveFlags = SAVE_None, const class ITargetPlatform* TargetPlatform = NULL, const FDateTime& FinalTimeStamp = FDateTime::MinValue(), bool bSlowTask = true);

	/** Wait for any SAVE_Async file writes to complete **/
	static void WaitForAsyncFileWrites();

	/**
	 * Static: Saves thumbnail data for the specified package outer and linker
	 *
	 * @param	InOuter							the outer to use for the new package
	 * @param	Linker							linker we're currently saving with
	 */
	static void SaveThumbnails( UPackage* InOuter, FLinkerSave* Linker );

	/**
	 * Static: Saves asset registry data for the specified package outer and linker
	 *
	 * @param	InOuter							the outer to use for the new package
	 * @param	Linker							linker we're currently saving with
	 */
	static void SaveAssetRegistryData( UPackage* InOuter, FLinkerSave* Linker );

	/**
	 * Static: Saves the level information used by the World browser
	 *
	 * @param	InOuter							the outer to use for the new package
	 * @param	Linker							linker we're currently saving with
	 */
	static void SaveWorldLevelInfo( UPackage* InOuter, FLinkerSave* Linker );

	/**
	 * Determines if a package contains no more assets.
	 *
	 * @param Package			the package to test
	 * @param LastReferencer	the optional last UObject referencer to this package. This object will be excluded when determining if the package is empty
	 * @return true if Package contains no more assets.
	 */
	static bool IsEmptyPackage(UPackage* Package, const UObject* LastReferencer = NULL);

	/**
	 * Determines the set of object marks that should be excluded for the target platform
	 *
	 * @param TargetPlatform	The platform being saved for
	 * @param bIsCooking		Whether we are cooking or not
	 *
	 * @return Excluded object marks specific for the particular target platform, objects with any of these marks will be rejected from the cook
	 */
	static EObjectMark GetExcludedObjectMarksForTargetPlatform( const class ITargetPlatform* TargetPlatform, const bool bIsCooking );

};
PRAGMA_ENABLE_DEPRECATION_WARNINGS
