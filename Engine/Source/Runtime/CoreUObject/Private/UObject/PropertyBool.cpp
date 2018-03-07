// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "UObject/PropertyHelper.h"

/*-----------------------------------------------------------------------------
	UBoolProperty.
-----------------------------------------------------------------------------*/

UBoolProperty::UBoolProperty( const FObjectInitializer& ObjectInitializer )
: UProperty( ObjectInitializer )
, FieldSize(0)
, ByteOffset(0)
, ByteMask(1)
, FieldMask(1)
{
	SetBoolSize( 1, false, 1 );
}

UBoolProperty::UBoolProperty(ECppProperty, int32 InOffset, uint64 InFlags, uint32 InBitMask, uint32 InElementSize, bool bIsNativeBool)
	: UProperty(FObjectInitializer::Get(), EC_CppProperty, InOffset, InFlags | CPF_HasGetValueTypeHash)
	, FieldSize(0)
	, ByteOffset(0)
	, ByteMask(1)
	, FieldMask(1)
{
	SetBoolSize(InElementSize, bIsNativeBool, InBitMask);
}

UBoolProperty::UBoolProperty( const FObjectInitializer& ObjectInitializer, ECppProperty, int32 InOffset, uint64 InFlags, uint32 InBitMask, uint32 InElementSize, bool bIsNativeBool )
: UProperty( ObjectInitializer, EC_CppProperty, InOffset, InFlags | CPF_HasGetValueTypeHash)
, FieldSize(0)
, ByteOffset(0)
, ByteMask(1)
, FieldMask(1)
{
	SetBoolSize( InElementSize, bIsNativeBool, InBitMask );
}

void UBoolProperty::SetBoolSize( const uint32 InSize, const bool bIsNativeBool, const uint32 InBitMask /*= 0*/ )
{
	if (bIsNativeBool)
	{
		PropertyFlags |= (CPF_IsPlainOldData | CPF_NoDestructor | CPF_ZeroConstructor);
	}
	else
	{
		PropertyFlags &= ~(CPF_IsPlainOldData | CPF_ZeroConstructor);
		PropertyFlags |= CPF_NoDestructor;
	}
	uint32 TestBitmask = InBitMask ? InBitMask : 1;
	ElementSize = InSize;
	FieldSize = (uint8)ElementSize;
	ByteOffset = 0;
	if (bIsNativeBool)
	{		
		ByteMask = true;
		FieldMask = 255;
	}
	else
	{
		// Calculate ByteOffset and get ByteMask.
		for (ByteOffset = 0; ByteOffset < InSize && ((ByteMask = *((uint8*)&TestBitmask + ByteOffset)) == 0); ByteOffset++);
		FieldMask = ByteMask;
	}
	check((int32)FieldSize == ElementSize);
	check(ElementSize != 0);
	check(FieldMask != 0);
	check(ByteMask != 0);
}

int32 UBoolProperty::GetMinAlignment() const
{
	int32 Alignment = 0;
	switch(ElementSize)
	{
	case sizeof(uint8):
		Alignment = alignof(uint8); break;
	case sizeof(uint16):
		Alignment = alignof(uint16); break;
	case sizeof(uint32):
		Alignment = alignof(uint32); break;
	case sizeof(uint64):
		Alignment = alignof(uint64); break;
	default:
		UE_LOG(LogProperty, Fatal, TEXT("Unsupported UBoolProperty %s size %d."), *GetName(), (int32)ElementSize);
	}
	return Alignment;
}
void UBoolProperty::LinkInternal(FArchive& Ar)
{
	check(FieldSize != 0);
	ElementSize = FieldSize;
	if (IsNativeBool())
	{
		PropertyFlags |= (CPF_IsPlainOldData | CPF_NoDestructor | CPF_ZeroConstructor);
	}
	else
	{
		PropertyFlags &= ~(CPF_IsPlainOldData | CPF_ZeroConstructor);
		PropertyFlags |= CPF_NoDestructor;
	}
}
void UBoolProperty::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	// Serialize additional flags which will help to identify this UBoolProperty type and size.
	uint8 BoolSize = (uint8)ElementSize;
	Ar << BoolSize;
	uint8 NativeBool = false;
	if( Ar.IsLoading())
	{
		Ar << NativeBool;
		if (!IsPendingKill())
		{
			SetBoolSize( BoolSize, !!NativeBool );
		}
	}
	else
	{
		NativeBool = (!HasAnyFlags(RF_ClassDefaultObject) && !IsPendingKill() && Ar.IsSaving()) ? (IsNativeBool() ? 1 : 0) : 0;
		Ar << NativeBool;
	}
}
FString UBoolProperty::GetCPPType( FString* ExtendedTypeText/*=NULL*/, uint32 CPPExportFlags/*=0*/ ) const
{
	check(FieldSize != 0);

	if (IsNativeBool() 
		|| ((CPPExportFlags & (CPPF_Implementation|CPPF_ArgumentOrReturnValue)) == (CPPF_Implementation|CPPF_ArgumentOrReturnValue))
		|| ((CPPExportFlags & CPPF_BlueprintCppBackend) != 0))
	{
		// Export as bool if this is actually a bool or it's being exported as a return value of C++ function definition.
		return TEXT("bool");
	}
	else
	{
		// Bitfields
		switch(ElementSize)
		{
		case sizeof(uint64):
			return TEXT("uint64");
		case sizeof(uint32):
			return TEXT("uint32");
		case sizeof(uint16):
			return TEXT("uint16");
		case sizeof(uint8):
			return TEXT("uint8");
		default:
			UE_LOG(LogProperty, Fatal, TEXT("Unsupported UBoolProperty %s size %d."), *GetName(), ElementSize);
			break;
		}
	}
	return TEXT("uint32");
}

FString UBoolProperty::GetCPPTypeForwardDeclaration() const
{
	return FString();
}

FString UBoolProperty::GetCPPMacroType( FString& ExtendedTypeText ) const
{
	check(FieldSize != 0);
	if (IsNativeBool())
	{
		return TEXT("UBOOL");
	}
	else
	{
		switch(ElementSize)
		{
		case sizeof(uint64):
			return TEXT("UBOOL64");
		case sizeof(uint32):
			return TEXT("UBOOL32");
		case sizeof(uint16):
			return TEXT("UBOOL16");
		case sizeof(uint8):
			return TEXT("UBOOL8");
		default:
			UE_LOG(LogProperty, Fatal, TEXT("Unsupported UBoolProperty %s size %d."), *GetName(), ElementSize);
			break;
		}
	}
	return TEXT("UBOOL32");
}

template<typename T>
void LoadFromType(UBoolProperty* Property, const FPropertyTag& Tag, FArchive& Ar, uint8* Data)
{
	T IntValue;
	Ar << IntValue;

	if (IntValue != 0)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (IntValue != 1)
		{
			UE_LOG(LogClass, Log, TEXT("Loading %s property (%s) that is now a bool - value '%d', expecting 0 or 1. Value set to true."), *Tag.Type.ToString(), *Property->GetPathName(), IntValue);
		}
#endif
		Property->SetPropertyValue_InContainer(Data, true, Tag.ArrayIndex);
	}
	else
	{
		Property->SetPropertyValue_InContainer(Data, false, Tag.ArrayIndex);
	}
}

bool UBoolProperty::ConvertFromType(const FPropertyTag& Tag, FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, bool& bOutAdvanceProperty)
{
	bOutAdvanceProperty = true;

	if (Tag.Type == NAME_IntProperty)
	{
		LoadFromType<int32>(this, Tag, Ar, Data);
	}
	else if (Tag.Type == NAME_Int8Property)
	{
		LoadFromType<int8>(this, Tag, Ar, Data);
	}
	else if (Tag.Type == NAME_Int16Property)
	{
		LoadFromType<int16>(this, Tag, Ar, Data);
	}
	else if (Tag.Type == NAME_Int64Property)
	{
		LoadFromType<int64>(this, Tag, Ar, Data);
	}
	else if (Tag.Type == NAME_ByteProperty)
	{
		// if the byte property was an enum we won't allow a conversion to bool
		if (Tag.EnumName == NAME_None)
		{
			// If we're a nested property the EnumName tag got lost, don't allow this
			UProperty* const PropertyOwner = Cast<UProperty>(GetOuterUField());

			if (PropertyOwner)
			{
				bOutAdvanceProperty = false;
				return bOutAdvanceProperty;
			}

			LoadFromType<uint8>(this, Tag, Ar, Data);
		}
		else
		{
			bOutAdvanceProperty = false;
		}
	}
	else if (Tag.Type == NAME_UInt16Property)
	{
		LoadFromType<uint16>(this, Tag, Ar, Data);
	}
	else if (Tag.Type == NAME_UInt32Property)
	{
		LoadFromType<uint32>(this, Tag, Ar, Data);
	}
	else if (Tag.Type == NAME_UInt64Property)
	{
		LoadFromType<uint64>(this, Tag, Ar, Data);
	}
	else
	{
		bOutAdvanceProperty = false;
	}
			
	return bOutAdvanceProperty;
}

void UBoolProperty::ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const
{
	check(FieldSize != 0);
	const uint8* ByteValue = (uint8*)PropertyValue + ByteOffset;
	const bool bValue = 0 != ((*ByteValue) & FieldMask);
	const TCHAR* Temp = nullptr;
	if (0 != (PortFlags & PPF_ExportCpp))
	{
		Temp = (bValue ? TEXT("true") : TEXT("false"));
	}
	else
	{
		Temp = (bValue ? TEXT("True") : TEXT("False"));
	}
	ValueStr += FString::Printf( TEXT("%s"), Temp );
}
const TCHAR* UBoolProperty::ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const
{
	FString Temp; 
	Buffer = UPropertyHelpers::ReadToken( Buffer, Temp );
	if( !Buffer )
	{
		return NULL;
	}

	check(FieldSize != 0);
	uint8* ByteValue = (uint8*)Data + ByteOffset;
	if( Temp==TEXT("1") || Temp==TEXT("True") || Temp==*(GTrue.ToString()) || Temp == TEXT("Yes") || Temp == *(GYes.ToString()) )
	{
		*ByteValue |= ByteMask;
	}
	else 
	if( Temp==TEXT("0") || Temp==TEXT("False") || Temp==*(GFalse.ToString()) || Temp == TEXT("No") || Temp == *(GNo.ToString()) )
	{
		*ByteValue &= ~FieldMask;
	}
	else
	{
		//UE_LOG(LogProperty, Log,  "Import: Failed to get bool" );
		return NULL;
	}
	return Buffer;
}
bool UBoolProperty::Identical( const void* A, const void* B, uint32 PortFlags ) const
{
	check(FieldSize != 0);
	const uint8* ByteValueA = (const uint8*)A + ByteOffset;
	const uint8* ByteValueB = (const uint8*)B + ByteOffset;
	return ((*ByteValueA ^ (B ? *ByteValueB : 0)) & FieldMask) == 0;
}

void UBoolProperty::SerializeItem( FArchive& Ar, void* Value, void const* Defaults ) const
{
	check(FieldSize != 0);
	uint8* ByteValue = (uint8*)Value + ByteOffset;
	uint8 B = (*ByteValue & FieldMask) ? 1 : 0;
	Ar << B;
	*ByteValue = ((*ByteValue) & ~FieldMask) | (B ? ByteMask : 0);
}

bool UBoolProperty::NetSerializeItem( FArchive& Ar, UPackageMap* Map, void* Data, TArray<uint8> * MetaData ) const
{
	check(FieldSize != 0);
	uint8* ByteValue = (uint8*)Data + ByteOffset;
	uint8 Value = ((*ByteValue & FieldMask)!=0);
	Ar.SerializeBits( &Value, 1 );
	*ByteValue = ((*ByteValue) & ~FieldMask) | (Value ? ByteMask : 0);
	return true;
}
void UBoolProperty::CopyValuesInternal( void* Dest, void const* Src, int32 Count  ) const
{
	check(FieldSize != 0 && !IsNativeBool());
	for (int32 Index = 0; Index < Count; Index++)
	{
		uint8* DestByteValue = (uint8*)Dest + Index * ElementSize + ByteOffset;
		uint8* SrcByteValue = (uint8*)Src + Index * ElementSize + ByteOffset;
		*DestByteValue = (*DestByteValue & ~FieldMask) | (*SrcByteValue & FieldMask);
	}
}
void UBoolProperty::ClearValueInternal( void* Data ) const
{
	check(FieldSize != 0);
	uint8* ByteValue = (uint8*)Data + ByteOffset;
	*ByteValue &= ~FieldMask;
}

void UBoolProperty::InitializeValueInternal( void* Data ) const
{
	check(FieldSize != 0);
	uint8* ByteValue = (uint8*)Data + ByteOffset;
	*ByteValue &= ~FieldMask;
}

uint32 UBoolProperty::GetValueTypeHashInternal(const void* Src) const
{
	return GetTypeHash(*(const bool*)Src);
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UBoolProperty, UProperty,
	{
	}
);

