// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SoftObjectPtr.h: Pointer to UObject asset, keeps extra information so that it is works even if the asset is not in memory
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Templates/Casts.h"
#include "UObject/PersistentObjectPtr.h"
#include "UObject/SoftObjectPath.h"

/**
 * FSoftObjectPtr is a type of weak pointer to a UObject, that also keeps track of the path to the object on disk.
 * It will change back and forth between being Valid and Pending as the referenced object loads or unloads.
 * It has no impact on if the object is garbage collected or not.
 *
 * This is useful to specify assets that you may want to asynchronously load on demand.
 */
struct FSoftObjectPtr : public TPersistentObjectPtr<FSoftObjectPath>
{
public:	
	/** Default constructor, will be null */
	FORCEINLINE FSoftObjectPtr()
	{
	}

	/** Construct from another soft pointer */
	FORCEINLINE FSoftObjectPtr(const FSoftObjectPtr& Other)
	{
		(*this)=Other;
	}

	/** Construct from a soft object path */
	explicit FORCEINLINE FSoftObjectPtr(const FSoftObjectPath& ObjectPath)
		: TPersistentObjectPtr<FSoftObjectPath>(ObjectPath)
	{
	}

	/** Construct from an object already in memory */
	explicit FORCEINLINE FSoftObjectPtr(const UObject* Object)
	{
		(*this)=Object;
	}

	/** Synchronously load (if necessary) and return the asset object represented by this asset ptr */
	UObject* LoadSynchronous() const
	{
		UObject* Asset = Get();
		if (Asset == nullptr && IsPending())
		{
			ToSoftObjectPath().TryLoad();
			
			// TryLoad will have loaded this pointer if it is valid
			Asset = Get();
		}
		return Asset;
	}

	/** Returns the StringObjectPath that is wrapped by this SoftObjectPtr */
	FORCEINLINE const FSoftObjectPath& ToSoftObjectPath() const
	{
		return GetUniqueID();
	}

	DEPRECATED(4.18, "ToStringReference was renamed to ToSoftObjectPath")
	FORCEINLINE const FSoftObjectPath& ToStringReference() const
	{
		return GetUniqueID();
	}

	/** Returns string representation of reference, in form /package/path.assetname */
	FORCEINLINE FString ToString() const
	{
		return ToSoftObjectPath().ToString();
	}

	/** Returns /package/path string, leaving off the asset name */
	FORCEINLINE FString GetLongPackageName() const
	{
		return ToSoftObjectPath().GetLongPackageName();
	}
	
	/** Returns assetname string, leaving off the /package/path. part */
	FORCEINLINE FString GetAssetName() const
	{
		return ToSoftObjectPath().GetAssetName();
	}

#if WITH_EDITOR
	/** Overridden to deal with PIE lookups */
	FORCEINLINE UObject* Get() const
	{
		if (GPlayInEditorID != INDEX_NONE)
		{
			// Cannot use or set the cached value in PIE as it may affect other PIE instances or the editor
			return GetUniqueID().ResolveObject();
		}
		return TPersistentObjectPtr<FSoftObjectPath>::Get();
	}
#endif

	using TPersistentObjectPtr<FSoftObjectPath>::operator=;
};

template <> struct TIsPODType<FSoftObjectPtr> { enum { Value = TIsPODType<TPersistentObjectPtr<FSoftObjectPath> >::Value }; };
template <> struct TIsWeakPointerType<FSoftObjectPtr> { enum { Value = TIsWeakPointerType<TPersistentObjectPtr<FSoftObjectPath> >::Value }; };
template <> struct THasGetTypeHash<FSoftObjectPtr> { enum { Value = THasGetTypeHash<TPersistentObjectPtr<FSoftObjectPath> >::Value }; };

/**
 * TSoftObjectPtr is templatized wrapper of the generic FSoftObjectPtr, it can be used in UProperties
 */
template<class T=UObject>
struct TSoftObjectPtr
{
	template <class U>
	friend struct TSoftObjectPtr;

public:
	/** Default constructor, will be null */
	FORCEINLINE TSoftObjectPtr()
	{
	}
	
	/** Construct from another soft pointer */
	template <class U, class = typename TEnableIf<TPointerIsConvertibleFromTo<U, T>::Value>::Type>
	FORCEINLINE TSoftObjectPtr(const TSoftObjectPtr<U>& Other)
		: SoftObjectPtr(Other.SoftObjectPtr)
	{
	}

	/** Construct from an object already in memory */
	template <typename U>
	FORCEINLINE TSoftObjectPtr(const U* Object)
		: SoftObjectPtr(Object)
	{
	}

	/** Construct from a nullptr */
	FORCEINLINE TSoftObjectPtr(TYPE_OF_NULLPTR)
		: SoftObjectPtr(nullptr)
	{
	}

	/** Construct from a soft object path */
	explicit FORCEINLINE TSoftObjectPtr(const FSoftObjectPath& ObjectPath)
		: SoftObjectPtr(ObjectPath)
	{
	}

	/** Reset the soft pointer back to the null state */
	FORCEINLINE void Reset()
	{
		SoftObjectPtr.Reset();
	}

	/** Resets the weak ptr only, call this when ObjectId may change */
	FORCEINLINE void ResetWeakPtr()
	{
		SoftObjectPtr.ResetWeakPtr();
	}

	/** Copy from an object already in memory */
	template <typename U>
	FORCEINLINE TSoftObjectPtr& operator=(const U* Object)
	{
		SoftObjectPtr = Object;
		return *this;
	}

	/** Assign from a nullptr */
	FORCEINLINE TSoftObjectPtr& operator=(TYPE_OF_NULLPTR)
	{
		SoftObjectPtr = nullptr;
		return *this;
	}

	/** Copy from a soft object path */
	FORCEINLINE TSoftObjectPtr& operator=(const FSoftObjectPath& ObjectPath)
	{
		SoftObjectPtr = ObjectPath;
		return *this;
	}

	/** Copy from a weak pointer to an object already in memory */
	template <class U, class = typename TEnableIf<TPointerIsConvertibleFromTo<U, T>::Value>::Type>
	FORCEINLINE TSoftObjectPtr& operator=(const TWeakObjectPtr<U>& Other)
	{
		SoftObjectPtr = Other;
		return *this;
	}

	/** Copy from another soft pointer */
	template <class U, class = typename TEnableIf<TPointerIsConvertibleFromTo<U, T>::Value>::Type>
	FORCEINLINE TSoftObjectPtr& operator=(const TSoftObjectPtr<U>& Other)
	{
		SoftObjectPtr = Other.SoftObjectPtr;
		return *this;
	}

	/**
	 * Compare soft pointers for equality
	 * Caution: Two soft pointers might not be equal to each other, but they both might return nullptr
	 *
	 * @param Other soft pointer to compare to
	 */
	FORCEINLINE friend bool operator==(const TSoftObjectPtr& Lhs, const TSoftObjectPtr& Rhs)
	{
		return Lhs.SoftObjectPtr == Rhs.SoftObjectPtr;
	}

	/**
	 * Compare soft pointers for equality
	 * Caution: Two soft pointers might not be equal to each other, but they both might return nullptr
	 *
	 * @param Other soft pointer to compare to
	 */
	FORCEINLINE friend bool operator==(const TSoftObjectPtr& Lhs, TYPE_OF_NULLPTR)
	{
		return Lhs.SoftObjectPtr == nullptr;
	}

	/**
	 * Compare soft pointers for equality
	 * Caution: Two soft pointers might not be equal to each other, but they both might return nullptr
	 *
	 * @param Other soft pointer to compare to
	 */
	FORCEINLINE friend bool operator==(TYPE_OF_NULLPTR, const TSoftObjectPtr& Rhs)
	{
		return nullptr == Rhs.SoftObjectPtr;
	}

	/**
	 * Compare soft pointers for inequality
	 * Caution: Two soft pointers might not be equal to each other, but they both might return nullptr
	 *
	 * @param Other soft pointer to compare to
	 */
	FORCEINLINE friend bool operator!=(const TSoftObjectPtr& Lhs, const TSoftObjectPtr& Rhs)
	{
		return Lhs.SoftObjectPtr != Rhs.SoftObjectPtr;
	}

	/**
	 * Compare soft pointers for inequality
	 * Caution: Two soft pointers might not be equal to each other, but they both might return nullptr
	 *
	 * @param Other soft pointer to compare to
	 */
	FORCEINLINE friend bool operator!=(const TSoftObjectPtr& Lhs, TYPE_OF_NULLPTR)
	{
		return Lhs.SoftObjectPtr != nullptr;
	}

	/**
	 * Compare soft pointers for inequality
	 * Caution: Two soft pointers might not be equal to each other, but they both might return nullptr
	 *
	 * @param Other soft pointer to compare to
	 */
	FORCEINLINE friend bool operator!=(TYPE_OF_NULLPTR, const TSoftObjectPtr& Rhs)
	{
		return nullptr != Rhs.SoftObjectPtr;
	}

	/**
	 * Dereference the soft pointer.
	 *
	 * @return nullptr if this object is gone or the lazy pointer was null, otherwise a valid UObject pointer
	 */
	FORCEINLINE T* Get() const
	{
		return dynamic_cast<T*>(SoftObjectPtr.Get());
	}

	/** Dereference the soft pointer */
	FORCEINLINE T& operator*() const
	{
		return *Get();
	}

	/** Dereference the soft pointer */
	FORCEINLINE T* operator->() const
	{
		return Get();
	}

	/** Synchronously load (if necessary) and return the asset object represented by this asset ptr */
	T* LoadSynchronous() const
	{
		UObject* Asset = SoftObjectPtr.LoadSynchronous();
		return Cast<T>(Asset);
	}

	/**  
	 * Test if this points to a live UObject
	 *
	 * @return true if Get() would return a valid non-null pointer
	 */
	FORCEINLINE bool IsValid() const
	{
		// This does the runtime type check
		return Get() != nullptr;
	}

	/**  
	 * Test if this does not point to a live UObject, but may in the future
	 *
	 * @return true if this does not point to a real object, but could possibly
	 */
	FORCEINLINE bool IsPending() const
	{
		return SoftObjectPtr.IsPending();
	}

	/**  
	 * Test if this can never point to a live UObject
	 *
	 * @return true if this is explicitly pointing to no object
	 */
	FORCEINLINE bool IsNull() const
	{
		return SoftObjectPtr.IsNull();
	}

	/** Returns the StringObjectPath that is wrapped by this SoftObjectPtr */
	FORCEINLINE const FSoftObjectPath& GetUniqueID() const
	{
		return SoftObjectPtr.GetUniqueID();
	}

	/** Returns the StringObjectPath that is wrapped by this SoftObjectPtr */
	FORCEINLINE const FSoftObjectPath& ToSoftObjectPath() const
	{
		return SoftObjectPtr.GetUniqueID();
	}

	DEPRECATED(4.18, "ToStringReference was renamed to ToSoftObjectPath")
	FORCEINLINE const FSoftObjectPath& ToStringReference() const
	{
		return SoftObjectPtr.GetUniqueID();
	}

	/** Returns string representation of reference, in form /package/path.assetname */
	FORCEINLINE FString ToString() const
	{
		return ToSoftObjectPath().ToString();
	}

	/** Returns /package/path string, leaving off the asset name */
	FORCEINLINE FString GetLongPackageName() const
	{
		return ToSoftObjectPath().GetLongPackageName();
	}
	
	/** Returns assetname string, leaving off the /package/path part */
	FORCEINLINE FString GetAssetName() const
	{
		return ToSoftObjectPath().GetAssetName();
	}

	/** Dereference soft pointer to see if it points somewhere valid */
	FORCEINLINE explicit operator bool() const
	{
		return IsValid();
	}

	/** Hash function */
	FORCEINLINE friend uint32 GetTypeHash(const TSoftObjectPtr<T>& Other)
	{
		return GetTypeHash(static_cast<const TPersistentObjectPtr<FSoftObjectPath>&>(Other.SoftObjectPtr));
	}

	friend FArchive& operator<<(FArchive& Ar, TSoftObjectPtr<T>& Other)
	{
		Ar << Other.SoftObjectPtr;
		return Ar;
	}

private:
	FSoftObjectPtr SoftObjectPtr;
};

template<class T> struct TIsPODType<TSoftObjectPtr<T> > { enum { Value = TIsPODType<FSoftObjectPtr>::Value }; };
template<class T> struct TIsWeakPointerType<TSoftObjectPtr<T> > { enum { Value = TIsWeakPointerType<FSoftObjectPtr>::Value }; };

/**
 * TSoftClassPtr is a templatized wrapper around FSoftObjectPtr that works like a TSubclassOf, it can be used in UProperties for blueprint subclasses
 */
template<class TClass=UObject>
class TSoftClassPtr
{
	template <class TClassA>
	friend class TSoftClassPtr;

public:
	/** Default constructor, will be null */
	FORCEINLINE TSoftClassPtr()
	{
	}
		
	/** Construct from another soft pointer */
	template <class TClassA, class = typename TEnableIf<TPointerIsConvertibleFromTo<TClassA, TClass>::Value>::Type>
	FORCEINLINE TSoftClassPtr(const TSoftClassPtr<TClassA>& Other)
		: SoftObjectPtr(Other.SoftObjectPtr)
	{
	}

	/** Construct from a class already in memory */
	FORCEINLINE TSoftClassPtr(const UClass* From)
		: SoftObjectPtr(From)
	{
	}

	/** Construct from a soft object path */
	explicit FORCEINLINE TSoftClassPtr(const FSoftObjectPath& ObjectPath)
		: SoftObjectPtr(ObjectPath)
	{
	}

	/** Reset the soft pointer back to the null state */
	FORCEINLINE void Reset()
	{
		SoftObjectPtr.Reset();
	}

	/** Resets the weak ptr only, call this when ObjectId may change */
	FORCEINLINE void ResetWeakPtr()
	{
		SoftObjectPtr.ResetWeakPtr();
	}

	/** Copy from a class already in memory */
	FORCEINLINE void operator=(const UClass* From)
	{
		SoftObjectPtr = From;
	}

	/** Copy from a soft object path */
	FORCEINLINE void operator=(const FSoftObjectPath& ObjectPath)
	{
		SoftObjectPtr = ObjectPath;
	}

	/** Copy from a weak pointer already in memory */
	template<class TClassA, class = typename TEnableIf<TPointerIsConvertibleFromTo<TClassA, UClass>::Value>::Type>
	FORCEINLINE TSoftClassPtr& operator=(const TWeakObjectPtr<TClassA>& Other)
	{
		SoftObjectPtr = Other;
		return *this;
	}

	/** Copy from another soft pointer */
	template<class TClassA, class = typename TEnableIf<TPointerIsConvertibleFromTo<TClassA, TClass>::Value>::Type>
	FORCEINLINE TSoftClassPtr& operator=(const TSoftObjectPtr<TClassA>& Other)
	{
		SoftObjectPtr = Other.SoftObjectPtr;
		return *this;
	}

	/**  
	 * Compare soft pointers for equality
	 * Caution: Two soft pointers might not be equal to each other, but they both might return nullptr
	 *
	 * @param Other soft pointer to compare to 
	 */
	FORCEINLINE friend bool operator==(const TSoftClassPtr& Lhs, const TSoftClassPtr& Rhs)
	{
		return Lhs.SoftObjectPtr == Rhs.SoftObjectPtr;
	}

	/**  
	 * Compare soft pointers for inequality
	 * Caution: Two soft pointers might not be equal to each other, but they both might return nullptr
	 *
	 * @param Other soft pointer to compare to
	 */
	FORCEINLINE friend bool operator!=(const TSoftClassPtr& Lhs, const TSoftClassPtr& Rhs)
	{
		return Lhs.SoftObjectPtr != Rhs.SoftObjectPtr;
	}

	/**  
	 * Dereference the soft pointer
	 *
	 * @return nullptr if this object is gone or the soft pointer was null, otherwise a valid UClass pointer
	 */
	FORCEINLINE UClass* Get() const
	{
		UClass* Class = dynamic_cast<UClass*>(SoftObjectPtr.Get());
		if (!Class || !Class->IsChildOf(TClass::StaticClass()))
		{
			return nullptr;
		}
		return Class;
	}

	/** Dereference the soft pointer */
	FORCEINLINE UClass& operator*() const
	{
		return *Get();
	}

	/** Dereference the soft pointer */
	FORCEINLINE UClass* operator->() const
	{
		return Get();
	}

	/**  
	 * Test if this points to a live UObject
	 *
	 * @return true if Get() would return a valid non-null pointer
	 */
	FORCEINLINE bool IsValid() const
	{
		// This also does the UClass type check
		return Get() != nullptr;
	}

	/**  
	 * Test if this does not point to a live UObject, but may in the future
	 *
	 * @return true if this does not point to a real object, but could possibly
	 */
	FORCEINLINE bool IsPending() const
	{
		return SoftObjectPtr.IsPending();
	}

	/**  
	 * Test if this can never point to a live UObject
	 *
	 * @return true if this is explicitly pointing to no object
	 */
	FORCEINLINE bool IsNull() const
	{
		return SoftObjectPtr.IsNull();
	}

	/** Returns the StringObjectPath that is wrapped by this SoftObjectPtr */
	FORCEINLINE const FSoftObjectPath& GetUniqueID() const
	{
		return SoftObjectPtr.GetUniqueID();
	}

	/** Returns the StringObjectPath that is wrapped by this SoftObjectPtr */
	FORCEINLINE const FSoftObjectPath& ToSoftObjectPath() const
	{
		return SoftObjectPtr.GetUniqueID();
	}

	DEPRECATED(4.18, "ToStringReference was renamed to ToSoftObjectPath")
	FORCEINLINE const FSoftObjectPath& ToStringReference() const
	{
		return SoftObjectPtr.GetUniqueID();
	}

	/** Returns string representation of reference, in form /package/path.assetname  */
	FORCEINLINE FString ToString() const
	{
		return ToSoftObjectPath().ToString();
	}

	/** Returns /package/path string, leaving off the asset name */
	FORCEINLINE FString GetLongPackageName() const
	{
		return ToSoftObjectPath().GetLongPackageName();
	}
	
	/** Returns assetname string, leaving off the /package/path part */
	FORCEINLINE FString GetAssetName() const
	{
		return ToSoftObjectPath().GetAssetName();
	}

	/** Dereference soft pointer to see if it points somewhere valid */
	FORCEINLINE explicit operator bool() const
	{
		return IsValid();
	}

	/** Hash function */
	FORCEINLINE friend uint32 GetTypeHash(const TSoftClassPtr<TClass>& Other)
	{
		return GetTypeHash(static_cast<const TPersistentObjectPtr<FSoftObjectPath>&>(Other.SoftObjectPtr));
	}

	/** Synchronously load (if necessary) and return the asset object represented by this asset ptr */
	UClass* LoadSynchronous() const
	{
		UObject* Asset = SoftObjectPtr.LoadSynchronous();
		UClass* Class = dynamic_cast<UClass*>(Asset);
		if (!Class || !Class->IsChildOf(TClass::StaticClass()))
		{
			return nullptr;
		}
		return Class;
	}

	friend FArchive& operator<<(FArchive& Ar, TSoftClassPtr<TClass>& Other)
	{
		Ar << static_cast<FSoftObjectPtr&>(Other.SoftObjectPtr);
		return Ar;
	}

private:
	FSoftObjectPtr SoftObjectPtr;
};

template <class T> struct TIsPODType<TSoftClassPtr<T> > { enum { Value = TIsPODType<FSoftObjectPtr>::Value }; };
template <class T> struct TIsWeakPointerType<TSoftClassPtr<T> > { enum { Value = TIsWeakPointerType<FSoftObjectPtr>::Value }; };

DEPRECATED(4.18, "FAssetPtr was renamed to FSoftObjectPtr as it is not necessarily an asset")
typedef FSoftObjectPtr FAssetPtr;

// Not deprecating these yet as it will lead to too many warnings in games
//DEPRECATED(4.18, "TAssetPtr was renamed to TSoftObjectPtr as it is not necessarily an asset")
template<class T=UObject>
using TAssetPtr = TSoftObjectPtr<T>;

//DEPRECATED(4.18, "TAssetSubclassOf was renamed to TSoftClassPtr")
template<class TClass = UObject>
using TAssetSubclassOf = TSoftClassPtr<TClass>;

