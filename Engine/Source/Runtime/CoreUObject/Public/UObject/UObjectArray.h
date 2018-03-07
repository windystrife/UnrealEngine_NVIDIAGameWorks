// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UObjectArray.h: Unreal object array
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "Containers/LockFreeList.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectBase.h"

/**
* Controls whether the number of available elements is being tracked in the ObjObjects array.
* By default it is only tracked in WITH_EDITOR builds as it adds a small amount of tracking overhead
*/
#if !defined(UE_GC_TRACK_OBJ_AVAILABLE)
#define UE_GC_TRACK_OBJ_AVAILABLE (WITH_EDITOR)
#endif

/**
* Single item in the UObject array.
*/
struct FUObjectItem
{
	// Pointer to the allocated object
	class UObjectBase* Object;
	// Internal flags
	int32 Flags;
	// UObject Owner Cluster Index
	int32 ClusterRootIndex;	
	// Weak Object Pointer Serial number associated with the object
	int32 SerialNumber;

	FUObjectItem()
		: Object(nullptr)
		, Flags(0)
		, ClusterRootIndex(0)
		, SerialNumber(0)
	{
	}

	FORCEINLINE void SetOwnerIndex(int32 OwnerIndex)
	{
		ClusterRootIndex = OwnerIndex;
	}

	FORCEINLINE int32 GetOwnerIndex() const
	{
		return ClusterRootIndex;
	}

	/** Encodes the cluster index in the ClusterRootIndex variable */
	FORCEINLINE void SetClusterIndex(int32 ClusterIndex)
	{
		ClusterRootIndex = -ClusterIndex - 1;
	}

	/** Decodes the cluster index from the ClusterRootIndex variable */
	FORCEINLINE int32 GetClusterIndex() const
	{
		checkSlow(ClusterRootIndex < 0);
		return -ClusterRootIndex - 1;
	}

	FORCEINLINE int32 GetSerialNumber() const
	{
		return SerialNumber;
	}

	FORCEINLINE void SetFlags(EInternalObjectFlags FlagsToSet)
	{
		check((int32(FlagsToSet) & ~int32(EInternalObjectFlags::AllFlags)) == 0);
		Flags |= int32(FlagsToSet);
	}

	FORCEINLINE EInternalObjectFlags GetFlags() const
	{
		return EInternalObjectFlags(Flags);
	}

	FORCEINLINE void ClearFlags(EInternalObjectFlags FlagsToClear)
	{
		check((int32(FlagsToClear) & ~int32(EInternalObjectFlags::AllFlags)) == 0);
		Flags &= ~int32(FlagsToClear);
	}

	/**
	 * Uses atomics to clear the specified flag(s).
	 * @param FlagsToClear
	 * @return True if this call cleared the flag, false if it has been cleared by another thread.
	 */
	FORCEINLINE bool ThisThreadAtomicallyClearedFlag(EInternalObjectFlags FlagToClear)
	{
		static_assert(sizeof(int32) == sizeof(Flags), "Flags must be 32-bit for atomics.");
		bool bIChangedIt = false;
		while (1)
		{
			int32 StartValue = int32(Flags);
			if (!(StartValue & int32(FlagToClear)))
			{
				break;
			}
			int32 NewValue = StartValue & ~int32(FlagToClear);
			checkSlow(NewValue != StartValue);
			if ((int32)FPlatformAtomics::InterlockedCompareExchange((int32*)&Flags, NewValue, StartValue) == StartValue)
			{
				bIChangedIt = true;
				break;
			}
		}
		return bIChangedIt;
	}

	FORCEINLINE bool ThisThreadAtomicallySetFlag(EInternalObjectFlags FlagToSet)
	{
		static_assert(sizeof(int32) == sizeof(Flags), "Flags must be 32-bit for atomics.");
		bool bIChangedIt = false;
		while (1)
		{
			int32 StartValue = int32(Flags);
			if (StartValue & int32(FlagToSet))
			{
				break;
			}
			int32 NewValue = StartValue | int32(FlagToSet);
			checkSlow(NewValue != StartValue);
			if ((int32)FPlatformAtomics::InterlockedCompareExchange((int32*)&Flags, NewValue, StartValue) == StartValue)
			{
				bIChangedIt = true;
				break;
			}
		}
		return bIChangedIt;
	}

	FORCEINLINE bool HasAnyFlags(EInternalObjectFlags InFlags) const
	{
		return !!(Flags & int32(InFlags));
	}

	FORCEINLINE void SetUnreachable()
	{
		Flags |= int32(EInternalObjectFlags::Unreachable);
	}
	FORCEINLINE void ClearUnreachable()
	{
		Flags &= ~int32(EInternalObjectFlags::Unreachable);
	}
	FORCEINLINE bool IsUnreachable() const
	{
		return !!(Flags & int32(EInternalObjectFlags::Unreachable));
	}
	FORCEINLINE bool ThisThreadAtomicallyClearedRFUnreachable()
	{
		return ThisThreadAtomicallyClearedFlag(EInternalObjectFlags::Unreachable);
	}

	FORCEINLINE void SetPendingKill()
	{
		Flags |= int32(EInternalObjectFlags::PendingKill);
	}
	FORCEINLINE void ClearPendingKill()
	{
		Flags &= ~int32(EInternalObjectFlags::PendingKill);
	}
	FORCEINLINE bool IsPendingKill() const
	{
		return !!(Flags & int32(EInternalObjectFlags::PendingKill));
	}

	FORCEINLINE void SetRootSet()
	{
		Flags |= int32(EInternalObjectFlags::RootSet);
	}
	FORCEINLINE void ClearRootSet()
	{
		Flags &= ~int32(EInternalObjectFlags::RootSet);
	}
	FORCEINLINE bool IsRootSet() const
	{
		return !!(Flags & int32(EInternalObjectFlags::RootSet));
	}

	FORCEINLINE void ResetSerialNumberAndFlags()
	{
		Flags = 0;
		ClusterRootIndex = 0;
		SerialNumber = 0;
	}
};

/**
* Fixed size UObject array.
*/
class FFixedUObjectArray
{
	/** Static master table to chunks of pointers **/
	FUObjectItem* Objects;
	/** Number of elements we currently have **/
	int32 MaxElements;
	/** Current number of UObject slots */
	int32 NumElements;

public:

	FFixedUObjectArray() TSAN_SAFE
		: Objects(nullptr)
		, MaxElements(0)
		, NumElements(0)
	{
	}

	~FFixedUObjectArray()
	{
		delete [] Objects;
	}

	/**
	* Expands the array so that Element[Index] is allocated. New pointers are all zero.
	* @param Index The Index of an element we want to be sure is allocated
	**/
	void PreAllocate(int32 InMaxElements) TSAN_SAFE
	{
		check(!Objects);
		Objects = new FUObjectItem[InMaxElements];
		MaxElements = InMaxElements;
	}

	int32 AddSingle() TSAN_SAFE
	{
		int32 Result = NumElements;
		checkf(NumElements + 1 <= MaxElements, TEXT("Maximum number of UObjects (%d) exceeded, make sure you update MaxObjectsInGame/MaxObjectsInEditor in project settings."), MaxElements);
		check(Result == NumElements);
		++NumElements;
		FPlatformMisc::MemoryBarrier();
		check(Objects[Result].Object == nullptr);
		return Result;
	}

	int32 AddRange(int32 Count) TSAN_SAFE
	{
		int32 Result = NumElements + Count - 1;
		checkf(NumElements + Count <= MaxElements, TEXT("Maximum number of UObjects (%d) exceeded, make sure you update MaxObjectsInGame/MaxObjectsInEditor in project settings."), MaxElements);
		check(Result == (NumElements + Count - 1));
		NumElements += Count;
		FPlatformMisc::MemoryBarrier();
		check(Objects[Result].Object == nullptr);
		return Result;
	}

	FORCEINLINE FUObjectItem const* GetObjectPtr(int32 Index) const TSAN_SAFE
	{
		check(Index >= 0 && Index < NumElements);
		return &Objects[Index];
	}

	FORCEINLINE FUObjectItem* GetObjectPtr(int32 Index) TSAN_SAFE
	{
		check(Index >= 0 && Index < NumElements);
		return &Objects[Index];
	}

	/**
	* Return the number of elements in the array
	* Thread safe, but you know, someone might have added more elements before this even returns
	* @return	the number of elements in the array
	**/
	FORCEINLINE int32 Num() const TSAN_SAFE
	{
		return NumElements;
	}
	/**
	* Return if this index is valid
	* Thread safe, if it is valid now, it is valid forever. Other threads might be adding during this call.
	* @param	Index	Index to test
	* @return	true, if this is a valid
	**/
	FORCEINLINE bool IsValidIndex(int32 Index) const
	{
		return Index < Num() && Index >= 0;
	}
	/**
	* Return a reference to an element
	* @param	Index	Index to return
	* @return	a reference to the pointer to the element
	* Thread safe, if it is valid now, it is valid forever. This might return nullptr, but by then, some other thread might have made it non-nullptr.
	**/
	FORCEINLINE FUObjectItem const& operator[](int32 Index) const
	{
		FUObjectItem const* ItemPtr = GetObjectPtr(Index);
		check(ItemPtr);
		return *ItemPtr;
	}

	FORCEINLINE FUObjectItem& operator[](int32 Index)
	{
		FUObjectItem* ItemPtr = GetObjectPtr(Index);
		check(ItemPtr);
		return *ItemPtr;
	}

	/**
	* Return a naked pointer to the fundamental data structure for debug visualizers.
	**/
	UObjectBase*** GetRootBlockForDebuggerVisualizers()
	{
		return nullptr;
	}
};


/***
*
* FUObjectArray replaces the functionality of GObjObjects and UObject::Index
*
* Note the layout of this data structure is mostly to emulate the old behavior and minimize code rework during code restructure.
* Better data structures could be used in the future, for example maybe all that is needed is a TSet<UObject *>
* One has to be a little careful with this, especially with the GC optimization. I have seen spots that assume
* that non-GC objects come before GC ones during iteration.
*
**/
class COREUOBJECT_API FUObjectArray
{
public:

	enum ESerialNumberConstants
	{
		START_SERIAL_NUMBER = 1000,
	};

	/**
	 * Base class for UObjectBase create class listeners
	 */
	class FUObjectCreateListener
	{
	public:
		virtual ~FUObjectCreateListener() {}
		/**
		* Provides notification that a UObjectBase has been added to the uobject array
		 *
		 * @param Object object that has been destroyed
		 * @param Index	index of object that is being deleted
		 */
		virtual void NotifyUObjectCreated(const class UObjectBase *Object, int32 Index)=0;
	};

	/**
	 * Base class for UObjectBase delete class listeners
	 */
	class FUObjectDeleteListener
	{
	public:
		virtual ~FUObjectDeleteListener() {}

		/**
		 * Provides notification that a UObjectBase has been removed from the uobject array
		 *
		 * @param Object object that has been destroyed
		 * @param Index	index of object that is being deleted
		 */
		virtual void NotifyUObjectDeleted(const class UObjectBase *Object, int32 Index)=0;
	};

	/**
	 * Constructor, initializes to no permanent object pool
	 */
	FUObjectArray();

	/**
	 * Allocates and initializes the permanent object pool
	 *
	 * @param MaxUObjects maximum number of UObjects that can ever exist in the array
	 * @param MaxObjectsNotConsideredByGC number of objects in the permanent object pool
	 */
	void AllocateObjectPool(int32 MaxUObjects, int32 MaxObjectsNotConsideredByGC);

	/**
	 * Disables the disregard for GC optimization.
	 *
	 */
	void DisableDisregardForGC();

	/**
	* If there's enough slack in the disregard pool, we can re-open it and keep adding objects to it
	*/
	void OpenDisregardForGC();

	/**
	 * After the initial load, this closes the disregard pool so that new object are GC-able
	 */
	void CloseDisregardForGC();

	/** Returns true if the disregard for GC pool is open */
	bool IsOpenForDisregardForGC() const
	{
		return OpenForDisregardForGC;
	}

	/**
	 * indicates if the disregard for GC optimization is active
	 *
	 * @return true if MaxObjectsNotConsideredByGC is greater than zero; this indicates that the disregard for GC optimization is enabled
	 */
	bool DisregardForGCEnabled() const 
	{ 
		return MaxObjectsNotConsideredByGC > 0;
	}

	/**
	 * Adds a uobject to the global array which is used for uobject iteration
	 *
	 * @param	Object Object to allocate an index for
	 */
	void AllocateUObjectIndex(class UObjectBase* Object, bool bMergingThreads = false);

	/**
	 * Returns a UObject index top to the global uobject array
	 *
	 * @param Object object to free
	 */
	void FreeUObjectIndex(class UObjectBase* Object);

	/**
	 * Returns the index of a UObject. Be advised this is only for very low level use.
	 *
	 * @param Object object to get the index of
	 * @return index of this object
	 */
	FORCEINLINE int32 ObjectToIndex(const class UObjectBase* Object) const
	{
		return Object->InternalIndex;
	}

	/**
	 * Returns the UObject corresponding to index. Be advised this is only for very low level use.
	 *
	 * @param Index index of object to return
	 * @return Object at this index
	 */
	FORCEINLINE FUObjectItem* IndexToObject(int32 Index)
	{
		check(Index >= 0);
		if (Index < ObjObjects.Num())
		{
			return const_cast<FUObjectItem*>(&ObjObjects[Index]);
		}
		return nullptr;
	}

	FORCEINLINE FUObjectItem* IndexToObjectUnsafeForGC(int32 Index)
	{
		return const_cast<FUObjectItem*>(&ObjObjects[Index]);
	}

	FORCEINLINE FUObjectItem* IndexToObject(int32 Index, bool bEvenIfPendingKill)
	{
		FUObjectItem* ObjectItem = IndexToObject(Index);
		if (ObjectItem && ObjectItem->Object)
		{
			if (!bEvenIfPendingKill && ObjectItem->IsPendingKill())
			{
				ObjectItem = nullptr;;
			}
		}
		return ObjectItem;
	}

	FORCEINLINE FUObjectItem* ObjectToObjectItem(UObjectBase* Object)
	{
		FUObjectItem* ObjectItem = IndexToObject(Object->InternalIndex);
		return ObjectItem;
	}

	FORCEINLINE bool IsValid(FUObjectItem* ObjectItem, bool bEvenIfPendingKill)
	{
		if (ObjectItem)
		{
			return bEvenIfPendingKill ? !ObjectItem->IsUnreachable() : !(ObjectItem->IsUnreachable() || ObjectItem->IsPendingKill());
		}
		return false;
	}

	FORCEINLINE FUObjectItem* IndexToValidObject(int32 Index, bool bEvenIfPendingKill)
	{
		FUObjectItem* ObjectItem = IndexToObject(Index);
		return IsValid(ObjectItem, bEvenIfPendingKill) ? ObjectItem : nullptr;
	}

	FORCEINLINE bool IsValid(int32 Index, bool bEvenIfPendingKill)
	{
		// This method assumes Index points to a valid object.
		FUObjectItem* ObjectItem = IndexToObject(Index);
		return IsValid(ObjectItem, bEvenIfPendingKill);
	}

	FORCEINLINE bool IsStale(FUObjectItem* ObjectItem, bool bEvenIfPendingKill)
	{
		// This method assumes ObjectItem is valid.
		return bEvenIfPendingKill ? (ObjectItem->IsPendingKill() || ObjectItem->IsUnreachable()) : (ObjectItem->IsUnreachable());
	}

	FORCEINLINE bool IsStale(int32 Index, bool bEvenIfPendingKill)
	{
		// This method assumes Index points to a valid object.
		FUObjectItem* ObjectItem = IndexToObject(Index);
		if (ObjectItem)
		{
			return IsStale(ObjectItem, bEvenIfPendingKill);
		}
		return true;
	}

	/**
	 * Adds a new listener for object creation
	 *
	 * @param Listener listener to notify when an object is deleted
	 */
	void AddUObjectCreateListener(FUObjectCreateListener* Listener);

	/**
	 * Removes a listener for object creation
	 *
	 * @param Listener listener to remove
	 */
	void RemoveUObjectCreateListener(FUObjectCreateListener* Listener);

	/**
	 * Adds a new listener for object deletion
	 *
	 * @param Listener listener to notify when an object is deleted
	 */
	void AddUObjectDeleteListener(FUObjectDeleteListener* Listener);

	/**
	 * Removes a listener for object deletion
	 *
	 * @param Listener listener to remove
	 */
	void RemoveUObjectDeleteListener(FUObjectDeleteListener* Listener);

	/**
	 * Checks if a UObject pointer is valid
	 *
	 * @param	Object object to test for validity
	 * @return	true if this index is valid
	 */
	bool IsValid(const UObjectBase* Object) const;

	/** Checks if the object index is valid. */
	FORCEINLINE bool IsValidIndex(const UObjectBase* Object) const 
	{ 
		return ObjObjects.IsValidIndex(Object->InternalIndex);
	}

	/**
	 * Returns true if this object is "disregard for GC"...same results as the legacy RF_DisregardForGC flag
	 *
	 * @param Object object to get for disregard for GC
	 * @return true if this object si disregard for GC
	 */
	FORCEINLINE bool IsDisregardForGC(const class UObjectBase* Object)
	{
		return Object->InternalIndex <= ObjLastNonGCIndex;
	}
	/**
	 * Returns the size of the global UObject array, some of these might be unused
	 *
	 * @return	the number of UObjects in the global array
	 */
	FORCEINLINE int32 GetObjectArrayNum() const 
	{ 
		return ObjObjects.Num();
	}

	/**
	 * Returns the size of the global UObject array minus the number of permanent objects
	 *
	 * @return	the number of UObjects in the global array
	 */
	FORCEINLINE int32 GetObjectArrayNumMinusPermanent() const 
	{ 
		return ObjObjects.Num() - (ObjLastNonGCIndex + 1);
	}

	/**
	 * Returns the number of permanent objects
	 *
	 * @return	the number of permanent objects
	 */
	FORCEINLINE int32 GetObjectArrayNumPermanent() const 
	{ 
		return ObjLastNonGCIndex + 1;
	}

#if UE_GC_TRACK_OBJ_AVAILABLE
	/**
	 * Returns the number of actual object indices that are claimed (the total size of the global object array minus
	 * the number of available object array elements
	 *
	 * @return	The number of objects claimed
	 */
	FORCEINLINE int32 GetObjectArrayNumMinusAvailable()
	{
		return ObjObjects.Num() - ObjAvailableCount.GetValue();
	}
#endif

	/**
	 * Clears some internal arrays to get rid of false memory leaks
	 */
	void ShutdownUObjectArray();

	/**
	* Given a UObject index return the serial number. If it doesn't have a serial number, give it one. Threadsafe.
	* @param Index - UObject Index
	* @return - the serial number for this UObject
	*/
	int32 AllocateSerialNumber(int32 Index);

	/**
	* Given a UObject index return the serial number. If it doesn't have a serial number, return 0. Threadsafe.
	* @param Index - UObject Index
	* @return - the serial number for this UObject
	*/
	FORCEINLINE int32 GetSerialNumber(int32 Index)
	{
		FUObjectItem* ObjectItem = IndexToObject(Index);
		checkSlow(ObjectItem);
		return ObjectItem->GetSerialNumber();
	}

	/**
	 * Low level iterator.
	 */
	class TIterator
	{
	public:
		enum EEndTagType
		{
			EndTag
		};

		/**
		 * Constructor
		 *
		 * @param	InArray				the array to iterate on
		 * @param	bOnlyGCedObjects	if true, skip all of the permanent objects
		 */
		TIterator( const FUObjectArray& InArray, bool bOnlyGCedObjects = false ) :	
			Array(InArray),
			Index(-1),
			CurrentObject(nullptr)
		{
			if (bOnlyGCedObjects)
			{
				Index = Array.ObjLastNonGCIndex;
			}
			Advance();
		}

		/**
		 * Constructor
		 *
		 * @param	InArray				the array to iterate on
		 * @param	bOnlyGCedObjects	if true, skip all of the permanent objects
		 */
		TIterator( EEndTagType, const TIterator& InIter ) :	
			Array (InIter.Array),
			Index(Array.ObjObjects.Num())
		{
		}

		/**
		 * Iterator advance
		 */
		FORCEINLINE void operator++()
		{
			Advance();
		}

		friend bool operator==(const TIterator& Lhs, const TIterator& Rhs) { return Lhs.Index == Rhs.Index; }
		friend bool operator!=(const TIterator& Lhs, const TIterator& Rhs) { return Lhs.Index != Rhs.Index; }

		/** Conversion to "bool" returning true if the iterator is valid. */
		FORCEINLINE explicit operator bool() const
		{ 
			return !!CurrentObject;
		}
		/** inverse of the "bool" operator */
		FORCEINLINE bool operator !() const 
		{
			return !(bool)*this;
		}

		FORCEINLINE int32 GetIndex() const
		{
			return Index;
		}

	protected:

		/**
		 * Dereferences the iterator with an ordinary name for clarity in derived classes
		 *
		 * @return	the UObject at the iterator
		 */
		FORCEINLINE FUObjectItem* GetObject() const
		{ 
			return CurrentObject;
		}
		/**
		 * Iterator advance with ordinary name for clarity in subclasses
		 * @return	true if the iterator points to a valid object, false if iteration is complete
		 */
		FORCEINLINE bool Advance()
		{
			//@todo UE4 check this for LHS on Index on consoles
			FUObjectItem* NextObject = nullptr;
			CurrentObject = nullptr;
			while(++Index < Array.GetObjectArrayNum())
			{
				NextObject = const_cast<FUObjectItem*>(&Array.ObjObjects[Index]);
				if (NextObject->Object)
				{
					CurrentObject = NextObject;
					return true;
				}
			}
			return false;
		}
	private:
		/** the array that we are iterating on, probably always GUObjectArray */
		const FUObjectArray& Array;
		/** index of the current element in the object array */
		int32 Index;
		/** Current object */
		mutable FUObjectItem* CurrentObject;
	};

private:

	//typedef TStaticIndirectArrayThreadSafeRead<UObjectBase, 8 * 1024 * 1024 /* Max 8M UObjects */, 16384 /* allocated in 64K/128K chunks */ > TUObjectArray;
	typedef FFixedUObjectArray TUObjectArray;

	// note these variables are left with the Obj prefix so they can be related to the historical GObj versions

	/** First index into objects array taken into account for GC.							*/
	int32 ObjFirstGCIndex;
	/** Index pointing to last object created in range disregarded for GC.					*/
	int32 ObjLastNonGCIndex;
	/** Maximum number of objects in the disregard for GC Pool */
	int32 MaxObjectsNotConsideredByGC;

	/** If true this is the intial load and we should load objects int the disregarded for GC range.	*/
	bool OpenForDisregardForGC;
	/** Array of all live objects.											*/
	TUObjectArray ObjObjects;
	/** Synchronization object for all live objects.											*/
	FCriticalSection ObjObjectsCritical;
	/** Available object indices.											*/
	TLockFreePointerListUnordered<int32, PLATFORM_CACHE_LINE_SIZE> ObjAvailableList;
#if UE_GC_TRACK_OBJ_AVAILABLE
	/** Available object index count.										*/
	FThreadSafeCounter ObjAvailableCount;
#endif
	/**
	 * Array of things to notify when a UObjectBase is created
	 */
	TArray<FUObjectCreateListener* > UObjectCreateListeners;
	/**
	 * Array of things to notify when a UObjectBase is destroyed
	 */
	TArray<FUObjectDeleteListener* > UObjectDeleteListeners;
#if THREADSAFE_UOBJECTS
	FCriticalSection UObjectDeleteListenersCritical;
#endif

	/** Current master serial number **/
	FThreadSafeCounter	MasterSerialNumber;
};

/** UObject cluster. Groups UObjects into a single unit for GC. */
struct FUObjectCluster
{
	FUObjectCluster()
		: RootIndex(INDEX_NONE)
		, bNeedsDissolving(false)
	{}

	/** Root object index */
	int32 RootIndex;
	/** Objects that belong to this cluster */
	TArray<int32> Objects;
	/** Other clusters referenced by this cluster */
	TArray<int32> ReferencedClusters;
	/** Objects that could not be added to the cluster but still need to be referenced by it */
	TArray<int32> MutableObjects;
	/** List of clusters that direcly reference this cluster. Used when dissolving a cluster. */
	TArray<int32> ReferencedByClusters;

	/** Cluster needs dissolving, probably due to PendingKill reference */
	bool bNeedsDissolving;
};

class COREUOBJECT_API FUObjectClusterContainer
{
	/** List of all clusters */
	TArray<FUObjectCluster> Clusters;
	/** List of available cluster indices */
	TArray<int32> FreeClusterIndices;
	/** Number of allocated clusters */
	int32 NumAllocatedClusters;
	/** Clusters need dissolving, probably due to PendingKill reference */
	bool bClustersNeedDissolving;

	/** Dissolves a cluster */
	void DissolveCluster(FUObjectCluster& Cluster);

public:

	FUObjectClusterContainer();

	FORCEINLINE FUObjectCluster& operator[](int32 Index)
	{
		return Clusters[Index];
	}

	/** Returns an index to a new cluster */
	int32 AllocateCluster(int32 InRootObjectIndex);

	/** Frees the cluster at the specified index */
	void FreeCluster(int32 InClusterIndex);

	/**
	* Gets the cluster the specified object is a root of or belongs to.
	* @Param ClusterRootOrObjectFromCluster Root cluster object or object that belongs to a cluster
	*/
	FUObjectCluster* GetObjectCluster(UObjectBaseUtility* ClusterRootOrObjectFromCluster);


	/** 
	 * Dissolves a cluster and all clusters that reference it 
	 * @Param ClusterRootOrObjectFromCluster Root cluster object or object that belongs to a cluster
	 */
	void DissolveCluster(UObjectBaseUtility* ClusterRootOrObjectFromCluster);

	/** Dissolve all clusters marked for dissolving */
	void DissolveClusters();

	/** Dissolve the specified cluster and all clusters that reference it */
	void DissolveClusterAndMarkObjectsAsUnreachable(const int32 CurrentIndex, FUObjectItem* RootObjectItem);

	/** Gets the clusters array (for internal use only!) */
	TArray<FUObjectCluster>& GetClustersUnsafe() 
	{ 
		return Clusters;  
	}

	/** Returns the number of currently allocated clusters */
	int32 GetNumAllocatedClusters() const
	{
		return NumAllocatedClusters;
	}

	/** Lets the FUObjectClusterContainer know some clusters need dissolving */
	void SetClustersNeedDissolving()
	{
		bClustersNeedDissolving = true;
	}
	
	/** Checks if any clusters need dissolving */
	bool ClustersNeedDissolving() const
	{
		return bClustersNeedDissolving;
	}
};

/** Global UObject allocator							*/
extern COREUOBJECT_API FUObjectArray GUObjectArray;
extern COREUOBJECT_API FUObjectClusterContainer GUObjectClusters;

/**
	* Static version of IndexToObject for use with TWeakObjectPtr.
	*/
struct FIndexToObject
{
	static FORCEINLINE class UObjectBase* IndexToObject(int32 Index, bool bEvenIfPendingKill)
	{
		FUObjectItem* ObjectItem = GUObjectArray.IndexToObject(Index, bEvenIfPendingKill);
		return ObjectItem ? ObjectItem->Object : nullptr;
	}
};
