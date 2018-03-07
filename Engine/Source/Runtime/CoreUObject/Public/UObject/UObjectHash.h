// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UObjectHash.h: Unreal object name hashes
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectArray.h"

DECLARE_STATS_GROUP_VERBOSE(TEXT("UObject Hash"), STATGROUP_UObjectHash, STATCAT_Advanced);

#if UE_GC_TRACK_OBJ_AVAILABLE
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("NumObjects"), STAT_Hash_NumObjects, STATGROUP_UObjectHash, COREUOBJECT_API);
#endif

/**
 * Private internal version of StaticFindObjectFast that allows using 0 exclusion flags.
 *
 * @param	Class			The to be found object's class
 * @param	InOuter			The to be found object's outer
 * @param	InName			The to be found object's class
 * @param	ExactClass		Whether to require an exact match with the passed in class
 * @param	AnyPackage		Whether to look in any package
 * @param	ExclusiveFlags	Ignores objects that contain any of the specified exclusive flags
 * @param	ExclusiveInternalFlags	Ignores objects that contain any of the specified internal exclusive flags
 * @return	Returns a pointer to the found object or NULL if none could be found
 */
UObject* StaticFindObjectFastInternal(UClass* Class, UObject* InOuter, FName InName, bool ExactClass = false, bool AnyPackage = false, EObjectFlags ExclusiveFlags = RF_NoFlags, EInternalObjectFlags ExclusiveInternalFlags = EInternalObjectFlags::None);

/**
 * Variation of StaticFindObjectFast that uses explicit path.
 *
 * @param	ObjectClass		The to be found object's class
 * @param	ObjectName		The to be found object's class
 * @param	ObjectPathName	Full path name for the object to search for
 * @param	ExactClass		Whether to require an exact match with the passed in class
 * @param	ExclusiveFlags	Ignores objects that contain any of the specified exclusive flags
 * @return	Returns a pointer to the found object or NULL if none could be found
 */
UObject* StaticFindObjectFastExplicit(UClass* ObjectClass, FName ObjectName, const FString& ObjectPathName, bool bExactClass, EObjectFlags ExcludeFlags = RF_NoFlags);

/**
 * Return all objects with a given outer
 *
 * @param	Outer						Outer to search for
 * @param	Results						Returned results
 * @param	bIncludeNestedObjects		If true, then things whose outers directly or indirectly have Outer as an outer are included, these are the nested objects.
 * @param	ExclusionFlags				Specifies flags to use as a filter for which objects to return
 * @param	ExclusiveInternalFlags	Specifies internal flags to use as a filter for which objects to return
 */
COREUOBJECT_API void GetObjectsWithOuter(const class UObjectBase* Outer, TArray<UObject *>& Results, bool bIncludeNestedObjects = true, EObjectFlags ExclusionFlags = RF_NoFlags, EInternalObjectFlags ExclusionInternalFlags = EInternalObjectFlags::None);

/**
 * Performs an operation on all objects with a given outer
 *
 * @param	Outer						Outer to search for
 * @param	Operation					Function to be called for each object
 * @param	bIncludeNestedObjects		If true, then things whose outers directly or indirectly have Outer as an outer are included, these are the nested objects.
 * @param	ExclusionFlags				Specifies flags to use as a filter for which objects to return
 * @param	ExclusiveInternalFlags	Specifies internal flags to use as a filter for which objects to return
 */
COREUOBJECT_API void ForEachObjectWithOuter(const class UObjectBase* Outer, TFunctionRef<void(UObject*)> Operation, bool bIncludeNestedObjects = true, EObjectFlags ExclusionFlags = RF_NoFlags, EInternalObjectFlags ExclusionInternalFlags = EInternalObjectFlags::None);

/**
 * Find an objects with a given name and or class within an outer
 *
 * @param	Outer						Outer to search for
 * @param	ClassToLookFor				if NULL, ignore this parameter, otherwise require the returned object have this class
 * @param	NameToLookFor				if NAME_None, ignore this parameter, otherwise require the returned object have this name
 */
COREUOBJECT_API class UObjectBase* FindObjectWithOuter(class UObjectBase* Outer, class UClass* ClassToLookFor = NULL, FName NameToLookFor = NAME_None);

/**
 * Returns an array of objects of a specific class. Optionally, results can include objects of derived classes as well.
 *
 * @param	ClassToLookFor				Class of the objects to return.
 * @param	Results						An output list of objects of the specified class.
 * @param	bIncludeDerivedClasses		If true, the results will include objects of child classes as well.
 * @param	AdditionalExcludeFlags		Objects with any of these flags will be excluded from the results.
 * @param	ExclusiveInternalFlags	Specifies internal flags to use as a filter for which objects to return
 */
COREUOBJECT_API void GetObjectsOfClass(UClass* ClassToLookFor, TArray<UObject *>& Results, bool bIncludeDerivedClasses = true, EObjectFlags ExcludeFlags = RF_ClassDefaultObject, EInternalObjectFlags ExclusionInternalFlags = EInternalObjectFlags::None);

/**
 * Performs an operation on all objects with a given outer
 *
 * @param	Outer						Outer to search for
 * @param	Operation					Function to be called for each object
 * @param	bIncludeDerivedClasses		If true, the results will include objects of child classes as well.
 * @param	AdditionalExcludeFlags		Objects with any of these flags will be excluded from the results.
 */
COREUOBJECT_API void ForEachObjectOfClass(UClass* ClassToLookFor, TFunctionRef<void(UObject*)> Operation, bool bIncludeDerivedClasses = true, EObjectFlags ExcludeFlags = RF_ClassDefaultObject, EInternalObjectFlags ExclusionInternalFlags = EInternalObjectFlags::None);

/**
 * Returns an array of classes that were derived from the specified class.
 *
 * @param	ClassToLookFor				The parent class of the classes to return.
 * @param	Results						An output list of child classes of the specified parent class.
 * @param	bRecursive					If true, the results will include children of the children classes, recursively. Otherwise, only direct decedents will be included.
 */
COREUOBJECT_API void GetDerivedClasses(UClass* ClassToLookFor, TArray<UClass *>& Results, bool bRecursive = true);

/**
 * Add an object to the name hash tables
 *
 * @param	Object		Object to add to the hash tables
 */
void HashObject(class UObjectBase* Object);
/**
 * Remove an object to the name hash tables
 *
 * @param	Object		Object to remove from the hash tables
 */
void UnhashObject(class UObjectBase* Object);

/**
 * Logs out information about the object hash for debug purposes
 *
 * @param Ar the archive to write the log data to
 * @param bShowHashBucketCollisionInfo whether to log each bucket's collision count
 */
COREUOBJECT_API void LogHashStatistics(FOutputDevice& Ar, const bool bShowHashBucketCollisionInfo);

/**
 * Logs out information about the outer object hash for debug purposes
 *
 * @param Ar the archive to write the log data to
 * @param bShowHashBucketCollisionInfo whether to log each bucket's collision count
 */
COREUOBJECT_API void LogHashOuterStatistics(FOutputDevice& Ar, const bool bShowHashBucketCollisionInfo);

/**
 * Adds a uobject to the global array which is used for uobject iteration
 *
 * @param	Object Object to allocate an index for
 */
void AllocateUObjectIndexForCurrentThread(class UObjectBase* Object);

