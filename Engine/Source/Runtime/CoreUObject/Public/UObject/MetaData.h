// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"

/*-----------------------------------------------------------------------------
	UMetaData.
-----------------------------------------------------------------------------*/

/**
 * An object that holds a map of key/value pairs. 
 */
class COREUOBJECT_API UMetaData : public UObject
{
	DECLARE_CLASS_INTRINSIC(UMetaData, UObject, 0, TEXT("/Script/CoreUObject"))

public:
	/**
	 * Mapping between an object, and its key->value meta-data pairs. 
	 */
	TMap< FWeakObjectPtr, TMap<FName, FString> > ObjectMetaDataMap;

	/**
	 * Root-level (not associated with a particular object) key->value meta-data pairs.
	 * Meta-data associated with the package itself should be stored here.
	 */
	TMap< FName, FString > RootMetaDataMap;

public:
	// MetaData utility functions

	/**
	 * Return the value for the given key in the given property
	 * @param Object the object to lookup the metadata for
	 * @param Key The key to lookup
	 * @return The value if found, otherwise an empty string
	 */
	const FString& GetValue(const UObject* Object, const TCHAR* Key);
	
	/**
	 * Return the value for the given key in the given property
	 * @param Object the object to lookup the metadata for
	 * @param Key The key to lookup
	 * @return The value if found, otherwise an empty string
	 */
	const FString& GetValue(const UObject* Object, FName Key);

	/**
	 * Return whether or not the Key is in the meta data
	 * @param Object the object to lookup the metadata for
	 * @param Key The key to query for existence
	 * @return true if found
	 */
	bool HasValue(const UObject* Object, const TCHAR* Key);

	/**
	 * Return whether or not the Key is in the meta data
	 * @param Object the object to lookup the metadata for
	 * @param Key The key to query for existence
	 * @return true if found
	 */
	bool HasValue(const UObject* Object, FName Key);

	/**
	 * Is there any metadata for this property?
	 * @param Object the object to lookup the metadata for
	 * @return TrUE if the object has any metadata at all
	 */
	bool HasObjectValues(const UObject* Object);

	/**
	 * Set the key/value pair in the Property's metadata
	 * @param Object the object to set the metadata for
	 * @Values The metadata key/value pairs
	 */
	void SetObjectValues(const UObject* Object, const TMap<FName, FString>& Values);

	/**
	 * Set the key/value pair in the Object's metadata
	 * @param Object the object to set the metadata for
	 * @param Key A key to set the data for
	 * @param Value The value to set for the key
	 */
	void SetValue(const UObject* Object, const TCHAR* Key, const TCHAR* Value);

	/**
	 * Set the key/value pair in the Property's metadata
	 * @param Object the object to set the metadata for
	 * @param Key A key to set the data for
	 * @param Value The value to set for the key
	 * @Values The metadata key/value pairs
	 */
	void SetValue(const UObject* Object, FName Key, const TCHAR* Value);

	/** 
	 *	Remove any entry with the supplied Key form the Property's metadata 
	 *	@param Object the object to clear the metadata for
	 *	@param Key A key to clear the data for
	 */
	void RemoveValue(const UObject* Object, const TCHAR* Key);

	/** 
	 *	Remove any entry with the supplied Key form the Property's metadata 
	 *	@param Object the object to clear the metadata for
	 *	@param Key A key to clear the data for
	 */
	void RemoveValue(const UObject* Object, FName Key);

	/** Find the name/value map for metadata for a specific object */
	static TMap<FName, FString>* GetMapForObject(const UObject* Object);

	/** Copy all metadata from the source object to the destination object. This will add to any existing metadata entries for SourceObject. */
	static void CopyMetadata(UObject* SourceObject, UObject* DestObject);

	/**
	 * Removes any metadata entries that are to objects not inside the same package as this UMetaData object.
	 */
	void RemoveMetaDataOutsidePackage();

	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	virtual bool NeedsLoadForEditorGame() const override;
	virtual bool IsAsset() const override { return false; }
	// End of UObject interface

	// Returns the remapped key name, or NAME_None was not remapped.
	static FName GetRemappedKeyName(FName OldKey);

#if HACK_HEADER_GENERATOR
	// Required by UHT makefiles for internal data serialization.
	friend struct FMetadataArchiveProxy;
#endif

private:
	static void InitializeRedirectMap();

private:
	// Redirect map from deprecated keys to current key names
	static TMap<FName, FName> KeyRedirectMap;
};


#if WITH_EDITOR
struct FMetaDataUtilities
{
private:
	/** Console command for dumping all metadata */
	static class FAutoConsoleCommand DumpAllConsoleCommand;

public:
	/** Find all UMetadata and print its contents to the log */
	COREUOBJECT_API static void DumpAllMetaData();

	/** Output contents of this metadata object to the log */
	COREUOBJECT_API static void DumpMetaData(UMetaData* Object);

private:
	friend class UObject;

	/** Helper class to backup and move the metadata for a given UObject (and optionally its children). */
	class FMoveMetadataHelperContext
	{
	public:
		/**
		 * Backs up the metadata for the UObject (and optionally its children).
		 *
		 * @param	SourceObject		The main UObject to move metadata for.
		 * @param	bSearchChildren		When true all the metadata for classes 
		 */
		FMoveMetadataHelperContext(UObject *SourceObject, bool bSearchChildren);

		/**
		 * Patches up the new metadata on destruction.
		 */
		~FMoveMetadataHelperContext();
	private:

		/** Keep the old package around so we can pull in the metadata without actually duplicating it. */
		UPackage* OldPackage;

		/** Cache a pointer to the object so we can do the search on the old metadata. */
		UObject* OldObject;

		/** When true, search children as well. */
		bool bShouldSearchChildren;
	};

private:
	FMetaDataUtilities() {}
};

#endif //WITH_EDITOR
