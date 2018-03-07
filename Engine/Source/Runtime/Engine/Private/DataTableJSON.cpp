// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DataTableJSON.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "DataTableUtils.h"
#include "Engine/DataTable.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/UserDefinedStruct.h"

#if WITH_EDITOR

namespace
{
	const TCHAR* JSONTypeToString(const EJson InType)
	{
		switch(InType)
		{
		case EJson::None:
			return TEXT("None");
		case EJson::Null:
			return TEXT("Null");
		case EJson::String:
			return TEXT("String");
		case EJson::Number:
			return TEXT("Number");
		case EJson::Boolean:
			return TEXT("Boolean");
		case EJson::Array:
			return TEXT("Array");
		case EJson::Object:
			return TEXT("Object");
		default:
			return TEXT("Unknown");
		}
	}

	void WriteJSONObjectStartWithOptionalIdentifier(FDataTableExporterJSON::FDataTableJsonWriter& InJsonWriter, const FString* InIdentifier)
	{
		if (InIdentifier)
		{
			InJsonWriter.WriteObjectStart(*InIdentifier);
		}
		else
		{
			InJsonWriter.WriteObjectStart();
		}
	}

	template <typename ValueType>
	void WriteJSONValueWithOptionalIdentifier(FDataTableExporterJSON::FDataTableJsonWriter& InJsonWriter, const FString* InIdentifier, const ValueType InValue)
	{
		if (InIdentifier)
		{
			InJsonWriter.WriteValue(*InIdentifier, InValue);
		}
		else
		{
			InJsonWriter.WriteValue(InValue);
		}
	}
}

FDataTableExporterJSON::FDataTableExporterJSON(const EDataTableExportFlags InDTExportFlags, FString& OutExportText)
	: DTExportFlags(InDTExportFlags)
	, JsonWriter(TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutExportText))
	, bJsonWriterNeedsClose(true)
{
}

FDataTableExporterJSON::FDataTableExporterJSON(const EDataTableExportFlags InDTExportFlags, TSharedRef<FDataTableJsonWriter> InJsonWriter)
	: DTExportFlags(InDTExportFlags)
	, JsonWriter(InJsonWriter)
	, bJsonWriterNeedsClose(false)
{
}

FDataTableExporterJSON::~FDataTableExporterJSON()
{
	if (bJsonWriterNeedsClose)
	{
		JsonWriter->Close();
	}
}

bool FDataTableExporterJSON::WriteTable(const UDataTable& InDataTable)
{
	if (!InDataTable.RowStruct)
	{
		return false;
	}

	JsonWriter->WriteArrayStart();

	// Iterate over rows
	for (auto RowIt = InDataTable.RowMap.CreateConstIterator(); RowIt; ++RowIt)
	{
		JsonWriter->WriteObjectStart();
		{
			// RowName
			const FName RowName = RowIt.Key();
			JsonWriter->WriteValue(TEXT("Name"), RowName.ToString());

			// Now the values
			uint8* RowData = RowIt.Value();
			WriteRow(InDataTable.RowStruct, RowData);
		}
		JsonWriter->WriteObjectEnd();
	}

	JsonWriter->WriteArrayEnd();

	return true;
}

bool FDataTableExporterJSON::WriteRow(const UScriptStruct* InRowStruct, const void* InRowData)
{
	if (!InRowStruct)
	{
		return false;
	}

	return WriteStruct(InRowStruct, InRowData);
}

bool FDataTableExporterJSON::WriteStruct(const UScriptStruct* InStruct, const void* InStructData)
{
	for (TFieldIterator<const UProperty> It(InStruct); It; ++It)
	{
		const UProperty* BaseProp = *It;
		check(BaseProp);

		if (BaseProp->ArrayDim == 1)
		{
			const void* Data = BaseProp->ContainerPtrToValuePtr<void>(InStructData, 0);
			WriteStructEntry(InStructData, BaseProp, Data);
		}
		else
		{
			const FString Identifier = DataTableUtils::GetPropertyExportName(BaseProp, DTExportFlags);

			JsonWriter->WriteArrayStart(Identifier);

			for (int32 ArrayEntryIndex = 0; ArrayEntryIndex < BaseProp->ArrayDim; ++ArrayEntryIndex)
			{
				const void* Data = BaseProp->ContainerPtrToValuePtr<void>(InStructData, ArrayEntryIndex);
				WriteContainerEntry(BaseProp, Data);
			}

			JsonWriter->WriteArrayEnd();
		}
	}

	return true;
}

bool FDataTableExporterJSON::WriteStructEntry(const void* InRowData, const UProperty* InProperty, const void* InPropertyData)
{
	const FString Identifier = DataTableUtils::GetPropertyExportName(InProperty, DTExportFlags);

	if (const UEnumProperty* EnumProp = Cast<const UEnumProperty>(InProperty))
	{
		const FString PropertyValue = DataTableUtils::GetPropertyValueAsString(EnumProp, (uint8*)InRowData, DTExportFlags);
		JsonWriter->WriteValue(Identifier, PropertyValue);
	}
	else if (const UNumericProperty *NumProp = Cast<const UNumericProperty>(InProperty))
	{
		if (NumProp->IsEnum())
		{
			const FString PropertyValue = DataTableUtils::GetPropertyValueAsString(InProperty, (uint8*)InRowData, DTExportFlags);
			JsonWriter->WriteValue(Identifier, PropertyValue);
		}
		else if (NumProp->IsInteger())
		{
			const int64 PropertyValue = NumProp->GetSignedIntPropertyValue(InPropertyData);
			JsonWriter->WriteValue(Identifier, PropertyValue);
		}
		else
		{
			const double PropertyValue = NumProp->GetFloatingPointPropertyValue(InPropertyData);
			JsonWriter->WriteValue(Identifier, PropertyValue);
		}
	}
	else if (const UBoolProperty* BoolProp = Cast<const UBoolProperty>(InProperty))
	{
		const bool PropertyValue = BoolProp->GetPropertyValue(InPropertyData);
		JsonWriter->WriteValue(Identifier, PropertyValue);
	}
	else if (const UArrayProperty* ArrayProp = Cast<const UArrayProperty>(InProperty))
	{
		JsonWriter->WriteArrayStart(Identifier);

		FScriptArrayHelper ArrayHelper(ArrayProp, InPropertyData);
		for (int32 ArrayEntryIndex = 0; ArrayEntryIndex < ArrayHelper.Num(); ++ArrayEntryIndex)
		{
			const uint8* ArrayEntryData = ArrayHelper.GetRawPtr(ArrayEntryIndex);
			WriteContainerEntry(ArrayProp->Inner, ArrayEntryData);
		}

		JsonWriter->WriteArrayEnd();
	}
	else if (const USetProperty* SetProp = Cast<const USetProperty>(InProperty))
	{
		JsonWriter->WriteArrayStart(Identifier);

		FScriptSetHelper SetHelper(SetProp, InPropertyData);
		for (int32 SetSparseIndex = 0; SetSparseIndex < SetHelper.GetMaxIndex(); ++SetSparseIndex)
		{
			if (SetHelper.IsValidIndex(SetSparseIndex))
			{
				const uint8* SetEntryData = SetHelper.GetElementPtr(SetSparseIndex);
				WriteContainerEntry(SetHelper.GetElementProperty(), SetEntryData);
			}
		}

		JsonWriter->WriteArrayEnd();
	}
	else if (const UMapProperty* MapProp = Cast<const UMapProperty>(InProperty))
	{
		JsonWriter->WriteObjectStart(Identifier);

		FScriptMapHelper MapHelper(MapProp, InPropertyData);
		for (int32 MapSparseIndex = 0; MapSparseIndex < MapHelper.GetMaxIndex(); ++MapSparseIndex)
		{
			if (MapHelper.IsValidIndex(MapSparseIndex))
			{
				const uint8* MapKeyData = MapHelper.GetKeyPtr(MapSparseIndex);
				const uint8* MapValueData = MapHelper.GetValuePtr(MapSparseIndex);

				// JSON object keys must always be strings
				const FString KeyValue = DataTableUtils::GetPropertyValueAsStringDirect(MapHelper.GetKeyProperty(), (uint8*)MapKeyData, DTExportFlags);
				WriteContainerEntry(MapHelper.GetValueProperty(), MapValueData, &KeyValue);
			}
		}

		JsonWriter->WriteObjectEnd();
	}
	else if (const UStructProperty* StructProp = Cast<const UStructProperty>(InProperty))
	{
		if (!!(DTExportFlags & EDataTableExportFlags::UseJsonObjectsForStructs))
		{
			JsonWriter->WriteObjectStart(Identifier);
			WriteStruct(StructProp->Struct, InPropertyData);
			JsonWriter->WriteObjectEnd();
		}
		else
		{
			const FString PropertyValue = DataTableUtils::GetPropertyValueAsString(InProperty, (uint8*)InRowData, DTExportFlags);
			JsonWriter->WriteValue(Identifier, PropertyValue);
		}
	}
	else
	{
		const FString PropertyValue = DataTableUtils::GetPropertyValueAsString(InProperty, (uint8*)InRowData, DTExportFlags);
		JsonWriter->WriteValue(Identifier, PropertyValue);
	}

	return true;
}

bool FDataTableExporterJSON::WriteContainerEntry(const UProperty* InProperty, const void* InPropertyData, const FString* InIdentifier)
{
	if (const UEnumProperty* EnumProp = Cast<const UEnumProperty>(InProperty))
	{
		const FString PropertyValue = DataTableUtils::GetPropertyValueAsStringDirect(InProperty, (uint8*)InPropertyData, DTExportFlags);
		WriteJSONValueWithOptionalIdentifier(*JsonWriter, InIdentifier, PropertyValue);
	}
	else if (const UNumericProperty *NumProp = Cast<const UNumericProperty>(InProperty))
	{
		if (NumProp->IsEnum())
		{
			const FString PropertyValue = DataTableUtils::GetPropertyValueAsStringDirect(InProperty, (uint8*)InPropertyData, DTExportFlags);
			WriteJSONValueWithOptionalIdentifier(*JsonWriter, InIdentifier, PropertyValue);
		}
		else if (NumProp->IsInteger())
		{
			const int64 PropertyValue = NumProp->GetSignedIntPropertyValue(InPropertyData);
			WriteJSONValueWithOptionalIdentifier(*JsonWriter, InIdentifier, PropertyValue);
		}
		else
		{
			const double PropertyValue = NumProp->GetFloatingPointPropertyValue(InPropertyData);
			WriteJSONValueWithOptionalIdentifier(*JsonWriter, InIdentifier, PropertyValue);
		}
	}
	else if (const UBoolProperty* BoolProp = Cast<const UBoolProperty>(InProperty))
	{
		const bool PropertyValue = BoolProp->GetPropertyValue(InPropertyData);
		WriteJSONValueWithOptionalIdentifier(*JsonWriter, InIdentifier, PropertyValue);
	}
	else if (const UStructProperty* StructProp = Cast<const UStructProperty>(InProperty))
	{
		if (!!(DTExportFlags & EDataTableExportFlags::UseJsonObjectsForStructs))
		{
			WriteJSONObjectStartWithOptionalIdentifier(*JsonWriter, InIdentifier);
			WriteStruct(StructProp->Struct, InPropertyData);
			JsonWriter->WriteObjectEnd();
		}
		else
		{
			const FString PropertyValue = DataTableUtils::GetPropertyValueAsStringDirect(InProperty, (uint8*)InPropertyData, DTExportFlags);
			WriteJSONValueWithOptionalIdentifier(*JsonWriter, InIdentifier, PropertyValue);
		}
	}
	else if (const UArrayProperty* ArrayProp = Cast<const UArrayProperty>(InProperty))
	{
		// Cannot nest arrays
		return false;
	}
	else if (const USetProperty* SetProp = Cast<const USetProperty>(InProperty))
	{
		// Cannot nest sets
		return false;
	}
	else if (const UMapProperty* MapProp = Cast<const UMapProperty>(InProperty))
	{
		// Cannot nest maps
		return false;
	}
	else
	{
		const FString PropertyValue = DataTableUtils::GetPropertyValueAsStringDirect(InProperty, (uint8*)InPropertyData, DTExportFlags);
		WriteJSONValueWithOptionalIdentifier(*JsonWriter, InIdentifier, PropertyValue);
	}

	return true;
}


FDataTableImporterJSON::FDataTableImporterJSON(UDataTable& InDataTable, const FString& InJSONData, TArray<FString>& OutProblems)
	: DataTable(&InDataTable)
	, JSONData(InJSONData)
	, ImportProblems(OutProblems)
{
}

FDataTableImporterJSON::~FDataTableImporterJSON()
{
}

bool FDataTableImporterJSON::ReadTable()
{
	if (JSONData.IsEmpty())
	{
		ImportProblems.Add(TEXT("Input data is empty."));
		return false;
	}

	// Check we have a RowStruct specified
	if (!DataTable->RowStruct)
	{
		ImportProblems.Add(TEXT("No RowStruct specified."));
		return false;
	}

	TArray< TSharedPtr<FJsonValue> > ParsedTableRows;
	{
		const TSharedRef< TJsonReader<TCHAR> > JsonReader = TJsonReaderFactory<TCHAR>::Create(JSONData);
		if (!FJsonSerializer::Deserialize(JsonReader, ParsedTableRows) || ParsedTableRows.Num() == 0)
		{
			ImportProblems.Add(FString::Printf(TEXT("Failed to parse the JSON data. Error: %s"), *JsonReader->GetErrorMessage()));
			return false;
		}
	}

	// Empty existing data
	DataTable->EmptyTable();

	// Iterate over rows
	for (int32 RowIdx = 0; RowIdx < ParsedTableRows.Num(); ++RowIdx)
	{
		const TSharedPtr<FJsonValue>& ParsedTableRowValue = ParsedTableRows[RowIdx];
		TSharedPtr<FJsonObject> ParsedTableRowObject = ParsedTableRowValue->AsObject();
		if (!ParsedTableRowObject.IsValid())
		{
			ImportProblems.Add(FString::Printf(TEXT("Row '%d' is not a valid JSON object."), RowIdx));
			continue;
		}

		ReadRow(ParsedTableRowObject.ToSharedRef(), RowIdx);
	}

	DataTable->Modify(true);

	return true;
}

bool FDataTableImporterJSON::ReadRow(const TSharedRef<FJsonObject>& InParsedTableRowObject, const int32 InRowIdx)
{
	// Get row name
	FName RowName = DataTableUtils::MakeValidName(InParsedTableRowObject->GetStringField(TEXT("Name")));

	// Check its not 'none'
	if (RowName.IsNone())
	{
		ImportProblems.Add(FString::Printf(TEXT("Row '%d' missing a name."), InRowIdx));
		return false;
	}

	// Check its not a duplicate
	if (DataTable->RowMap.Find(RowName) != nullptr)
	{
		ImportProblems.Add(FString::Printf(TEXT("Duplicate row name '%s'."), *RowName.ToString()));
		return false;
	}

	// Allocate data to store information, using UScriptStruct to know its size
	uint8* RowData = (uint8*)FMemory::Malloc(DataTable->RowStruct->GetStructureSize());
	DataTable->RowStruct->InitializeStruct(RowData);
	// And be sure to call DestroyScriptStruct later

	if (auto UDStruct = Cast<const UUserDefinedStruct>(DataTable->RowStruct))
	{
		UDStruct->InitializeDefaultValue(RowData);
	}

	// Add to row map
	DataTable->RowMap.Add(RowName, RowData);

	return ReadStruct(InParsedTableRowObject, DataTable->RowStruct, RowName, RowData);
}

bool FDataTableImporterJSON::ReadStruct(const TSharedRef<FJsonObject>& InParsedObject, UScriptStruct* InStruct, const FName InRowName, void* InStructData)
{
	// Now read in each property
	for (TFieldIterator<UProperty> It(InStruct); It; ++It)
	{
		UProperty* BaseProp = *It;
		check(BaseProp);

		const FString ColumnName = DataTableUtils::GetPropertyDisplayName(BaseProp, BaseProp->GetName());

		TSharedPtr<FJsonValue> ParsedPropertyValue;
		for (const FString& PropertyName : DataTableUtils::GetPropertyImportNames(BaseProp))
		{
			ParsedPropertyValue = InParsedObject->TryGetField(PropertyName);
			if (ParsedPropertyValue.IsValid())
			{
				break;
			}
		}

		if (!ParsedPropertyValue.IsValid())
		{
			ImportProblems.Add(FString::Printf(TEXT("Row '%s' is missing an entry for '%s'."), *InRowName.ToString(), *ColumnName));
			continue;
		}

		if (BaseProp->ArrayDim == 1)
		{
			void* Data = BaseProp->ContainerPtrToValuePtr<void>(InStructData, 0);
			ReadStructEntry(ParsedPropertyValue.ToSharedRef(), InRowName, ColumnName, InStructData, BaseProp, Data);
		}
		else
		{
			const TCHAR* const ParsedPropertyType = JSONTypeToString(ParsedPropertyValue->Type);

			const TArray< TSharedPtr<FJsonValue> >* PropertyValuesPtr;
			if (!ParsedPropertyValue->TryGetArray(PropertyValuesPtr))
			{
				ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is the incorrect type. Expected Array, got %s."), *ColumnName, *InRowName.ToString(), ParsedPropertyType));
				return false;
			}

			if (BaseProp->ArrayDim != PropertyValuesPtr->Num())
			{
				ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is a static sized array with %d elements, but we have %d values to import"), *ColumnName, *InRowName.ToString(), BaseProp->ArrayDim, PropertyValuesPtr->Num()));
			}

			for (int32 ArrayEntryIndex = 0; ArrayEntryIndex < BaseProp->ArrayDim; ++ArrayEntryIndex)
			{
				if (PropertyValuesPtr->IsValidIndex(ArrayEntryIndex))
				{
					void* Data = BaseProp->ContainerPtrToValuePtr<void>(InStructData, ArrayEntryIndex);
					const TSharedPtr<FJsonValue>& PropertyValueEntry = (*PropertyValuesPtr)[ArrayEntryIndex];
					ReadContainerEntry(PropertyValueEntry.ToSharedRef(), InRowName, ColumnName, ArrayEntryIndex, BaseProp, Data);
				}
			}
		}
	}

	return true;
}

bool FDataTableImporterJSON::ReadStructEntry(const TSharedRef<FJsonValue>& InParsedPropertyValue, const FName InRowName, const FString& InColumnName, const void* InRowData, UProperty* InProperty, void* InPropertyData)
{
	const TCHAR* const ParsedPropertyType = JSONTypeToString(InParsedPropertyValue->Type);

	if (UEnumProperty* EnumProp = Cast<UEnumProperty>(InProperty))
	{
		FString EnumValue;
		if (InParsedPropertyValue->TryGetString(EnumValue))
		{
			FString Error = DataTableUtils::AssignStringToProperty(EnumValue, InProperty, (uint8*)InRowData);
			if (!Error.IsEmpty())
			{
				ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' has invalid enum value: %s."), *InColumnName, *InRowName.ToString(), *EnumValue));
				return false;
			}
		}
		else
		{
			int64 PropertyValue = 0;
			if (!InParsedPropertyValue->TryGetNumber(PropertyValue))
			{
				ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is the incorrect type. Expected Integer, got %s."), *InColumnName, *InRowName.ToString(), ParsedPropertyType));
				return false;
			}

			EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(InPropertyData, PropertyValue);
		}
	}
	else if (UNumericProperty *NumProp = Cast<UNumericProperty>(InProperty))
	{
		FString EnumValue;
		if (NumProp->IsEnum() && InParsedPropertyValue->TryGetString(EnumValue))
		{
			FString Error = DataTableUtils::AssignStringToProperty(EnumValue, InProperty, (uint8*)InRowData);
			if (!Error.IsEmpty())
			{
				ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' has invalid enum value: %s."), *InColumnName, *InRowName.ToString(), *EnumValue));
				return false;
			}
		}
		else if (NumProp->IsInteger())
		{
			int64 PropertyValue = 0;
			if (!InParsedPropertyValue->TryGetNumber(PropertyValue))
			{
				ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is the incorrect type. Expected Integer, got %s."), *InColumnName, *InRowName.ToString(), ParsedPropertyType));
				return false;
			}

			NumProp->SetIntPropertyValue(InPropertyData, PropertyValue);
		}
		else
		{
			double PropertyValue = 0.0;
			if (!InParsedPropertyValue->TryGetNumber(PropertyValue))
			{
				ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is the incorrect type. Expected Double, got %s."), *InColumnName, *InRowName.ToString(), ParsedPropertyType));
				return false;
			}

			NumProp->SetFloatingPointPropertyValue(InPropertyData, PropertyValue);
		}
	}
	else if (UBoolProperty* BoolProp = Cast<UBoolProperty>(InProperty))
	{
		bool PropertyValue = false;
		if (!InParsedPropertyValue->TryGetBool(PropertyValue))
		{
			ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is the incorrect type. Expected Boolean, got %s."), *InColumnName, *InRowName.ToString(), ParsedPropertyType));
			return false;
		}

		BoolProp->SetPropertyValue(InPropertyData, PropertyValue);
	}
	else if (UArrayProperty* ArrayProp = Cast<UArrayProperty>(InProperty))
	{
		const TArray< TSharedPtr<FJsonValue> >* PropertyValuesPtr;
		if (!InParsedPropertyValue->TryGetArray(PropertyValuesPtr))
		{
			ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is the incorrect type. Expected Array, got %s."), *InColumnName, *InRowName.ToString(), ParsedPropertyType));
			return false;
		}

		FScriptArrayHelper ArrayHelper(ArrayProp, InPropertyData);
		ArrayHelper.EmptyValues();
		for (const TSharedPtr<FJsonValue>& PropertyValueEntry : *PropertyValuesPtr)
		{
			const int32 NewEntryIndex = ArrayHelper.AddValue();
			uint8* ArrayEntryData = ArrayHelper.GetRawPtr(NewEntryIndex);
			ReadContainerEntry(PropertyValueEntry.ToSharedRef(), InRowName, InColumnName, NewEntryIndex, ArrayProp->Inner, ArrayEntryData);
		}
	}
	else if (USetProperty* SetProp = Cast<USetProperty>(InProperty))
	{
		const TArray< TSharedPtr<FJsonValue> >* PropertyValuesPtr;
		if (!InParsedPropertyValue->TryGetArray(PropertyValuesPtr))
		{
			ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is the incorrect type. Expected Array, got %s."), *InColumnName, *InRowName.ToString(), ParsedPropertyType));
			return false;
		}

		FScriptSetHelper SetHelper(SetProp, InPropertyData);
		SetHelper.EmptyElements();
		for (const TSharedPtr<FJsonValue>& PropertyValueEntry : *PropertyValuesPtr)
		{
			const int32 NewEntryIndex = SetHelper.AddDefaultValue_Invalid_NeedsRehash();
			uint8* SetEntryData = SetHelper.GetElementPtr(NewEntryIndex);
			ReadContainerEntry(PropertyValueEntry.ToSharedRef(), InRowName, InColumnName, NewEntryIndex, SetHelper.GetElementProperty(), SetEntryData);
		}
		SetHelper.Rehash();
	}
	else if (UMapProperty* MapProp = Cast<UMapProperty>(InProperty))
	{
		const TSharedPtr<FJsonObject>* PropertyValue;
		if (!InParsedPropertyValue->TryGetObject(PropertyValue))
		{
			ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is the incorrect type. Expected Object, got %s."), *InColumnName, *InRowName.ToString(), ParsedPropertyType));
			return false;
		}

		FScriptMapHelper MapHelper(MapProp, InPropertyData);
		MapHelper.EmptyValues();
		for (const auto& PropertyValuePair : (*PropertyValue)->Values)
		{
			const int32 NewEntryIndex = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
			uint8* MapKeyData = MapHelper.GetKeyPtr(NewEntryIndex);
			uint8* MapValueData = MapHelper.GetValuePtr(NewEntryIndex);

			// JSON object keys are always strings
			const FString KeyError = DataTableUtils::AssignStringToPropertyDirect(PropertyValuePair.Key, MapHelper.GetKeyProperty(), MapKeyData);
			if (KeyError.Len() > 0)
			{
				MapHelper.RemoveAt(NewEntryIndex);
				ImportProblems.Add(FString::Printf(TEXT("Problem assigning key '%s' to property '%s' on row '%s' : %s"), *PropertyValuePair.Key, *InColumnName, *InRowName.ToString(), *KeyError));
				return false;
			}

			if (!ReadContainerEntry(PropertyValuePair.Value.ToSharedRef(), InRowName, InColumnName, NewEntryIndex, MapHelper.GetValueProperty(), MapValueData))
			{
				MapHelper.RemoveAt(NewEntryIndex);
				return false;
			}
		}
		MapHelper.Rehash();
	}
	else if (UStructProperty* StructProp = Cast<UStructProperty>(InProperty))
	{
		const TSharedPtr<FJsonObject>* PropertyValue = nullptr;
		if (InParsedPropertyValue->TryGetObject(PropertyValue))
		{
			return ReadStruct(PropertyValue->ToSharedRef(), StructProp->Struct, InRowName, InPropertyData);
		}
		else
		{
			// If the JSON does not contain a JSON object for this struct, we try to use the backwards-compatible string deserialization, same as the "else" block below
			FString PropertyValueString;
			if (!InParsedPropertyValue->TryGetString(PropertyValueString))
			{
				ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is the incorrect type. Expected String, got %s."), *InColumnName, *InRowName.ToString(), ParsedPropertyType));
				return false;
			}

			const FString Error = DataTableUtils::AssignStringToProperty(PropertyValueString, InProperty, (uint8*)InRowData);
			if (Error.Len() > 0)
			{
				ImportProblems.Add(FString::Printf(TEXT("Problem assigning string '%s' to property '%s' on row '%s' : %s"), *PropertyValueString, *InColumnName, *InRowName.ToString(), *Error));
				return false;
			}

			return true;
		}
	}
	else
	{
		FString PropertyValue;
		if (!InParsedPropertyValue->TryGetString(PropertyValue))
		{
			ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is the incorrect type. Expected String, got %s."), *InColumnName, *InRowName.ToString(), ParsedPropertyType));
			return false;
		}

		const FString Error = DataTableUtils::AssignStringToProperty(PropertyValue, InProperty, (uint8*)InRowData);
		if(Error.Len() > 0)
		{
			ImportProblems.Add(FString::Printf(TEXT("Problem assigning string '%s' to property '%s' on row '%s' : %s"), *PropertyValue, *InColumnName, *InRowName.ToString(), *Error));
			return false;
		}
	}

	return true;
}

bool FDataTableImporterJSON::ReadContainerEntry(const TSharedRef<FJsonValue>& InParsedPropertyValue, const FName InRowName, const FString& InColumnName, const int32 InArrayEntryIndex, UProperty* InProperty, void* InPropertyData)
{
	const TCHAR* const ParsedPropertyType = JSONTypeToString(InParsedPropertyValue->Type);

	if (UEnumProperty* EnumProp = Cast<UEnumProperty>(InProperty))
	{
		FString EnumValue;
		if (InParsedPropertyValue->TryGetString(EnumValue))
		{
			FString Error = DataTableUtils::AssignStringToPropertyDirect(EnumValue, InProperty, (uint8*)InPropertyData);
			if (!Error.IsEmpty())
			{
				ImportProblems.Add(FString::Printf(TEXT("Entry %d on property '%s' on row '%s' has invalid enum value: %s."), InArrayEntryIndex, *InColumnName, *InRowName.ToString(), *EnumValue));
				return false;
			}
		}
		else
		{
			int64 PropertyValue = 0;
			if (!InParsedPropertyValue->TryGetNumber(PropertyValue))
			{
				ImportProblems.Add(FString::Printf(TEXT("Entry %d on property '%s' on row '%s' is the incorrect type. Expected Integer, got %s."), InArrayEntryIndex, *InColumnName, *InRowName.ToString(), ParsedPropertyType));
				return false;
			}

			EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(InPropertyData, PropertyValue);
		}
	}
	else if (UNumericProperty *NumProp = Cast<UNumericProperty>(InProperty))
	{
		FString EnumValue;
		if (NumProp->IsEnum() && InParsedPropertyValue->TryGetString(EnumValue))
		{
			FString Error = DataTableUtils::AssignStringToPropertyDirect(EnumValue, InProperty, (uint8*)InPropertyData);
			if (!Error.IsEmpty())
			{
				ImportProblems.Add(FString::Printf(TEXT("Entry %d on property '%s' on row '%s' has invalid enum value: %s."), InArrayEntryIndex, *InColumnName, *InRowName.ToString(), *EnumValue));
				return false;
			}
		}
		else if(NumProp->IsInteger())
		{
			int64 PropertyValue = 0;
			if (!InParsedPropertyValue->TryGetNumber(PropertyValue))
			{
				ImportProblems.Add(FString::Printf(TEXT("Entry %d on property '%s' on row '%s' is the incorrect type. Expected Integer, got %s."), InArrayEntryIndex, *InColumnName, *InRowName.ToString(), ParsedPropertyType));
				return false;
			}

			NumProp->SetIntPropertyValue(InPropertyData, PropertyValue);
		}
		else
		{
			double PropertyValue = 0.0;
			if (!InParsedPropertyValue->TryGetNumber(PropertyValue))
			{
				ImportProblems.Add(FString::Printf(TEXT("Entry %d on property '%s' on row '%s' is the incorrect type. Expected Double, got %s."), InArrayEntryIndex, *InColumnName, *InRowName.ToString(), ParsedPropertyType));
				return false;
			}

			NumProp->SetFloatingPointPropertyValue(InPropertyData, PropertyValue);
		}
	}
	else if (UBoolProperty* BoolProp = Cast<UBoolProperty>(InProperty))
	{
		bool PropertyValue = false;
		if (!InParsedPropertyValue->TryGetBool(PropertyValue))
		{
			ImportProblems.Add(FString::Printf(TEXT("Entry %d on property '%s' on row '%s' is the incorrect type. Expected Boolean, got %s."), InArrayEntryIndex, *InColumnName, *InRowName.ToString(), ParsedPropertyType));
			return false;
		}

		BoolProp->SetPropertyValue(InPropertyData, PropertyValue);
	}
	else if (UArrayProperty* ArrayProp = Cast<UArrayProperty>(InProperty))
	{
		// Cannot nest arrays
		return false;
	}
	else if (USetProperty* SetProp = Cast<USetProperty>(InProperty))
	{
		// Cannot nest sets
		return false;
	}
	else if (UMapProperty* MapProp = Cast<UMapProperty>(InProperty))
	{
		// Cannot nest maps
		return false;
	}
	else if (UStructProperty* StructProp = Cast<UStructProperty>(InProperty))
	{
		const TSharedPtr<FJsonObject>* PropertyValue = nullptr;
		if (InParsedPropertyValue->TryGetObject(PropertyValue))
		{
			return ReadStruct(PropertyValue->ToSharedRef(), StructProp->Struct, InRowName, InPropertyData);
		}
		else
		{
			// If the JSON does not contain a JSON object for this struct, we try to use the backwards-compatible string deserialization, same as the "else" block below
			FString PropertyValueString;
			if (!InParsedPropertyValue->TryGetString(PropertyValueString))
			{
				ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is the incorrect type. Expected String, got %s."), *InColumnName, *InRowName.ToString(), ParsedPropertyType));
				return false;
			}

			const FString Error = DataTableUtils::AssignStringToPropertyDirect(PropertyValueString, InProperty, (uint8*)InPropertyData);
			if (Error.Len() > 0)
			{
				ImportProblems.Add(FString::Printf(TEXT("Problem assigning string '%s' to entry %d on property '%s' on row '%s' : %s"), InArrayEntryIndex, *PropertyValueString, *InColumnName, *InRowName.ToString(), *Error));
				return false;
			}

			return true;
		}
	}
	else
	{
		FString PropertyValue;
		if (!InParsedPropertyValue->TryGetString(PropertyValue))
		{
			ImportProblems.Add(FString::Printf(TEXT("Entry %d on property '%s' on row '%s' is the incorrect type. Expected String, got %s."), InArrayEntryIndex, *InColumnName, *InRowName.ToString(), ParsedPropertyType));
			return false;
		}

		const FString Error = DataTableUtils::AssignStringToPropertyDirect(PropertyValue, InProperty, (uint8*)InPropertyData);
		if(Error.Len() > 0)
		{
			ImportProblems.Add(FString::Printf(TEXT("Problem assigning string '%s' to entry %d on property '%s' on row '%s' : %s"), InArrayEntryIndex, *PropertyValue, *InColumnName, *InRowName.ToString(), *Error));
			return false;
		}
	}

	return true;
}

#endif // WITH_EDITOR
