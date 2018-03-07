// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "Templates/Casts.h"
#include "UObject/UnrealType.h"

int64 UNumericProperty::ReadEnumAsInt64(FArchive& Ar, UStruct* DefaultsStruct, const FPropertyTag& Tag)
{
	//@warning: mirrors loading code in UByteProperty::SerializeItem() and UEnumProperty::SerializeItem()
	FName EnumName;
	Ar << EnumName;

	UEnum* Enum = FindField<UEnum>(dynamic_cast<UClass*>(DefaultsStruct) ? static_cast<UClass*>(DefaultsStruct) : DefaultsStruct->GetTypedOuter<UClass>(), Tag.EnumName);
	if (!Enum)
	{
		Enum = FindObject<UEnum>(ANY_PACKAGE, *Tag.EnumName.ToString());
	}

	if (!Enum)
	{
		UE_LOG(LogClass, Warning, TEXT("Failed to find enum '%s' when converting property '%s' during property loading - setting to 0"), *Tag.EnumName.ToString(), *Tag.Name.ToString());
		return 0;
	}

	Ar.Preload(Enum);

	// This handles redirects internally
	int64 Result = Enum->GetValueByName(EnumName);
	if (!Enum->IsValidEnumValue(Result))
	{
		UE_LOG(
			LogClass,
			Warning,
			TEXT("Failed to find valid enum value '%s' for enum type '%s' when converting property '%s' during property loading - setting to '%s'"),
			*EnumName.ToString(),
			*Enum->GetName(),
			*Tag.Name.ToString(),
			*Enum->GetNameByValue(Enum->GetMaxEnumValue()).ToString()
			);

		return Enum->GetMaxEnumValue();
	}

	return Result;
};

const TCHAR* UNumericProperty::ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const
{
	if ( Buffer != NULL )
	{
		const TCHAR* Start = Buffer;
		if (IsInteger())
		{
			if (FChar::IsAlpha(*Buffer))
			{
				int64 EnumValue = UEnum::ParseEnum(Buffer);
				if (EnumValue != INDEX_NONE)
				{
					SetIntPropertyValue(Data, EnumValue);
					return Buffer;
				}
				else
				{
					return NULL;
				}
			}
			else
			{
				if ( !FCString::Strnicmp(Start,TEXT("0x"),2) )
				{
					Buffer+=2;
					while ( Buffer && (FParse::HexDigit(*Buffer) != 0 || *Buffer == TCHAR('0')) )
					{
						Buffer++;
					}
				}
				else
				{
					while ( Buffer && (*Buffer == TCHAR('-') || *Buffer == TCHAR('+')) )
					{
						Buffer++;
					}

					while ( Buffer &&  FChar::IsDigit(*Buffer) )
					{
						Buffer++;
					}
				}

				if (Start == Buffer)
				{
					// import failure
					return NULL;
				}
			}
		}
		else
		{
			check(IsFloatingPoint());
			// floating point
			while( *Buffer == TCHAR('+') || *Buffer == TCHAR('-') || *Buffer == TCHAR('.') || (*Buffer >= TCHAR('0') && *Buffer <= TCHAR('9')) )
			{
				Buffer++;
			}
			if ( *Buffer == TCHAR('f') || *Buffer == TCHAR('F') )
			{
				Buffer++;
			}
		}
		SetNumericPropertyValueFromString(Data, Start);
	}
	return Buffer;
}

void UNumericProperty::ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const
{
	ValueStr += GetNumericPropertyValueToString(PropertyValue);
}

bool UNumericProperty::IsFloatingPoint() const
{
	return false;
}

bool UNumericProperty::IsInteger() const
{
	return true;
}

UEnum* UNumericProperty::GetIntPropertyEnum() const
{
	return nullptr;
}

/** 
	* Set the value of an unsigned integral property type
	* @param Data - pointer to property data to set
	* @param Value - Value to set data to
**/
void UNumericProperty::SetIntPropertyValue(void* Data, uint64 Value) const
{
	check(0);
}

/** 
	* Set the value of a signed integral property type
	* @param Data - pointer to property data to set
	* @param Value - Value to set data to
**/
void UNumericProperty::SetIntPropertyValue(void* Data, int64 Value) const
{
	check(0);
}

/** 
	* Set the value of a floating point property type
	* @param Data - pointer to property data to set
	* @param Value - Value to set data to
**/
void UNumericProperty::SetFloatingPointPropertyValue(void* Data, double Value) const
{
	check(0);
}

/** 
	* Set the value of any numeric type from a string point
	* @param Data - pointer to property data to set
	* @param Value - Value (as a string) to set 
	* CAUTION: This routine does not do enum name conversion
**/
void UNumericProperty::SetNumericPropertyValueFromString(void* Data, TCHAR const* Value) const
{
	check(0);
}

/** 
	* Gets the value of a signed integral property type
	* @param Data - pointer to property data to get
	* @return Data as a signed int
**/
int64 UNumericProperty::GetSignedIntPropertyValue(void const* Data) const
{
	check(0);
	return 0;
}

/** 
	* Gets the value of an unsigned integral property type
	* @param Data - pointer to property data to get
	* @return Data as an unsigned int
**/
uint64 UNumericProperty::GetUnsignedIntPropertyValue(void const* Data) const
{
	check(0);
	return 0;
}

/** 
	* Gets the value of an floating point property type
	* @param Data - pointer to property data to get
	* @return Data as a double
**/
double UNumericProperty::GetFloatingPointPropertyValue(void const* Data) const
{
	check(0);
	return 0.0;
}

/** 
	* Get the value of any numeric type and return it as a string
	* @param Data - pointer to property data to get
	* @return Data as a string
	* CAUTION: This routine does not do enum name conversion
**/
FString UNumericProperty::GetNumericPropertyValueToString(void const* Data) const
{
	check(0);
	return FString();
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UNumericProperty, UProperty,
{
}
);
