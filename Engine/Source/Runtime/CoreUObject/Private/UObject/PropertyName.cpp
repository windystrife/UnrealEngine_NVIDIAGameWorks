// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UnrealType.h"

/*-----------------------------------------------------------------------------
	UNameProperty.
-----------------------------------------------------------------------------*/

void UNameProperty::ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const
{
	FName Temp = *(FName*)PropertyValue;
	if (0 != (PortFlags & PPF_ExportCpp))
	{
		ValueStr += (Temp == NAME_None) 
			? TEXT("FName()") 
			: FString::Printf(TEXT("FName(TEXT(\"%s\"))"), *(Temp.ToString().ReplaceCharWithEscapedChar()));
	}
	else if( !(PortFlags & PPF_Delimited) )
	{
		ValueStr += Temp.ToString();
	}
	else if ( Temp != NAME_None )
	{
		ValueStr += FString::Printf( TEXT("\"%s\""), *Temp.ToString().ReplaceCharWithEscapedChar() );
	}
	else
	{
		ValueStr += TEXT("\"\"");
	}
}
const TCHAR* UNameProperty::ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const
{
	if (!(PortFlags & PPF_Delimited))
	{
		*(FName*)Data = FName(Buffer, FNAME_Add);

		// in order to indicate that the value was successfully imported, advance the buffer past the last character that was imported
		Buffer += FCString::Strlen(Buffer);
	}
	else
	{
		FString Temp;
		Buffer = UPropertyHelpers::ReadToken(Buffer, Temp, true);
		if (!Buffer)
			return NULL;

		*(FName*)Data = FName(*Temp, FNAME_Add);
	}
	return Buffer;
}

bool UNameProperty::ConvertFromType(const FPropertyTag& Tag, FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, bool& bOutAdvanceProperty)
{
	if (Tag.Type == NAME_StrProperty)
	{
		FString str;
		Ar << str;
		SetPropertyValue_InContainer(Data, FName(*str), Tag.ArrayIndex);
		bOutAdvanceProperty = true;
	}
	// Convert serialized text to name.
	else if (Tag.Type == NAME_TextProperty)
	{ 
		FText Text;  
		Ar << Text;
		const FName Name = FName(*Text.ToString());
		SetPropertyValue_InContainer(Data, Name, Tag.ArrayIndex);
		bOutAdvanceProperty = true; 
	}
	else
	{
		bOutAdvanceProperty = false;
	}

	return bOutAdvanceProperty;
}

FString UNameProperty::GetCPPTypeForwardDeclaration() const
{
	return FString();
}

uint32 UNameProperty::GetValueTypeHashInternal(const void* Src) const
{
	return GetTypeHash(*(const FName*)Src);
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UNameProperty, UProperty,
	{
	}
);

