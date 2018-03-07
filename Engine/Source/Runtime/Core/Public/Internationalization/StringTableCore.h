// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "StringTableCoreFwd.h"
#include "Containers/Map.h"
#include "UObject/NameTypes.h"
#include "Containers/UnrealString.h"
#include "Internationalization/Text.h"
#include "LocKeyFuncs.h"

DECLARE_LOG_CATEGORY_EXTERN(LogStringTable, Log, All);

/** Entry within a string table */
class CORE_API FStringTableEntry
{
public:
	/** Create a new string table entry using the given data */
	static FStringTableEntryRef NewStringTableEntry(FStringTableConstRef InOwnerTable, FString InSourceString, FTextDisplayStringPtr InDisplayString)
	{
		return MakeShared<FStringTableEntry, ESPMode::ThreadSafe>(MoveTemp(InOwnerTable), MoveTemp(InSourceString), MoveTemp(InDisplayString));
	}

	/** Default constructor */
	FStringTableEntry();

	/** Create a new string table entry using the given data */
	FStringTableEntry(FStringTableConstRef InOwnerTable, FString InSourceString, FTextDisplayStringPtr InDisplayString);

	/** @return true if this entry is currently owned by a string table, false if it's been disowned (and should be re-cached) */
	bool IsOwned() const;

	/** Disown this string table entry. This is used to notify external code that has cached this entry that it needs to re-cache it from the string table */
	void Disown();

	/** Get the source string of this string table entry */
	const FString& GetSourceString() const;

	/** Get the display string of this string table entry */
	FTextDisplayStringPtr GetDisplayString() const;

private:
	/** The string table that owns us (if any) */
	FStringTableConstWeakPtr OwnerTable;

	/** The source string of this entry */
	FString SourceString;

	/** The display string of this entry */
	FTextDisplayStringPtr DisplayString;
};

/** String table implementation. Holds Key->SourceString pairs of text. */
class CORE_API FStringTable : public TSharedFromThis<FStringTable, ESPMode::ThreadSafe>
{
public:
	/** Create a new string table */
	static FStringTableRef NewStringTable()
	{
		return MakeShared<FStringTable, ESPMode::ThreadSafe>();
	}

	/** Default constructor */
	FStringTable();

	/** Destructor */
	~FStringTable();

	/** @return The asset that owns this string table instance (if any) */
	UStringTable* GetOwnerAsset() const;

	/** Set the asset that owns this string table instance (if any) */
	void SetOwnerAsset(UStringTable* InOwnerAsset);

	/** Has this string table been fully loaded yet? (used during asset loading) */
	bool IsLoaded() const;

	/** Set whether this string table has been fully loaded yet */
	void IsLoaded(const bool bInIsLoaded);

	/** @return The namespace used by all entries in this string table */
	FString GetNamespace() const;

	/** Set the namespace used by all entries in this string table */
	void SetNamespace(const FString& InNamespace);

	/** Get the source string used by the given entry (if any) */
	bool GetSourceString(const FString& InKey, FString& OutSourceString) const;

	/** Set the source string used by the given entry (will replace any existing data for that entry) */
	void SetSourceString(const FString& InKey, const FString& InSourceString);

	/** Remove the given entry (including its meta-data) */
	void RemoveSourceString(const FString& InKey);

	/** Enumerate all source strings in the table. Return true from the enumerator to continue, or false to stop */
	void EnumerateSourceStrings(const TFunctionRef<bool(const FString&, const FString&)>& InEnumerator) const;

	/** Clear all entries from the table (including their meta-data) */
	void ClearSourceStrings(const int32 InSlack = 0);

	/** Find the entry with the given key (if any) */
	FStringTableEntryConstPtr FindEntry(const FString& InKey) const;

	/** Given an entry, check to see if it exists in this table, and if so, get its key */
	bool FindKey(const FStringTableEntryConstRef& InEntry, FString& OutKey) const;

	/** Given the display string of an entry, check to see if it exists in this table, and if so, get its key */
	bool FindKey(const FTextDisplayStringRef& InDisplayString, FString& OutKey) const;

	/** Get the meta-data with the given ID associated with the given entry, or an empty string if not found */
	FString GetMetaData(const FString& InKey, const FName InMetaDataId) const;

	/** Set the meta-data with the given ID associated with the given entry */
	void SetMetaData(const FString& InKey, const FName InMetaDataId, const FString& InMetaDataValue);

	/** Remove the meta-data with the given ID associated with the given entry */
	void RemoveMetaData(const FString& InKey, const FName InMetaDataId);

	/** Enumerate all meta-data associated with the given entry. Return true from the enumerator to continue, or false to stop */
	void EnumerateMetaData(const FString& InKey, const TFunctionRef<bool(FName, const FString&)>& InEnumerator) const;

	/** Remove all meta-data associated with the given entry */
	void ClearMetaData(const FString& InKey);

	/** Clear all meta-data from the table */
	void ClearMetaData(const int32 InSlack = 0);

	/** Serialize this string table to/from an archive */
	void Serialize(FArchive& Ar);

	/** Export the key, string, and meta-data information in this string table to a CSV file (does not export the namespace) */
	bool ExportStrings(const FString& InFilename) const;

	/** Import key, string, and meta-data information from a CSV file to this string table (does not import the namespace) */
	bool ImportStrings(const FString& InFilename);

private:
	/** Pointer back to the asset that owns this table */
	UStringTable* OwnerAsset;

	/** True if this table has been fully loaded (used for assets) */
	bool bIsLoaded;

	/** The namespace to use for all the strings in this table */
	FString TableNamespace;

	/** Mapping between the text key and entry data for the strings within this table */
	TMap<FString, FStringTableEntryPtr, FDefaultSetAllocator, FLocKeyMapFuncs<FStringTableEntryPtr>> KeysToEntries;

	/** Mapping between the display string and the text key for the strings within this table */
	TMap<FTextDisplayStringPtr, FString> DisplayStringsToKeys;

	/** Critical section preventing concurrent modification of KeysToEntries */
	mutable FCriticalSection KeyMappingCS;

	/** Mapping between the text key and its meta-data map */
	typedef TMap<FName, FString> FMetaDataMap;
	TMap<FString, FMetaDataMap, FDefaultSetAllocator, FLocKeyMapFuncs<FMetaDataMap>> KeysToMetaData;

	/** Critical section preventing concurrent modification of KeysToMetaData */
	mutable FCriticalSection KeysToMetaDataCS;
};

/** Interface to allow Core code to access String Table assets from the Engine */
class CORE_API IStringTableEngineBridge
{
public:
	/** Redirect and load a string table asset by its name */
	static void RedirectAndLoadStringTableAsset(FName& InOutTableId, const EStringTableLoadingPolicy InLoadingPolicy)
	{
		if (InstancePtr)
		{
			InstancePtr->RedirectAndLoadStringTableAssetImpl(InOutTableId, InLoadingPolicy);
		}
	}

	/** Collect a string table asset reference */
	static void CollectStringTableAssetReferences(const FName InTableId, FArchive& InAr)
	{
		if (InstancePtr)
		{
			InstancePtr->CollectStringTableAssetReferencesImpl(InTableId, InAr);
		}
	}

protected:
	virtual ~IStringTableEngineBridge() {}

	virtual void RedirectAndLoadStringTableAssetImpl(FName& InOutTableId, const EStringTableLoadingPolicy InLoadingPolicy) = 0;
	virtual void CollectStringTableAssetReferencesImpl(const FName InTableId, FArchive& InAr) = 0;

	/** Singleton instance, populated by the derived type */
	static IStringTableEngineBridge* InstancePtr;
};

/** String table redirect utils */
struct CORE_API FStringTableRedirects
{
	/** Initialize the string table redirects */
	static void InitStringTableRedirects();

	/** Redirect a table ID */
	static void RedirectTableId(FName& InOutTableId, const EStringTableLoadingPolicy InLoadingPolicy);

	/** Redirect a key */
	static void RedirectKey(const FName InTableId, FString& InOutKey);

	/** Redirect a table ID and key */
	static void RedirectTableIdAndKey(FName& InOutTableId, FString& InOutKey, const EStringTableLoadingPolicy InLoadingPolicy);
};

/** String table reference collection utils */
struct CORE_API FStringTableReferenceCollection
{
	/** Collect asset references from the given table ID */
	static void CollectAssetReferences(const FName InTableId, FArchive& InAr);
};
