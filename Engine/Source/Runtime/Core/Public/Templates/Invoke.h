// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/PointerIsConvertibleFromTo.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/Decay.h"


namespace UE4Invoke_Private
{
	template <typename BaseType, typename CallableType>
	FORCEINLINE auto DereferenceIfNecessary(CallableType&& Callable)
		-> typename TEnableIf<TPointerIsConvertibleFromTo<typename TDecay<CallableType>::Type, typename TDecay<BaseType>::Type>::Value, decltype((CallableType&&)Callable)>::Type
	{
		return (CallableType&&)Callable;
	}

	template <typename BaseType, typename CallableType>
	FORCEINLINE auto DereferenceIfNecessary(CallableType&& Callable)
		-> typename TEnableIf<!TPointerIsConvertibleFromTo<typename TDecay<CallableType>::Type, typename TDecay<BaseType>::Type>::Value, decltype(*(CallableType&&)Callable)>::Type
	{
		return *(CallableType&&)Callable;
	}
}


/**
 * Invokes a callable with a set of arguments.  Allows the following:
 *
 * - Calling a functor object given a set of arguments.
 * - Calling a function pointer given a set of arguments.
 * - Calling a member function given a reference to an object and a set of arguments.
 * - Calling a member function given a pointer (including smart pointers) to an object and a set of arguments.
 * - Projecting via a data member pointer given a reference to an object.
 * - Projecting via a data member pointer given a pointer (including smart pointers) to an object.
 *
 * See: http://en.cppreference.com/w/cpp/utility/functional/invoke
 */
template <typename FuncType, typename... ArgTypes>
FORCEINLINE auto Invoke(FuncType&& Func, ArgTypes&&... Args)
	-> decltype(Forward<FuncType>(Func)(Forward<ArgTypes>(Args)...))
{
	return Forward<FuncType>(Func)(Forward<ArgTypes>(Args)...);
}

template <typename ReturnType, typename ObjType, typename CallableType>
FORCEINLINE auto Invoke(ReturnType ObjType::*pdm, CallableType&& Callable)
	-> decltype(UE4Invoke_Private::DereferenceIfNecessary<ObjType>(Forward<CallableType>(Callable)).*pdm)
{
	return UE4Invoke_Private::DereferenceIfNecessary<ObjType>(Forward<CallableType>(Callable)).*pdm;
}

template <typename ReturnType, typename ObjType, typename... PMFArgTypes, typename CallableType, typename... ArgTypes>
FORCEINLINE auto Invoke(ReturnType (ObjType::*PtrMemFun)(PMFArgTypes...), CallableType&& Callable, ArgTypes&&... Args)
	-> decltype((UE4Invoke_Private::DereferenceIfNecessary<ObjType>(Forward<CallableType>(Callable)).*PtrMemFun)(Forward<ArgTypes>(Args)...))
{
	return (UE4Invoke_Private::DereferenceIfNecessary<ObjType>(Forward<CallableType>(Callable)).*PtrMemFun)(Forward<ArgTypes>(Args)...);
}

template <typename ReturnType, typename ObjType, typename... PMFArgTypes, typename CallableType, typename... ArgTypes>
FORCEINLINE auto Invoke(ReturnType (ObjType::*PtrMemFun)(PMFArgTypes...) const, CallableType&& Callable, ArgTypes&&... Args)
	-> decltype((UE4Invoke_Private::DereferenceIfNecessary<ObjType>(Forward<CallableType>(Callable)).*PtrMemFun)(Forward<ArgTypes>(Args)...))
{
	return (UE4Invoke_Private::DereferenceIfNecessary<ObjType>(Forward<CallableType>(Callable)).*PtrMemFun)(Forward<ArgTypes>(Args)...);
}
