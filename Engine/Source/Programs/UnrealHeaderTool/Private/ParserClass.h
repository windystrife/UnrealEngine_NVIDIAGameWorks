// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"

class UField;

struct EEnforceInterfacePrefix
{
	enum Type
	{
		None,
		I,
		U
	};
};

class FClasses;

class FClass : public UClass
{
public:
	FClass();

	/** 
	 * Returns the name of the given class with a valid prefix.
	 *
	 * @param InClass Class used to create a valid class name with prefix
	 */
	FString GetNameWithPrefix(EEnforceInterfacePrefix::Type EnforceInterfacePrefix = EEnforceInterfacePrefix::None) const;

	/**
	 * Returns the super class of this class, or NULL if there is no superclass.
	 *
	 * @return The super class of this class.
	 */
	FClass* GetSuperClass() const;

	/**
	 * Returns the 'within' class of this class.
	 *
	 * @return The 'within' class of this class.
	 */
	FClass* GetClassWithin() const;

	TArray<FClass*> GetInterfaceTypes() const;

	void GetHideCategories(TArray<FString>& OutHideCategories) const;
	void GetShowCategories(TArray<FString>& OutShowCategories) const;

	/** Helper function that checks if the field is a dynamic type (can be constructed post-startup) */
	static bool IsDynamic(const UField* Field);

	/** Helper function that checks if the field is belongs to a dynamic type */
	static bool IsOwnedByDynamicType(const UField* Field);

	/** Helper function to get the source replaced package name */
	static FString GetTypePackageName(const UField* Field);
};
