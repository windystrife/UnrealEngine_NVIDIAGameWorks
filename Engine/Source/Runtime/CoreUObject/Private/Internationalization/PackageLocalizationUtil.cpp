// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Internationalization/PackageLocalizationUtil.h"
#include "Misc/PackageName.h"

bool FPackageLocalizationUtil::ConvertLocalizedToSource(const FString& InLocalized, FString& OutSource)
{
	if (FPackageName::IsLocalizedPackage(InLocalized))
	{
		const int32 L10NStart = InLocalized.Find(TEXT("L10N"), ESearchCase::IgnoreCase);
		if (L10NStart != INDEX_NONE)
		{
			// /Game/L10N/fr/Folder/MyAsset
			//       ^ We matched here, so we need to walk over the L10N folder, and then walk over the culture code to find the range of characters to remove
			int32 NumSlashesToFind = 2;
			int32 L10NEnd = L10NStart;
			for (; L10NEnd < InLocalized.Len() && NumSlashesToFind > 0; ++L10NEnd)
			{
				if (InLocalized[L10NEnd] == TEXT('/'))
				{
					--NumSlashesToFind;
				}
			}

			OutSource = InLocalized;
			OutSource.RemoveAt(L10NStart, L10NEnd - L10NStart, false);
			return true;
		}
	}

	return false;
}

bool FPackageLocalizationUtil::ConvertSourceToLocalized(const FString& InSource, const FString& InCulture, FString& OutLocalized)
{
	if (!FPackageName::IsLocalizedPackage(InSource))
	{
		if (InSource.Len() > 0 && InSource[0] == TEXT('/'))
		{
			const int32 RootPathEnd = InSource.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromStart, 1);
			if (RootPathEnd != INDEX_NONE)
			{
				OutLocalized = InSource;
				OutLocalized.InsertAt(RootPathEnd, TEXT("/L10N") / InCulture);
				return true;
			}
		}
	}

	return false;
}

bool FPackageLocalizationUtil::GetLocalizedRoot(const FString& InPath, const FString& InCulture, FString& OutLocalized)
{
	if (InPath.Len() > 0 && InPath[0] == TEXT('/'))
	{
		const int32 RootPathEnd = InPath.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromStart, 1);
		if (RootPathEnd != INDEX_NONE)
		{
			OutLocalized = InPath.Left(RootPathEnd);
			OutLocalized /= TEXT("L10N");
			if (InCulture.Len() > 0)
			{
				OutLocalized /= InCulture;
			}
			return true;
		}
	}

	return false;
}
