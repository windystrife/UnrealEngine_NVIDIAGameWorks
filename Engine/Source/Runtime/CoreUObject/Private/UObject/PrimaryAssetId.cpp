// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UObject/PrimaryAssetId.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UnrealType.h"
#include "UObject/ObjectRedirector.h"

bool FPrimaryAssetType::ExportTextItem(FString& ValueStr, FPrimaryAssetType const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	if (0 != (PortFlags & PPF_ExportCpp))
	{
		ValueStr += FString::Printf(TEXT("FPrimaryAssetType(TEXT(\"%s\"))"), *ToString().ReplaceCharWithEscapedChar());
	}
	else if (!(PortFlags & PPF_Delimited))
	{
		ValueStr += ToString();
	}
	else
	{
		ValueStr += FString::Printf(TEXT("\"%s\""), *ToString().ReplaceCharWithEscapedChar());
	}

	return true;
}

bool FPrimaryAssetType::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
{
	// This handles both quoted and unquoted
	FString ImportedString = TEXT("");
	const TCHAR* NewBuffer = UPropertyHelpers::ReadToken(Buffer, ImportedString, 1);

	if (!NewBuffer)
	{
		return false;
	}

	*this = FPrimaryAssetType(*ImportedString);
	Buffer = NewBuffer;

	return true;
}

bool FPrimaryAssetType::SerializeFromMismatchedTag(struct FPropertyTag const& Tag, FArchive& Ar)
{
	if (Tag.Type == NAME_NameProperty)
	{
		FName InName;
		Ar << InName;
		*this = FPrimaryAssetType(InName);
		return true;
	}
	else if (Tag.Type == NAME_StrProperty)
	{
		FString InString;
		Ar << InString;
		*this = FPrimaryAssetType(*InString);
		return true;
	}

	return false;
}

bool FPrimaryAssetId::ExportTextItem(FString& ValueStr, FPrimaryAssetId const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	if (0 != (PortFlags & PPF_ExportCpp))
	{
		ValueStr += FString::Printf(TEXT("FPrimaryAssetId(TEXT(\"%s\"))"), *ToString().ReplaceCharWithEscapedChar());
	}
	else if (!(PortFlags & PPF_Delimited))
	{
		ValueStr += ToString();
	}
	else
	{
		ValueStr += FString::Printf(TEXT("\"%s\""), *ToString().ReplaceCharWithEscapedChar());
	}

	return true;
}

bool FPrimaryAssetId::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
{
	// This handles both quoted and unquoted
	FString ImportedString = TEXT("");
	const TCHAR* NewBuffer = UPropertyHelpers::ReadToken(Buffer, ImportedString, 1);

	if (!NewBuffer)
	{
		return false;
	}

	*this = FPrimaryAssetId(ImportedString);
	Buffer = NewBuffer;

	return true;
}

bool FPrimaryAssetId::SerializeFromMismatchedTag(struct FPropertyTag const& Tag, FArchive& Ar)
{
	if (Tag.Type == NAME_NameProperty)
	{
		FName InName;
		Ar << InName;
		*this = FPrimaryAssetId(InName.ToString());
		return true;
	}
	else if (Tag.Type == NAME_StrProperty)
	{
		FString InString;
		Ar << InString;
		*this = FPrimaryAssetId(InString);
		return true;
	}

	return false;
}

