// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Templates/UnrealTypeTraits.h"
#include "Serialization/Archive.h"

/**
 * Wrapper around a raw pointer that destroys it automatically.
 * Calls operator delete on the object, so it must have been allocated with
 * operator new. Modeled after boost::scoped_ptr.
 * 
 * If a custom deallocator is needed, this class will have to
 * expanded with a deletion policy.
 */
template<typename ReferencedType> 
class TScopedPointer
{
private:

	ReferencedType* Reference;
	typedef TScopedPointer<ReferencedType> SelfType;

public:

	/** Initialization constructor. */
	DEPRECATED(4.15, "TScopedPointer has been deprecated and should be replaced with TUniquePtr.")
	explicit TScopedPointer(ReferencedType* InReference = nullptr)
		: Reference(InReference)
	{ }

	/** Copy constructor. */
	TScopedPointer(const TScopedPointer& InCopy)
	{
		Reference = InCopy.Reference ?
			new ReferencedType(*InCopy.Reference) :
			nullptr;
	}

	/** Destructor. */
	~TScopedPointer()
	{
		delete Reference;
		Reference = nullptr;
	}

	/** Assignment operator. */
	TScopedPointer& operator=(const TScopedPointer& InCopy)
	{
		if(&InCopy != this)
		{
			delete Reference;
			Reference = InCopy.Reference ?
				new ReferencedType(*InCopy.Reference) :
				nullptr;
		}
		return *this;
	}

	/** Assignment operator. */
	TScopedPointer& operator=(ReferencedType* InReference)
	{
		Reset(InReference);
		return *this;
	}

	// Dereferencing operators.
	ReferencedType& operator*() const
	{
		check(Reference != 0);
		return *Reference;
	}

	ReferencedType* operator->() const
	{
		check(Reference != 0);
		return Reference;
	}

	ReferencedType* GetOwnedPointer() const
	{
		return Reference;
	}

	/** Returns true if the pointer is valid */
	bool IsValid() const
	{
		return ( Reference != 0 );
	}

	// implicit conversion to the reference type.
	operator ReferencedType*() const
	{
		return Reference;
	}

	void Swap(SelfType& b)
	{
		ReferencedType* Tmp = b.Reference;
		b.Reference = Reference;
		Reference = Tmp;
	}

	/** Deletes the current pointer and sets it to a new value. */
	void Reset(ReferencedType* NewReference = nullptr)
	{
		check(!Reference || Reference != NewReference);
		delete Reference;
		Reference = NewReference;
	}

	/** Releases the owned pointer and returns it so it doesn't get deleted. */
	ReferencedType* Release()
	{
		ReferencedType* Result = GetOwnedPointer();
		Reference = nullptr;
		return Result;
	}

	// Serializer.
	friend FArchive& operator<<(FArchive& Ar,SelfType& P)
	{
		if(Ar.IsLoading())
		{
			// When loading, allocate a new value.
			ReferencedType* OldReference = P.Reference;
			P.Reference = new ReferencedType;

			// Delete the old value.
			delete OldReference;
		}

		// Serialize the value.  The caller of this serializer is responsible to only serialize for saving non-nullptr pointers. */
		check(P.Reference);
		Ar << *P.Reference;

		return Ar;
	}
};


/** specialize container traits */
template<typename ReferencedType>
struct TTypeTraits<TScopedPointer<ReferencedType> > : public TTypeTraitsBase<TScopedPointer<ReferencedType> >
{
	typedef ReferencedType* ConstInitType;
	typedef ReferencedType* ConstPointerType;
};


/** Implement movement of a scoped pointer to avoid copying the referenced value. */
template<typename ReferencedType> void Move(TScopedPointer<ReferencedType>& A,ReferencedType* B)
{
	A.Reset(B);
}


/**
 * Wrapper around a raw pointer that destroys it automatically.
 * Calls operator delete on the object, so it must have been allocated with
 * operator new.
 * 
 * Same as TScopedPointer, except never calls new to make a duplicate
 */
template<typename ReferencedType> 
class TAutoPtr
{
private:

	ReferencedType* Reference;
	typedef TAutoPtr<ReferencedType> SelfType;

public:

	/** Initialization constructor. */
	DEPRECATED(4.15, "TAutoPtr has been deprecated and should be replaced with TUniquePtr.")
	explicit TAutoPtr(ReferencedType* InReference = nullptr)
		:	Reference(InReference)
	{}

	/** Destructor. */
	~TAutoPtr()
	{
		delete Reference;
		Reference = nullptr;
	}

	/** Assignment operator. */
	TAutoPtr& operator=(ReferencedType* InReference)
	{
		Reset(InReference);
		return *this;
	}
	// Dereferencing operators.
	ReferencedType& operator*() const
	{
		check(Reference != 0);
		return *Reference;
	}

	ReferencedType* operator->() const
	{
		check(Reference != 0);
		return Reference;
	}

	ReferencedType* GetOwnedPointer() const
	{
		return Reference;
	}

	/** Returns true if the pointer is valid */
	bool IsValid() const
	{
		return ( Reference != 0 );
	}

	// implicit conversion to the reference type.
	operator ReferencedType*() const
	{
		return Reference;
	}

	void Swap(SelfType& b)
	{
		ReferencedType* Tmp = b.Reference;
		b.Reference = Reference;
		Reference = Tmp;
	}

	/** Deletes the current pointer and sets it to a new value. */
	void Reset(ReferencedType* NewReference = nullptr)
	{
		check(!Reference || Reference != NewReference);
		delete Reference;
		Reference = NewReference;
	}

};


/** specialize container traits */
template<typename ReferencedType>
struct TTypeTraits<TAutoPtr<ReferencedType> > : public TTypeTraitsBase<TAutoPtr<ReferencedType> >
{
	typedef ReferencedType* ConstInitType;
	typedef ReferencedType* ConstPointerType;
};


/** Implement movement of a scoped pointer to avoid copying the referenced value. */
template<typename ReferencedType> void Move(TAutoPtr<ReferencedType>& A,ReferencedType* B)
{
	A.Reset(B);
}
