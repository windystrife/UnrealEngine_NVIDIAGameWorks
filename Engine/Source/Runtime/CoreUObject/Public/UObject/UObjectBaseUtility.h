// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UObjectBaseUtility.h: Unreal UObject functions that only depend on UObjectBase
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectBase.h"
#include "UObject/UObjectArray.h"
#include "UObject/UObjectMarks.h"

#if defined(_MSC_VER) && _MSC_VER == 1900
	#ifdef PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS
		PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS
	#endif
#endif


class COREUOBJECT_API UObjectBaseUtility : public UObjectBase
{
public:
	// Constructors.
	UObjectBaseUtility() {}
	UObjectBaseUtility( EObjectFlags InFlags )
		: UObjectBase(InFlags)
	{
	}

	/***********************/
	/******** Flags ********/
	/***********************/

	FORCEINLINE void SetFlags( EObjectFlags NewFlags )
	{
		checkSlow(!(NewFlags & (RF_MarkAsNative | RF_MarkAsRootSet))); // These flags can't be used outside of constructors / internal code
		SetFlagsTo(GetFlags() | NewFlags);
	}
	FORCEINLINE void ClearFlags( EObjectFlags NewFlags )
	{
		checkSlow(!(NewFlags & (RF_MarkAsNative | RF_MarkAsRootSet)) || NewFlags == RF_AllFlags); // These flags can't be used outside of constructors / internal code
		SetFlagsTo(GetFlags() & ~NewFlags);
	}
	/**
	 * Used to safely check whether any of the passed in flags are set. 
	 *
	 * @param FlagsToCheck	Object flags to check for.
	 * @return				true if any of the passed in flags are set, false otherwise  (including no flags passed in).
	 */
	FORCEINLINE bool HasAnyFlags( EObjectFlags FlagsToCheck ) const
	{
		checkSlow(!(FlagsToCheck & (RF_MarkAsNative | RF_MarkAsRootSet)) || FlagsToCheck == RF_AllFlags); // These flags can't be used outside of constructors / internal code
		return (GetFlags() & FlagsToCheck) != 0;
	}
	/**
	 * Used to safely check whether all of the passed in flags are set. 
	 *
	 * @param FlagsToCheck	Object flags to check for
	 * @return true if all of the passed in flags are set (including no flags passed in), false otherwise
	 */
	FORCEINLINE bool HasAllFlags( EObjectFlags FlagsToCheck ) const
	{
		checkSlow(!(FlagsToCheck & (RF_MarkAsNative | RF_MarkAsRootSet)) || FlagsToCheck == RF_AllFlags); // These flags can't be used outside of constructors / internal code
		return ((GetFlags() & FlagsToCheck) == FlagsToCheck);
	}
	/**
	 * Returns object flags that are both in the mask and set on the object.
	 *
	 * @param Mask	Mask to mask object flags with
	 * @param Objects flags that are set in both the object and the mask
	 */
	FORCEINLINE EObjectFlags GetMaskedFlags( EObjectFlags Mask = RF_AllFlags ) const
	{
		return EObjectFlags(GetFlags() & Mask);
	}

	/***********************/
	/******** Marks *******  UObjectMarks.cpp */
	/***********************/

	/**
	 * Adds marks to an object
	 *
	 * @param	Marks	Logical OR of OBJECTMARK_'s to apply 
	 */
	FORCEINLINE void Mark(EObjectMark Marks) const
	{
		MarkObject(this,Marks);
	}

	/**
	 * Removes marks from and object
	 *
	 * @param	Marks	Logical OR of OBJECTMARK_'s to remove 
	 */
	FORCEINLINE void UnMark(EObjectMark Marks) const
	{
		UnMarkObject(this,Marks);
	}

	/**
	 * Tests an object for having ANY of a set of marks
	 *
	 * @param	Marks	Logical OR of OBJECTMARK_'s to test
	 * @return	true if the object has any of the given marks.
	 */
	FORCEINLINE bool HasAnyMarks(EObjectMark Marks) const
	{
		return ObjectHasAnyMarks(this,Marks);
	}

	/**
	 * Tests an object for having ALL of a set of marks
	 *
	 * @param	Marks	Logical OR of OBJECTMARK_'s to test
	 * @return	true if the object has any of the given marks.
	 */
	FORCEINLINE bool HasAllMarks(EObjectMark Marks) const
	{
		return ObjectHasAllMarks(this,Marks);
	}

	/**
	 * Checks the PendingKill flag to see if it is dead but memory still valid
	 */
	FORCEINLINE bool IsPendingKill() const
	{
		return GUObjectArray.IndexToObject(InternalIndex)->IsPendingKill();
	}

	/**
	 * Marks this object as RF_PendingKill.
	 */
	FORCEINLINE void MarkPendingKill()
	{
		check(!IsRooted());
		GUObjectArray.IndexToObject(InternalIndex)->SetPendingKill();
	}

	/**
	* Unmarks this object as PendingKill.
	*/
	FORCEINLINE void ClearPendingKill()
	{
		GUObjectArray.IndexToObject(InternalIndex)->ClearPendingKill();
	}

	//
	// Add an object to the root set. This prevents the object and all
	// its descendants from being deleted during garbage collection.
	//
	FORCEINLINE void AddToRoot()
	{
		GUObjectArray.IndexToObject(InternalIndex)->SetRootSet();
	}

	//
	// Remove an object from the root set.
	//
	FORCEINLINE void RemoveFromRoot()
	{
		GUObjectArray.IndexToObject(InternalIndex)->ClearRootSet();
	}

	/**
	 * Returns true if this object is explicitly rooted
	 *
	 * @return true if the object was explicitly added as part of the root set.
	 */
	FORCEINLINE bool IsRooted()
	{
		return GUObjectArray.IndexToObject(InternalIndex)->IsRootSet();
	}

	/**
	* Atomically clear the unreachable flag
	*
	* @return true if we are the thread that cleared RF_Unreachable
	**/
	FORCEINLINE bool ThisThreadAtomicallyClearedRFUnreachable()
	{
		return GUObjectArray.IndexToObject(InternalIndex)->ThisThreadAtomicallyClearedRFUnreachable();
	}

	/**
	* Checks if the object is unreachable.
	**/
	FORCEINLINE bool IsUnreachable() const
	{
		return GUObjectArray.IndexToObject(InternalIndex)->IsUnreachable();
	}

	/**
	* Checks if the object is pending kill or unreachable.
	**/
	FORCEINLINE bool IsPendingKillOrUnreachable() const
	{
		return GUObjectArray.IndexToObject(InternalIndex)->HasAnyFlags(EInternalObjectFlags::PendingKill | EInternalObjectFlags::Unreachable);
	}

	/**
	* Checks if the object is native.
	**/
	FORCEINLINE bool IsNative() const
	{
		return GUObjectArray.IndexToObject(InternalIndex)->HasAnyFlags(EInternalObjectFlags::Native);
	}

	/**
	* Clears passed in internal flags .
	 *
	* @param FlagsToClear	Object flags to clear.
	* @return				true if any of the passed in flags are set, false otherwise  (including no flags passed in).
	 */
	FORCEINLINE void SetInternalFlags(EInternalObjectFlags FlagsToSet) const
	{
		GUObjectArray.IndexToObject(InternalIndex)->SetFlags(FlagsToSet);
	}

	/**
	* Gets internal flags.
	 *
	* @param FlagsToClear	Object flags to clear.
	* @return				true if any of the passed in flags are set, false otherwise  (including no flags passed in).
	 */
	FORCEINLINE EInternalObjectFlags GetInternalFlags() const
	{
		return GUObjectArray.IndexToObject(InternalIndex)->GetFlags();
	}

	/**
	* Used to safely check whether any of the passed in internal flags are set.
	 *
	* @param FlagsToCheck	Object flags to check for.
	* @return				true if any of the passed in flags are set, false otherwise  (including no flags passed in).
	 */
	FORCEINLINE bool HasAnyInternalFlags(EInternalObjectFlags FlagsToCheck) const
	{
		return GUObjectArray.IndexToObject(InternalIndex)->HasAnyFlags(FlagsToCheck);
	}

	/**
	* Clears passed in internal flags .
	 *
	* @param FlagsToClear	Object flags to clear.
	* @return				true if any of the passed in flags are set, false otherwise  (including no flags passed in).
	 */
	FORCEINLINE void ClearInternalFlags(EInternalObjectFlags FlagsToClear) const
	{
		GUObjectArray.IndexToObject(InternalIndex)->ClearFlags(FlagsToClear);
	}

	/**
	* Atomically clears passed in internal flags .
	*
	* @param FlagsToClear	Object flags to clear.
	* @return				true if any of the passed in flags are set, false otherwise  (including no flags passed in).
	*/
	FORCEINLINE bool AtomicallyClearInternalFlags(EInternalObjectFlags FlagsToClear) const
	{
		return GUObjectArray.IndexToObject(InternalIndex)->ThisThreadAtomicallyClearedFlag(FlagsToClear);
	}

	/***********************/
	/******** Names ********/
	/***********************/

	/**
	 * Returns the fully qualified pathname for this object as well as the name of the class, in the format:
	 * 'ClassName Outermost[.Outer].Name'.
	 *
	 * @param	StopOuter	if specified, indicates that the output string should be relative to this object.  if StopOuter
	 *						does not exist in this object's Outer chain, the result would be the same as passing NULL.
	 *
	 * @note	safe to call on NULL object pointers!
	 */
	FString GetFullName( const UObject* StopOuter=NULL ) const;

	/**
	 * Returns the fully qualified pathname for this object, in the format:
	 * 'Outermost[.Outer].Name'
	 *
	 * @param	StopOuter	if specified, indicates that the output string should be relative to this object.  if StopOuter
	 *						does not exist in this object's Outer chain, the result would be the same as passing NULL.
	 *
	 * @note	safe to call on NULL object pointers!
	 */
	FString GetPathName( const UObject* StopOuter=NULL ) const;

public:

	/**
	* Called after load to determine if the object can be a cluster root
	*
	* @return	true if this object can be a cluster root
	*/
	virtual bool CanBeClusterRoot() const
	{
		return false;
	}

	/**
	* Called during cluster construction if the object can be added to a cluster
	*
	* @return	true if this object can be inside of a cluster
	*/
	virtual bool CanBeInCluster() const;

	/**
	* Called after PostLoad to create UObject cluster
	*/
	virtual void CreateCluster();

	/**
	* Called during Garbage Collection to perform additional cleanup when the cluster is about to be destroyed due to PendingKill flag being set on it.
	*/
	virtual void OnClusterMarkedAsPendingKill() {}

	/**
	* Adds this objects to a GC cluster that already exists
	* @param ClusterRootOrObjectFromCluster Object that belongs to the cluster we want to add this object to.
	* @param Add this object to the target cluster as a mutable object without adding this object's references.
	*/
	virtual void AddToCluster(UObjectBaseUtility* ClusterRootOrObjectFromCluster, bool bAddAsMutableObject = false);

protected:

	/** Helper function to create a cluster from UObject */
	static void CreateClusterFromObject(UObjectBaseUtility* ClusterRootObject, UObjectBaseUtility* ReferencingObject);

	/**
	 * Internal version of GetPathName() that eliminates lots of copies.
	 */
	void GetPathName( const UObject* StopOuter, FString& ResultString ) const;

public:
	/**
	 * Walks up the chain of packages until it reaches the top level, which it ignores.
	 *
	 * @param	bStartWithOuter		whether to include this object's name in the returned string
	 * @return	string containing the path name for this object, minus the outermost-package's name
	 */
	FString GetFullGroupName( bool bStartWithOuter ) const;

	/**
	 * Returns the name of this object (with no path information)
	 * 
	 * @return Name of the object.
	 */
	FORCEINLINE FString GetName() const
	{
		return GetFName().ToString();
	}
	// GetFullName optimization
	FORCEINLINE void GetName(FString &ResultString) const
	{
		GetFName().ToString(ResultString);
	}
	FORCEINLINE void AppendName(FString& ResultString) const
	{
		GetFName().AppendString(ResultString);
	}

	/***********************/
	/******** Outer ********/
	/***********************/

	/** 
	 * Walks up the list of outers until it finds the highest one.
	 *
	 * @return outermost non NULL Outer.
	 */
	UPackage* GetOutermost() const;

	/** 
	 * Finds the outermost package and marks it dirty. 
	 * The editor suppresses this behavior during load as it is against policy to dirty packages simply by loading them.
	 *
	 * @return false if the request to mark the package dirty was suppressed by the editor and true otherwise.
	 */
	bool MarkPackageDirty() const;

	/**
	* Determines whether this object is a template object
	*
	* @return	true if this object is a template object (owned by a UClass)
	*/
	bool IsTemplate( EObjectFlags TemplateTypes = RF_ArchetypeObject|RF_ClassDefaultObject ) const;

	/**
	 * Traverses the outer chain searching for the next object of a certain type.  (T must be derived from UObject)
	 *
	 * @param	Target class to search for
	 * @return	a pointer to the first object in this object's Outer chain which is of the correct type.
	 */
	UObject* GetTypedOuter(UClass* Target) const;

	/**
	 * Traverses the outer chain searching for the next object of a certain type.  (T must be derived from UObject)
	 *
	 * @return	a pointer to the first object in this object's Outer chain which is of the correct type.
	 */
	template<typename T>
	T* GetTypedOuter() const
	{
		return (T *)GetTypedOuter(T::StaticClass());
	}

	/**
	 * @return	true if the specified object appears somewhere in this object's outer chain.
	 */
	bool IsIn( const UObject* SomeOuter ) const;

	/**
	 * Find out if this object is inside (has an outer) that is of the specified class
	 * @param SomeBaseClass	The base class to compare against
	 * @return True if this object is in an object of the given type.
	 */
	bool IsInA( const UClass* SomeBaseClass ) const;

	/**
	 * Checks whether this object's top-most package has any of the specified flags
	 *
	 * @param	CheckFlagMask	a bitmask of EPackageFlags values to check for
	 *
	 * @return	true if the PackageFlags member of this object's top-package has any bits from the mask set.
	 */
	bool RootPackageHasAnyFlags( uint32 CheckFlagMask ) const;

	/***********************/
	/******** Class ********/
	/***********************/

	/**
	 * @return	true if this object is of the specified type.
	 */
	#if !UCLASS_FAST_ISA_COMPARE_WITH_OUTERWALK && UCLASS_FAST_ISA_IMPL != UCLASS_ISA_OUTERWALK
	private:
		template <typename ClassType>
		static FORCEINLINE bool IsAWorkaround(const ClassType* ObjClass, const ClassType* TestCls)
		{
			#if UCLASS_FAST_ISA_IMPL == UCLASS_ISA_INDEXTREE
				return ObjClass->IsAUsingFastTree(*TestCls);
			#elif UCLASS_FAST_ISA_IMPL == UCLASS_ISA_CLASSARRAY
				return ObjClass->IsAUsingClassArray(*TestCls);
			#endif
		}

	public:
		template <typename OtherClassType>
		FORCEINLINE bool IsA( OtherClassType SomeBase ) const
		{
			// We have a cyclic dependency between UObjectBaseUtility and UClass,
			// so we use a template to allow inlining of something we haven't yet seen, because it delays compilation until the function is called.

			// 'static_assert' that this thing is actually a UClass pointer or convertible to it.
			const UClass* SomeBaseClass = SomeBase;
			(void)SomeBaseClass;
			checkfSlow(SomeBaseClass, TEXT("IsA(NULL) cannot yield meaningful results"));

			const UClass* ThisClass = GetClass();

			// Stop the compiler doing some unnecessary branching for nullptr checks
			ASSUME(SomeBaseClass);
			ASSUME(ThisClass);

			return IsAWorkaround(ThisClass, SomeBaseClass);
		}
	#else
		bool IsA( const UClass* SomeBase ) const;
	#endif

	/**
	 * @return	true if this object is of the template type.
	 */
	template<class T>
	bool IsA() const
	{
		return IsA(T::StaticClass());
	}

	/**
	 * Finds the most-derived class which is a parent of both TestClass and this object's class.
	 *
	 * @param	TestClass	the class to find the common base for
	 */
	const UClass* FindNearestCommonBaseClass( const UClass* TestClass ) const;

	/**
	 * Returns a pointer to this object safely converted to a pointer of the specified interface class.
	 *
	 * @param	InterfaceClass	the interface class to use for the returned type
	 *
	 * @return	a pointer that can be assigned to a variable of the interface type specified, or NULL if this object's
	 *			class doesn't implement the interface indicated.  Will be the same value as 'this' if the interface class
	 *			isn't native.
	 */
	void* GetInterfaceAddress( UClass* InterfaceClass );

	/** 
	 *	Returns a pointer to the I* native interface object that this object implements.
	 *	Returns NULL if this object does not implement InterfaceClass, or does not do so natively.
	 */
	void* GetNativeInterfaceAddress(UClass* InterfaceClass);

	/** 
	 *	Returns a pointer to the const I* native interface object that this object implements.
	 *	Returns NULL if this object does not implement InterfaceClass, or does not do so natively.
	 */
	const void* GetNativeInterfaceAddress(UClass* InterfaceClass) const
	{
		return const_cast<UObjectBaseUtility*>(this)->GetNativeInterfaceAddress(InterfaceClass);
	}

	/**
	 * Returns whether this component was instanced from a component/subobject template, or if it is a component/subobject template.
	 * This is based on a name comparison with the outer class instance lookup table
	 *
	 * @return	true if this component was instanced from a template.  false if this component was created manually at runtime.
	 */
	bool IsDefaultSubobject() const;
	

	/***********************/
	/******** Linker ****  UObjectLinker.cpp */
	/***********************/

	/**
	 * Returns the linker for this object.
	 *
	 * @return	a pointer to the linker for this object, or NULL if this object has no linker
	 */
	class FLinkerLoad* GetLinker() const;
	/**
	 * Returns this object's LinkerIndex.
	 *
	 * @return	the index into my linker's ExportMap for the FObjectExport
	 *			corresponding to this object.
	 */
	int32 GetLinkerIndex() const;

	/**
	 * Returns the UE4 version of the linker for this object.
	 *
	 * @return	the UE4 version of the engine's package file when this object
	 *			was last saved, or GPackageFileUE4Version (current version) if
	 *			this object does not have a linker, which indicates that
	 *			a) this object is a native only class, or
	 *			b) this object's linker has been detached, in which case it is already fully loaded
	 */
	int32 GetLinkerUE4Version() const;

	/**
	 * Returns the licensee version of the linker for this object.
	 *
	 * @return	the licensee version of the engine's package file when this object
	 *			was last saved, or GPackageFileLicenseeVersion (current version) if
	 *			this object does not have a linker, which indicates that
	 *			a) this object is a native only class, or
	 *			b) this object's linker has been detached, in which case it is already fully loaded
	 */
	int32 GetLinkerLicenseeUE4Version() const;

	/**
	 * Returns the custom version of the linker for this object corresponding to the given custom version key.
	 *
	 * @return	the custom version of the engine's package file when this object
	 *			was last saved, or the current version if
	 *			this object does not have a linker, which indicates that
	 *			a) this object is a native only class, or
	 *			b) this object's linker has been detached, in which case it is already fully loaded
	 */
	int32 GetLinkerCustomVersion(FGuid CustomVersionKey) const;

	/** 
	 * Overloaded < operator. Compares objects by name.
	 *
	 * @return true if this object's name is lexicographically smaller than the other object's name
	 */
	FORCEINLINE bool operator<( const UObjectBaseUtility& Other ) const
	{
		return GetName() < Other.GetName();
	}
};

FORCEINLINE bool IsPossiblyAllocatedUObjectPointer(UObject* Ptr)
{
	auto CountByteValues = [](UPTRINT Val, UPTRINT ByteVal) -> int32
	{
		int32 Result = 0;

		for (int32 I = 0; I != sizeof(UPTRINT); ++I)
		{
			if ((Val & 0xFF) == ByteVal)
			{
				++Result;
			}
			Val >>= 8;
		}

		return Result;
	};

	UPTRINT PtrVal = (UPTRINT)Ptr;
	return PtrVal >= 0x1000 && CountByteValues(PtrVal, 0xCD) < sizeof(UPTRINT) / 2;
}

/**
 * Returns the name of this object (with no path information)
 * @param Object object to retrieve the name for; NULL gives "None"
 * @return Name of the object.
*/
FORCEINLINE FString GetNameSafe(const UObjectBaseUtility *Object)
{
	if( Object == NULL )
	{
		return TEXT("None");
	}
	else
	{
		return Object->GetName();
	}
}

/**
 * Returns the path name of this object
 * @param Object object to retrieve the path name for; NULL gives "None"
 * @return path name of the object.
*/
FORCEINLINE FString GetPathNameSafe(const UObjectBaseUtility *Object)
{
	if( Object == NULL )
	{
		return TEXT("None");
	}
	else
	{
		return Object->GetPathName();
	}
}

/**
 * Returns the full name of this object
 * @param Object object to retrieve the full name for; NULL (or a null class!) gives "None"
 * @return full name of the object.
*/
FORCEINLINE FString GetFullNameSafe(const UObjectBaseUtility *Object)
{
	if( !Object || !Object->GetClass())
	{
		return TEXT("None");
	}
	else
	{
		return Object->GetFullName();
	}
}

#if STATS
struct FScopeCycleCounterUObject : public FCycleCounter
{
#if USE_MALLOC_PROFILER
	/** Package path being tracked */
	FName PackageTag;
	/** Class path being tracked */
	FName ClassTag;
	/** Object path being tracked */
	FName ObjectTag;
#endif

	/**
	 * Constructor, starts timing
	 */
	FORCEINLINE_STATS FScopeCycleCounterUObject(const UObjectBaseUtility *Object)
	{
		if (Object)
		{
			TStatId ObjectStatId = Object->GetStatID();
			if (FThreadStats::IsCollectingData(ObjectStatId))
			{
				Start(ObjectStatId);
			}
		}

#if USE_MALLOC_PROFILER
		if (Object)
		{
			TrackObjectForMallocProfiling(Object);
		}
#endif
	}
	/**
	 * Constructor, starts timing with an alternate enable stat to use high performance disable for only SOME UObject stats
	 */
	FORCEINLINE_STATS FScopeCycleCounterUObject(const UObjectBaseUtility *Object, TStatId OtherStat)
	{
		if (FThreadStats::IsCollectingData(OtherStat) && Object)
		{
			TStatId ObjectStatId = Object->GetStatID();
			if (!ObjectStatId.IsNone())
			{
				Start(ObjectStatId);
			}
		}

#if USE_MALLOC_PROFILER
		if (Object)
		{
			TrackObjectForMallocProfiling(Object);
		}
#endif
	}
	/**
	 * Updates the stat with the time spent
	 */
	FORCEINLINE_STATS ~FScopeCycleCounterUObject()
	{
		Stop();

#if USE_MALLOC_PROFILER
		UntrackObjectForMallocProfiling();
#endif
	}

#if USE_MALLOC_PROFILER
	COREUOBJECT_API void TrackObjectForMallocProfiling(const UObjectBaseUtility *InObject);
	COREUOBJECT_API void TrackObjectForMallocProfiling(const FName InPackageName, const FName InClassName, const FName InObjectName);
	COREUOBJECT_API void UntrackObjectForMallocProfiling();
#endif
};
#else
struct FScopeCycleCounterUObject
{
	FORCEINLINE_STATS FScopeCycleCounterUObject(const UObjectBaseUtility *Object)
	{
	}
	FORCEINLINE_STATS FScopeCycleCounterUObject(const UObjectBaseUtility *Object, TStatId OtherStat)
	{
	}
};
#endif

#if defined(_MSC_VER) && _MSC_VER == 1900
	#ifdef PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS
		PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS
	#endif
#endif
