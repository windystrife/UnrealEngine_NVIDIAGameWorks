// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Internationalization/TextNamespaceUtil.h"

FString TextNamespaceUtil::BuildFullNamespace(const FString& InTextNamespace, const FString& InPackageNamespace, const bool bAlwaysApplyPackageNamespace)
{
	int32 StartMarkerIndex = INDEX_NONE;
	int32 EndMarkerIndex = InTextNamespace.Len() - 1;
	if (InTextNamespace.Len() > 0 && InTextNamespace[EndMarkerIndex] == PackageNamespaceEndMarker && InTextNamespace.FindLastChar(PackageNamespaceStartMarker, StartMarkerIndex))
	{
		FString FullNamespace = InTextNamespace;

		FullNamespace.RemoveAt(StartMarkerIndex + 1, EndMarkerIndex - StartMarkerIndex - 1, /*bAllowShrinking*/false);
		FullNamespace.InsertAt(StartMarkerIndex + 1, InPackageNamespace);

		return FullNamespace;
	}
	else if (bAlwaysApplyPackageNamespace)
	{
		if (InTextNamespace.IsEmpty())
		{
			return FString::Printf(TEXT("%c%s%c"), PackageNamespaceStartMarker, *InPackageNamespace, PackageNamespaceEndMarker);
		}
		else
		{
			return FString::Printf(TEXT("%s %c%s%c"), *InTextNamespace, PackageNamespaceStartMarker, *InPackageNamespace, PackageNamespaceEndMarker);
		}
	}

	return InTextNamespace;
}

FString TextNamespaceUtil::ExtractPackageNamespace(const FString& InTextNamespace)
{
	int32 StartMarkerIndex = INDEX_NONE;
	int32 EndMarkerIndex = InTextNamespace.Len() - 1;
	if (InTextNamespace.Len() > 0 && InTextNamespace[EndMarkerIndex] == PackageNamespaceEndMarker && InTextNamespace.FindLastChar(PackageNamespaceStartMarker, StartMarkerIndex))
	{
		return InTextNamespace.Mid(StartMarkerIndex + 1, EndMarkerIndex - StartMarkerIndex - 1);
	}

	return FString();
}

FString TextNamespaceUtil::StripPackageNamespace(const FString& InTextNamespace)
{
	int32 StartMarkerIndex = INDEX_NONE;
	int32 EndMarkerIndex = InTextNamespace.Len() - 1;
	if (InTextNamespace.Len() > 0 && InTextNamespace[EndMarkerIndex] == PackageNamespaceEndMarker && InTextNamespace.FindLastChar(PackageNamespaceStartMarker, StartMarkerIndex))
	{
		FString StrippedNamespace = InTextNamespace.Left(StartMarkerIndex);
		StrippedNamespace.TrimEndInline();

		return StrippedNamespace;
	}

	return InTextNamespace;
}

#if USE_STABLE_LOCALIZATION_KEYS

FString TextNamespaceUtil::GetPackageNamespace(FArchive& InArchive)
{
	return InArchive.GetLocalizationNamespace();
}

#endif // USE_STABLE_LOCALIZATION_KEYS
