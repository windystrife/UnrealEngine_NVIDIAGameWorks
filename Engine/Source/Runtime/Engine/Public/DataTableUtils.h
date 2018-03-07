// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/EnumClassFlags.h"

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogDataTable, Log, All);

enum class EDataTableExportFlags : uint8
{
	/** No specific options. */
	None = 0,

	/** Export properties using their display name, rather than their internal name. */
	UsePrettyPropertyNames = 1<<0,

	/** Export User Defined Enums using their display name, rather than their internal name. */
	UsePrettyEnumNames = 1<<1,

	/** Export nested structs as JSON objects (JSON exporter only), rather than as exported text. */
	UseJsonObjectsForStructs = 1<<2,
};
ENUM_CLASS_FLAGS(EDataTableExportFlags);

namespace DataTableUtils
{
	/**
	 * Util to assign a value (given as a string) to a struct property.
	 * This always assigns the string to the given property without adjusting the address.
	 */
	ENGINE_API FString AssignStringToPropertyDirect(const FString& InString, const UProperty* InProp, uint8* InData);

	/**
	 * Util to assign a value (given as a string) to a struct property.
	 * When the property is a static sized array, this will split the string and assign the split parts to each element in the array.
	 */
	ENGINE_API FString AssignStringToProperty(const FString& InString, const UProperty* InProp, uint8* InData);

	/** 
	 * Util to get a property as a string.
	 * This always gets a string for the given property without adjusting the address.
	 */
	ENGINE_API FString GetPropertyValueAsStringDirect(const UProperty* InProp, uint8* InData, const EDataTableExportFlags InDTExportFlags);

	/** 
	 * Util to get a property as a string.
	 * When the property is a static sized array, this will return a string containing each element in the array.
	 */
	ENGINE_API FString GetPropertyValueAsString(const UProperty* InProp, uint8* InData, const EDataTableExportFlags InDTExportFlags);

	/** 
	 * Util to get a property as text (this will use the display name of the value where available - use GetPropertyValueAsString if you need an internal identifier).
	 * This always gets a string for the given property without adjusting the address.
	 */
	ENGINE_API FText GetPropertyValueAsTextDirect(const UProperty* InProp, uint8* InData);

	/** 
	 * Util to get a property as text (this will use the display name of the value where available - use GetPropertyValueAsString if you need an internal identifier).
	 * When the property is a static sized array, this will return a string containing each element in the array. 
	 */
	ENGINE_API FText GetPropertyValueAsText(const UProperty* InProp, uint8* InData);

	/**
	 * Util to get all property names from a struct.
	 */
	ENGINE_API TArray<FName> GetStructPropertyNames(UStruct* InStruct);

	/**
	 * Util that removes invalid chars and then make an FName.
	 */
	ENGINE_API FName MakeValidName(const FString& InString);

	/**
	 * Util to see if this property is supported in a row struct.
	 */
	ENGINE_API bool IsSupportedTableProperty(const UProperty* InProp);

	/**
	 * Util to get the friendly display unlocalized name of a given property for export to files.
	 */
	ENGINE_API FString GetPropertyExportName(const UProperty* Prop, const EDataTableExportFlags InDTExportFlags);

	/**
	 * Util to get the all variants for export names for backwards compatibility.
	 */
	ENGINE_API TArray<FString> GetPropertyImportNames(const UProperty* Prop);

	/**
	 * Util to get the friendly display name of a given property.
	 */
	ENGINE_API FString GetPropertyDisplayName(const UProperty* Prop, const FString& DefaultName);
}
