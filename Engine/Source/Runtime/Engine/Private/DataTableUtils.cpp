// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DataTableUtils.h"
#include "UObject/UnrealType.h"
#include "UObject/TextProperty.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/UserDefinedStruct.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/EnumProperty.h"
#include "DataTableJSON.h"

DEFINE_LOG_CATEGORY(LogDataTable);

namespace DataTableUtilsImpl
{

FString GetSourceString(const FText& Text)
{
	if (const FString* SourceString = FTextInspector::GetSourceString(Text))
	{
		return *SourceString;
	}
	return Text.ToString();
}

void AssignStringToPropertyDirect(const FString& InString, const UProperty* InProp, uint8* InData, const int32 InPortFlags, FStringOutputDevice& OutImportError)
{
	auto DoImportText = [&](const FString& InStringToImport)
	{
		InProp->ImportText(*InStringToImport, InData, InPortFlags, nullptr, &OutImportError);
	};

	bool bNeedsImport = true;

	UEnum* Enum = nullptr;

	if (const UEnumProperty* EnumProp = Cast<const UEnumProperty>(InProp))
	{
		Enum = EnumProp->GetEnum();
	}
	else if (const UByteProperty* ByteProp = Cast<const UByteProperty>(InProp))
	{
		if (ByteProp->IsEnum())
		{
			Enum = ByteProp->GetIntPropertyEnum();
		}
	}

	if (Enum)
	{
		// Enum properties may use the friendly name in their import data, however the UPropertyByte::ImportText function will only accept the internal enum entry name
		// Detect if we're using a friendly name for an entry, and if so, try and map it to the correct internal name before performing the import
		const int32 EnumIndex = Enum->GetIndexByNameString(InString);
		if(EnumIndex == INDEX_NONE)
		{
			// Couldn't find a match for the name we were given, try and find a match using the friendly names
			for(int32 EnumEntryIndex = 0; EnumEntryIndex < Enum->NumEnums(); ++EnumEntryIndex)
			{
				const FText FriendlyEnumEntryName = Enum->GetDisplayNameTextByIndex(EnumEntryIndex);
				if ((FriendlyEnumEntryName.ToString() == InString) || (GetSourceString(FriendlyEnumEntryName) == InString))
				{
					// Get the corresponding internal name and warn the user that we're using this fallback if not a user-defined enum
					FString StringToImport = Enum->GetNameStringByIndex(EnumEntryIndex);
					if (!Enum->IsA<UUserDefinedEnum>())
					{
						UE_LOG(LogDataTable, Warning, TEXT("Could not a find matching enum entry for '%s', but did find a matching display name. Will import using the enum entry corresponding to that display name ('%s')"), *InString, *StringToImport);
					}
					DoImportText(StringToImport);
					bNeedsImport = false;
					break;
				}
			}
		}
	}

	if(bNeedsImport)
	{
		DoImportText(InString);
	}
}

void AssignStringToProperty(const FString& InString, const UProperty* InProp, uint8* InData, const int32 InIndex, const int32 InPortFlags, FStringOutputDevice& OutImportError)
{
	uint8* ValuePtr = InProp->ContainerPtrToValuePtr<uint8>(InData, InIndex);
	AssignStringToPropertyDirect(InString, InProp, ValuePtr, InPortFlags, OutImportError);
}

void GetPropertyValueAsStringDirect(const UProperty* InProp, const uint8* InData, const int32 InPortFlags, const EDataTableExportFlags InDTExportFlags, FString& OutString)
{
	if (!!(InDTExportFlags & EDataTableExportFlags::UsePrettyEnumNames))
	{
		UEnum* Enum = nullptr;
		int64 Val = 0;
		if (const UEnumProperty* EnumProp = Cast<UEnumProperty>(InProp))
		{
			Enum = EnumProp->GetEnum();
			Val = EnumProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(InData);
		}
		else if (const UByteProperty* ByteProp = Cast<UByteProperty>(InProp))
		{
			Enum = ByteProp->GetIntPropertyEnum();
			Val = *InData;
		}

		if (UUserDefinedEnum* UDEnum = Cast<UUserDefinedEnum>(Enum))
		{
			OutString.Append(GetSourceString(UDEnum->GetDisplayNameTextByValue(Val)));
			return;
		}
	}
#if WITH_EDITOR
	if (InPortFlags & PPF_PropertyWindow)
	{
		auto ExportStructAsJson = [InDTExportFlags](const UScriptStruct* InStruct, const void* InStructData) -> FString
		{
			FString JsonOutputStr;
			{
				TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonOutputStr);

				JsonWriter->WriteObjectStart();
				FDataTableExporterJSON(InDTExportFlags, JsonWriter).WriteStruct(InStruct, InStructData);
				JsonWriter->WriteObjectEnd();

				JsonWriter->Close();
			}

			JsonOutputStr.ReplaceInline(TEXT("\t"), TEXT(""), ESearchCase::CaseSensitive);
			JsonOutputStr.ReplaceInline(TEXT("\r"), TEXT(""), ESearchCase::CaseSensitive);
			JsonOutputStr.ReplaceInline(TEXT("\n"), TEXT(" "), ESearchCase::CaseSensitive);

			return JsonOutputStr;
		};

		const UArrayProperty* ArrayProp = Cast<const UArrayProperty>(InProp);
		const USetProperty* SetProp = Cast<const USetProperty>(InProp);
		const UMapProperty* MapProp = Cast<const UMapProperty>(InProp);

		if (ArrayProp && ArrayProp->Inner->IsA<UStructProperty>() && !!(InDTExportFlags & EDataTableExportFlags::UseJsonObjectsForStructs))
		{
			const UStructProperty* StructInner = CastChecked<const UStructProperty>(ArrayProp->Inner);

			OutString.AppendChar('(');

			FScriptArrayHelper ArrayHelper(ArrayProp, InData);
			for (int32 ArrayEntryIndex = 0; ArrayEntryIndex < ArrayHelper.Num(); ++ArrayEntryIndex)
			{
				if (ArrayEntryIndex > 0)
				{
					OutString.AppendChar(',');
					OutString.AppendChar(' ');
				}

				const uint8* ArrayEntryData = ArrayHelper.GetRawPtr(ArrayEntryIndex);
				OutString.Append(ExportStructAsJson(StructInner->Struct, ArrayEntryData));
			}

			OutString.AppendChar(')');
			return;
		}
		else if (SetProp && SetProp->ElementProp->IsA<UStructProperty>() && !!(InDTExportFlags & EDataTableExportFlags::UseJsonObjectsForStructs))
		{
			const UStructProperty* StructInner = CastChecked<const UStructProperty>(SetProp->ElementProp);

			OutString.AppendChar('(');

			int32 NumWrittenSetEntries = 0;
			FScriptSetHelper SetHelper(SetProp, InData);
			for (int32 SetSparseIndex = 0; SetSparseIndex < SetHelper.GetMaxIndex(); ++SetSparseIndex)
			{
				if (SetHelper.IsValidIndex(SetSparseIndex))
				{
					if (NumWrittenSetEntries++ > 0)
					{
						OutString.AppendChar(',');
						OutString.AppendChar(' ');
					}

					const uint8* SetEntryData = SetHelper.GetElementPtr(SetSparseIndex);
					OutString.Append(ExportStructAsJson(StructInner->Struct, SetEntryData));
				}
			}

			OutString.AppendChar(')');
			return;
		}
		else if (MapProp)
		{
			OutString.AppendChar('(');

			int32 NumWrittenMapEntries = 0;
			FScriptMapHelper MapHelper(MapProp, InData);
			for (int32 MapSparseIndex = 0; MapSparseIndex < MapHelper.GetMaxIndex(); ++MapSparseIndex)
			{
				if (MapHelper.IsValidIndex(MapSparseIndex))
				{
					if (NumWrittenMapEntries++ > 0)
					{
						OutString.AppendChar(',');
						OutString.AppendChar(' ');
					}

					const uint8* MapKeyData = MapHelper.GetKeyPtr(MapSparseIndex);
					const uint8* MapValueData = MapHelper.GetValuePtr(MapSparseIndex);

					OutString.AppendChar('"');
					GetPropertyValueAsStringDirect(MapHelper.GetKeyProperty(), MapKeyData, InPortFlags, InDTExportFlags, OutString);
					OutString.AppendChar('"');

					OutString.Append(TEXT(" = "));

					if (MapHelper.GetValueProperty()->IsA<UStructProperty>() && !!(InDTExportFlags & EDataTableExportFlags::UseJsonObjectsForStructs))
					{
						const UStructProperty* StructMapValue = CastChecked<const UStructProperty>(MapHelper.GetValueProperty());
						OutString.Append(ExportStructAsJson(StructMapValue->Struct, MapValueData));
					}
					else
					{
						GetPropertyValueAsStringDirect(MapHelper.GetValueProperty(), MapValueData, InPortFlags, InDTExportFlags, OutString);
					}
				}
			}

			OutString.AppendChar(')');
			return;
		}
		else if (const UStructProperty* StructProp = Cast<const UStructProperty>(InProp))
		{
			OutString.Append(ExportStructAsJson(StructProp->Struct, InData));
			return;
		}
	}
#endif // WITH_EDITOR
	InProp->ExportText_Direct(OutString, InData, InData, nullptr, InPortFlags);
}

void GetPropertyValueAsString(const UProperty* InProp, const uint8* InData, const int32 InIndex, const int32 InPortFlags, const EDataTableExportFlags InDTExportFlags, FString& OutString)
{
	const uint8* ValuePtr = InProp->ContainerPtrToValuePtr<uint8>(InData, InIndex);
	return GetPropertyValueAsStringDirect(InProp, ValuePtr, InPortFlags, InDTExportFlags, OutString);
}

}

FString DataTableUtils::AssignStringToPropertyDirect(const FString& InString, const UProperty* InProp, uint8* InData)
{
	FStringOutputDevice ImportError;
	if(InProp && IsSupportedTableProperty(InProp))
	{
		DataTableUtilsImpl::AssignStringToPropertyDirect(InString, InProp, InData, PPF_None, ImportError);
	}

	FString Error = ImportError;
	return Error;
}

FString DataTableUtils::AssignStringToProperty(const FString& InString, const UProperty* InProp, uint8* InData)
{
	FStringOutputDevice ImportError;
	if(InProp && IsSupportedTableProperty(InProp))
	{
		if(InProp->ArrayDim == 1)
		{
			DataTableUtilsImpl::AssignStringToProperty(InString, InProp, InData, 0, PPF_None, ImportError);
		}
		else
		{
			if(InString.Len() >= 2 && InString[0] == '(' && InString[InString.Len() - 1] == ')')
			{
				// Trim the ( and )
				FString StringToSplit = InString.Mid(1, InString.Len() - 2);

				TArray<FString> Values;
				StringToSplit.ParseIntoArray(Values, TEXT(","), 1);

				if (InProp->ArrayDim != Values.Num())
				{
					UE_LOG(LogDataTable, Warning, TEXT("%s - Array is %d elements large, but we have %d values to import"), *InProp->GetName(), InProp->ArrayDim, Values.Num());
				}

				for (int32 Index = 0; Index < InProp->ArrayDim; ++Index)
				{
					if (Values.IsValidIndex(Index))
					{
						DataTableUtilsImpl::AssignStringToProperty(Values[Index], InProp, InData, Index, PPF_Delimited, ImportError);
					}
				}
			}
			else
			{
				UE_LOG(LogDataTable, Warning, TEXT("%s - Malformed array string. It must start with '(' and end with ')'"), *InProp->GetName());
			}
		}
	}

	FString Error = ImportError;
	return Error;
}

FString DataTableUtils::GetPropertyValueAsStringDirect(const UProperty* InProp, uint8* InData, const EDataTableExportFlags InDTExportFlags)
{
	FString Result;

	if(InProp && IsSupportedTableProperty(InProp))
	{
		DataTableUtilsImpl::GetPropertyValueAsStringDirect(InProp, InData, PPF_None, InDTExportFlags, Result);
	}

	return Result;
}

FString DataTableUtils::GetPropertyValueAsString(const UProperty* InProp, uint8* InData, const EDataTableExportFlags InDTExportFlags)
{
	FString Result;

	if(InProp && IsSupportedTableProperty(InProp))
	{
		if(InProp->ArrayDim == 1)
		{
			DataTableUtilsImpl::GetPropertyValueAsString(InProp, InData, 0, PPF_None, InDTExportFlags, Result);
		}
		else
		{
			Result.AppendChar('(');

			for(int32 Index = 0; Index < InProp->ArrayDim; ++Index)
			{
				DataTableUtilsImpl::GetPropertyValueAsString(InProp, InData, Index, PPF_Delimited, InDTExportFlags, Result);

				if((Index + 1) < InProp->ArrayDim)
				{
					Result.AppendChar(',');
				}
			}

			Result.AppendChar(')');
		}
	}

	return Result;
}

FText DataTableUtils::GetPropertyValueAsTextDirect(const UProperty* InProp, uint8* InData)
{
	FText Result;

	if(InProp && IsSupportedTableProperty(InProp))
	{
		FString ExportedString;
		DataTableUtilsImpl::GetPropertyValueAsStringDirect(InProp, InData, PPF_PropertyWindow, EDataTableExportFlags::UsePrettyPropertyNames | EDataTableExportFlags::UsePrettyEnumNames | EDataTableExportFlags::UseJsonObjectsForStructs, ExportedString);

		Result = FText::FromString(MoveTemp(ExportedString));
	}

	return Result;
}

FText DataTableUtils::GetPropertyValueAsText(const UProperty* InProp, uint8* InData)
{
	FText Result;

	if(InProp && IsSupportedTableProperty(InProp))
	{
		FString ExportedString;

		if(InProp->ArrayDim == 1)
		{
			DataTableUtilsImpl::GetPropertyValueAsString(InProp, InData, 0, PPF_PropertyWindow, EDataTableExportFlags::UsePrettyPropertyNames | EDataTableExportFlags::UsePrettyEnumNames | EDataTableExportFlags::UseJsonObjectsForStructs, ExportedString);
		}
		else
		{
			ExportedString.AppendChar('(');

			for(int32 Index = 0; Index < InProp->ArrayDim; ++Index)
			{
				DataTableUtilsImpl::GetPropertyValueAsString(InProp, InData, Index, PPF_PropertyWindow | PPF_Delimited, EDataTableExportFlags::UsePrettyPropertyNames | EDataTableExportFlags::UsePrettyEnumNames | EDataTableExportFlags::UseJsonObjectsForStructs, ExportedString);

				if((Index + 1) < InProp->ArrayDim)
				{
					ExportedString.AppendChar(',');
					ExportedString.AppendChar(' ');
				}
			}

			ExportedString.AppendChar(')');
		}
		
		Result = FText::FromString(MoveTemp(ExportedString));
	}

	return Result;
}

TArray<FName> DataTableUtils::GetStructPropertyNames(UStruct* InStruct)
{
	TArray<FName> PropNames;
	for (TFieldIterator<UProperty> It(InStruct); It; ++It)
	{
		PropNames.Add(It->GetFName());
	}
	return PropNames;
}

FName DataTableUtils::MakeValidName(const FString& InString)
{
	FString InvalidChars(INVALID_NAME_CHARACTERS);

	FString FixedString;
	TArray<TCHAR>& FixedCharArray = FixedString.GetCharArray();

	// Iterate over input string characters
	for(int32 CharIdx=0; CharIdx<InString.Len(); CharIdx++)
	{
		// See if this char occurs in the InvalidChars string
		FString Char = InString.Mid( CharIdx, 1 );
		if( !InvalidChars.Contains(Char) )
		{
			// Its ok, add to result
			FixedCharArray.Add(Char[0]);
		}
	}
	FixedCharArray.Add(0);

	return FName(*FixedString);
}

bool DataTableUtils::IsSupportedTableProperty(const UProperty* InProp)
{
	return(	InProp->IsA(UIntProperty::StaticClass()) || 
			InProp->IsA(UNumericProperty::StaticClass()) ||
			InProp->IsA(UDoubleProperty::StaticClass()) ||
			InProp->IsA(UFloatProperty::StaticClass()) ||
			InProp->IsA(UNameProperty::StaticClass()) ||
			InProp->IsA(UStrProperty::StaticClass()) ||
			InProp->IsA(UBoolProperty::StaticClass()) ||
			InProp->IsA(UObjectPropertyBase::StaticClass()) ||
			InProp->IsA(UStructProperty::StaticClass()) ||
			InProp->IsA(UByteProperty::StaticClass()) ||
			InProp->IsA(UTextProperty::StaticClass()) ||
			InProp->IsA(UArrayProperty::StaticClass()) ||
			InProp->IsA(USetProperty::StaticClass()) ||
			InProp->IsA(UMapProperty::StaticClass()) ||
			InProp->IsA(UEnumProperty::StaticClass())
			);
}

FString DataTableUtils::GetPropertyExportName(const UProperty* Prop, const EDataTableExportFlags InDTExportFlags)
{
	if (!ensure(Prop))
	{
		return FString();
	}
	if (Prop->GetOwnerStruct()->IsA(UUserDefinedStruct::StaticClass()) && !!(InDTExportFlags & EDataTableExportFlags::UsePrettyPropertyNames))
	{
		return GetPropertyDisplayName(Prop, Prop->GetName());
	}
	return Prop->GetName();
}

TArray<FString> DataTableUtils::GetPropertyImportNames(const UProperty* Prop)
{
	TArray<FString> Result;
	if (ensure(Prop))
	{
		Result.AddUnique(Prop->GetName());
	}
	Result.AddUnique(GetPropertyExportName(Prop, EDataTableExportFlags::UsePrettyPropertyNames));
	return Result;
}

FString DataTableUtils::GetPropertyDisplayName(const UProperty* Prop, const FString& DefaultName)
{
#if WITH_EDITOR
	static const FName DisplayNameKey(TEXT("DisplayName"));
	return (Prop && Prop->HasMetaData(DisplayNameKey)) ? Prop->GetMetaData(DisplayNameKey) : DefaultName;
#else  // WITH_EDITOR
	return DefaultName;
#endif // WITH_EDITOR
}
