// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"

/**
 * Template to allow UClass's to be passed around with type safety 
 */
template<class TClass>
class TSubclassOf
{
	template <class TClassA>
	friend class TSubclassOf;

public:
	/** Default Constructor, defaults to null */
	FORCEINLINE TSubclassOf() :
		Class(nullptr)
	{
	}

	/** Constructor that takes a UClass and does a runtime check to make sure this is a compatible class */
	FORCEINLINE TSubclassOf(UClass* From) :
		Class(From)
	{
	}

	/** Copy Constructor, will only compile if types are compatible */
	template <class TClassA, class = typename TEnableIf<TPointerIsConvertibleFromTo<TClassA, TClass>::Value>::Type>
	FORCEINLINE TSubclassOf(const TSubclassOf<TClassA>& From) :
		Class(*From)
	{
	}

	/** Assignment operator, will only compile if types are compatible */
	template <class TClassA, class = typename TEnableIf<TPointerIsConvertibleFromTo<TClassA, TClass>::Value>::Type>
	FORCEINLINE TSubclassOf& operator=(const TSubclassOf<TClassA>& From)
	{
		Class = *From;
		return *this;
	}
	
	/** Assignment operator from UClass, the type is checked on get not on set */
	FORCEINLINE TSubclassOf& operator=(UClass* From)
	{
		Class = From;
		return *this;
	}
	
	/** Dereference back into a UClass, does runtime type checking */
	FORCEINLINE UClass* operator*() const
	{
		if (!Class || !Class->IsChildOf(TClass::StaticClass()))
		{
			return nullptr;
		}
		return Class;
	}
	
	/** Dereference back into a UClass */
	FORCEINLINE UClass* Get() const
	{
		return **this;
	}

	/** Dereference back into a UClass */
	FORCEINLINE UClass* operator->() const
	{
		return **this;
	}

	/** Implicit conversion to UClass */
	FORCEINLINE operator UClass* () const
	{
		return **this;
	}

	/**
	 * Get the CDO if we are referencing a valid class
	 *
	 * @return the CDO, or null if class is null
	 */
	FORCEINLINE TClass* GetDefaultObject() const
	{
		return Class ? Class->GetDefaultObject<TClass>() : nullptr;
	}

	friend FArchive& operator<<(FArchive& Ar, TSubclassOf& SubclassOf)
	{
		Ar << SubclassOf.Class;
		return Ar;
	}

	friend uint32 GetTypeHash(const TSubclassOf& SubclassOf)
	{
		return GetTypeHash(SubclassOf.Class);
	}

private:
	UClass* Class;
};
