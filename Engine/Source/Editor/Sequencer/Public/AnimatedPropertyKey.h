// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealType.h"
#include "SubclassOf.h"

struct FAnimatedPropertyKey
{
	/**
	 * The name of the type of property that can be animated (i.e. UBoolProperty)
	 */
	FName PropertyTypeName;

	/**
	 * The name of the type of object that can be animated inside the property (i.e. the name of the struct or object for UStructProperty or UObjectProperty). NAME_None for any properties.
	 */
	FName ObjectTypeName;

	friend uint32 GetTypeHash(FAnimatedPropertyKey InKey)
	{
		return GetTypeHash(InKey.PropertyTypeName) ^ GetTypeHash(InKey.ObjectTypeName);
	}

	friend bool operator==(FAnimatedPropertyKey A, FAnimatedPropertyKey B)
	{
		return A.PropertyTypeName == B.PropertyTypeName && A.ObjectTypeName == B.ObjectTypeName;
	}

	static FAnimatedPropertyKey FromProperty(const UProperty* Property)
	{
		FAnimatedPropertyKey Definition;
		Definition.PropertyTypeName = Property->GetClass()->GetFName();

		if (const UStructProperty* StructProperty = Cast<const UStructProperty>(Property))
		{
			Definition.ObjectTypeName = StructProperty->Struct->GetFName();
		}
		else if (const UObjectPropertyBase* ObjectProperty = Cast<const UObjectPropertyBase>(Property))
		{
			if (ObjectProperty->PropertyClass)
			{
				Definition.ObjectTypeName = ObjectProperty->PropertyClass->GetFName();
			}
		}
		else if(const UArrayProperty* ArrayProperty = Cast<const UArrayProperty>(Property))
		{
			Definition.PropertyTypeName = ArrayProperty->Inner->GetClass()->GetFName();
			if (const UStructProperty* InnerStructProperty = Cast<const UStructProperty>(ArrayProperty->Inner))
			{
				Definition.ObjectTypeName = InnerStructProperty->Struct->GetFName();
			}
		}
		return Definition;
	}

	static FAnimatedPropertyKey FromObjectType(const UClass* Class)
	{
		FAnimatedPropertyKey Definition;
		Definition.PropertyTypeName = NAME_ObjectProperty;
		Definition.ObjectTypeName = Class->GetFName();
		return Definition;
	}

	static FAnimatedPropertyKey FromStructType(const UStruct* Struct)
	{
		check(Struct);
		return FromStructType(Struct->GetFName());
	}

	static FAnimatedPropertyKey FromStructType(FName StructName)
	{
		FAnimatedPropertyKey Definition;
		Definition.PropertyTypeName = NAME_StructProperty;
		Definition.ObjectTypeName = StructName;
		return Definition;
	}

	static FAnimatedPropertyKey FromPropertyTypeName(FName PropertyTypeName)
	{
		FAnimatedPropertyKey Definition;
		Definition.PropertyTypeName = PropertyTypeName;
		return Definition;
	}

	static FAnimatedPropertyKey FromPropertyType(TSubclassOf<UProperty> PropertyType)
	{
		FAnimatedPropertyKey Definition;
		Definition.PropertyTypeName = PropertyType->GetFName();
		return Definition;
	}

private:

	FAnimatedPropertyKey()
		: PropertyTypeName(NAME_None)
		, ObjectTypeName(NAME_None)
	{}
};