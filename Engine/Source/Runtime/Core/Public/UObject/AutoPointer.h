// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/IsPODType.h"
#include "Templates/UnrealTemplate.h"

// TAutoPointer wraps a smart-pointer and adds an implicit conversion to raw pointer
// Its main use is for converting a variable from raw pointer to a smart pointer without breaking existing code
// Not the same thing as TAutoPtr :(
template<class T, class TBASE>
class TAutoPointer : public TBASE
{
public:
	/** NULL constructor **/
	DEPRECATED(4.15, "TAutoPointer has been deprecated - please remove its usage from your project") 
	FORCEINLINE TAutoPointer()
	{
	}
	/** Construct from a single argument (I'd use inheriting constructors if all our compilers supported it) **/
	template<typename X>
	DEPRECATED(4.15, "TAutoPointer has been deprecated - please remove its usage from your project") 
	FORCEINLINE TAutoPointer(X&& Target)
		: TBASE(Forward<X>(Target))
	{
	}

	DEPRECATED(4.15, "TAutoPointer has been deprecated - please remove its usage from your project") 
	FORCEINLINE operator T* () const
	{
		return TBASE::Get();
	}

	DEPRECATED(4.15, "TAutoPointer has been deprecated - please remove its usage from your project") 
	FORCEINLINE operator const T* () const
	{
		return (const T*)TBASE::Get();
	}

	DEPRECATED(4.15, "TAutoPointer has been deprecated - please remove its usage from your project") 
	FORCEINLINE explicit operator bool() const
	{
		return TBASE::Get() != nullptr;
	}
};


template<class T, class TBASE> struct TIsPODType<TAutoPointer<T, TBASE> > { enum { Value = TIsPODType<TBASE>::Value }; }; // pod-ness is the same as the podness of the base pointer type
