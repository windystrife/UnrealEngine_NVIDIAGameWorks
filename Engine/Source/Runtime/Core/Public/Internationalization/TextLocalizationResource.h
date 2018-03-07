// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "LocKeyFuncs.h"

/** Utility class for working with Localization MetaData Resource (LocMeta) files. */
class CORE_API FTextLocalizationMetaDataResource
{
public:
	FString NativeCulture;
	FString NativeLocRes;

	/** Load the given LocMeta file into this resource. */
	bool LoadFromFile(const FString& FilePath);

	/** Load the given LocMeta archive into this resource. */
	bool LoadFromArchive(FArchive& Archive, const FString& LocMetaID);

	/** Save this resource to the given LocMeta file. */
	bool SaveToFile(const FString& FilePath);

	/** Save this resource to the given LocMeta archive. */
	bool SaveToArchive(FArchive& Archive, const FString& LocMetaID);
};

/** Utility class for working with Localization Resource (LocRes) files. */
class CORE_API FTextLocalizationResource
{
public:
	/** Data struct for tracking a localization entry from a localization resource. */
	struct FEntry
	{
		FString LocResID;
		uint32 SourceStringHash;
		FString LocalizedString;
	};

	typedef TArray<FEntry> FEntryArray;
	typedef TMap<FString, FEntryArray, FDefaultSetAllocator, FLocKeyMapFuncs<FEntryArray>> FKeysTable;
	typedef TMap<FString, FKeysTable, FDefaultSetAllocator, FLocKeyMapFuncs<FKeysTable>> FNamespacesTable;

	FNamespacesTable Namespaces;

	/** Load all LocRes files in the specified directory into this resource. */
	void LoadFromDirectory(const FString& DirectoryPath);

	/** Load the given LocRes file into this resource. */
	bool LoadFromFile(const FString& FilePath);

	/** Load the given LocRes archive into this resource. */
	bool LoadFromArchive(FArchive& Archive, const FString& LocResID);

	/** Save this resource to the given LocRes file. */
	bool SaveToFile(const FString& FilePath);

	/** Save this resource to the given LocRes archive. */
	bool SaveToArchive(FArchive& Archive, const FString& LocResID);

	/** Detect conflicts between loaded localization resources and log them as warnings. */
	void DetectAndLogConflicts() const;
};
