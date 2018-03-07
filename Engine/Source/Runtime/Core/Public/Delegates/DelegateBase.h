// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Math/UnrealMathUtility.h"
#include "UObject/NameTypes.h"
#include "Delegates/DelegateSettings.h"
#include "Delegates/IDelegateInstance.h"

struct FWeakObjectPtr;

#if !defined(_WIN32) || defined(_WIN64)
	// Let delegates store up to 32 bytes which are 16-byte aligned before we heap allocate
	typedef TAlignedBytes<16, 16> AlignedInlineDelegateType;
	typedef TInlineAllocator<2> DelegateAllocatorType;
#else
	// ... except on Win32, because we can't pass 16-byte aligned types by value, as some delegates are
	// so we'll just keep it heap-allocated, which are always sufficiently aligned.
	typedef TAlignedBytes<16, 8> AlignedInlineDelegateType;
	typedef FHeapAllocator DelegateAllocatorType;
#endif


struct FWeakObjectPtr;

template <typename ObjectPtrType>
class FMulticastDelegateBase;

/**
 * Base class for unicast delegates.
 */
class FDelegateBase
{
	friend class FMulticastDelegateBase<FWeakObjectPtr>;

public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InDelegateInstance The delegate instance to assign.
	 */
	explicit FDelegateBase()
		: DelegateSize(0)
	{
	}

	~FDelegateBase()
	{
		Unbind();
	}

	/**
	 * Move constructor.
	 */
	FDelegateBase(FDelegateBase&& Other)
	{
		DelegateAllocator.MoveToEmpty(Other.DelegateAllocator);
		DelegateSize = Other.DelegateSize;
		Other.DelegateSize = 0;
	}

	/**
	 * Move assignment.
	 */
	FDelegateBase& operator=(FDelegateBase&& Other)
	{
		Unbind();
		DelegateAllocator.MoveToEmpty(Other.DelegateAllocator);
		DelegateSize = Other.DelegateSize;
		Other.DelegateSize = 0;
		return *this;
	}

#if USE_DELEGATE_TRYGETBOUNDFUNCTIONNAME

	/**
	 * Tries to return the name of a bound function.  Returns NAME_None if the delegate is unbound or
	 * a binding name is unavailable.
	 *
	 * Note: Only intended to be used to aid debugging of delegates.
	 *
	 * @return The name of the bound function, NAME_None if no name was available.
	 */
	FName TryGetBoundFunctionName() const
	{
		if (IDelegateInstance* Ptr = GetDelegateInstanceProtected())
		{
			return Ptr->TryGetBoundFunctionName();
		}

		return NAME_None;
	}

#endif

	/**
	 * If this is a UFunction or UObject delegate, return the UObject.
	 *
	 * @return The object associated with this delegate if there is one.
	 */
	inline class UObject* GetUObject( ) const
	{
		if (IDelegateInstance* Ptr = GetDelegateInstanceProtected())
		{
			return Ptr->GetUObject();
		}

		return nullptr;
	}

	/**
	 * Checks to see if the user object bound to this delegate is still valid.
	 *
	 * @return True if the user object is still valid and it's safe to execute the function call.
	 */
	inline bool IsBound( ) const
	{
		IDelegateInstance* Ptr = GetDelegateInstanceProtected();

		return Ptr && Ptr->IsSafeToExecute();
	}

	/** 
	 * Checks to see if this delegate is bound to the given user object.
	 *
	 * @return True if this delegate is bound to InUserObject, false otherwise.
	 */
	inline bool IsBoundToObject( void const* InUserObject ) const
	{
		if (!InUserObject)
		{
			return false;
		}

		IDelegateInstance* Ptr = GetDelegateInstanceProtected();

		return Ptr && Ptr->HasSameObject(InUserObject);
	}

	/**
	 * Unbinds this delegate
	 */
	inline void Unbind( )
	{
		if (IDelegateInstance* Ptr = GetDelegateInstanceProtected())
		{
			Ptr->~IDelegateInstance();
			DelegateAllocator.ResizeAllocation(0, 0, sizeof(AlignedInlineDelegateType));
			DelegateSize = 0;
		}
	}

	/**
	 * Gets the delegate instance.
	 *
	 * @return The delegate instance.
	 * @see SetDelegateInstance
	 */
	DEPRECATED(4.11, "GetDelegateInstance has been deprecated - calls to IDelegateInstance::GetUObject() and IDelegateInstance::GetHandle() should call the same functions on the delegate.  Other calls should be reconsidered.")
	FORCEINLINE IDelegateInstance* GetDelegateInstance( ) const
	{
		return GetDelegateInstanceProtected();
	}

	/**
	 * Gets a handle to the delegate.
	 *
	 * @return The delegate instance.
	 */
	inline FDelegateHandle GetHandle() const
	{
		FDelegateHandle Result;
		if (IDelegateInstance* Ptr = GetDelegateInstanceProtected())
		{
			Result = Ptr->GetHandle();
		}

		return Result;
	}

protected:
	/**
	 * Gets the delegate instance.  Not intended for use by user code.
	 *
	 * @return The delegate instance.
	 * @see SetDelegateInstance
	 */
	FORCEINLINE IDelegateInstance* GetDelegateInstanceProtected( ) const
	{
		return DelegateSize ? (IDelegateInstance*)DelegateAllocator.GetAllocation() : nullptr;
	}

private:
	friend void* operator new(size_t Size, FDelegateBase& Base);

	void* Allocate(int32 Size)
	{
		if (IDelegateInstance* CurrentInstance = GetDelegateInstanceProtected())
		{
			CurrentInstance->~IDelegateInstance();
		}

		int32 NewDelegateSize = FMath::DivideAndRoundUp(Size, (int32)sizeof(AlignedInlineDelegateType));
		if (DelegateSize != NewDelegateSize)
		{
			DelegateAllocator.ResizeAllocation(0, NewDelegateSize, sizeof(AlignedInlineDelegateType));
			DelegateSize = NewDelegateSize;
		}

		return DelegateAllocator.GetAllocation();
	}

private:
	DelegateAllocatorType::ForElementType<AlignedInlineDelegateType> DelegateAllocator;
	int32 DelegateSize;
};

inline void* operator new(size_t Size, FDelegateBase& Base)
{
	return Base.Allocate(Size);
}
