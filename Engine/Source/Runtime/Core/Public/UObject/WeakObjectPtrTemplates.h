// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/IsPointer.h"
#include "Templates/PointerIsConvertibleFromTo.h"
#include "Templates/AreTypesEqual.h"
#include "Containers/Map.h"
#include "UObject/AutoPointer.h"

struct FWeakObjectPtr;

/***
 * 
 * FWeakObjectPtr is a weak pointer to a UObject. 
 * It can return nullptr later if the object is garbage collected.
 * It has no impact on if the object is garbage collected or not.
 * It can't be directly used across a network.
 *
 * Most often it is used when you explicitly do NOT want to prevent something from being garbage collected.
 **/
struct FWeakObjectPtr;

template<class T=UObject, class TWeakObjectPtrBase=FWeakObjectPtr>
struct TWeakObjectPtr;


/***
* 
* TWeakObjectPtr is templatized version of the generic FWeakObjectPtr
* 
**/
template<class T, class TWeakObjectPtrBase>
struct TWeakObjectPtr : private TWeakObjectPtrBase
{
	// Although templated, these parameters are not intended to be anything other than the default,
	// and are only templates for module organization reasons.
	static_assert(TAreTypesEqual<TWeakObjectPtrBase, FWeakObjectPtr>::Value, "TWeakObjectPtrBase should not be overridden");

public:

	/** Default constructor (no initialization). **/
	FORCEINLINE TWeakObjectPtr() { }

	/**  
	 * Construct from an object pointer
	 * @param Object object to create a weak pointer to
	**/
	FORCEINLINE TWeakObjectPtr(const T* Object) :
		TWeakObjectPtrBase((UObject*)Object)
	{
		// This static assert is in here rather than in the body of the class because we want
		// to be able to define TWeakObjectPtr<UUndefinedClass>.
		static_assert(TPointerIsConvertibleFromTo<T, const volatile UObject>::Value, "TWeakObjectPtr can only be constructed with UObject types");
	}

	/**  
	 * Construct from another weak pointer
	 * @param Other weak pointer to copy from
	**/
	FORCEINLINE TWeakObjectPtr(const TWeakObjectPtr& Other) :
		TWeakObjectPtrBase(Other)
	{ }

	/**  
	 * Construct from another weak pointer of another type, intended for derived-to-base conversions
	 * @param Other weak pointer to copy from
	**/
	template <typename OtherT>
	FORCEINLINE TWeakObjectPtr(const TWeakObjectPtr<OtherT, TWeakObjectPtrBase>& Other) :
		TWeakObjectPtrBase(*(TWeakObjectPtrBase*)&Other) // we do a C-style cast to private base here to avoid clang 3.6.0 compilation problems with friend declarations
	{
		// It's also possible that this static_assert may fail for valid conversions because
		// one or both of the types have only been forward-declared.
		static_assert(TPointerIsConvertibleFromTo<OtherT, T>::Value, "Unable to convert TWeakObjectPtr - types are incompatible");
	}

	/**
	 * Reset the weak pointer back to the NULL state
	 */
	FORCEINLINE void Reset()
	{
		TWeakObjectPtrBase::Reset();
	}

	/**  
	 * Copy from an object pointer
	 * @param Object object to create a weak pointer to
	**/
	template<class U>
	FORCEINLINE void operator=(const U* Object)
	{
		const T* TempObject = Object;
		TWeakObjectPtrBase::operator=(TempObject);
	}

	/**  
	 * Construct from another weak pointer
	 * @param Other weak pointer to copy from
	**/
	FORCEINLINE void operator=(const TWeakObjectPtr& Other)
	{
		TWeakObjectPtrBase::operator=(Other);
	}

	/**  
	 * Assign from another weak pointer, intended for derived-to-base conversions
	 * @param Other weak pointer to copy from
	**/
	template <typename OtherT>
	FORCEINLINE void operator=(const TWeakObjectPtr<OtherT, TWeakObjectPtrBase>& Other)
	{
		// It's also possible that this static_assert may fail for valid conversions because
		// one or both of the types have only been forward-declared.
		static_assert(TPointerIsConvertibleFromTo<OtherT, T>::Value, "Unable to convert TWeakObjectPtr - types are incompatible");

		*(TWeakObjectPtrBase*)this = *(TWeakObjectPtrBase*)&Other; // we do a C-style cast to private base here to avoid clang 3.6.0 compilation problems with friend declarations
	}

	/**  
	 * Dereference the weak pointer
	 * @param bEvenIfPendingKill, if this is true, pendingkill objects are considered valid	
	 * @return NULL if this object is gone or the weak pointer was NULL, otherwise a valid uobject pointer
	**/
	FORCEINLINE T* Get(bool bEvenIfPendingKill) const
	{
		return (T*)TWeakObjectPtrBase::Get(bEvenIfPendingKill);
	}

	/**  
	 * Dereference the weak pointer. This is an optimized version implying bEvenIfPendingKill=false.
	 */
	FORCEINLINE T* Get(/*bool bEvenIfPendingKill = false*/) const
	{
		return (T*)TWeakObjectPtrBase::Get();
	}

	/** Deferences the weak pointer even if its marked RF_Unreachable. This is needed to resolve weak pointers during GC (such as ::AddReferenceObjects) */
	FORCEINLINE T* GetEvenIfUnreachable() const
	{
		return (T*)TWeakObjectPtrBase::GetEvenIfUnreachable();
	}

	/**  
	 * Dereference the weak pointer
	**/
	FORCEINLINE T & operator*() const
	{
		return *Get();
	}

	/**  
	 * Dereference the weak pointer
	**/
	FORCEINLINE T * operator->() const
	{
		return Get();
	}

	/**  
	 * Test if this points to a live UObject
	 * @param bEvenIfPendingKill, if this is true, pendingkill objects are considered valid
	 * @param bThreadsafeTest, if true then function will just give you information whether referenced 
	 *							UObject is gone forever (@return false) or if it is still there (@return true, no object flags checked).
	 * @return true if Get() would return a valid non-null pointer
	**/
	FORCEINLINE bool IsValid(bool bEvenIfPendingKill, bool bThreadsafeTest = false) const
	{
		return TWeakObjectPtrBase::IsValid(bEvenIfPendingKill, bThreadsafeTest);
	}

	/**
	 * Test if this points to a live UObject. This is an optimized version implying bEvenIfPendingKill=false, bThreadsafeTest=false.
	 * @return true if Get() would return a valid non-null pointer
	 */
	FORCEINLINE bool IsValid(/*bool bEvenIfPendingKill = false, bool bThreadsafeTest = false*/) const
	{
		return TWeakObjectPtrBase::IsValid();
	}

	/**  
	 * Slightly different than !IsValid(), returns true if this used to point to a UObject, but doesn't any more and has not been assigned or reset in the mean time.
	 * @param bIncludingIfPendingKill, if this is true, pendingkill objects are considered stale
	 * @param bThreadsafeTest, set it to true when testing outside of Game Thread. Results in false if WeakObjPtr point to an existing object (no flags checked) 
	 * @return true if this used to point at a real object but no longer does.
	**/
	FORCEINLINE bool IsStale(bool bIncludingIfPendingKill = false, bool bThreadsafeTest = false) const
	{
		return TWeakObjectPtrBase::IsStale(bIncludingIfPendingKill, bThreadsafeTest);
	}

	FORCEINLINE bool HasSameIndexAndSerialNumber(const TWeakObjectPtr& Other) const
	{
		return static_cast<const TWeakObjectPtrBase&>(*this).HasSameIndexAndSerialNumber(static_cast<const TWeakObjectPtrBase&>(Other));
	}

	/** Hash function. */
	FORCEINLINE friend uint32 GetTypeHash(const TWeakObjectPtr& WeakObjectPtr)
	{
		return GetTypeHash(static_cast<const TWeakObjectPtrBase&>(WeakObjectPtr));
	}

	friend FArchive& operator<<( FArchive& Ar, TWeakObjectPtr& WeakObjectPtr )
	{
		Ar << static_cast<TWeakObjectPtrBase&>(WeakObjectPtr);
		return Ar;
	}
};

template <typename LhsT, typename RhsT, typename OtherTWeakObjectPtrBase>
FORCENOINLINE bool operator==(const TWeakObjectPtr<LhsT, OtherTWeakObjectPtrBase>& Lhs, const TWeakObjectPtr<RhsT, OtherTWeakObjectPtrBase>& Rhs)
{
	// It's also possible that this static_assert may fail for valid conversions because
	// one or both of the types have only been forward-declared.
	static_assert(TPointerIsConvertibleFromTo<LhsT, RhsT>::Value || TPointerIsConvertibleFromTo<RhsT, LhsT>::Value, "Unable to compare TWeakObjectPtrs - types are incompatible");

	return (const OtherTWeakObjectPtrBase&)Lhs == (const OtherTWeakObjectPtrBase&)Rhs;
}

template <typename LhsT, typename RhsT, typename OtherTWeakObjectPtrBase>
FORCENOINLINE bool operator==(const TWeakObjectPtr<LhsT, OtherTWeakObjectPtrBase>& Lhs, const RhsT* Rhs)
{
	// It's also possible that these static_asserts may fail for valid conversions because
	// one or both of the types have only been forward-declared.
	static_assert(TPointerIsConvertibleFromTo<RhsT, UObject>::Value, "TWeakObjectPtr can only be compared with UObject types");
	static_assert(TPointerIsConvertibleFromTo<LhsT, RhsT>::Value || TPointerIsConvertibleFromTo<RhsT, LhsT>::Value, "Unable to compare TWeakObjectPtr with raw pointer - types are incompatible");

	// NOTE: this constructs a TWeakObjectPtrBase, which has some amount of overhead, so this may not be an efficient operation
	return (const OtherTWeakObjectPtrBase&)Lhs == OtherTWeakObjectPtrBase(Rhs);
}

template <typename LhsT, typename RhsT, typename OtherTWeakObjectPtrBase>
FORCENOINLINE bool operator==(const LhsT* Lhs, const TWeakObjectPtr<RhsT, OtherTWeakObjectPtrBase>& Rhs)
{
	// It's also possible that these static_asserts may fail for valid conversions because
	// one or both of the types have only been forward-declared.
	static_assert(TPointerIsConvertibleFromTo<LhsT, UObject>::Value, "TWeakObjectPtr can only be compared with UObject types");
	static_assert(TPointerIsConvertibleFromTo<LhsT, RhsT>::Value || TPointerIsConvertibleFromTo<RhsT, LhsT>::Value, "Unable to compare TWeakObjectPtr with raw pointer - types are incompatible");

	// NOTE: this constructs a TWeakObjectPtrBase, which has some amount of overhead, so this may not be an efficient operation
	return OtherTWeakObjectPtrBase(Lhs) == (const OtherTWeakObjectPtrBase&)Rhs;
}

template <typename LhsT, typename OtherTWeakObjectPtrBase>
FORCENOINLINE bool operator==(const TWeakObjectPtr<LhsT, OtherTWeakObjectPtrBase>& Lhs, TYPE_OF_NULLPTR)
{
	return !Lhs.IsValid();
}

template <typename RhsT, typename OtherTWeakObjectPtrBase>
FORCENOINLINE bool operator==(TYPE_OF_NULLPTR, const TWeakObjectPtr<RhsT, OtherTWeakObjectPtrBase>& Rhs)
{
	return !Rhs.IsValid();
}

template <typename LhsT, typename RhsT, typename OtherTWeakObjectPtrBase>
FORCENOINLINE bool operator!=(const TWeakObjectPtr<LhsT, OtherTWeakObjectPtrBase>& Lhs, const TWeakObjectPtr<RhsT, OtherTWeakObjectPtrBase>& Rhs)
{
	// It's also possible that this static_assert may fail for valid conversions because
	// one or both of the types have only been forward-declared.
	static_assert(TPointerIsConvertibleFromTo<LhsT, RhsT>::Value || TPointerIsConvertibleFromTo<RhsT, LhsT>::Value, "Unable to compare TWeakObjectPtrs - types are incompatible");

	return (const OtherTWeakObjectPtrBase&)Lhs != (const OtherTWeakObjectPtrBase&)Rhs;
}

template <typename LhsT, typename RhsT, typename OtherTWeakObjectPtrBase>
FORCENOINLINE bool operator!=(const TWeakObjectPtr<LhsT, OtherTWeakObjectPtrBase>& Lhs, const RhsT* Rhs)
{
	// It's also possible that these static_asserts may fail for valid conversions because
	// one or both of the types have only been forward-declared.
	static_assert(TPointerIsConvertibleFromTo<RhsT, UObject>::Value, "TWeakObjectPtr can only be compared with UObject types");
	static_assert(TPointerIsConvertibleFromTo<LhsT, RhsT>::Value || TPointerIsConvertibleFromTo<RhsT, LhsT>::Value, "Unable to compare TWeakObjectPtr with raw pointer - types are incompatible");

	// NOTE: this constructs a TWeakObjectPtrBase, which has some amount of overhead, so this may not be an efficient operation
	return (const OtherTWeakObjectPtrBase&)Lhs != OtherTWeakObjectPtrBase(Rhs);
}

template <typename LhsT, typename RhsT, typename OtherTWeakObjectPtrBase>
FORCENOINLINE bool operator!=(const LhsT* Lhs, const TWeakObjectPtr<RhsT, OtherTWeakObjectPtrBase>& Rhs)
{
	// It's also possible that these static_asserts may fail for valid conversions because
	// one or both of the types have only been forward-declared.
	static_assert(TPointerIsConvertibleFromTo<LhsT, UObject>::Value, "TWeakObjectPtr can only be compared with UObject types");
	static_assert(TPointerIsConvertibleFromTo<LhsT, RhsT>::Value || TPointerIsConvertibleFromTo<RhsT, LhsT>::Value, "Unable to compare TWeakObjectPtr with raw pointer - types are incompatible");

	// NOTE: this constructs a TWeakObjectPtrBase, which has some amount of overhead, so this may not be an efficient operation
	return OtherTWeakObjectPtrBase(Lhs) != (const OtherTWeakObjectPtrBase&)Rhs;
}

template <typename LhsT, typename OtherTWeakObjectPtrBase>
FORCENOINLINE bool operator!=(const TWeakObjectPtr<LhsT, OtherTWeakObjectPtrBase>& Lhs, TYPE_OF_NULLPTR)
{
	return Lhs.IsValid();
}

template <typename RhsT, typename OtherTWeakObjectPtrBase>
FORCENOINLINE bool operator!=(TYPE_OF_NULLPTR, const TWeakObjectPtr<RhsT, OtherTWeakObjectPtrBase>& Rhs)
{
	return Rhs.IsValid();
}


template<class T> struct TIsPODType<TWeakObjectPtr<T> > { enum { Value = true }; };
template<class T> struct TIsZeroConstructType<TWeakObjectPtr<T> > { enum { Value = true }; };
template<class T> struct TIsWeakPointerType<TWeakObjectPtr<T> > { enum { Value = true }; };


/**
 * MapKeyFuncs for TWeakObjectPtrs which allow the key to become stale without invalidating the map.
 */
template <typename KeyType, typename ValueType, bool bInAllowDuplicateKeys = false>
struct TWeakObjectPtrMapKeyFuncs : public TDefaultMapKeyFuncs<KeyType, ValueType, bInAllowDuplicateKeys>
{
	typedef typename TDefaultMapKeyFuncs<KeyType, ValueType, bInAllowDuplicateKeys>::KeyInitType KeyInitType;

	static FORCEINLINE bool Matches(KeyInitType A, KeyInitType B)
	{
		return A.HasSameIndexAndSerialNumber(B);
	}

	static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
	{
		return GetTypeHash(Key);
	}
};

/**
 * Automatic version of the weak object pointer
 */
template<class T> 
class TAutoWeakObjectPtr : public TWeakObjectPtr<T>
{
public:
	/** NULL constructor **/
	DEPRECATED(4.15, "TAutoWeakObjectPtr has been deprecated - use TWeakObjectPtr instead")
	FORCEINLINE TAutoWeakObjectPtr()
	{
	}
	/** Construct from a raw pointer **/
	DEPRECATED(4.15, "TAutoWeakObjectPtr has been deprecated - use TWeakObjectPtr instead")
	FORCEINLINE TAutoWeakObjectPtr(const T* Target)
		: TWeakObjectPtr<T>(Target)
	{
	}
	/**  Construct from the base type **/
	DEPRECATED(4.15, "TAutoWeakObjectPtr has been deprecated - use TWeakObjectPtr instead")
	FORCEINLINE TAutoWeakObjectPtr(const TWeakObjectPtr<T>& Other) 
		: TWeakObjectPtr<T>(Other)
	{
	}
	DEPRECATED(4.15, "Implicit conversion from TAutoWeakObjectPtr to the pointer type has been deprecated - use Get() instead")
	FORCEINLINE operator T* () const
	{
		return this->Get();
	}
	DEPRECATED(4.15, "Implicit conversion from TAutoWeakObjectPtr to the pointer type has been deprecated - use Get() instead")
	FORCEINLINE operator const T* () const
	{
		return (const T*)this->Get();
	}

	DEPRECATED(4.15, "Implicit conversion from TAutoWeakObjectPtr to the pointer type has been deprecated - use Get() instead")
	FORCEINLINE explicit operator bool() const
	{
		return this->Get() != nullptr;
	}
};

template<class T> struct TIsPODType<TAutoWeakObjectPtr<T> > { enum { Value = true }; };
template<class T> struct TIsZeroConstructType<TAutoWeakObjectPtr<T> > { enum { Value = true }; };
template<class T> struct TIsWeakPointerType<TAutoWeakObjectPtr<T> > { enum { Value = true }; };

template<typename DestArrayType, typename SourceArrayType>
void CopyFromWeakArray(DestArrayType& Dest, const SourceArrayType& Src)
{
	Dest.Empty(Src.Num());
	for (int32 Index = 0; Index < Src.Num(); Index++)
	{
		if (auto Value = Src[Index].Get())
		{
			Dest.Add(Value);
		}
	}
}