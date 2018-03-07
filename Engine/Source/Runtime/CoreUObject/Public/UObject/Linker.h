// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectResource.h"
#include "Internationalization/GatherableTextData.h"
#include "UObject/PackageFileSummary.h"

class FReferenceCollector;

DECLARE_LOG_CATEGORY_EXTERN(LogLinker, Log, All);

/**
 * Information about a compressed chunk in a file.
 */
struct FCompressedChunk
{
	/** Default constructor, zero initializing all members. */
	FCompressedChunk();

	/** Original offset in uncompressed file.	*/
	int32		UncompressedOffset;
	/** Uncompressed size in bytes.				*/
	int32		UncompressedSize;
	/** Offset in compressed file.				*/
	int32		CompressedOffset;
	/** Compressed size in bytes.				*/
	int32		CompressedSize;

	/** I/O function */
	friend COREUOBJECT_API FArchive& operator<<(FArchive& Ar,FCompressedChunk& Chunk);
};


class FLinkerTables
{
public:
	/** The list of FObjectImports found in the package */
	TArray<FObjectImport> ImportMap;
	/** The list of FObjectExports found in the package */
	TArray<FObjectExport> ExportMap;
	/** List of dependency lists for each export */
	TArray<TArray<FPackageIndex> > DependsMap;
	/** List of packages that are soft referenced by this package */
	TArray<FName> SoftPackageReferenceList;
	/** List of Searchable Names, by object containing them. Not in MultiMap to allow sorting, and sizes are usually small enough where TArray makes sense */
	TMap<FPackageIndex, TArray<FName> > SearchableNamesMap;

	/**
	 * Check that this Index is non-null and return an import or export
	 * @param	Index	Package index to get
	 * @return	the resource corresponding to this index
	 */
	FORCEINLINE FObjectResource& ImpExp(FPackageIndex Index)
	{
		check(!Index.IsNull());
		if (Index.IsImport())
		{
			return Imp(Index);
		}
		else
		{
			return Exp(Index);
		}
	}
	/**
	 * Check that this Index is non-null and return an import or export
	 * @param	Index	Package index to get
	 * @return	the resource corresponding to this index
	 */
	FORCEINLINE FObjectResource const& ImpExp(FPackageIndex Index) const
	{
		check(!Index.IsNull());
		if (Index.IsImport())
		{
			return Imp(Index);
		}
		else
		{
			return Exp(Index);
		}
	}	
	/**
	 * Return an import or export for this index
	 * @param	Index	Package index to get
	 * @return	the resource corresponding to this index, or NULL if the package index is null
	 */
	FORCEINLINE FObjectResource* ImpExpPtr(FPackageIndex Index)
	{
		if (Index.IsImport())
		{
			return ImpPtr(Index);
		}
		else
		{
			return ExpPtr(Index);
		}
	}

	/**
	 * Check that this Index is non-null and an import and return an import
	 * @param	Index	Package index to get, must be an import
	 * @return	the import corresponding to this index
	 */
	FORCEINLINE FObjectImport& Imp(FPackageIndex Index)
	{
		check(Index.IsImport() && ImportMap.IsValidIndex(Index.ToImport()));
		return ImportMap[Index.ToImport()];
	}
	FORCEINLINE FObjectImport const& Imp(FPackageIndex Index) const
	{
		check(Index.IsImport() && ImportMap.IsValidIndex(Index.ToImport()));
		return ImportMap[Index.ToImport()];
	}
	/**
	 * Return an import for this index
	 * @param	Index	Package index to get
	 * @return	the import corresponding to this index, or NULL if the package index is null or an export
	 */
	FORCEINLINE FObjectImport* ImpPtr(FPackageIndex Index)
	{
		if (Index.IsImport())
		{
			check(ImportMap.IsValidIndex(Index.ToImport()));
			return &ImportMap[Index.ToImport()];
		}
		return NULL;
	}

	/**
	 * Check that this Index is non-null and an export and return an import
	 * @param	Index	Package index to get, must be an export
	 * @return	the export corresponding to this index
	 */
	FORCEINLINE FObjectExport& Exp(FPackageIndex Index)
	{
		check(Index.IsExport() && ExportMap.IsValidIndex(Index.ToExport()));
		return ExportMap[Index.ToExport()];
	}
	FORCEINLINE FObjectExport const& Exp(FPackageIndex Index) const
	{
		check(Index.IsExport() && ExportMap.IsValidIndex(Index.ToExport()));
		return ExportMap[Index.ToExport()];
	}
	/**
	 * Return an export for this index
	 * @param	Index	Package index to get
	 * @return	the export corresponding to this index, or NULL if the package index is null or an import
	 */
	FORCEINLINE FObjectExport* ExpPtr(FPackageIndex Index)
	{
		if (Index.IsExport())
		{
			check(ExportMap.IsValidIndex(Index.ToExport()));
			return &ExportMap[Index.ToExport()];
		}
		return NULL;
	}

	/** Serializes the searchable name map */
	COREUOBJECT_API void SerializeSearchableNamesMap(FArchive &Ar);
};


struct FLinkerNamePairKeyFuncs : DefaultKeyFuncs<FName, false>
{
	static FORCEINLINE bool Matches(FName A, FName B)
	{
		// The linker requires that FNames preserve case, but the numeric suffix can be ignored since
		// that is stored separately for each FName instance saved
		return A.IsEqual(B, ENameCase::CaseSensitive, false/*bCompareNumber*/);
	}

	static FORCEINLINE uint32 GetKeyHash(FName Key)
	{
		return Key.GetComparisonIndex();
	}
};


template<typename ValueType>
struct TLinkerNameMapKeyFuncs : TDefaultMapKeyFuncs<FName, ValueType, false>
{
	static FORCEINLINE bool Matches(FName A, FName B)
	{
		// The linker requires that FNames preserve case, but the numeric suffix can be ignored since
		// that is stored separately for each FName instance saved
		return A.IsEqual(B, ENameCase::CaseSensitive, false/*bCompareNumber*/);
	}

	static FORCEINLINE uint32 GetKeyHash(FName Key)
	{
		return Key.GetComparisonIndex();
	}
};


/*----------------------------------------------------------------------------
	FLinker.
----------------------------------------------------------------------------*/
namespace ELinkerType
{
	enum Type
	{
		None,
		Load,
		Save
	};
}

/**
 * Manages the data associated with an Unreal package.  Acts as the bridge between
 * the file on disk and the UPackage object in memory for all Unreal package types.
 */
class FLinker : public FLinkerTables
{
private:	

	ELinkerType::Type LinkerType;

public:

	/** The top-level UPackage object for the package associated with this linker */
	UPackage*				LinkerRoot;

	/** Table of contents for this package's file */
	FPackageFileSummary		Summary;

	/** Names used by objects contained within this package */
	TArray<FName>			NameMap;

	/** Gatherable text data contained within this package */
	TArray<FGatherableTextData> GatherableTextDataMap;

	/** The name of the file for this package */
	FString					Filename;

	/** If true, filter out exports that are for clients but not servers */
	bool					FilterClientButNotServer;

	/** If true, filter out exports that are for servers but not clients */
	bool					FilterServerButNotClient;

	/** The SHA1 key generator for this package, if active */
	class FSHA1*			ScriptSHA;

	/** Constructor. */
	FLinker(ELinkerType::Type InType, UPackage* InRoot, const TCHAR* InFilename);

	virtual ~FLinker();

	/** Gets the class name for the specified index in the export map. */
	COREUOBJECT_API FName GetExportClassName(int32 ExportIdx);
	/** Gets the class name for the specified index in the import map. */
	FName GetExportClassName(FPackageIndex PackageIndex)
	{
		if (PackageIndex.IsExport())
		{
			return GetExportClassName(PackageIndex.ToExport());
		}
		return NAME_None;
	}
	/** Gets the class name for the specified index in the import map. */
	FName GetImportClassName(int32 ImportIdx)
	{
		return ImportMap[ImportIdx].ClassName;
	}
	/** Gets the class name for the specified index in the import map. */
	FName GetImportClassName(FPackageIndex PackageIndex)
	{
		if (PackageIndex.IsImport())
		{
			return GetImportClassName(PackageIndex.ToImport());
		}
		return NAME_None;
	}
	/** Gets the class name for the specified package index. */
	FName GetClassName(FPackageIndex PackageIndex)
	{
		if (PackageIndex.IsImport())
		{
			return GetImportClassName(PackageIndex);
		}
		else if (PackageIndex.IsExport())
		{
			return GetExportClassName(PackageIndex);
		}
		return NAME_None;
	}

	FORCEINLINE ELinkerType::Type GetType() const
	{
		return LinkerType;
	}

	/**
	 * I/O function
	*
	 * @param	Ar	the archive to read/write into
	 */
	void Serialize(FArchive& Ar);
	void AddReferencedObjects(FReferenceCollector& Collector);
	/**
	 * Return the path name of the UObject represented by the specified import. 
	 * (can be used with StaticFindObject)
	 * 
	 * @param	ImportIndex	index into the ImportMap for the resource to get the name for
	 *
	 * @return	the path name of the UObject represented by the resource at ImportIndex
	 */
	COREUOBJECT_API FString GetImportPathName(int32 ImportIndex);
	/**
	 * Return the path name of the UObject represented by the specified import. 
	 * (can be used with StaticFindObject)
	 * 
	 * @param	PackageIndex	package index for the resource to get the name for
	 *
	 * @return	the path name of the UObject represented by the resource at PackageIndex, or the empty string if this isn't an import
	 */
	FString GetImportPathName(FPackageIndex PackageIndex)
	{
		if (PackageIndex.IsImport())
		{
			return GetImportPathName(PackageIndex.ToImport());
		}
		return FString();
	}
	/**
	 * Return the path name of the UObject represented by the specified export.
	 * (can be used with StaticFindObject)
	 * 
	 * @param	ExportIndex				index into the ExportMap for the resource to get the name for
	 * @param	FakeRoot				Optional name to replace use as the root package of this object instead of the linker
	 * @param	bResolveForcedExports	if true, the package name part of the return value will be the export's original package,
	 *									not the name of the package it's currently contained within.
	 *
	 * @return	the path name of the UObject represented by the resource at ExportIndex
	 */
	COREUOBJECT_API FString GetExportPathName(int32 ExportIndex, const TCHAR* FakeRoot=NULL,bool bResolveForcedExports=false);
	/**
	 * Return the path name of the UObject represented by the specified export.
	 * (can be used with StaticFindObject)
	 * 
	 * @param	PackageIndex			package index for the resource to get the name for
	 * @param	FakeRoot				Optional name to replace use as the root package of this object instead of the linker
	 * @param	bResolveForcedExports	if true, the package name part of the return value will be the export's original package,
	 *									not the name of the package it's currently contained within.
	 *
	 * @return	the path name of the UObject represented by the resource at PackageIndex, or the empty string if this isn't an export
	 */
	FString GetExportPathName(FPackageIndex PackageIndex, const TCHAR* FakeRoot=NULL,bool bResolveForcedExports=false)
	{
		if (PackageIndex.IsExport())
		{
			return GetExportPathName(PackageIndex.ToExport(), FakeRoot, bResolveForcedExports);
		}
		return FString();
	}

	/**
	 * Return the path name of the UObject represented by the specified import. 
	 * (can be used with StaticFindObject)
	 * 
	 * @param	PackageIndex	package index
	 *
	 * @return	the path name of the UObject represented by the resource at PackageIndex, or the empty string if this is null
	 */
	FString GetPathName(FPackageIndex PackageIndex)
	{
		if (PackageIndex.IsImport())
		{
			return GetImportPathName(PackageIndex);
		}
		else if (PackageIndex.IsExport())
		{
			return GetExportPathName(PackageIndex);
		}
		return FString();
	}
	/**
	 * Return the full name of the UObject represented by the specified import.
	 * 
	 * @param	ImportIndex	index into the ImportMap for the resource to get the name for
	 *
	 * @return	the full name of the UObject represented by the resource at ImportIndex
	 */
	COREUOBJECT_API FString GetImportFullName(int32 ImportIndex);
	/**
	 * Return the full name of the UObject represented by the specified package index
	 * 
	 * @param	PackageIndex	package index for the resource to get the name for
	 *
	 * @return	the full name of the UObject represented by the resource at PackageIndex
	 */
	FString GetImportFullName(FPackageIndex PackageIndex)
	{
		if (PackageIndex.IsImport())
		{
			return GetImportFullName(PackageIndex.ToImport());
		}
		return FString();
	}

	/**
	 * Return the full name of the UObject represented by the specified export.
	 * 
	 * @param	ExportIndex				index into the ExportMap for the resource to get the name for
	 * @param	FakeRoot				Optional name to replace use as the root package of this object instead of the linker
	 * @param	bResolveForcedExports	if true, the package name part of the return value will be the export's original package,
	 *									not the name of the package it's currently contained within.
	 *
	 * @return	the full name of the UObject represented by the resource at ExportIndex
	 */
	COREUOBJECT_API FString GetExportFullName(int32 ExportIndex, const TCHAR* FakeRoot=NULL,bool bResolveForcedExports=false);
	/**
	 * Return the full name of the UObject represented by the specified package index
	 * 
	 * @param	PackageIndex			package index for the resource to get the name for
	 * @param	FakeRoot				Optional name to replace use as the root package of this object instead of the linker
	 * @param	bResolveForcedExports	if true, the package name part of the return value will be the export's original package,
	 *									not the name of the package it's currently contained within.
	 *
	 * @return	the full name of the UObject represented by the resource at PackageIndex
	 */
	FString GetExportFullName(FPackageIndex PackageIndex, const TCHAR* FakeRoot=NULL,bool bResolveForcedExports=false)
	{
		if (PackageIndex.IsExport())
		{
			return GetExportFullName(PackageIndex.ToExport(), FakeRoot, bResolveForcedExports);
		}
		return FString();
	}

	/**
	 * Return the full name of the UObject represented by the specified export.
	 * 
	 * @param	PackageIndex	package index
	 *
	 * @return	the path name of the UObject represented by the resource at PackageIndex, or the empty string if this is null
	 */
	FString GetFullImpExpName(FPackageIndex PackageIndex)
	{
		if (PackageIndex.IsImport())
		{
			return GetImportFullName(PackageIndex);
		}
		else if (PackageIndex.IsExport())
		{
			return GetExportFullName(PackageIndex);
		}
		return FString();
	}

	/**
	 * Tell this linker to start SHA calculations
	 */
	void StartScriptSHAGeneration();

	/**
	 * If generating a script SHA key, update the key with this script code
	 *
	 * @param ScriptCode Code to SHAify
	 */
	void UpdateScriptSHAKey(const TArray<uint8>& ScriptCode);

	/**
	 * After generating the SHA key for all of the 
	 *
	 * @param OutKey Storage for the key bytes (20 bytes)
	 */
	void GetScriptSHAKey(uint8* OutKey);

	/**
	 * Test and object against the load flag filters
	 *
	 * @return	true if the object should be filtered and not loaded
	 */
	bool FilterExport(const FObjectExport& Export)
	{
		if (Export.bExportLoadFailed || Export.bWasFiltered)
		{
			return true;
		}
#if WITH_EDITOR
		if (!Export.bNotAlwaysLoadedForEditorGame) // Always load, even if is editor only
		{
			return false;
		}
#endif
		if (FilterClientButNotServer && Export.bNotForServer) // "we are a dedicated server"
		{
			return true;
		}
		if (FilterServerButNotClient && Export.bNotForClient) // "we are a client only"
		{
			return true;
		}
		if (Export.ThisIndex.IsNull()) // Export is invalid and shouldn't be processed.
		{
			return true;
		}
		return false;
	}

};



template<typename T> 
FORCEINLINE T* Cast(FLinker* Src)
{
	return Src && T::StaticType() == Src->GetType() ? (T*)Src : nullptr;
}

template<typename T>
FORCEINLINE T* CastChecked(FLinker* Src)
{
	T* LinkerCastResult = Src && T::StaticType() == Src->GetType() ? (T*)Src : nullptr;
	check(LinkerCastResult);
	return LinkerCastResult;
}

/*-----------------------------------------------------------------------------
	Lazy loading.
-----------------------------------------------------------------------------*/

/**
 * Flags serialized with the lazy loader.
 */
typedef uint32 ELazyLoaderFlags;

/**
 * Empty flag set.
 */
#define LLF_None					0x00000000
/**
 * If set, payload is [going to be] stored in separate file		
 */
#define	LLF_PayloadInSeparateFile	0x00000001
/**
 * If set, payload should be [un]compressed during serialization. Only bulk data that doesn't require 
 * any special serialization or endian conversion can be compressed! The code will simply serialize a 
 * block from disk and use the byte order agnostic Serialize( Data, Length ) function to fill the memory.
 */
#define	LLF_SerializeCompressed		0x00000002
/**
 * Mask of all flags
 */
#define	LLF_AllFlags				0xFFFFFFFF

/*-----------------------------------------------------------------------------
	Global functions
-----------------------------------------------------------------------------*/

/** Resets linkers on packages after they have finished loading */
COREUOBJECT_API void ResetLoaders( UObject* InOuter );

/** Deletes all linkers that have finished loading */
COREUOBJECT_API void DeleteLoaders();

/** Queues linker for deletion */
COREUOBJECT_API void DeleteLoader(FLinkerLoad* Loader);

COREUOBJECT_API FLinkerLoad* GetPackageLinker(UPackage* InOuter, const TCHAR* InLongPackageName, uint32 LoadFlags, UPackageMap* Sandbox, FGuid* CompatibleGuid);
COREUOBJECT_API FString GetPrestreamPackageLinkerName(const TCHAR* InLongPackageName, bool bExistSkip = true);


/**
 * 
 * Ensure thumbnails are loaded and then reset the loader in preparation for a package save
 *
 * @param	InOuter			The outer for the package we are saving
 * @param	Filename		The filename we are saving too
 */
COREUOBJECT_API void ResetLoadersForSave(UObject* InOuter, const TCHAR *Filename);
