// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NameTypes.h"
#include "Containers/Map.h"
#include "Class.h"

class FClass;
class FClasses;
struct FPropertySpecifier;

/** Structure that holds class meta data generated from its UCLASS declaration */
class FClassDeclarationMetaData
{
public:
	FClassDeclarationMetaData();

	EClassFlags ClassFlags;
	TMap<FName, FString> MetaData;
	FString ClassWithin;
	FString ConfigName;
	
	TArray<FString> HideCategories;
	TArray<FString> ShowSubCatgories;
	TArray<FString> HideFunctions;
	TArray<FString> AutoExpandCategories;
	TArray<FString> AutoCollapseCategories;
	TArray<FString> DependsOn;
	TArray<FString> ClassGroupNames;
	
	/**
	* Parse Class's properties to generate its declaration data.
	*
	* @param	InClassSpecifiers Class properties collected from its UCLASS macro
	* @param	InRequiredAPIMacroIfPresent *_API macro if present (empty otherwise)
	* @param	OutClassData Parsed class meta data
	*/
	void ParseClassProperties(const TArray<FPropertySpecifier>& InClassSpecifiers, const FString& InRequiredAPIMacroIfPresent);

	/**
	* Merges all category properties with the class which at this point only has its parent propagated categories
	*
	* @param	Class Class to merge categories for
	*/
	void MergeClassCategories(FClass* Class);

	/**
	* Merges all class flags and validates them
	*
	* @param	DeclaredClassName Name this class was declared with (for validation)
	* @param	PreviousClassFlags Class flags before resetting the class (for validation)
	* @param	Class Class to merge flags for
	* @param  AllClasses All known classes
	*/
	void MergeAndValidateClassFlags(const FString& DeclaredClassName, uint32 PreviousClassFlags, FClass* Class, const FClasses& AllClasses);
private:

	/** Merges all 'show' categories */
	void MergeShowCategories();
	/** Sets and validates 'within' property */
	void SetAndValidateWithinClass(FClass* Class, const FClasses& AllClasses);
	/** Sets and validates 'ConfigName' property */
	void SetAndValidateConfigName(FClass* Class);

	TArray<FString> ShowCategories;
	TArray<FString> ShowFunctions;
	TArray<FString> DontAutoCollapseCategories;
	bool WantsToBePlaceable;
};
