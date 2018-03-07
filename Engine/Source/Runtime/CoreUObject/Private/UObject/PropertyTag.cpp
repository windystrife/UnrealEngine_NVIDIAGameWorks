// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UObject/PropertyTag.h"
#include "UObject/DebugSerializationFlags.h"
#include "Serialization/SerializedPropertyScope.h"
#include "UObject/UnrealType.h"
#include "EnumProperty.h"
#include "UObject/BlueprintsObjectVersion.h"

/*-----------------------------------------------------------------------------
FPropertyTag
-----------------------------------------------------------------------------*/

// Constructors.
FPropertyTag::FPropertyTag()
	: Type      (NAME_None)
	, BoolVal   (0)
	, Name      (NAME_None)
	, StructName(NAME_None)
	, EnumName  (NAME_None)
	, InnerType (NAME_None)
	, ValueType	(NAME_None)
	, Size      (0)
	, ArrayIndex(INDEX_NONE)
	, SizeOffset(INDEX_NONE)
	, HasPropertyGuid(0)
{}

FPropertyTag::FPropertyTag( FArchive& InSaveAr, UProperty* Property, int32 InIndex, uint8* Value, uint8* Defaults )
	: Type      (Property->GetID())
	, BoolVal   (0)
	, Name      (Property->GetFName())
	, StructName(NAME_None)
	, EnumName	(NAME_None)
	, InnerType	(NAME_None)
	, ValueType	(NAME_None)
	, Size		(0)
	, ArrayIndex(InIndex)
	, SizeOffset(INDEX_NONE)
	, HasPropertyGuid(0)
{
	if (Property)
	{
		// Handle structs.
		if (UStructProperty* StructProperty = Cast<UStructProperty>(Property))
		{
			StructName = StructProperty->Struct->GetFName();
			StructGuid = StructProperty->Struct->GetCustomGuid();
		}
		else if (UEnumProperty* EnumProp = Cast<UEnumProperty>(Property))
		{
			if (UEnum* Enum = EnumProp->GetEnum())
			{
				EnumName = Enum->GetFName();
			}
		}
		else if (UByteProperty* ByteProp = Cast<UByteProperty>(Property))
		{
			if (ByteProp->Enum != nullptr)
			{
				EnumName = ByteProp->Enum->GetFName();
			}
		}
		else if (UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property))
		{
			InnerType = ArrayProp->Inner->GetID();
		}
		else if (USetProperty* SetProp = Cast<USetProperty>(Property))
		{
			InnerType = SetProp->ElementProp->GetID();
		}
		else if (UMapProperty* MapProp = Cast<UMapProperty>(Property))
		{
			InnerType = MapProp->KeyProp->GetID();
			ValueType = MapProp->ValueProp->GetID();
		}
		else if (UBoolProperty* Bool = Cast<UBoolProperty>(Property))
		{
			BoolVal = Bool->GetPropertyValue(Value);
		}
	}
}

// Set optional property guid
void FPropertyTag::SetPropertyGuid(const FGuid& InPropertyGuid)
{
	if (InPropertyGuid.IsValid())
	{
		PropertyGuid = InPropertyGuid;
		HasPropertyGuid = true;
	}
}

// Serializer.
FArchive& operator<<( FArchive& Ar, FPropertyTag& Tag )
{
	// Name.
	Ar << Tag.Name;
	if ((Tag.Name == NAME_None) || !Tag.Name.IsValid())
	{
		return Ar;
	}

	Ar << Tag.Type;
	if ( Ar.IsSaving() )
	{
		// remember the offset of the Size variable - UStruct::SerializeTaggedProperties will update it after the
		// property has been serialized.
		Tag.SizeOffset = Ar.Tell();
	}
	{
		FArchive::FScopeSetDebugSerializationFlags S(Ar, DSF_IgnoreDiff);
		Ar << Tag.Size << Tag.ArrayIndex;
	}
	// only need to serialize this for structs
	if (Tag.Type == NAME_StructProperty)
	{
		Ar << Tag.StructName;
		if (Ar.UE4Ver() >= VER_UE4_STRUCT_GUID_IN_PROPERTY_TAG)
		{
			Ar << Tag.StructGuid;
		}
	}
	// only need to serialize this for bools
	else if (Tag.Type == NAME_BoolProperty)
	{
		Ar << Tag.BoolVal;
	}
	// only need to serialize this for bytes/enums
	else if (Tag.Type == NAME_ByteProperty || Tag.Type == NAME_EnumProperty)
	{
		Ar << Tag.EnumName;
	}
	// only need to serialize this for arrays
	else if (Tag.Type == NAME_ArrayProperty)
	{
		if (Ar.UE4Ver() >= VAR_UE4_ARRAY_PROPERTY_INNER_TAGS)
		{
			Ar << Tag.InnerType;
		}
	}

	if (Ar.UE4Ver() >= VER_UE4_PROPERTY_TAG_SET_MAP_SUPPORT)
	{
		if (Tag.Type == NAME_SetProperty)
		{
			Ar << Tag.InnerType;
		}
		else if (Tag.Type == NAME_MapProperty)
		{
			Ar << Tag.InnerType;
			Ar << Tag.ValueType;
		}
	}

	// Property tags to handle renamed blueprint properties effectively.
	if (Ar.UE4Ver() >= VER_UE4_PROPERTY_GUID_IN_PROPERTY_TAG)
	{
		Ar << Tag.HasPropertyGuid;
		if (Tag.HasPropertyGuid)
		{
			Ar << Tag.PropertyGuid;
		}
	}

	return Ar;
}

// Property serializer.
void FPropertyTag::SerializeTaggedProperty( FArchive& Ar, UProperty* Property, uint8* Value, uint8* Defaults ) const
{
	if (Property->GetClass() == UBoolProperty::StaticClass())
	{
		UBoolProperty* Bool = (UBoolProperty*)Property;
		if (Ar.IsLoading())
		{
			Bool->SetPropertyValue(Value, BoolVal != 0);
		}
	}
	else
	{
#if WITH_EDITOR
		static const FName NAME_SerializeTaggedProperty = FName(TEXT("SerializeTaggedProperty"));
		FArchive::FScopeAddDebugData P(Ar, NAME_SerializeTaggedProperty);
		FArchive::FScopeAddDebugData A(Ar, Property->GetFName());
#endif
		FSerializedPropertyScope SerializedProperty(Ar, Property);
		Property->SerializeItem( Ar, Value, Defaults );
	}
}
