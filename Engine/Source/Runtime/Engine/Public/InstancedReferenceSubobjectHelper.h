// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UnrealType.h"

/** 
 * Meant to represent a specific object property that is setup to reference a 
 * instanced sub-object. Tracks the property hierarchy used to reach the 
 * property, so that we can easily retrieve instanced sub-objects from a 
 * container object.
 */
struct FInstancedPropertyPath
{
private:
	struct FPropertyLink
	{
		FPropertyLink(const UProperty* Property, int32 ArrayIndexIn = INDEX_NONE)
			: PropertyPtr(Property), ArrayIndex(ArrayIndexIn)
		{}

		const UProperty* PropertyPtr;
		int32            ArrayIndex;
	};

public:
	//--------------------------------------------------------------------------
	FInstancedPropertyPath(UProperty* RootProperty)
	{
		PropertyChain.Add(FPropertyLink(RootProperty));
	}

	//--------------------------------------------------------------------------
	void Push(const UProperty* Property, int32 ArrayIndex = INDEX_NONE)
	{
		PropertyChain.Add(FPropertyLink(Property, ArrayIndex));		
	}

	//--------------------------------------------------------------------------
	void Pop()
	{
 		PropertyChain.RemoveAt(PropertyChain.Num() - 1);
	}

	//--------------------------------------------------------------------------
	const UProperty* Head() const
	{
		return PropertyChain.Last().PropertyPtr;
	}

	//--------------------------------------------------------------------------
	ENGINE_API UObject* Resolve(const UObject* Container) const;

private:
	TArray<FPropertyLink> PropertyChain;
};

/** 
 * Can be used as a raw sub-object pointer, but also contains a 
 * FInstancedPropertyPath to identify the property that this sub-object is 
 * referenced by. Paired together for ease of use (so API users don't have to manage a map).
 */
struct FInstancedSubObjRef
{
	FInstancedSubObjRef(UObject* SubObj, const FInstancedPropertyPath& PropertyPathIn)
		: SubObjInstance(SubObj)
		, PropertyPath(PropertyPathIn)
	{}

	//--------------------------------------------------------------------------
	operator UObject*() const
	{
		return SubObjInstance;
	}

	//--------------------------------------------------------------------------
	UObject* operator->() const
	{
		return SubObjInstance;
	}

	//--------------------------------------------------------------------------
	friend uint32 GetTypeHash(const FInstancedSubObjRef& SubObjRef)
	{
		return GetTypeHash((UObject*)SubObjRef);
	}

	UObject* SubObjInstance;
	FInstancedPropertyPath PropertyPath;
};
 
/** 
 * Contains a set of utility functions useful for searching out and identifying
 * instanced sub-objects contained within a specific outer object.
 */
class ENGINE_API FFindInstancedReferenceSubobjectHelper
{
public:
	template<typename T>
	static void GetInstancedSubObjects(const UObject* Container, T& OutObjects)
	{
		const UClass* ContainerClass = Container->GetClass();
		for (UProperty* Prop = ContainerClass->RefLink; Prop; Prop = Prop->NextRef)
		{
			FInstancedPropertyPath RootPropertyPath(Prop);
			GetInstancedSubObjects_Inner(RootPropertyPath, reinterpret_cast<const uint8*>(Container), [&OutObjects](const FInstancedSubObjRef& Ref){ OutObjects.Add(Ref); });
		}
	}

	static void Duplicate(UObject* OldObject, UObject* NewObject, TMap<UObject*, UObject*>& ReferenceReplacementMap, TArray<UObject*>& DuplicatedObjects);

private:
	static void GetInstancedSubObjects_Inner(FInstancedPropertyPath& PropertyPath, const uint8* ContainerAddress, TFunctionRef<void(const FInstancedSubObjRef& Ref)> OutObjects);
};
