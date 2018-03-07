// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PropertyHelpers.h"

class UProperty;
class UStruct;
class IUsdPrim;
class FUsdAttribute;
class AActor;
struct FUsdImportContext;

typedef TFunction<void(void*, const FUsdAttribute&, UProperty*, int32)> FStructSetterFunction;

class FUSDPropertySetter
{
public:
	FUSDPropertySetter(FUsdImportContext& InImportContext);
	
	/**
	 * Applies properties found on a UsdPrim (and possibly its children) to a spawned actor
	 */
	void ApplyPropertiesToActor(AActor* SpawnedActor, IUsdPrim* Prim, const FString& StartingPropertyPath);

	/**
	 * Registers a setter for a struct type to set the struct in bulk instad of by individual inner property
	 */
	void RegisterStructSetter(FName StructName, FStructSetterFunction Function);
private:
	/**
	 * Finds properties and addresses for those properties and applies them from values in USD attributes
	 */
	void ApplyPropertiesFromUsdAttributes(IUsdPrim* Prim, AActor* SpawnedActor, const FString& StartingPropertyPath);

	/**
	 * Sets a property value from a USD Attribute
	 */
	void SetFromUSDValue(PropertyHelpers::FPropertyAddress& PropertyAddress, IUsdPrim* Prim, const FUsdAttribute& Attribute, int32 ArrayIndex);

	/**
	 * Finds Key/Value pairs for TMap properties;
	 */
	bool FindMapKeyAndValues(IUsdPrim* Prim, const FUsdAttribute*& OutKey, TArray<const FUsdAttribute*>& OutValues);

	/**
	 * Verifies the result of trying to set a given usd attribute with a given usd property.  Will produce an error if the types are incompatible
	 */
	bool VerifyResult(bool bResult, const FUsdAttribute& Attribute, UProperty* Property);

	/**
	 * Combines two property paths into a single "." delimited property path
	 */
	FString CombinePropertyPaths(const FString& Path1, const FString& Path2);
private:
	/** Registered struct types that have setters to set the values in bulk without walking the properties */
	TMap<FName, FStructSetterFunction> StructToSetterMap;

	FUsdImportContext& ImportContext;
};

