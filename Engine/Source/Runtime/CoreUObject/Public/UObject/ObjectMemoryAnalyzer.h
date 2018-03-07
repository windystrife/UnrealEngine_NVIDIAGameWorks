// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UObjectAnnotation.h"

/** Object mempory usage info */
struct FObjectMemoryUsage
{
	struct EObjFlags
	{
		enum Type
		{
			IsRoot					= 1<<0,
			IsReferencedByRoot		= 1<<1,
			IsProcessed				= 1<<2,
			IsReferencedByNonRoot	= 1<<3,
		};
	};

	/**
	 * default constructor
	 * Default constructor must be the default item
	 */
	FObjectMemoryUsage()
		: InclusiveMemoryUsage(0)
		, ExclusiveMemoryUsage(0)
		, Flags(0)
		, Object(NULL)
	{
	}
	/**
	 * Initialization constructor
	 * @param InMarks marks to initialize to
	 */
	FObjectMemoryUsage(SIZE_T InclusiveMemUsage, SIZE_T ExclusiveMemUsage, uint32 InFlags = 0)
		: InclusiveMemoryUsage(InclusiveMemUsage)
		, ExclusiveMemoryUsage(ExclusiveMemUsage)
		, Flags(InFlags)
		, Object(NULL)
	{
	}
	/**
	 * Determine if this annotation
	 * @return true is this is a default pair. We only check the linker because CheckInvariants rules out bogus combinations
	 */
	FORCEINLINE bool IsDefault()
	{
		return false;
	}

	FORCEINLINE bool IsRoot() const { return !!(Flags&EObjFlags::IsRoot); }
	FORCEINLINE bool IsReferencedByRoot() const { return !!(Flags&EObjFlags::IsReferencedByRoot); }
	FORCEINLINE bool IsProcessed() const { return !!(Flags&EObjFlags::IsProcessed); }
	FORCEINLINE bool IsReferencedByNonRoot() const { return !!(Flags&EObjFlags::IsReferencedByNonRoot); }
	
	SIZE_T InclusiveMemoryUsage;
	SIZE_T ExclusiveMemoryUsage;
	SIZE_T InclusiveResourceSize;
	SIZE_T ExclusiveResourceSize;
	uint32 Flags;

	UObject*		 Object;
	TArray<UObject*> RootReferencer;
	TArray<UObject*> NonRootReferencer;
};


/** 
 * Analyzes memory usage of UObjects
 **/
struct COREUOBJECT_API FObjectMemoryAnalyzer
{
	/** Flags to modify the memory counting behavior */
	struct EAnalyzeFlags
	{
		enum Type
		{
			/** Accounts for default objects */
			IncludeDefaultObjects	= 1<<0,
		};
	};

	struct ESortKey
	{
		enum Type
		{
			InclusiveSize,
			ExclusiveSize,
			InclusiveResSize,
			ExclusiveResSize,
			InclusiveTotal,
			ExclusiveTotal,
		};
	};

	struct FCompareFSortBySize
	{		
		FCompareFSortBySize( ESortKey::Type InSortKey )
			: SortKey( InSortKey )
		{}

		FORCEINLINE bool operator()( const FObjectMemoryUsage& A, const FObjectMemoryUsage& B ) const
		{
			switch (SortKey)
			{
				case ESortKey::InclusiveSize:		return A.InclusiveMemoryUsage > B.InclusiveMemoryUsage;
				case ESortKey::ExclusiveSize:		return A.ExclusiveMemoryUsage > B.ExclusiveMemoryUsage;
				case ESortKey::InclusiveResSize:	return A.InclusiveResourceSize > B.InclusiveResourceSize;
				case ESortKey::ExclusiveResSize:	return A.ExclusiveResourceSize > B.ExclusiveResourceSize;
				case ESortKey::InclusiveTotal:		return (A.InclusiveMemoryUsage + A.InclusiveResourceSize*1024) > (B.InclusiveMemoryUsage + B.InclusiveResourceSize*1024);
				case ESortKey::ExclusiveTotal:		return (A.ExclusiveMemoryUsage + A.ExclusiveResourceSize*1024) > (B.ExclusiveMemoryUsage + B.ExclusiveResourceSize*1024);
			}

			check(false);
			return false;
		}

		ESortKey::Type SortKey;
	};

	FObjectMemoryAnalyzer(uint32 Flags = 0);
	FObjectMemoryAnalyzer(class UClass* BaseClass, uint32 Flags = 0);
	FObjectMemoryAnalyzer(class UObject* Object, uint32 Flags = 0);
	FObjectMemoryAnalyzer(const TArray<class UObject*>& ObjectList, uint32 Flags = 0);

	/** Analyzes the memory usage of the specified object */
	void AnalyzeObject(class UObject* Object);
	/** Analyzes the memory usage of the specified object list */
	void AnalyzeObjects(const TArray<class UObject*>& ObjectList);
	/** Analyzes the memory usage of all objects with the specified class */
	void AnalyzeObjects(class UClass* BaseClass);

	/** Returns the results */
	int32 GetResults(TArray<FObjectMemoryUsage>& Results);
	/** Returns the memory usage of an object not in the result set ( ex. referenced by a result ) */
	const FObjectMemoryUsage& GetObjectMemoryUsage(class UObject* Obj);

	struct EPrintFlags
	{
		enum Type
		{
			PrintReferences	= 1<<0,
			PrintReferencer = 1<<1,

			PrintAll		= PrintReferences | PrintReferencer,
		};
	};

	void PrintResults(FOutputDevice& Ar, uint32 PrintFlags = 0);

private:
	int32 GetReferencedObjects(UObject* Obj, TArray<UObject*>& ReferencedObjects);
	void ProcessSubObjRecursive(UObject* Root, UObject* Object);
	SIZE_T CalculateSizeRecursive(UObject* Object);
	FString GetFlagsString(const FObjectMemoryUsage& Annotation);
	void PrintSubObjects(FOutputDevice& Ar, const FString& Indent, UObject* Parent, uint32 PrintFlags);


	/** All objects of this class will be analyzed */
	UClass* BaseClass;
	/** List of objects which memory usage should be analyzed*/
	TArray<UObject*> ObjectList;
	/** Anootations containing the gathered data*/
	FUObjectAnnotationDense<FObjectMemoryUsage, true> MemUsageAnnotations;
	/** Flags to modify mem counting behavior */
	uint32 AnalyzeFlags;
};
