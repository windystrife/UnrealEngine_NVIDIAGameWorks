// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/AssertionMacros.h"
#include "Templates/TypeCompatibleBytes.h"
#include "Templates/Tuple.h"

class FDelegateBase;
class IDelegateInstance;
template <typename FuncType> struct IBaseDelegateInstance;

template <typename FuncType>
struct IBaseDelegateInstance;

template <typename FuncType>
struct IBaseDelegateInstanceCommon;

template <typename RetType, typename... Args>
struct IBaseDelegateInstanceCommon<RetType(Args...)> : public IDelegateInstance
{
	/**
	 * Emplaces a copy of the delegate instance into the FDelegateBase.
	 */
	virtual void CreateCopy(FDelegateBase& Base) = 0;

	/**
	 * Returns true if this delegate points to exactly the same object and method as the specified delegate,
	 * even if the delegate objects themselves are different.  Also, the delegate types *must* be compatible.
	 *
	 * @param  InOtherDelegate
	 * @return  True if delegates match
	 */
	virtual bool IsSameFunction( const IBaseDelegateInstance<RetType(Args...)>& InOtherDelegate ) const = 0;

	/**
	 * Execute the delegate.  If the function pointer is not valid, an error will occur.
	 */
	virtual RetType Execute(Args...) const = 0;
};

template <typename FuncType>
struct IBaseDelegateInstance : public IBaseDelegateInstanceCommon<FuncType>
{
};

template <typename... Args>
struct IBaseDelegateInstance<void(Args...)> : public IBaseDelegateInstanceCommon<void(Args...)>
{
	/**
	 * Execute the delegate, but only if the function pointer is still valid
	 *
	 * @return  Returns true if the function was executed
	 */
	// NOTE: Currently only delegates with no return value support ExecuteIfSafe()
	virtual bool ExecuteIfSafe(Args...) const = 0;
};

template <bool Const, typename Class, typename FuncType>
struct TMemFunPtrType;

template <typename Class, typename RetType, typename... Args>
struct TMemFunPtrType<false, Class, RetType(Args...)>
{
	typedef RetType (Class::* Type)(Args...);
};

template <typename Class, typename RetType, typename... Args>
struct TMemFunPtrType<true, Class, RetType(Args...)>
{
	typedef RetType (Class::* Type)(Args...) const;
};

template <typename FuncType>
struct TPayload;

template <typename Ret, typename... Types>
struct TPayload<Ret(Types...)>
{
	TTuple<Types..., Ret> Values;

	template <typename... ArgTypes>
	explicit TPayload(ArgTypes&&... Args)
		: Values(Forward<ArgTypes>(Args)..., Ret())
	{
	}

	Ret& GetResult()
	{
		return Values.template Get<sizeof...(Types)>();
	}
};

template <typename... Types>
struct TPayload<void(Types...)>
{
	TTuple<Types...> Values;

	template <typename... ArgTypes>
	explicit TPayload(ArgTypes&&... Args)
		: Values(Forward<ArgTypes>(Args)...)
	{
	}

	void GetResult()
	{
	}
};

template <typename T>
struct TPlacementNewer
{
	TPlacementNewer()
		: bConstructed(false)
	{
	}

	~TPlacementNewer()
	{
		if (bConstructed)
		{
			// We need a typedef here because VC won't compile the destructor call below if T itself has a member called T
			typedef T TPlacementNewerTTypeTypedef;

			((TPlacementNewerTTypeTypedef*)&Bytes)->TPlacementNewerTTypeTypedef::~TPlacementNewerTTypeTypedef();
		}
	}

	template <typename... ArgTypes>
	T* operator()(ArgTypes&&... Args)
	{
		check(!bConstructed);
		T* Result = new (&Bytes) T(Forward<ArgTypes>(Args)...);
		bConstructed = true;
		return Result;
	}

	T* operator->()
	{
		check(bConstructed);
		return (T*)&Bytes;
	}

private:
	TTypeCompatibleBytes<T> Bytes;
	bool                    bConstructed;
};

template <typename T, typename MemFunPtrType>
class TMemberFunctionCaller
{
	T*            Obj;
	MemFunPtrType MemFunPtr;

public:
	TMemberFunctionCaller(T* InObj, MemFunPtrType InMemFunPtr)
		: Obj      (InObj)
		, MemFunPtr(InMemFunPtr)
	{
	}

	template <typename... ArgTypes>
	auto operator()(ArgTypes&&... Args) -> decltype((Obj->*MemFunPtr)(Forward<ArgTypes>(Args)...))
	{
		return (Obj->*MemFunPtr)(Forward<ArgTypes>(Args)...);
	}
};
