// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "CollectionViewUtils.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "ICollectionManager.h"
#include "CollectionManagerModule.h"

#define LOCTEXT_NAMESPACE "CollectionView"

namespace CollectionViewUtils
{
	/** Create a string of the form "CollectionName:CollectionType */
	FString ToConfigKey(const FString& InCollectionName, const ECollectionShareType::Type& InCollectionType)
	{
		static_assert(ECollectionShareType::CST_All == 4, "Update CollectionViewUtils::ToConfigKey for the updated ECollectionShareType values");

		check(InCollectionType != ECollectionShareType::CST_All);

		FString CollectionTypeStr;
		switch(InCollectionType)
		{
		case ECollectionShareType::CST_System:
			CollectionTypeStr = "System";
			break;
		case ECollectionShareType::CST_Local:
			CollectionTypeStr = "Local";
			break;
		case ECollectionShareType::CST_Private:
			CollectionTypeStr = "Private";
			break;
		case ECollectionShareType::CST_Shared:
			CollectionTypeStr = "Shared";
			break;
		default:
			break;
		}

		return InCollectionName + ":" + CollectionTypeStr;
	}

	/** Convert a string of the form "CollectionName:CollectionType back into its individual elements */
	bool FromConfigKey(const FString& InKey, FString& OutCollectionName, ECollectionShareType::Type& OutCollectionType)
	{
		static_assert(ECollectionShareType::CST_All == 4, "Update CollectionViewUtils::FromConfigKey for the updated ECollectionShareType values");

		FString CollectionTypeStr;
		if(InKey.Split(":", &OutCollectionName, &CollectionTypeStr, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
		{
			if(CollectionTypeStr == "System")
			{
				OutCollectionType = ECollectionShareType::CST_System;
			}
			else if(CollectionTypeStr == "Local")
			{
				OutCollectionType = ECollectionShareType::CST_Local;
			}
			else if(CollectionTypeStr == "Private")
			{
				OutCollectionType = ECollectionShareType::CST_Private;
			}
			else if(CollectionTypeStr == "Shared")
			{
				OutCollectionType = ECollectionShareType::CST_Shared;
			}
			else
			{
				return false;
			}

			return !OutCollectionName.IsEmpty();
		}

		return false;
	}

	// Keep a map of all the collections that have custom colors, so updating the color in one location updates them all
	static TMap<FString, TSharedPtr<FLinearColor>> CollectionColors;
}

const TSharedPtr<FLinearColor> CollectionViewUtils::LoadColor(const FString& InCollectionName, const ECollectionShareType::Type& InCollectionType)
{
	check(InCollectionType != ECollectionShareType::CST_All);

	const FString ColorKeyStr = ToConfigKey(InCollectionName, InCollectionType);

	// See if we have a value cached first
	{
		TSharedPtr<FLinearColor>* const CachedColor = CollectionColors.Find(ColorKeyStr);
		if(CachedColor)
		{
			return *CachedColor;
		}
	}
		
	// Loads the color of collection at the given path from the config
	if(FPaths::FileExists(GEditorPerProjectIni))
	{
		// Create a new entry from the config, skip if it's default
		FString ColorStr;
		if(GConfig->GetString(TEXT("CollectionColor"), *ColorKeyStr, ColorStr, GEditorPerProjectIni))
		{
			FLinearColor Color;
			if(Color.InitFromString(ColorStr) && !Color.Equals(CollectionViewUtils::GetDefaultColor()))
			{
				return CollectionColors.Add(ColorKeyStr, MakeShareable(new FLinearColor(Color)));
			}
		}
		else
		{
			return CollectionColors.Add(ColorKeyStr, MakeShareable(new FLinearColor(CollectionViewUtils::GetDefaultColor())));
		}
	}

	return nullptr;
}

void CollectionViewUtils::SaveColor(const FString& InCollectionName, const ECollectionShareType::Type& InCollectionType, const TSharedPtr<FLinearColor> CollectionColor, const bool bForceAdd)
{
	check(InCollectionType != ECollectionShareType::CST_All);

	const FString ColorKeyStr = ToConfigKey(InCollectionName, InCollectionType);

	// Remove the color if it's invalid or default
	const bool bRemove = !CollectionColor.IsValid() || (!bForceAdd && CollectionColor->Equals(CollectionViewUtils::GetDefaultColor()));

	// Saves the color of the collection to the config
	if(FPaths::FileExists(GEditorPerProjectIni))
	{
		// If this is no longer custom, remove it
		if(bRemove)
		{
			GConfig->RemoveKey(TEXT("CollectionColor"), *ColorKeyStr, GEditorPerProjectIni);
		}
		else
		{
			GConfig->SetString(TEXT("CollectionColor"), *ColorKeyStr, *CollectionColor->ToString(), GEditorPerProjectIni);
		}
	}

	// Update the map too
	if(bRemove)
	{
		CollectionColors.Remove(ColorKeyStr);
	}
	else
	{
		CollectionColors.Add(ColorKeyStr, CollectionColor);
	}
}

bool CollectionViewUtils::HasCustomColors( TArray< FLinearColor >* OutColors )
{
	if(!FPaths::FileExists(GEditorPerProjectIni))
	{
		return false;
	}

	// Read individual entries from a config file.
	TArray<FString> Section;
	GConfig->GetSection(TEXT("CollectionColor"), Section, GEditorPerProjectIni);

	bool bHasCustom = false;
	const FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
	const ICollectionManager& CollectionManager = CollectionManagerModule.Get();

	for(FString& EntryStr : Section)
	{
		EntryStr.TrimStartInline();

		FString ColorKeyStr;
		FString ColorStr;
		if(!EntryStr.Split("=", &ColorKeyStr, &ColorStr))
		{
			continue;
		}

		// Ignore any that have invalid or default colors
		FLinearColor CurrentColor;
		if(!CurrentColor.InitFromString(ColorStr) || CurrentColor.Equals(CollectionViewUtils::GetDefaultColor()))
		{
			continue;
		}

		// Ignore any that reference old collections
		FString CollectionName;
		ECollectionShareType::Type CollectionType;
		if(!FromConfigKey(ColorKeyStr, CollectionName, CollectionType) || !CollectionManager.CollectionExists(*CollectionName, CollectionType))
		{
			continue;
		}

		bHasCustom = true;
		if(OutColors)
		{
			// Only add if not already present (ignores near matches too)
			bool bAdded = false;
			for(const FLinearColor& Color : *OutColors)
			{
				if(CurrentColor.Equals(Color))
				{
					bAdded = true;
					break;
				}
			}
			if(!bAdded)
			{
				OutColors->Add(CurrentColor);
			}
		}
		else
		{
			break;
		}
	}

	return bHasCustom;
}

FLinearColor CollectionViewUtils::GetDefaultColor()
{
	// The default tint the folder should appear as
	return FLinearColor::Gray;
}

#undef LOCTEXT_NAMESPACE
