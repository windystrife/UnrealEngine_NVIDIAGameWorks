// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UnrealType.h"

/*-----------------------------------------------------------------------------
	UStrProperty.
-----------------------------------------------------------------------------*/

bool UStrProperty::ConvertFromType(const FPropertyTag& Tag, FArchive& Ar, uint8* Data, UStruct* DefaultsStruct, bool& bOutAdvanceProperty)
{
	// Convert serialized text to string.
	if (Tag.Type==NAME_TextProperty) 
	{ 
		FText Text;  
		Ar << Text;
		const FString String = FTextInspector::GetSourceString(Text) ? *FTextInspector::GetSourceString(Text) : TEXT("");
		SetPropertyValue_InContainer(Data, String, Tag.ArrayIndex);
		bOutAdvanceProperty = true;
	}
	else
	{
		bOutAdvanceProperty = false;
	}

	return bOutAdvanceProperty;
}

FString UStrProperty::GetCPPTypeForwardDeclaration() const
{
	return FString();
}

// Necessary to fix Compiler Error C2026 and C1091
FString UStrProperty::ExportCppHardcodedText(const FString& InSource, const FString& Indent)
{
	const FString Source = InSource.ReplaceCharWithEscapedChar();
	FString Result{};
	const int32 PreferredLineSize = 256;
	int32 StartPos = 0;

	const int32 LinesPerString = 16;
	const bool bUseSubStrings = InSource.Len() > (LinesPerString * PreferredLineSize);
	int32 LineNum = 0;
	if (bUseSubStrings)
	{
		Result += TEXT("*(FString(");
	}

	do
	{
		if (StartPos)
		{
			Result += TEXT("\n");
			Result += Indent;
		}

		++LineNum;
		if (bUseSubStrings && 0 == (LineNum % LinesPerString))
		{
			Result += TEXT(") + FString(");
		}

		int32 WantedSize = FMath::Min<int32>(Source.Len() - StartPos, PreferredLineSize);
		while (((WantedSize + StartPos) < Source.Len()) && (Source[WantedSize + StartPos - 1] == TCHAR('\\')))
		{
			WantedSize++;
		}
		Result += FString::Printf(TEXT("TEXT(\"%s\")"), *Source.Mid(StartPos, WantedSize));
		StartPos += WantedSize;
	} while (StartPos < Source.Len());

	if (bUseSubStrings)
	{
		Result += TEXT("))");
	}
	return Result;
}

void UStrProperty::ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const
{
	FString& StringValue = *(FString*)PropertyValue;
	if (0 != (PortFlags & PPF_ExportCpp))
	{
		ValueStr += FString::Printf(TEXT("FString(%s)"), *ExportCppHardcodedText(StringValue, FString()));
	}
	else if (!(PortFlags & PPF_Delimited))
	{
		ValueStr += StringValue;
	}
	else if ( StringValue.Len() > 0 )
	{
		ValueStr += FString::Printf( TEXT("\"%s\""), *(StringValue.ReplaceCharWithEscapedChar()) );
	}
	else
	{
		ValueStr += TEXT("\"\"");
	}
}
const TCHAR* UStrProperty::ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const
{
	if( !(PortFlags & PPF_Delimited) )
	{
		*(FString*)Data = Buffer;

		// in order to indicate that the value was successfully imported, advance the buffer past the last character that was imported
		Buffer += FCString::Strlen(Buffer);
	}
	else
	{
		// require quoted string here
		if (*Buffer != TCHAR('"'))
		{
			ErrorText->Logf(TEXT("Missing opening '\"' in string property value: %s"), Buffer);
			return NULL;
		}
		const TCHAR* Start = Buffer;
		FString Temp;
		Buffer = UPropertyHelpers::ReadToken(Buffer, Temp);
		if (Buffer == NULL)
		{
			return NULL;
		}
		if (Buffer > Start && Buffer[-1] != TCHAR('"'))
		{
			ErrorText->Logf(TEXT("Missing terminating '\"' in string property value: %s"), Start);
			return NULL;
		}
		*(FString*)Data = Temp;
	}
	return Buffer;
}

uint32 UStrProperty::GetValueTypeHashInternal(const void* Src) const
{
	return GetTypeHash(*(const FString*)Src);
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UStrProperty, UProperty,
	{
	}
);
